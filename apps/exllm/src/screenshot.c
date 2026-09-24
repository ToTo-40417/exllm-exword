#include "screenshot.h"
#include "ui_jp_font.h"
#include <graphics/color.h>
#include <graphics/drawing.h>
#include <graphics/lcdc.h>
#include <syscalls/syscalls.h>
#include <stdio.h>
#include <string.h>

#define SHOT_W 528
#define SHOT_H 320
#define SHOT_BYTES (SHOT_W * SHOT_H * 2UL)
#define RTC_RCR1 (*(unsigned char volatile *)0xa413fedc)
#define RTC_RSECCNT (*(unsigned char volatile *)0xa413fec2)
#define RTC_RMINCNT (*(unsigned char volatile *)0xa413fec4)
#define RTC_RHRCNT (*(unsigned char volatile *)0xa413fec6)
#define RTC_RDAYCNT (*(unsigned char volatile *)0xa413feca)
#define RTC_RMONCNT (*(unsigned char volatile *)0xa413fecc)
#define RTC_RYRCNT (*(unsigned short volatile *)0xa413fece)

static unsigned short converted[512];
static unsigned char bcd2bin(unsigned char b){return (b&15)+(b>>4)*10;}
static void timestamp(char *date,char *clock){unsigned char sec,min,hour,day,mon;unsigned short year;do{RTC_RCR1&=0x6f;sec=bcd2bin(RTC_RSECCNT);min=bcd2bin(RTC_RMINCNT);hour=bcd2bin(RTC_RHRCNT);day=bcd2bin(RTC_RDAYCNT);mon=bcd2bin(RTC_RMONCNT);year=(unsigned short)(bcd2bin(RTC_RYRCNT>>8)*100+bcd2bin(RTC_RYRCNT&255));}while(RTC_RCR1&0x80);sprintf(date,"%04d%02d%02d",year,mon,day);sprintf(clock,"%02d%02d%02d",hour,min,sec);}
/* EX-word media use DOS 8.3 names. Keep the date as an 8-char directory and
 * the time as a 6-char basename; two collision digits still fit in 8 chars. */
static int unused_path(char *path,const char *base,const char *stem,const char *ext){int fd,n;char name[9];for(n=0;n<100;n++){sprintf(name,"%s",stem);if(n){name[6]='0'+n/10;name[7]='0'+n%10;}sprintf(path,"%s\\%s.%s",base,name,ext);fd=sys_open(path,FILE_RD);if(fd<0)return 1;sys_close(fd);}return 0;}
static void le32(unsigned char *p,unsigned long v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}

static int bmp_save(const char *date,const char *clock){
 static unsigned char header[70]={0x42,0x4d,0,0,0,0,0,0,0,0,0x46,0,0,0,0x28,0,0,0,0,0,0,0,0,0,0,0,1,0,16,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0xf8,0,0,0xe0,7,0,0,0x1f,0,0,0,0,0,0};
 unsigned char *vram=(unsigned char*)0xac200000;unsigned long drive,off;char id[10],base[64],path[80];int fd,i;
 le32(header+2,70+SHOT_BYTES);le32(header+18,SHOT_W);le32(header+22,(unsigned long)(-(long)SHOT_H));le32(header+34,SHOT_BYTES);
 /* Stable internal-memory layout used by the earlier working builds. */
 sys_dict_info(&drive,id);sprintf(base,"\\\\drv0\\%s\\_USER\\%s",id,date);sys_create(base,5);if(!unused_path(path,base,clock,"bmp"))return 0;
 if(sys_create(path,1)<0)return 0;fd=sys_open(path,FILE_WR);if(fd<0)return 0;if(sys_write(fd,header,sizeof(header))!=(int)sizeof(header)){sys_close(fd);return 0;}
 for(off=0;off<SHOT_BYTES;off+=1024){for(i=0;i<512;i++){unsigned long p=off+i*2;converted[i]=((unsigned short)vram[p+1]<<8)|vram[p];}if(sys_write(fd,converted,1024)!=1024){sys_close(fd);return 0;}}sys_close(fd);return 1;
}

int screenshot_save(void){char date[9],clock[7];timestamp(date,clock);return bmp_save(date,clock)?1:0;}
void screenshot_notice(int result){set_pen(create_rgb16(0,0,0));draw_rect(0,278,528,42);set_pen(result?create_rgb16(0,255,0):create_rgb16(255,0,0));render_text_jp(12,286,result?"BMPで画面を保存しました":"画面の保存に失敗しました");lcdc_copy_vram();}
