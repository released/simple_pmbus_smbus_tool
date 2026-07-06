/*_____ I N C L U D E S ____________________________________________________*/
#include "misc_config.h"

/*_____ D E C L A R A T I O N S ____________________________________________*/

struct flag_8bit flag_MISC_CTL;
#define FLAG_MISC_ERROR                                 (flag_MISC_CTL.bit0)
#define FLAG_MISC_REVERSE1                 				(flag_MISC_CTL.bit1)
#define FLAG_MISC_REVERSE2                 				(flag_MISC_CTL.bit2)
#define FLAG_MISC_REVERSE3                              (flag_MISC_CTL.bit3)
#define FLAG_MISC_REVERSE4                              (flag_MISC_CTL.bit4)
#define FLAG_MISC_REVERSE5                              (flag_MISC_CTL.bit5)
#define FLAG_MISC_REVERSE6                              (flag_MISC_CTL.bit6)
#define FLAG_MISC_REVERSE7                              (flag_MISC_CTL.bit7)

/*_____ D E F I N I T I O N S ______________________________________________*/

#if defined (ENABLE_TICK_EVENT)
typedef void (*sys_pvTimeFunPtr)(void);   /* function pointer */
typedef struct timeEvent_t
{
    unsigned char       active;
    unsigned long       initTick;
    unsigned long       curTick;
    sys_pvTimeFunPtr    funPtr;
} TimeEvent_T;

#define TICKEVENTCOUNT                                 (8)                   
volatile  TimeEvent_T tTimerEvent[TICKEVENTCOUNT];
volatile unsigned char _sys_uTimerEventCount = 0;             /* Speed up interrupt reponse time if no callback function */
#endif

/*_____ M A C R O S ________________________________________________________*/

/*_____ F U N C T I O N S __________________________________________________*/

/*
    example : 
    read_64_words((volatile unsigned long *)0xF1000, buf);
*/
void read_64_words(volatile unsigned long *start_addr,unsigned long *buffer)
{
    unsigned long i;

    for (i = 0; i < 64U; i++)
    {
        buffer[i] = start_addr[i];
    }
}

/*
    example : 
    read_u32((volatile unsigned long *)0xF1000);
*/
unsigned long read_u32(volatile unsigned long *addr)
{
    return *addr;
}

unsigned short read_u16(volatile unsigned short *addr)
{
    return *addr;
}

unsigned char read_u8(volatile unsigned char *addr)
{
    return *addr;
}

int compare_buffer(const void *src, const void *dest, size_t nBytes)
{
    if (memcmp(src, dest, nBytes) == 0) {
        dbg_printf("compare_buffer complete\r\n");
        return 0;
    }

    dbg_printf("Mismatch!! - %zu bytes\r\n", nBytes);
    return 1;
}

void reset_buffer(void *dest, unsigned long val, unsigned long size)
{
    unsigned char *pu8Dest;
//    unsigned lpng i;
    
    pu8Dest = (unsigned char *)dest;

	#if 1
	while (size-- > 0)
		*pu8Dest++ = val;
	#else
	memset(pu8Dest, val, size * (sizeof(pu8Dest[0]) ));
	#endif
	
}

void copy_buffer(void *dest, void *src, unsigned long size)
{
    unsigned char *pu8Src, *pu8Dest;
    unsigned long i;
    
    pu8Dest = (unsigned char *)dest;
    pu8Src  = (unsigned char *)src;


	#if 0
	  while (size--)
	    *pu8Dest++ = *pu8Src++;
	#else
    for (i = 0; i < size; i++)
        pu8Dest[i] = pu8Src[i];
	#endif
}


void dump_buffer32(unsigned long *pucBuff, int nBytes)
{
    unsigned short  i = 0;
    
    dbg_printf("dump_buffer : %2d\r\n" , nBytes);    
    for (i = 0 ; i < nBytes ; i++)
    {
        dbg_printf("0x%08lX," , pucBuff[i]);
        if ((i+1)%4 ==0)
        {
            dbg_printf("\r\n");
        }            
    }
    dbg_printf("\r\n");
}

void dump_buffer32_hex(unsigned long *p, int nWords)
{
	unsigned char ch = 0;
	int b = 0;
	unsigned long w = 0;
	int i=0;
    int idx = 0;
    while (nWords > 0) {
        dbg_printf("0x%04X  ", idx*4);
        for (i=0;i<4;i++) dbg_printf("%08lX ", p[idx+i]);
        dbg_printf("  ");
        for (i=0;i<4;i++) {
            w = p[idx+i];
            for (b=0;b<4;b++) {
                ch = (w >> (8*(3-b))) & 0xFF;
                dbg_printf("%c", (ch>=0x20 && ch<0x7F) ? ch : '.');
            }
        }
        dbg_printf("\n");
        idx += 4; nWords -= 4;
    }
}

void dump_buffer16(unsigned short *pucBuff, int nBytes)
{
    unsigned short  i = 0;
    
    dbg_printf("dump_buffer : %2d\r\n" , nBytes);    
    for (i = 0 ; i < nBytes ; i++)
    {
        dbg_printf("0x%02X," , pucBuff[i]);
        if ((i+1)%8 ==0)
        {
            dbg_printf("\r\n");
        }            
    }
    dbg_printf("\r\n\r\n");
}

void dump_buffer16_hex(unsigned short *pucBuff, int nBytes)
{
    int nIdx, i;

    nIdx = 0;
    while (nBytes > 0)
    {
        dbg_printf("0x%04X  ", nIdx);
        for (i = 0; i < 16; i++)
            dbg_printf("%02X ", pucBuff[nIdx + i]);
        dbg_printf("  ");
        for (i = 0; i < 16; i++)
        {
            if ((pucBuff[nIdx + i] >= 0x20) && (pucBuff[nIdx + i] < 127))
                dbg_printf("%c", pucBuff[nIdx + i]);
            else
                dbg_printf(".");
            nBytes--;
        }
        nIdx += 16;
        dbg_printf("\n");
    }
    dbg_printf("\n");
}

void dump_buffer8(unsigned char *pucBuff, int nBytes)
{
    unsigned short  i = 0;
    
    dbg_printf("dump_buffer : %2d\r\n" , nBytes);    
    for (i = 0 ; i < nBytes ; i++)
    {
        dbg_printf("0x%02X," , pucBuff[i]);
        if ((i+1)%8 ==0)
        {
            dbg_printf("\r\n");
        }            
    }
    dbg_printf("\r\n\r\n");
}

void dump_buffer8_hex(unsigned char *pucBuff, int nBytes)
{
    int nIdx, i;

    nIdx = 0;
    while (nBytes > 0)
    {
        dbg_printf("0x%04X  ", nIdx);
        for (i = 0; i < 16; i++)
            dbg_printf("%02X ", pucBuff[nIdx + i]);
        dbg_printf("  ");
        for (i = 0; i < 16; i++)
        {
            if ((pucBuff[nIdx + i] >= 0x20) && (pucBuff[nIdx + i] < 127))
                dbg_printf("%c", pucBuff[nIdx + i]);
            else
                dbg_printf(".");
            nBytes--;
        }
        nIdx += 16;
        dbg_printf("\n");
    }
    dbg_printf("\n");
}

#if defined (ENABLE_TICK_EVENT)
void TickCallback_processB(void)
{
    dbg_printf("%s test \r\n" , __FUNCTION__);
}

void TickCallback_processA(void)
{
    dbg_printf("%s test \r\n" , __FUNCTION__);
}

void TickClearTickEvent(unsigned char u8TimeEventID)
{
    if (u8TimeEventID > TICKEVENTCOUNT)
        return;

    if (tTimerEvent[u8TimeEventID].active == TRUE)
    {
        tTimerEvent[u8TimeEventID].active = FALSE;
        _sys_uTimerEventCount--;
    }
}

signed char TickSetTickEvent(unsigned long uTimeTick, void *pvFun)
{
    int  i;
    int u8TimeEventID = 0;

    for (i = 0; i < TICKEVENTCOUNT; i++)
    {
        if (tTimerEvent[i].active == FALSE)
        {
            tTimerEvent[i].active = TRUE;
            tTimerEvent[i].initTick = uTimeTick;
            tTimerEvent[i].curTick = uTimeTick;
            tTimerEvent[i].funPtr = (sys_pvTimeFunPtr)pvFun;
            u8TimeEventID = i;
            _sys_uTimerEventCount += 1;
            break;
        }
    }

    if (i == TICKEVENTCOUNT)
    {
        return -1;    /* -1 means invalid channel */
    }
    else
    {
        return u8TimeEventID;    /* Event ID start from 0*/
    }
}

void TickCheckTickEvent(void)
{
    unsigned char i = 0;

    if (_sys_uTimerEventCount)
    {
        for (i = 0; i < TICKEVENTCOUNT; i++)
        {
            if (tTimerEvent[i].active)
            {
                tTimerEvent[i].curTick--;

                if (tTimerEvent[i].curTick == 0)
                {
                    (*tTimerEvent[i].funPtr)();
                    tTimerEvent[i].curTick = tTimerEvent[i].initTick;
                }
            }
        }
    }
}

void TickInitTickEvent(void)
{
    unsigned char i = 0;

    _sys_uTimerEventCount = 0;

    /* Remove all callback function */
    for (i = 0; i < TICKEVENTCOUNT; i++)
        TickClearTickEvent(i);

    _sys_uTimerEventCount = 0;
}
#endif 

