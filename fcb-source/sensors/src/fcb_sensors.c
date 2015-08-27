/******************************************************************************
 * @file    usb_cdc_cli.c
 * @author  Dragonfly
 * @version v. 1.0.0
 * @date    2015-07-24
 * @brief   Implementation of interface publicised in fcb_sensors.h
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "fcb_sensors.h"
#include "gyroscope.h"
#include "fcb_error.h"
#include "fcb_retval.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "queue.h"

#include <stdint.h>

/* Private define ------------------------------------------------------------*/
#define FCB_SENSORS_DEBUG /* todo delete */

#define SENSOR_PRINT_SAMPLING_THREAD_PRIO		3

#ifdef FCB_SENSORS_DEBUG
static uint32_t cbk_gyro_counter = 0;
#endif

/* Private typedef -----------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
/* static data declarations */

static xTaskHandle tFcbSensors;
static xQueueHandle qFcbSensors = NULL;

/* Task handle for printing of sensor values task */
xTaskHandle SensorPrintSamplingTaskHandle = NULL;
static volatile uint16_t sensorPrintSampleTime;
static volatile uint16_t sensorPrintSampleDuration;

/* Private function prototypes -----------------------------------------------*/
static void ProcessSensorValues(void*);
static void SensorPrintSamplingTask(void const *argument);

/* static fcn declarations */

/* Exported functions --------------------------------------------------------*/
/* global fcn definitions */
int FcbSensorsConfig(void) {
    portBASE_TYPE rtosRetVal;
    int retVal = FCB_OK;

    if (pdFAIL == (qFcbSensors = xQueueCreate(FCB_SENSORS_QUEUE_SIZE, FCB_SENSORS_Q_MSG_SIZE))) {
    	ErrorHandler();
        goto Error;
    }


    if (pdPASS != (rtosRetVal = xTaskCreate((pdTASK_CODE)ProcessSensorValues,
                                (signed portCHAR*)"tFcbSensors",
                                4 * configMINIMAL_STACK_SIZE,
                                NULL /* parameter */,
                                1 /* priority */,
                                &tFcbSensors))) {
    	ErrorHandler();
        goto Error;
    }

Exit:
    return retVal;
Error:
	/* clean up */
	retVal = FCB_ERR_INIT;
	goto Exit;

}

void FcbSendSensorMessageFromISR(uint8_t msg) {
    portBASE_TYPE higherPriorityTaskWoken = pdFALSE;
#ifdef FCB_SENSORS_DEBUG
    if ((cbk_gyro_counter % 48) == 0) {
        BSP_LED_Toggle(LED5);
    }
    cbk_gyro_counter++;
#endif

    if (pdTRUE != xQueueSendFromISR(qFcbSensors, &msg, &higherPriorityTaskWoken)) {
    	ErrorHandler();
    }

    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

/*
 * @brief  Creates a task to sample print receiver values over USB
 * @param  sampleTime : Sets how often a sample should be printed
 * @param  sampleDuration : Sets for how long sampling should be performed
 * @retval RECEIVER_OK if thread started, else RECEIVER_ERROR
 */
SensorsErrorStatus StartSensorSamplingTask(const uint16_t sampleTime, const uint32_t sampleDuration) {
	sensorPrintSampleTime = sampleTime;
	sensorPrintSampleDuration = sampleDuration;
//	bool printGyroSamples; // TODO booleans to set which sensors should be sampled
//	bool printAccSamples;
//	bool printMagnSamples;

	/* Sensor value print sampling handler thread creation
	 * Task function pointer: SensorPrintSamplingTask
	 * Task name: SENS_PRINT_SAMPL
	 * Stack depth: configMINIMAL_STACK_SIZE
	 * Parameter: NULL
	 * Priority: SENSOR_PRINT_SAMPLING_THREAD_PRIO ([0, inf] possible)
	 * Handle: SensorPrintSamplingTaskHandle
	 * */
	if (pdPASS != xTaskCreate((pdTASK_CODE )SensorPrintSamplingTask, (signed portCHAR*)"SENS_PRINT_SAMPL",
			configMINIMAL_STACK_SIZE, NULL, SENSOR_PRINT_SAMPLING_THREAD_PRIO, &SensorPrintSamplingTaskHandle)) {
		ErrorHandler();
		return SENSORS_ERROR;
	}

	return SENSORS_OK;
}

/*
 * @brief  Stops receiver print sampling by deleting the task
 * @param  None
 * @retval RECEIVER_OK if task deleted
 */
SensorsErrorStatus StopSensorSamplingTask(void) {
	vTaskDelete(SensorPrintSamplingTaskHandle);
	return SENSORS_OK;
}

/* Private functions ---------------------------------------------------------*/

static void ProcessSensorValues(void* val) {
	/*
	 * configures the sensors to start giving Data Ready interrupts
	 * and then polls the queue in an infinite loop
	 */
    uint8_t msg;

    if (FCB_OK != InitialiseGyroscope()) {
    	ErrorHandler();
    }

    while (1) {
        if (pdFALSE == xQueueReceive (qFcbSensors,
                                      &msg,
                                      portMAX_DELAY /* 1000 *//* configTICK_RATE_HZ is 1000 */)) {
            /*
             * if no message was received, no interrupts from the sensors
             * aren't arriving and this is a serious error.
             */
        	ErrorHandler();
            goto Error;
        }

        switch (msg) {
            case FCB_SENSOR_GYRO_DATA_READY:
                /*
                 * As settings are in BSP_GYRO_Init, the callback is called with a frequency
                 * of 94.5 Hz according to oscilloscope.
                 */
                FetchAngleDotFromGyroscope();
                break;
            case FCB_SENSOR_GYRO_CALIBRATE:
                break;
            case FCB_SENSOR_MAGNETO_ACC_DATA_READY:
                break;
            case FCB_SENSOR_MAGNETO_ACC_CALIBRATE:
                break;
        }

        /* todo: call the state correction part of the Kalman Filter every time
         * we get new sensor values.
         *
         * Either from this function or the separate gyro / acc / magnetometer functions.
         */
    }
Exit:
    return;
Error:
    goto Exit;
}

/**
 * @brief  Task code handles sensor print sampling
 * @param  argument : Unused parameter
 * @retval None
 */
static void SensorPrintSamplingTask(void const *argument) {
	(void) argument;

	portTickType xLastWakeTime;
	portTickType xSampleStartTime;

	/* Initialise the xLastWakeTime variable with the current time */
	xLastWakeTime = xTaskGetTickCount();
	xSampleStartTime = xLastWakeTime;

	for (;;) {
		vTaskDelayUntil(&xLastWakeTime, sensorPrintSampleTime);

		PrintGyroscopeValues();
		// TODO PrintAccelerometerValues();
		// TODO PrintMagnetometerValues();

		/* If sampling duration exceeded, delete task to stop sampling */
		if (xTaskGetTickCount() >= xSampleStartTime + sensorPrintSampleDuration * configTICK_RATE_HZ)
			StopSensorSamplingTask();
	}
}

/**
 * @}
 */

/**
 * @}
 */
/*****END OF FILE****/