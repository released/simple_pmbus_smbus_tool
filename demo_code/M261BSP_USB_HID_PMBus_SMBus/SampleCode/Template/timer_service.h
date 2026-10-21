#ifndef __TIMER_SERVICE_H__
#define __TIMER_SERVICE_H__

/*_____ I N C L U D E S ____________________________________________________*/
// #include <stdio.h>
#include "NuMicro.h"

/*_____ D E C L A R A T I O N S ____________________________________________*/

#define TIMER_SERVICE_MAX_TIMERS 				(16U)
#define TIMER_EVENT_QUEUE_SIZE   				(16U)

/* timer type */
#define TIMER_KIND_FLAG                         (0U)  /* flag-based, not into queue */
#define TIMER_KIND_QUEUE                        (1U)  /* queue-based, into ring buffer */

/*_____ D E F I N I T I O N S ______________________________________________*/

/*  
	template
	typedef struct _peripheral_manager_t
	{
		uint16_t* pu16Far;
		uint8_t u8Cmd;
		uint8_t au8Buf[33];
		uint8_t u8RecCnt;
		uint8_t bByPass;
	}PERIPHERAL_MANAGER_T;

	volatile PERIPHERAL_MANAGER_T g_PeripheralManager = 
	{
		.pu16Far = NULL,	//.pu16Far = 0	
		.u8Cmd = 0,
		.au8Buf = {0},		//.au8Buf = {100U, 200U},
		.u8RecCnt = 0,
		.bByPass = FALSE,
	};
	extern volatile PERIPHERAL_MANAGER_T g_PeripheralManager;
	
	/////////////////////////////////////////////////////////////
	
*/

typedef struct _timer_event_queue_t
{
    unsigned char  head;
    unsigned char  tail;
    int            ids[TIMER_EVENT_QUEUE_SIZE];

} TIMER_EVENT_QUEUE_T;

typedef void (*TIMER_CALLBACK_T)(void *user_data);

typedef struct _timer_instance_t
{
    unsigned short   period_ms;
    unsigned short   counter_ms;
    unsigned char    active;
    unsigned char    kind;        	/* TIMER_KIND_FLAG / TIMER_KIND_QUEUE */
    unsigned char    pending;      	/* flag-based: 1=callback wait to be executed; queue-based: reserved */
    unsigned char    reserved;
    TIMER_CALLBACK_T callback;
    void            *user_data;

} TIMER_INSTANCE_T;


/*_____ M A C R O S ________________________________________________________*/

/*_____ F U N C T I O N S __________________________________________________*/

/* init */
void TimerService_Init(void);

/* 
 * queue-based timer
 * return >=0 : timer ID
 *        -1  : no free slot
 */
int  TimerService_CreateTimerQueue(unsigned short period_ms,
                                   TIMER_CALLBACK_T cb,
                                   void *user_data);

/* 
 * flag-based timer（for 1ms or high frequency task）
 * return >=0 : timer ID
 *        -1  : no free slot
 */
int  TimerService_CreateTimerFlag(unsigned short period_ms,
                                  TIMER_CALLBACK_T cb,
                                  void *user_data);

/* reserved for queue-based */
int  TimerService_CreateTimer(unsigned short period_ms,
                              TIMER_CALLBACK_T cb,
                              void *user_data);

/* Control functions */
void TimerService_StartTimer(unsigned int timer_id);
void TimerService_StopTimer(unsigned int timer_id);
void TimerService_ChangePeriod(unsigned int timer_id,
                               unsigned short new_period_ms);

/* 1 ms tick hook, must be called from 1ms Timer IRQ */
void TimerService_Tick1ms(void);

/* Dequeue one timer event.
 * return 1: got an event, *out_id valid
 * return 0: queue empty
 */
uint8_t TimerService_DequeueEvent(int *out_id);

/* execute in main loop , proceed queue-based + flag-based callback */
void TimerService_Dispatch(void);

#endif //__TIMER_SERVICE_H__
