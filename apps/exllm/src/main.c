#include <graphics/drawing.h>
#include <graphics/color.h>
#include <graphics/text.h>
#include <graphics/init.h>
#include <graphics/lcdc.h>
#include <sh4a/input/keypad.h>
#include <syscalls/syscalls.h>
#include <stdio.h>
#include <string.h>
#include "libc/memmgr.h"
#include "ui_jp_font.h"
#include "runtime_data.h"
#include "prompt_input.h"
#include "model_config.h"
#include "screenshot.h"
#define SW 528
#define SH 320
#define FINAL_MAX_TOKENS 48
#define THINK_DRAFT_MAX 10
typedef struct { unsigned long rows,cols,nscale,nbytes; const unsigned char *scale,*data; } Tensor;
typedef struct { Tensor tok,pos,n1[LAYERS],qkv[LAYERS],proj[LAYERS],n2[LAYERS],fc1[LAYERS],fc2[LAYERS],norm; } Model;
typedef struct { char sig[12]; char model[4]; unsigned long magic,nor_size; } RomHeader;
static unsigned char *model_mem; static short *key_cache,*val_cache; static unsigned long probe_free_kib;
static short x[D],h[D],qkv[3*D],att[D],ff[FF],tmp[D]; static long scores[CTX]; static char model_path[48];
static const char *example_questions[]={"こんにちは","あなたは何というモデルですか？","RAMとは何ですか？","オフラインとは何ですか？"};
static unsigned long u16(const unsigned char *p){return p[0]|((unsigned long)p[1]<<8);}
static unsigned long u32(const unsigned char *p){return p[0]|((unsigned long)p[1]<<8)|((unsigned long)p[2]<<16)|((unsigned long)p[3]<<24);}
static long s32(const unsigned char *p){return (long)u32(p);} static short s16(const unsigned char *p){return (short)(p[0]|((unsigned short)p[1]<<8));}
static short sat(long v){if(v>32767)return 32767;if(v<-32768)return -32768;return (short)v;}
static void clear(void){set_pen(create_rgb16(0,0,0));draw_rect(0,0,SW,SH);}
static void title(void){set_pen(create_rgb16(0,255,255));render_text(12,12,"EXLLM v1.1.0");}
static void screen(const char *jp,const char *en,unsigned short c){clear();title();set_pen(c);render_text_jp(12,52,jp);if(en){set_pen(create_rgb16(255,255,255));render_text(12,82,en);}lcdc_copy_vram();}
static int load_model(Model *m){
 static const unsigned char magic[8]={'E','X','Q','1','2',0,0,0}; Tensor *order[EX_TENSOR_COUNT]; unsigned long size,off=16,count,i,l; int fd;
 order[0]=&m->tok;order[1]=&m->pos;for(l=0;l<LAYERS;l++){order[2+l*6]=&m->n1[l];order[3+l*6]=&m->qkv[l];order[4+l*6]=&m->proj[l];order[5+l*6]=&m->n2[l];order[6+l*6]=&m->fc1[l];order[7+l*6]=&m->fc2[l];}order[2+6*LAYERS]=&m->norm;
 sprintf(model_path,"\\\\drv0\\MODELS\\model.q12");fd=sys_open(model_path,FILE_RD);if(fd<0)return -1;size=sys_get_filesize(fd);if(size!=EX_MODEL_BYTES){sys_close(fd);return -2;}
 model_mem=memmgr_alloc(size);if(!model_mem){sys_close(fd);return -3;}{unsigned long got=0;while(got<size){int n=sys_read(fd,model_mem+got,size-got);if(n<=0){sys_close(fd);return -4;}got+=n;}}sys_close(fd);
 if(memcmp(model_mem,magic,8)||u32(model_mem+8)!=1)return -5;count=u32(model_mem+12);if(count!=EX_TENSOR_COUNT)return -6;
 for(i=0;i<count;i++){unsigned long nl=u16(model_mem+off),nd=model_mem[off+2],qt=model_mem[off+3];Tensor *t=order[i];off+=4+nl;if(nd<1||nd>2||(qt!=1&&qt!=3))return -7;t->rows=u32(model_mem+off);off+=4;t->cols=nd==2?u32(model_mem+off):1;off+=nd==2?4:0;t->nscale=u32(model_mem+off);t->nbytes=u32(model_mem+off+4);off+=8;t->scale=model_mem+off;off+=t->nscale*4;t->data=model_mem+off;off+=t->nbytes;}
 key_cache=memmgr_alloc(LAYERS*CTX*D*2);val_cache=memmgr_alloc(LAYERS*CTX*D*2);if(!key_cache||!val_cache)return -8;return 0;
}
static void row(const Tensor *t,unsigned long r,short *out){unsigned long j;long sc=s32(t->scale+r*4);const signed char *w=(const signed char *)(t->data+r*t->cols);for(j=0;j<t->cols;j++)out[j]=sat(((long)w[j]*sc)>>8);}
static void matvec(const Tensor *t,const short *in,short *out){unsigned long r,j;for(r=0;r<t->rows;r++){long long sum=0;const signed char *w=(const signed char *)(t->data+r*t->cols);long sc=s32(t->scale+r*4);for(j=0;j<t->cols;j++)sum+=(long)w[j]*in[j];out[r]=sat((long)((sum*sc)>>20));}}
static unsigned long isqrt(unsigned long n){unsigned long r=0,bit=1UL<<30;while(bit>n)bit>>=2;while(bit){if(n>=r+bit){n-=r+bit;r=(r>>1)+bit;}else r>>=1;bit>>=2;}return r;}
static void rms(const Tensor *w,const short *in,short *out){unsigned long i,r,sum=0;for(i=0;i<D;i++){long v=in[i]>>4;sum+=(unsigned long)(v*v);}r=isqrt(sum/D+1);if(!r)r=1;for(i=0;i<D;i++)out[i]=sat(((long)in[i]*s16(w->data+i*2))/(long)(r*16));}
static void add(short *a,const short *b,unsigned long n){unsigned long i;for(i=0;i<n;i++)a[i]=sat((long)a[i]+b[i]);}
static void attend(unsigned int layer,unsigned int pos){unsigned int head,t,j,base;short *kc=key_cache+(layer*CTX*D),*vc=val_cache+(layer*CTX*D);for(j=0;j<D;j++){kc[pos*D+j]=qkv[D+j];vc[pos*D+j]=qkv[2*D+j];}for(head=0;head<HEADS;head++){long mx=-2147483647;unsigned long sumw=0;base=head*HD;for(t=0;t<=pos;t++){long s=0;for(j=0;j<HD;j++)s+=((long)qkv[base+j]*kc[t*D+base+j])>>8;scores[t]=((s>>8)*181)>>6;if(scores[t]>mx)mx=scores[t];}for(t=0;t<=pos;t++){unsigned long d=(unsigned long)(mx-scores[t])>>7;if(d>256)d=256;scores[t]=ex_exp_lut[d];sumw+=(unsigned long)scores[t];}if(!sumw)sumw=1;for(j=0;j<HD;j++){long acc=0;for(t=0;t<=pos;t++)acc+=(scores[t]*vc[t*D+base+j])>>8;att[base+j]=sat((acc<<8)/(long)sumw);}}}
static void forward(Model *m,unsigned short token,unsigned int pos){unsigned int l,j;row(&m->tok,token,x);row(&m->pos,pos,tmp);add(x,tmp,D);for(l=0;l<LAYERS;l++){rms(&m->n1[l],x,h);matvec(&m->qkv[l],h,qkv);attend(l,pos);matvec(&m->proj[l],att,tmp);add(x,tmp,D);rms(&m->n2[l],x,h);matvec(&m->fc1[l],h,ff);for(j=0;j<FF;j++)if(ff[j]<0)ff[j]=0;matvec(&m->fc2[l],ff,tmp);add(x,tmp,D);}rms(&m->norm,x,h);}
static unsigned short greedy(Model *m){unsigned int r,j,best=0;long long bestv=-(1LL<<62);for(r=0;r<EX_VOCAB;r++){long long sum=0,v;const signed char *w=(const signed char *)(m->tok.data+r*D);for(j=0;j<D;j++)sum+=(long)w[j]*h[j];v=sum*s32(m->tok.scale+r*4);if(r>=863&&r!=EX_EOS)continue;if(v>bestv){bestv=v;best=r;}}return (unsigned short)best;}
static void append_token(char *out,unsigned int *len,unsigned short tok){unsigned int i;if(tok<256){if(*len<510)out[(*len)++]=(char)tok;}else if(tok<256+EX_CHAR_COUNT){unsigned int a=ex_char_offsets[tok-256],b=ex_char_offsets[tok-255];for(i=a;i<b&&*len<510;i++)out[(*len)++]=(char)ex_char_bytes[i];}out[*len]=0;}
static void probe_memory(void){void *p[28],*large;unsigned int n=0,i;while(n<28&&(p[n]=memmgr_alloc(256UL*1024UL))!=0){((unsigned char*)p[n])[0]=0x5a;((unsigned char*)p[n])[256UL*1024UL-1]=0xa5;n++;}for(i=n;i>0;i--)memmgr_free(p[i-1]);probe_free_kib=n*256UL;if(n>1){large=memmgr_alloc((n-1)*256UL*1024UL);if(large){((unsigned char*)large)[0]=0x3c;((unsigned char*)large)[(n-1)*256UL*1024UL-1]=0xc3;memmgr_free(large);probe_free_kib=(n-1)*256UL;}else probe_free_kib=0;}}
static void info_screen(void){char a[64];RomHeader *rh=(RomHeader*)0x8001ff80;unsigned long pvr=*(volatile unsigned long*)0xff000030,prr=*(volatile unsigned long*)0xff000044,frqcr=*(volatile unsigned long*)0xa4150000;clear();title();set_pen(create_rgb16(255,255,0));render_text_jp(12,39,"端末・モデル情報");set_pen(create_rgb16(255,255,255));sprintf(a,"Model: %u params",(unsigned int)EX_MODEL_PARAMS);render_text(12,68,a);sprintf(a,"File: %u KiB / KV: %u KiB",(unsigned int)(EX_MODEL_BYTES/1024),(unsigned int)(LAYERS*CTX*D*4/1024));render_text(12,94,a);sprintf(a,"Free contiguous: %u KiB",(unsigned int)probe_free_kib);render_text(12,120,a);if(!memcmp(rh->sig,"CASIODICS",9))sprintf(a,"ROM model: %.4s / NOR: %08x",rh->model,(unsigned int)rh->nor_size);else sprintf(a,"ROM header: unavailable");render_text(12,158,a);sprintf(a,"PVR: %08x  PRR: %08x",(unsigned int)pvr,(unsigned int)prr);render_text(12,184,a);sprintf(a,"FRQCR: %08x",(unsigned int)frqcr);render_text(12,210,a);set_pen(create_rgb16(0,255,0));render_text_jp(12,292,"決定:質問へ 戻る:終了 履歴:画面保存");lcdc_copy_vram();}
static int tokenize_prompt(const char *text,unsigned short *ids,int cap){const unsigned char *p=(const unsigned char*)text;int n=0;if(cap<4)return -1;ids[n++]=EX_BOS;ids[n++]=EX_USER;while(*p){unsigned int bytes=(*p<0x80)?1:((*p&0xe0)==0xc0?2:((*p&0xf0)==0xe0?3:4));unsigned int i,j,found=EX_CHAR_COUNT;for(i=0;i<EX_CHAR_COUNT;i++){unsigned int a=ex_char_offsets[i],b=ex_char_offsets[i+1];if(b-a!=bytes)continue;for(j=0;j<bytes&&ex_char_bytes[a+j]==p[j];j++){}if(j==bytes){found=i;break;}}if(found<EX_CHAR_COUNT){if(n>=cap-1)return -1;ids[n++]=256+found;}else{for(i=0;i<bytes;i++){if(n>=cap-1)return -1;ids[n++]=p[i];}}p+=bytes;}ids[n++]=EX_ASSIST;return n;}
static int token_equals(unsigned short tok,const char *s){unsigned int i,a,b,n=(unsigned int)strlen(s);if(tok<256)return n==1&&(unsigned char)s[0]==tok;if(tok>=256+EX_CHAR_COUNT)return 0;a=ex_char_offsets[tok-256];b=ex_char_offsets[tok-255];if(b-a!=n)return 0;for(i=0;i<n;i++)if(ex_char_bytes[a+i]!=(unsigned char)s[i])return 0;return 1;}
static int draft_stop(unsigned short tok){return token_equals(tok,"。")||token_equals(tok,"！")||token_equals(tok,"？")||token_equals(tok,"!")||token_equals(tok,"?");}
static unsigned int thinking_prompt(const unsigned short *prompt,unsigned int prompt_len,const unsigned short *draft,unsigned int draft_len,unsigned short *out){unsigned short suffix[16];unsigned int i,n=0,qcount,keep,start,payload,budget;int sn=tokenize_prompt("\n回答:",suffix,16);if(sn<3)return 0;payload=(unsigned int)sn-3;qcount=prompt_len>3?prompt_len-3:0;budget=CTX-FINAL_MAX_TOKENS;if(budget<=3+payload+draft_len)return 0;keep=budget-3-payload-draft_len;if(keep>qcount)keep=qcount;start=2+qcount-keep;while(keep&&prompt[start]>=0x80&&prompt[start]<=0xbf){start++;keep--;}out[n++]=EX_BOS;out[n++]=EX_USER;for(i=0;i<keep;i++)out[n++]=prompt[start+i];for(i=0;i<payload;i++)out[n++]=suffix[2+i];for(i=0;i<draft_len;i++)out[n++]=draft[i];out[n++]=EX_ASSIST;return n;}
static void menu(unsigned int sel,int thinking){static const char *q[]={"自由入力：キーボードから質問","例題：こんにちは","例題：あなたは何というモデルですか？","例題：RAMとは何ですか？","例題：オフラインとは何ですか？","端末情報：メモリ・モデル"};unsigned int i;clear();title();set_pen(create_rgb16(255,255,255));render_text_jp(12,39,"機能・例題を選んでください");for(i=0;i<6;i++){set_pen(i==sel?create_rgb16(255,255,0):create_rgb16(180,180,180));render_text_jp(24,65+i*30,q[i]);}set_pen(sel==6?create_rgb16(255,255,0):create_rgb16(180,180,180));render_text(24,245,thinking?"Thinking ON":"Thinking OFF");set_pen(create_rgb16(0,255,0));render_text_jp(12,282,"上下:選択 決定:実行 戻る:終了 履歴:画面保存");lcdc_copy_vram();}
static void conversation_screen(const char *question,const char *answer,int generating,unsigned int tokens){char st[40];clear();title();set_pen(create_rgb16(255,255,0));render_text_jp(12,39,"質問");set_pen(create_rgb16(255,255,255));render_text_jp_wrapped_clipped(12,60,500,116,question);set_pen(create_rgb16(70,70,70));draw_line(12,119,516,119);set_pen(create_rgb16(255,255,0));render_text_jp(12,130,generating==2?"考えています":generating?"回答を生成中":"回答");set_pen(create_rgb16(255,255,255));render_text_jp_wrapped_clipped(12,153,500,282,answer);if(generating){sprintf(st,"%u tokens / HISTORY: screenshot",tokens);set_pen(create_rgb16(0,255,255));render_text(12,292,st);}else{set_pen(create_rgb16(0,255,0));render_text_jp(12,292,"決定:質問へ 戻る:終了 履歴:画面保存");}lcdc_copy_vram();}
static int utf8_complete(const char *s){const unsigned char *p=(const unsigned char*)s;unsigned int need=0;while(*p){if(!need){if(*p<0x80){}else if((*p&0xe0)==0xc0)need=1;else if((*p&0xf0)==0xe0)need=2;else if((*p&0xf8)==0xf0)need=3;else return 0;}else{if((*p&0xc0)!=0x80)return 0;need--;}p++;}return need==0;}
static int pressed(int k){keypad_read();return get_key_state(k);}static void release(int k){while(get_key_state(k))keypad_read();}
static int capture_if_requested(void){keypad_read();if(!get_key_state(KEY_HISTORY)&&!get_key_state(KEY_FUNC_8))return 0;if(get_key_state(KEY_HISTORY))release(KEY_HISTORY);if(get_key_state(KEY_FUNC_8))release(KEY_FUNC_8);screenshot_notice(screenshot_save());return 1;}
int main(void *ptr){
 Model m;int rc,thinking=0;unsigned int sel=0;
 if(ptr&&*(long*)ptr==1)return -1;
 memmgr_init();graphics_init(SW,SH,(void*)0xAC200000);
 screen("モデルを読み込んでいます",0,create_rgb16(255,255,0));rc=load_model(&m);
 if(rc){char e[32];sprintf(e,"LOAD ERROR %d",rc);screen("モデル読込エラー",e,create_rgb16(255,0,0));for(;;){if(capture_if_requested())continue;if(pressed(KEY_POWER)||pressed(KEY_BACK))return -2;}}
 probe_memory();
 for(;;){
  menu(sel,thinking);
  for(;;){
   if(capture_if_requested())continue;
   if(pressed(KEY_POWER)||pressed(KEY_BACK))return -2;
   if(get_key_state(KEY_UP)){release(KEY_UP);sel=(sel+6)%7;menu(sel,thinking);}
   if(get_key_state(KEY_DOWN)){release(KEY_DOWN);sel=(sel+1)%7;menu(sel,thinking);}
   if(get_key_state(KEY_ENTER)){
    unsigned int i,pos=0,n=0;unsigned short tok,*prompt;unsigned int prompt_len;
    unsigned short custom_ids[98],draft_ids[THINK_DRAFT_MAX],final_ids[CTX];char question[192],out[512],draft[64];release(KEY_ENTER);
    if(sel==6){thinking=!thinking;menu(sel,thinking);continue;}
    if(sel==5){
     info_screen();
     for(;;){if(capture_if_requested())continue;if(pressed(KEY_POWER)||pressed(KEY_BACK))return -2;if(get_key_state(KEY_ENTER)){release(KEY_ENTER);break;}}
     menu(sel,thinking);continue;
    }
    if(sel==0){
     if(!prompt_input(question,sizeof(question))){menu(sel,thinking);continue;}
     rc=tokenize_prompt(question,custom_ids,96);
     if(rc<0){screen("質問が長すぎます","96 token limit",create_rgb16(255,0,0));for(;;){if(capture_if_requested())continue;if(pressed(KEY_POWER)||pressed(KEY_BACK))return -2;if(get_key_state(KEY_ENTER)){release(KEY_ENTER);break;}}menu(sel,thinking);continue;}
     prompt=custom_ids;prompt_len=(unsigned int)rc;
    }else{strcpy(question,example_questions[sel-1]);prompt=(unsigned short*)ex_prompts[sel-1];prompt_len=ex_prompt_lengths[sel-1];}
    draft[0]=0;
    if(thinking){
     unsigned int dn=0,draft_len=0;conversation_screen(question,"",2,0);
     memset(key_cache,0,LAYERS*CTX*D*2);memset(val_cache,0,LAYERS*CTX*D*2);
     for(i=0;i<prompt_len&&pos<CTX;i++,pos++)forward(&m,prompt[i],pos);
     for(i=0;i<THINK_DRAFT_MAX&&pos<CTX;i++,pos++){tok=greedy(&m);if(tok==EX_EOS)break;draft_ids[draft_len++]=tok;append_token(draft,&dn,tok);forward(&m,tok,pos);if(utf8_complete(draft)){conversation_screen(question,"",2,draft_len);capture_if_requested();}if(draft_stop(tok))break;}
     prompt_len=thinking_prompt(prompt,prompt_len,draft_ids,draft_len,final_ids);prompt=final_ids;
    }
    pos=0;out[0]=0;conversation_screen(question,"",1,0);
    memset(key_cache,0,LAYERS*CTX*D*2);memset(val_cache,0,LAYERS*CTX*D*2);
    for(i=0;i<prompt_len&&pos<CTX;i++,pos++)forward(&m,prompt[i],pos);
    for(i=0;i<FINAL_MAX_TOKENS&&pos<CTX;i++,pos++){
     tok=greedy(&m);if(tok==EX_EOS)break;append_token(out,&n,tok);forward(&m,tok,pos);
     if(utf8_complete(out)){conversation_screen(question,out,1,i+1);capture_if_requested();}
    }
    conversation_screen(question,out[0]?out:"（回答を生成できませんでした）",0,i);
    for(;;){if(capture_if_requested())continue;if(pressed(KEY_POWER)||pressed(KEY_BACK))return -2;if(get_key_state(KEY_ENTER)){release(KEY_ENTER);break;}}
    break;
   }
  }
 }
}
