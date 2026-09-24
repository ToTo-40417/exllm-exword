#ifndef EXLLM_BENCHMARK_H
#define EXLLM_BENCHMARK_H

int benchmark_init(void);
void benchmark_begin(void);
unsigned long benchmark_elapsed_ms(void);
void benchmark_end(void);
int benchmark_log(const char *question, unsigned int input_tokens,
                  unsigned int output_tokens, int thinking,
                  unsigned long ttft_ms, unsigned long total_ms);

#endif
