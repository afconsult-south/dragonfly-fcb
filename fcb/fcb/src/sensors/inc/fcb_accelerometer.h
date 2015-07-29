#ifndef FCB_ACCELEROMETER_H
#define FCB_ACCELEROMETER_H
/**
 * @file fcb_accelerometer.h
 *
 * Implements an interface to the accelerometer part of the LSM303DLHC
 * combined magnetometer & accelerometer.
 *
 * @see fcb_magnetometer.h
 */

#include "fcb_retval.h"
#include "stm32f3_discovery.h"

/**
 * The Data Ready input from the gyro sensor.
 *
 * This definition is intended to be used in the
 * HAL_GPIO_EXTI_Callback function.
 */
#define GPIO_ACCELEROMETER_DRDY GPIO_PIN_2


/**
 * Initialises
 *
 * @retval FCB_OK, error otherwise
 */
uint8_t FcbInitialiseAccelerometer(void);


/**
 * This method is intended to be called from the EXTI1 ISR.
 *
 * It won't get called until InitialiseAccelerometer has returned
 * success.
 *
 * It reads the accelerometer values.
 */
void AccelerometerHandleDataReady(void);


/**
 * Fetches data (rotation speed, or angle dot) from accelerometer
 * sensor.
 */
void FetchDataFromAccelerometer(void);

/*
 * get the current reading from the accelerometer.
 *
 * It is updated at a rate of 50 Hz (configurable).
 */
void GetAcceleration(int16_t * xDotDot, int16_t * yDotDot, int16_t * zDotDot);

#endif /* FCB_ACCELEROMETER_H */
