#include <syscalls/syscalls.h>
#include <stdio.h>
#include <string.h>
#include "benchmark.h"

#define RTC_SEC  (*(volatile unsigned char *)0xa413fec2)
#define RTC_MIN  (*(volatile unsigned char *)0xa413fec4)
#define RTC_HOUR (*(volatile unsigned char *)0xa413fec6)
#define RTC_DAY  (*(volatile unsigned char *)0xa413feca)
#define RTC_MON  (*(volatile unsigned char *)0xa413fecc)
#define RTC_YEAR (*(volatile unsigned short *)0xa413fece)
#define TMU_TSTR (*(volatile unsigned char *)0xa4490004)
#define TMU2_TCOR (*(volatile unsigned long *)0xa4490020)
#define TMU2_TCNT (*(volatile unsigned long *)0xa4490024)
#define TMU2_TCR  (*(volatile unsigned short *)0xa4490028)

static unsigned long ticks_per_second;
static unsigned char saved_tstr;
static unsigned short saved_tcr;
static unsigned long saved_tcor, saved_tcnt, start_count;
static int running;

static unsigned int bcd8(unsigned int v){return (v&15)+((v>>4)&15)*10;}
static void save_timer(void){saved_tstr=TMU_TSTR;saved_tcr=TMU2_TCR;saved_tcor=TMU2_TCOR;saved_tcnt=TMU2_TCNT;}
static void start_timer(void){TMU_TSTR=(unsigned char)(saved_tstr&~4);TMU2_TCOR=0xffffffffUL;TMU2_TCNT=0xffffffffUL;TMU2_TCR=0;TMU_TSTR=(unsigned char)((saved_tstr&~4)|4);start_count=TMU2_TCNT;running=1;}
static void restore_timer(void){TMU_TSTR=(unsigned char)(TMU_TSTR&~4);TMU2_TCOR=saved_tcor;TMU2_TCNT=saved_tcnt;TMU2_TCR=saved_tcr;TMU_TSTR=saved_tstr;running=0;}

int benchmark_init(void){
 unsigned char s0,s1;unsigned long a,b,guard=0;
 save_timer();start_timer();s0=RTC_SEC;
 while((s1=RTC_SEC)==s0&&++guard<200000000UL){}
 if(guard>=200000000UL){restore_timer();return 0;}
 a=TMU2_TCNT;guard=0;s0=s1;
 while((s1=RTC_SEC)==s0&&++guard<200000000UL){}
 if(guard>=200000000UL){restore_timer();return 0;}
 b=TMU2_TCNT;restore_timer();ticks_per_second=a-b;
 return ticks_per_second!=0;
}
void benchmark_begin(void){save_timer();start_timer();}
unsigned long benchmark_elapsed_ms(void){unsigned long ticks,per_ms;if(!running||!ticks_per_second)return 0;ticks=start_count-TMU2_TCNT;per_ms=ticks_per_second/1000UL;if(!per_ms)per_ms=1;return ticks/per_ms;}
void benchmark_end(void){if(running)restore_timer();}

int benchmark_log(const char *question,unsigned int input_tokens,unsigned int output_tokens,int thinking,unsigned long ttft_ms,unsigned long total_ms){
 char path[64],line[512],id[10];unsigned long drive,size;int fd,n,written=0;unsigned int y;
 sys_dict_info(&drive,id);sprintf(path,"\\\\drv0\\%s\\_USER\\PERFLOG.TSV",id);
 fd=sys_open(path,FILE_WR);
 if(fd<0){if(sys_create(path,1)<0)return 0;fd=sys_open(path,FILE_WR);if(fd<0)return 0;}
 size=sys_get_filesize(fd);sys_seek(fd,size,0);y=RTC_YEAR;
 n=sprintf(line,"%04u-%02u-%02uT%02u:%02u:%02u\tthink=%u\tin=%u\tout=%u\tttft_ms=%u\ttotal_ms=%u\ttok_s_x100=%u\tprompt=%s\r\n",
  bcd8(y>>8)*100+bcd8(y&255),bcd8(RTC_MON),bcd8(RTC_DAY),bcd8(RTC_HOUR),bcd8(RTC_MIN),bcd8(RTC_SEC),
  thinking?1:0,input_tokens,output_tokens,(unsigned int)ttft_ms,(unsigned int)total_ms,
  total_ms>ttft_ms?(unsigned int)(((output_tokens>1?output_tokens-1:0)*100000UL)/(total_ms-ttft_ms)):0,question);
 while(written<n){int r=sys_write(fd,line+written,(unsigned long)(n-written));if(r<=0){sys_close(fd);return 0;}written+=r;}
 sys_close(fd);return 1;
}
