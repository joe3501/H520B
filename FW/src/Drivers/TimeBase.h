#ifndef _TIMEBASE_H_
#define _TIMEBASE_H_

typedef void (*TimerIsrHook)(void);

void Delay(unsigned int delay);
void TimeBase_Init(void);
void TIM2_UpdateISRHandler(void);
void start_timer(TimerIsrHook hook_func);
void stop_timer(void);

void StartDelay(unsigned short nTime);
unsigned char DelayIsEnd(void);

#endif