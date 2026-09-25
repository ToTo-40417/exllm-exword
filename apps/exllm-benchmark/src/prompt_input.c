#include "prompt_input.h"
#include "screenshot.h"
#include "ui_jp_font.h"
#include <graphics/drawing.h>
#include <graphics/color.h>
#include <graphics/text.h>
#include <graphics/lcdc.h>
#include <sh4a/input/keypad.h>
#include <string.h>
#include <stdio.h>

#define SW 528
#define SH 320

struct roma { const char *key; const char *kana; };
static const struct roma table[] = {
 {"A","あ"},{"I","い"},{"U","う"},{"E","え"},{"O","お"},
 {"KA","か"},{"KI","き"},{"KU","く"},{"KE","け"},{"KO","こ"},
 {"GA","が"},{"GI","ぎ"},{"GU","ぐ"},{"GE","げ"},{"GO","ご"},
 {"SA","さ"},{"SI","し"},{"SHI","し"},{"SU","す"},{"SE","せ"},{"SO","そ"},
 {"ZA","ざ"},{"ZI","じ"},{"JI","じ"},{"ZU","ず"},{"ZE","ぜ"},{"ZO","ぞ"},
 {"TA","た"},{"TI","ち"},{"CHI","ち"},{"TU","つ"},{"TSU","つ"},{"TE","て"},{"TO","と"},
 {"DA","だ"},{"DI","ぢ"},{"DU","づ"},{"DE","で"},{"DO","ど"},
 {"NA","な"},{"NI","に"},{"NU","ぬ"},{"NE","ね"},{"NO","の"},{"NN","ん"},
 {"HA","は"},{"HI","ひ"},{"HU","ふ"},{"FU","ふ"},{"HE","へ"},{"HO","ほ"},
 {"BA","ば"},{"BI","び"},{"BU","ぶ"},{"BE","べ"},{"BO","ぼ"},
 {"PA","ぱ"},{"PI","ぴ"},{"PU","ぷ"},{"PE","ぺ"},{"PO","ぽ"},
 {"MA","ま"},{"MI","み"},{"MU","む"},{"ME","め"},{"MO","も"},
 {"YA","や"},{"YU","ゆ"},{"YO","よ"},{"RA","ら"},{"RI","り"},{"RU","る"},{"RE","れ"},{"RO","ろ"},
 {"WA","わ"},{"WO","を"},{"VU","ゔ"},
 {"XA","ぁ"},{"XI","ぃ"},{"XU","ぅ"},{"XE","ぇ"},{"XO","ぉ"},{"XYA","ゃ"},{"XYU","ゅ"},{"XYO","ょ"},{"XTU","っ"},
 {"KYA","きゃ"},{"KYU","きゅ"},{"KYO","きょ"},{"GYA","ぎゃ"},{"GYU","ぎゅ"},{"GYO","ぎょ"},
 {"SYA","しゃ"},{"SYU","しゅ"},{"SYO","しょ"},{"SHA","しゃ"},{"SHU","しゅ"},{"SHO","しょ"},
 {"ZYA","じゃ"},{"ZYU","じゅ"},{"ZYO","じょ"},{"JA","じゃ"},{"JU","じゅ"},{"JO","じょ"},
 {"TYA","ちゃ"},{"TYU","ちゅ"},{"TYO","ちょ"},{"CHA","ちゃ"},{"CHU","ちゅ"},{"CHO","ちょ"},
 {"NYA","にゃ"},{"NYU","にゅ"},{"NYO","にょ"},{"HYA","ひゃ"},{"HYU","ひゅ"},{"HYO","ひょ"},
 {"BYA","びゃ"},{"BYU","びゅ"},{"BYO","びょ"},{"PYA","ぴゃ"},{"PYU","ぴゅ"},{"PYO","ぴょ"},
 {"MYA","みゃ"},{"MYU","みゅ"},{"MYO","みょ"},{"RYA","りゃ"},{"RYU","りゅ"},{"RYO","りょ"}
};
static char pending[5]; static int plen;

static int prefix(const char *word,const char *p){while(*p){if(*word++!=*p++)return 0;}return 1;}
static const char *exact(const char *p){unsigned int i;for(i=0;i<sizeof(table)/sizeof(table[0]);i++)if(!strcmp(table[i].key,p))return table[i].kana;return 0;}
static int has_prefix(const char *p){unsigned int i;for(i=0;i<sizeof(table)/sizeof(table[0]);i++)if(prefix(table[i].key,p))return 1;return 0;}
static void append(char *out,int *n,int cap,const char *s){while(*s&&*n<cap-1)out[(*n)++]=*s++;out[*n]=0;}
static void flush_n(char *out,int *n,int cap){if(plen==1&&pending[0]=='N')append(out,n,cap,"ん");plen=0;pending[0]=0;}
static int vowel(char c){return c=='A'||c=='I'||c=='U'||c=='E'||c=='O';}
static void kana_key(char *out,int *n,int cap,char c){const char *s;
 if(plen==1&&pending[0]==c&&c!='N'&&!vowel(c)){append(out,n,cap,"っ");plen=0;pending[0]=0;}
 if(plen==1&&pending[0]=='N'){char z[3]={'N',c,0};if(!exact(z)&&!has_prefix(z)){append(out,n,cap,"ん");plen=0;pending[0]=0;}}
 if(plen<4){pending[plen++]=c;pending[plen]=0;}s=exact(pending);if(s){append(out,n,cap,s);plen=0;pending[0]=0;return;}
 if(!has_prefix(pending)){pending[0]=c;pending[1]=0;plen=1;s=exact(pending);if(s){append(out,n,cap,s);plen=0;pending[0]=0;}else if(!has_prefix(pending)){plen=0;pending[0]=0;}}
}
static void erase_utf8(char *out,int *n){if(*n<=0)return;(*n)--;while(*n>0&&(((unsigned char)out[*n]&0xc0)==0x80))(*n)--;out[*n]=0;}
static char keychar(int k){switch(k){
 case KEY_CHAR_A:return'A';case KEY_CHAR_B:return'B';case KEY_CHAR_C:return'C';case KEY_CHAR_D:return'D';case KEY_CHAR_E:return'E';case KEY_CHAR_F:return'F';case KEY_CHAR_G:return'G';case KEY_CHAR_H:return'H';case KEY_CHAR_I:return'I';case KEY_CHAR_J:return'J';case KEY_CHAR_K:return'K';case KEY_CHAR_L:return'L';case KEY_CHAR_M:return'M';case KEY_CHAR_N:return'N';case KEY_CHAR_O:return'O';case KEY_CHAR_P:return'P';case KEY_CHAR_Q:return'Q';case KEY_CHAR_R:return'R';case KEY_CHAR_S:return'S';case KEY_CHAR_T:return'T';case KEY_CHAR_U:return'U';case KEY_CHAR_V:return'V';case KEY_CHAR_W:return'W';case KEY_CHAR_X:return'X';case KEY_CHAR_Y:return'Y';case KEY_CHAR_Z:return'Z';default:return 0;}}
static int current_key(void){int c,r;for(c=0;c<=8;c++)for(r=1;r<=8;r++){int k=(c+1)*10+r;if(get_key_state(k))return k;}return -1;}
static void release_key(int k){while(get_key_state(k))keypad_read();}
static void redraw(const char *out,int kana,int sym){char st[64];(void)sym;set_pen(create_rgb16(0,0,0));draw_rect(0,0,SW,SH);set_pen(create_rgb16(0,255,255));render_text(12,12,"EXLLM v1.1.0");set_pen(create_rgb16(255,255,0));render_text_jp(12,43,"質問を入力してください");set_pen(create_rgb16(255,255,255));render_text_jp_wrapped_clipped(12,74,500,226,out);sprintf(st,kana?"[かな] %s":"[ABC] %s",pending);set_pen(create_rgb16(0,255,255));render_text_jp(12,232,st);set_pen(create_rgb16(0,255,0));render_text_jp(12,258,"記号連打: ？ 。 、 ！ ー");render_text_jp(12,283,"決定:推論 SHIFT:切替 削除:一字 履歴:画面保存");lcdc_copy_vram();}
int prompt_input(char *out,int cap){static const char *symbols[]={"？","。","、","！","ー"};int n=0,k,kana=1,sym=-1;char c;out[0]=0;plen=0;pending[0]=0;redraw(out,kana,sym);for(;;){keypad_read();k=current_key();if(k<0)continue;if(k==KEY_FUNC_8||k==KEY_HISTORY){release_key(k);screenshot_notice(screenshot_save());continue;}if(k==KEY_BACK){release_key(k);return 0;}if(k==KEY_ENTER){release_key(k);flush_n(out,&n,cap);return n>0;}if(k==KEY_SHIFT){release_key(k);flush_n(out,&n,cap);sym=-1;kana=!kana;redraw(out,kana,sym);continue;}if(k==KEY_BACKSPACE){release_key(k);sym=-1;if(plen){pending[--plen]=0;}else erase_utf8(out,&n);redraw(out,kana,sym);continue;}if(k==KEY_SYMBOL){release_key(k);flush_n(out,&n,cap);if(sym>=0)erase_utf8(out,&n);sym=(sym+1)%5;append(out,&n,cap,symbols[sym]);redraw(out,kana,sym);continue;}if(k==KEY_RIGHT){release_key(k);flush_n(out,&n,cap);sym=-1;append(out,&n,cap," ");redraw(out,kana,sym);continue;}c=keychar(k);release_key(k);if(c){sym=-1;if(kana)kana_key(out,&n,cap,c);else{char z[2]={c,0};append(out,&n,cap,z);}redraw(out,kana,sym);}}
}
