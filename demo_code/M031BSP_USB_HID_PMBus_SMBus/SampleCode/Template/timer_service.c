/*_____ I N C L U D E S ____________________________________________________*/
// #include <stdio.h>
#include "NuMicro.h"

#include "timer_service.h"

/*_____ D E C L A R A T I O N S ____________________________________________*/

/*_____ D E F I N I T I O N S ______________________________________________*/

volatile TIMER_EVENT_QUEUE_T g_TimerEventQueue;
volatile TIMER_INSTANCE_T    g_TimerService_List[TIMER_SERVICE_MAX_TIMERS];

/*_____ M A C R O S ________________________________________________________*/

/*_____ F U N C T I O N S __________________________________________________*/

/* enqueue in ISR (queue-based timer only) */
static void TimerService_EnqueueEventFromISR(int timer_id)
{
    volatile TIMER_EVENT_QUEUE_T *q;
    unsigned char next_tail;

    q = &g_TimerEventQueue;

    next_tail = (unsigned char)(q->tail + 1U);
    if (next_tail >= TIMER_EVENT_QUEUE_SIZE)
    {
        next_tail = 0U;
    }

    /* Full when next_tail catches head */
    if (next_tail == q->head)
    {
        return; /* drop */
    }

    q->ids[q->tail] = timer_id;
    q->tail = next_tail;
}

/* 1 ms tick: proceed in timer irq */
void TimerService_Tick1ms(void)
{
    unsigned int i;
    volatile TIMER_INSTANCE_T *p;

    for (i = 0U; i < TIMER_SERVICE_MAX_TIMERS; i++)
    {
        p = &g_TimerService_List[i];

        if ((p->active != 0U) && (p->callback != (TIMER_CALLBACK_T)0))
        {
            if (p->counter_ms < 0xFFFFU)
            {
                p->counter_ms++;
            }

            if (p->counter_ms >= p->period_ms)
            {
                p->counter_ms = 0U;

                if (p->kind == TIMER_KIND_FLAG)
                {
                    /* flag-based: only set pending , not into queue */
                    if (p->pending == 0U)
                    {
                        p->pending = 1U;
                    }
                }
                else
                {
                    /* queue-based: proceed event into ring buffer */
                    TimerService_EnqueueEventFromISR((int)i);
                }
            }
        }
    }
}

static void TimerService_EnterCritical(void)
{
    __disable_irq();
}

static void TimerService_ExitCritical(void)
{
    __enable_irq();
}

/* Return 1 if got event, else 0 */
uint8_t TimerService_DequeueEvent(int *out_id)
{
    volatile TIMER_EVENT_QUEUE_T *q;
    uint8_t has_event;
    int id;

    q = &g_TimerEventQueue;
    has_event = 0U;
    id = -1;

    TimerService_EnterCritical();

    /* Empty when head == tail */
    if (q->head != q->tail)
    {
        id = q->ids[q->head];

        q->head++;
        if (q->head >= TIMER_EVENT_QUEUE_SIZE)
        {
            q->head = 0U;
        }

        has_event = 1U;
    }

    TimerService_ExitCritical();

    if (has_event != 0U)
    {
        *out_id = id;
    }

    return has_event;
}

void TimerService_DispatchQueue(void)
{
    int id;
    volatile TIMER_INSTANCE_T *p;
    TIMER_CALLBACK_T cb;
    void *user;

    while (TimerService_DequeueEvent(&id) != 0U)
    {
        if ((id >= 0) && (id < (int)TIMER_SERVICE_MAX_TIMERS))
        {
            p = &g_TimerService_List[id];
            cb = p->callback;
            user = p->user_data;

            if (cb != (TIMER_CALLBACK_T)0)
            {
                cb(user);
            }
        }
    }
}

static uint8_t TimerService_TakePending(volatile TIMER_INSTANCE_T *p)
{
    uint8_t taken;

    taken = 0U;

    TimerService_EnterCritical();
    if (p->pending != 0U)
    {
        p->pending = 0U;
        taken = 1U;
    }
    TimerService_ExitCritical();

    return taken;
}

void TimerService_DispatchFlag(void)
{
    unsigned int i;
    volatile TIMER_INSTANCE_T *p;
    TIMER_CALLBACK_T cb;
    void *user;

    for (i = 0U; i < TIMER_SERVICE_MAX_TIMERS; i++)
    {
        p = &g_TimerService_List[i];

        if ((p->kind == TIMER_KIND_FLAG) &&
            (p->active != 0U) &&
            (p->callback != (TIMER_CALLBACK_T)0))
        {
            if (TimerService_TakePending(p) != 0U)
            {
                cb = p->callback;
                user = p->user_data;
                cb(user);
            }
        }
    }
}

/* dispatch event IN main loop */
void TimerService_Dispatch(void)
{
    TimerService_DispatchQueue();
    TimerService_DispatchFlag();
}

void TimerService_ChangePeriod(unsigned int timer_id,
                               unsigned short new_period_ms)
{
    volatile TIMER_INSTANCE_T *p;

    if (timer_id >= TIMER_SERVICE_MAX_TIMERS)
    {
        return;
    }

    p = &g_TimerService_List[timer_id];

    p->period_ms = new_period_ms;
}

void TimerService_StopTimer(unsigned int timer_id)
{
    volatile TIMER_INSTANCE_T *p;

    if (timer_id >= TIMER_SERVICE_MAX_TIMERS)
    {
        return;
    }

    p = &g_TimerService_List[timer_id];

    p->active  = 0U;
    p->pending = 0U;
}

void TimerService_StartTimer(unsigned int timer_id)
{
    volatile TIMER_INSTANCE_T *p;

    if (timer_id >= TIMER_SERVICE_MAX_TIMERS)
    {
        return;
    }

    p = &g_TimerService_List[timer_id];

    if (p->callback == (TIMER_CALLBACK_T)0)
    {
        return;
    }

    p->counter_ms = 0U;
    p->pending    = 0U;
    p->active     = 1U;
}

/* create queue-based timer */
int TimerService_CreateTimerQueue(unsigned short period_ms,
                                  TIMER_CALLBACK_T cb,
                                  void *user_data)
{
    unsigned int i;
    volatile TIMER_INSTANCE_T *p;

    for (i = 0U; i < TIMER_SERVICE_MAX_TIMERS; i++)
    {
        p = &g_TimerService_List[i];

        if (p->callback == (TIMER_CALLBACK_T)0)
        {
            p->period_ms  = period_ms;
            p->counter_ms = 0U;
            p->active     = 0U;
            p->kind       = TIMER_KIND_QUEUE;
            p->pending    = 0U;
            p->reserved   = 0U;
            p->callback   = cb;
            p->user_data  = user_data;

            return (int)i;
        }
    }

    return -1;
}

/* create flag-based timer（for 1ms or high frequency task） */
int TimerService_CreateTimerFlag(unsigned short period_ms,
                                 TIMER_CALLBACK_T cb,
                                 void *user_data)
{
    unsigned int i;
    volatile TIMER_INSTANCE_T *p;

    for (i = 0U; i < TIMER_SERVICE_MAX_TIMERS; i++)
    {
        p = &g_TimerService_List[i];

        if (p->callback == (TIMER_CALLBACK_T)0)
        {
            p->period_ms  = period_ms;
            p->counter_ms = 0U;
            p->active     = 0U;
            p->kind       = TIMER_KIND_FLAG;
            p->pending    = 0U;
            p->reserved   = 0U;
            p->callback   = cb;
            p->user_data  = user_data;

            return (int)i;
        }
    }

    return -1;
}

/* old API：default set as queue-based */
int TimerService_CreateTimer(unsigned short period_ms,
                             TIMER_CALLBACK_T cb,
                             void *user_data)
{
    return TimerService_CreateTimerQueue(period_ms, cb, user_data);
}

void TimerService_Init(void)
{
    unsigned int i;
    volatile TIMER_INSTANCE_T *p;
    volatile TIMER_EVENT_QUEUE_T *q;

    /* Init event queue */
    q = &g_TimerEventQueue;
    q->head     = 0U;
    q->tail     = 0U;

    /* Init timers */
    for (i = 0U; i < TIMER_SERVICE_MAX_TIMERS; i++)
    {
        p = &g_TimerService_List[i];

        p->period_ms  = 0U;
        p->counter_ms = 0U;
        p->active     = 0U;
        p->kind       = TIMER_KIND_QUEUE;
        p->pending    = 0U;
        p->reserved   = 0U;
        p->callback   = (TIMER_CALLBACK_T)0;
        p->user_data  = (void *)0;
    }
}

