/******************************************************************************
 * @file    gyroscope.c
 * @author  Dragonfly
 * @version v. 1.0.0
 * @date    2015-08-26
 * @brief   File contains functionality to handle and read the STM32F3-Discovery
 * 			on-board gyroscope sensor L3DG20.
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "gyroscope.h"

#include "stm32f3_discovery_gyroscope.h"

#include "fcb_error.h"
#include "FreeRTOS.h"
#include "usbd_cdc_if.h"

#include "trace.h"

#include <string.h>
#include <stdio.h>

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
// #define FCB_GYRO_DEBUG

/* static & local declarations */

enum { GYROSCOPE_OFFSET_SAMPLES = 200 };

enum { XDOT_IDX = 0 }; /* index of sGyroXYZAngleDot & ditto Offset */
enum { YDOT_IDX = 1 }; /* as above */
enum { ZDOT_IDX = 2 }; /* as above */

/**
 * @todo in the ideal world we shouldn't read the gyro data
 * in the ISR itself, but in a dedicated thread.
 *
 * At the time of writing, giving a semaphore from GyroHandleDataReady
 * (which is called from an ISR) doesn't work, FreeRTOS hangs.
 *
 * So this is an interim solution.
 */
#define READ_GYRO_FROM_ISR
#define GYRO_SAMPLING_MAX_STRING_SIZE			128
#define	GYRO_SAMPLE_VALUE_STRING_SIZE			8

/**
 * Angular velocities, in degrees.
 *
 */

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

static volatile float sGyroXYZAngleDot[3] = { 0.0, 0.0, 0.0 };
static volatile float sGyroXYZAngleDotOffset[3] = { 0.0, 0.0, 0.0 };

/* Private function prototypes -----------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/* global fcn definitions */
uint8_t InitialiseGyroscope(void) {
    uint8_t retVal = FCB_OK;

    GPIO_InitTypeDef GPIO_InitStructure;
    BSP_LED_On(LED4);

    /* configure GYRO DRDY (data ready) interrupt */
    GYRO_CS_GPIO_CLK_ENABLE(); /* happens to be GPIOE */

    GPIO_InitStructure.Pin = GPIO_PIN_1; /* STM32F3 doc UM1570 page 27/36 */
    GPIO_InitStructure.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStructure.Pull = GPIO_NOPULL;
    GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
    HAL_GPIO_Init(GYRO_INT_GPIO_PORT, &GPIO_InitStructure);

    HAL_NVIC_SetPriority(EXTI1_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY, 0);
    HAL_NVIC_EnableIRQ(EXTI1_IRQn);

    BSP_GYRO_Reset();

    /* sets full scale (and sensitivity) plus data rate of L3GD20 gyroscope */
    if(BSP_GYRO_Init() != HAL_OK)
    {

        /* Initialization Error */
    	ErrorHandler();
    }

    FetchAngleDotFromGyroscope(); /* necessary so a fresh DRDY can be triggered */
    return retVal;
}


void FetchAngleDotFromGyroscope(void) {
#ifdef FCB_GYRO_DEBUG
    static uint32_t call_counter = 0;
#else
    static uint16_t call_counter = 0;
#endif
    /* returns rad/s */
    BSP_GYRO_GetXYZ((float*)sGyroXYZAngleDot);

    if (GYROSCOPE_OFFSET_SAMPLES > call_counter) {
    	sGyroXYZAngleDotOffset[XDOT_IDX] += sGyroXYZAngleDot[XDOT_IDX];
    	sGyroXYZAngleDotOffset[YDOT_IDX] += sGyroXYZAngleDot[YDOT_IDX];
    	sGyroXYZAngleDotOffset[ZDOT_IDX] += sGyroXYZAngleDot[ZDOT_IDX];
        call_counter++;
    } else if (GYROSCOPE_OFFSET_SAMPLES  == call_counter) {
    	sGyroXYZAngleDotOffset[XDOT_IDX] = sGyroXYZAngleDotOffset[XDOT_IDX] / GYROSCOPE_OFFSET_SAMPLES;
    	sGyroXYZAngleDotOffset[YDOT_IDX] = sGyroXYZAngleDotOffset[YDOT_IDX] / GYROSCOPE_OFFSET_SAMPLES;
    	sGyroXYZAngleDotOffset[ZDOT_IDX] = sGyroXYZAngleDotOffset[ZDOT_IDX] / GYROSCOPE_OFFSET_SAMPLES;
        call_counter++;
    } else {
    	sGyroXYZAngleDot[XDOT_IDX] = sGyroXYZAngleDot[XDOT_IDX] - sGyroXYZAngleDotOffset[XDOT_IDX];
    	sGyroXYZAngleDot[YDOT_IDX] = sGyroXYZAngleDot[YDOT_IDX] - sGyroXYZAngleDotOffset[YDOT_IDX];
    	sGyroXYZAngleDot[ZDOT_IDX] = sGyroXYZAngleDot[ZDOT_IDX] - sGyroXYZAngleDotOffset[ZDOT_IDX];

#ifdef FCB_GYRO_DEBUG
    	if (call_counter % 200) {
    		TRACE_SYNC("tim:%u sum xyzdot:%1.1f, %1.1f, %1.1f\n",
    				(uint32_t) call_counter,
    				sGyroXYZAngleDot[XDOT_IDX],
    				sGyroXYZAngleDot[YDOT_IDX],
    				sGyroXYZAngleDot[ZDOT_IDX]);
    	}
    	call_counter++;
#endif
    }


}

#warning TODO - implement get x, y, z separately. And perphaps use integer values to represent sensor values
void GetAngleDot(float * xAngleDot, float * yAngleDot, float * zAngleDot)
{
	*xAngleDot = sGyroXYZAngleDot[XDOT_IDX];
	*yAngleDot = sGyroXYZAngleDot[YDOT_IDX];
	*zAngleDot = sGyroXYZAngleDot[ZDOT_IDX];
}

/**
 * @brief  Prints the latest gyroscope sensor values to the USB com port
 * @param  none
 * @retval none
 */
void PrintGyroscopeValues(void)
{
	static char sampleString[GYRO_SAMPLING_MAX_STRING_SIZE];

	float rollDot, pitchDot, yawDot;
	char sampleValueTmpString[GYRO_SAMPLE_VALUE_STRING_SIZE]; // Only needs enough space to store a float in string format
	memset(sampleValueTmpString, 0x00, sizeof(sampleValueTmpString));

	GetAngleDot(&rollDot, &pitchDot, &yawDot);

	strncpy(sampleString, "Gyroscope roll rates [rad/s]: ", GYRO_SAMPLING_MAX_STRING_SIZE);
	strncat((char*) sampleString, "\nRoll: ", GYRO_SAMPLING_MAX_STRING_SIZE - strlen(sampleString) - 1);
	snprintf((char*) sampleValueTmpString, 8, "%1.6f", rollDot);
	strncat((char*) sampleString, sampleValueTmpString, GYRO_SAMPLING_MAX_STRING_SIZE - strlen(sampleString) - 1);
	strncat((char*) sampleString, "\nPitch: ", GYRO_SAMPLING_MAX_STRING_SIZE - strlen(sampleString) - 1);
	snprintf((char*) sampleValueTmpString, 8, "%1.6f", pitchDot);
	strncat((char*) sampleString, sampleValueTmpString, GYRO_SAMPLING_MAX_STRING_SIZE - strlen(sampleString) - 1);
	strncat((char*) sampleString, "\nYaw: ", GYRO_SAMPLING_MAX_STRING_SIZE - strlen(sampleString) - 1);
	snprintf((char*) sampleValueTmpString, 8, "%1.6f", yawDot);
	strncat((char*) sampleString, sampleValueTmpString, GYRO_SAMPLING_MAX_STRING_SIZE - strlen(sampleString) - 1);
	strncat((char*) sampleString, "\r\n\n", GYRO_SAMPLING_MAX_STRING_SIZE - strlen(sampleString) - 1);

	USBComSendString(sampleString);
}