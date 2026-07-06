#ifndef __APP_TYPES_H__
#define __APP_TYPES_H__

/*_____ I N C L U D E S ____________________________________________________*/
// #include <stdio.h>
// #include "NuMicro.h"

/*_____ D E C L A R A T I O N S ____________________________________________*/

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

typedef enum
{
    CHANNEL0, 
    CHANNEL1, 
    CHANNEL2, 
    CHANNEL3, 
    CHANNEL4, 
    CHANNEL5,
    CHANNEL6, 
    CHANNEL7, 
    CHANNEL8, 
    CHANNEL9, 
    CHANNEL10,
    CHANNEL11,
    CHANNEL12,
    CHANNEL13,
    CHANNEL14,
    CHANNEL15,
} channel_t;

typedef enum
{
    APP_IDLE = 0,
    APP_WAIT_TEMP,
    APP_WAIT_INPUT,
    APP_TEST_INPUT,
    APP_RESULT_BLINK,
    APP_HOLD
} app_state_t;

typedef struct
{
    uint8_t wait_units_5s;   /* 0~15 */
    uint8_t test_units_5s;   /* 0~15 */
    uint8_t temp_set;        /* 0~255 (mapping later) */
} app_config_t;


#define GPIO_SET_BIT(x, v)   do { (x) = ((v) != 0U) ? 1U : 0U; } while (0)

/*_____ M A C R O S ________________________________________________________*/

/*_____ F U N C T I O N S __________________________________________________*/


#endif //__APP_TYPES_H__
