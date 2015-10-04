/**
 * @file fcb_accelerometer.c
 *
 * Implements fcb_accelerometer_magnetometer.h API
 *
 * This file does most of the GPIO & NVIC setup to receive the interrupts
 * from LSM303DLHC.
 *
 * The lsm303dlhc.c file does most of the configuration of the acc/magneto-meter
 * itself.
 *
 * This file also handles coordinate transform to translate magnetometer &
 * accelerometer data to the quadcopter x y z axes.
 *
 * @see fcb_accelerometer.h
 */
#include "fcb_accelerometer_magnetometer.h"
#include "fcb_sensor_calibration.h"
#include "fcb_sensors.h"
#include "fcb_error.h"
#include "lsm303dlhc.h"
#include "usbd_cdc_if.h"
#include "arm_math.h"
#include "trace.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include <stdint.h>
#include <stdio.h>

#define FCB_ACCMAG_DEBUG

enum { ACCMAG_AXES_N = 3 };
enum { X_IDX = 0 }; /* index into sGyroXYZDotDot & ditto Offset sXYZMagVectors */
enum { Y_IDX = 1 }; /* as above */
enum { Z_IDX = 2 }; /* as above */

enum { ACCMAG_SAMPLING_MAX_STRING_SIZE = 128 }; /* print-to-usb com port sampling */

static uint8_t sampleMax = ACCMAG_CALIBRATION_SAMPLES_N;
static uint32_t sampleIndex = 0;
#ifndef FCB_SENSORS_SCILAB_CALIB
static float32_t calibrationSamples[ACCMAG_CALIBRATION_SAMPLES_N][ACCMAG_AXES_N];
#endif // FCB_SENSORS_SCILAB_CALIB
static float32_t sXYZDotDot[] = { 0, 0 , 0 };
static float32_t sXYZMagVector[] = { 0, 0 , 0 };

/**
 * @see FcbSensorCalibrationParmIndex for what the numbers mean.
 */
static float32_t sXYZMagCalPrm[CALIB_IDX_MAX] {
  - 0.0127656,    0.0804974,    0.0338544,    0.9025001,    0.9189748,    0.9415154
}; /* values copied from MagnetometerCalibration.sce */

static enum FcbMagnetometerMode magMode = MAGMTR_UNINITIALISED;
static enum FcbAccelerometerMode accMode = ACCMTR_UNINITIALISED;


/* static fcn declarations */

#ifndef FCB_SENSORS_SCILAB_CALIB
/**
 * Gauss-Newton method for least sphere fitting.
 */
static void GaussNewtonLeastSphereFit(void);
#endif

/* public fcn definitions */

uint8_t FcbInitialiseAccMagSensor(void) {
  uint8_t retVal = FCB_OK;

  if ((magMode != MAGMTR_UNINITIALISED) && (accMode != ACCMTR_UNINITIALISED)) {
    /* they are already initialised - this is a logical error. */
    return FCB_ERR_INIT;
  }

  /* configure STM32 interrupts & GPIO */
  ACCELERO_DRDY_GPIO_CLK_ENABLE(); /* GPIOE clock */

  /* STM32F3 doc UM1570 page 27/36. Accelerometer interrupt */
  FcbSensorsInitGpioPinForInterrupt(GPIOE, GPIO_PIN_4);

  /* STM32F3 doc UM1570 page 27/36. Magnetometer interrupt */
  FcbSensorsInitGpioPinForInterrupt(GPIOE, GPIO_PIN_2);

#ifdef FCB_ACCMAG_DEBUG
  {
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };
    GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
  }
#endif

  HAL_NVIC_SetPriority(EXTI4_IRQn,
      configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

  HAL_NVIC_SetPriority(EXTI2_TSC_IRQn,
      configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY, 0);
  HAL_NVIC_EnableIRQ(EXTI2_TSC_IRQn);

  /* ISSUE1_TODO - fetch accelerometer calib from flash */
  /* ISSUE2_TODO - fetch magnetometer calib from flash */

  LSM303DLHC_AccConfig();
  LSM303DLHC_MagInit();

  /* do a pre-read to get the DRDY interrupts going. Since we trig on
   * rising flank and the sensor has data from power-on, by the time we get
   * here the interrupt is already high. Reading the data trigs the
   * sensor to load a new set of values into its registers.
   */
  LSM303DLHC_AccReadXYZ(sXYZDotDot);
  LSM303DLHC_MagReadXYZ(sXYZMagVector);

  accMode = ACCMTR_FETCHING;
  magMode = MAGMTR_FETCHING;
  return retVal;
}

void FetchDataFromAccelerometer(void) {
  float32_t acceleroMeterData[3] = { 0.0f, 0.0f, 0.0f };
#ifdef FCB_ACCMAG_DEBUG
  static uint32_t call_counter = 0;

  {
    if ((call_counter % 50) == 0) {
      BSP_LED_Toggle(LED8);
    }
    call_counter++;
  }

  /* measure duration with oscilloscope on pin PD9 */
  HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_9);
#endif

  LSM303DLHC_AccReadXYZ(acceleroMeterData);

  /* TODO apply calibration */

  /* from accelerometer to quadcopter coordinate axes
   * see "Sensors" page in Wiki.
   */
  sXYZDotDot[X_IDX] = acceleroMeterData[Y_IDX];
  sXYZDotDot[Y_IDX] = -acceleroMeterData[X_IDX];
  sXYZDotDot[Z_IDX] = -acceleroMeterData[Z_IDX];

#ifdef FCB_ACCMAG_DEBUG
  HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_9);
#endif
}

void BeginMagnetometerCalibration(uint8_t samples) {
  configASSERT(samples < 251);
  configASSERT(samples >= ACCMAG_CALIBRATION_SAMPLES_N);

  sampleMax = samples;
  magMode = MAGMTR_CALIBRATING;
#ifdef FCB_ACCMAG_DEBUG
  BSP_LED_On(LED6); /** ACCMAG_TODO - LED doesn't really work to indicate btn press. Nice to have - delete if hard to fix */
#endif
}


void FetchDataFromMagnetometer(void) {
  float magnetoMeterData[3] = { 0.0f, 0.0f, 0.0f };
#ifdef FCB_ACCMAG_DEBUG
  static uint32_t call_counter = 0;

  {
    if ((call_counter % 75) == 0) {
      if (MAGMTR_FETCHING == magMode) {
        BSP_LED_Toggle(LED6);
      }
    }
    call_counter++;
  }

  /* measure duration with oscilloscope on pin PD11 */
    HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_11);
#endif

  if (MAGMTR_FETCHING == magMode || MAGMTR_CALIBRATING == magMode) {
    LSM303DLHC_MagReadXYZ(magnetoMeterData);

    /* TODO ... and then copy them into a mutex-protected sXYZMagVector here */

    /* adjust magnetometer axes to the axes of the quadcopter fuselage
     * see "Sensors" page in Wiki.
     */
    sXYZMagVector[X_IDX] = magnetoMeterData[X_IDX];
    sXYZMagVector[Y_IDX] = - magnetoMeterData[Y_IDX];
    sXYZMagVector[Z_IDX] = - magnetoMeterData[Z_IDX];

    sXYZMagVector[X_IDX] = (sXYZMagVector[X_IDX] - sXYZMagCalPrm[X_OFFSET_CALIB_IDX]) * sXYZMagCalPrm[X_SCALING_CALIB_IDX];
    sXYZMagVector[Y_IDX] = (sXYZMagVector[Y_IDX] - sXYZMagCalPrm[Y_OFFSET_CALIB_IDX]) * sXYZMagCalPrm[Y_SCALING_CALIB_IDX];
    sXYZMagVector[Z_IDX] = (sXYZMagVector[Z_IDX] - sXYZMagCalPrm[Z_OFFSET_CALIB_IDX]) * sXYZMagCalPrm[Z_SCALING_CALIB_IDX];

#ifdef FCB_ACCMAG_DEBUG
    HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_11);
#endif
  }

  if (MAGMTR_CALIBRATING == magMode) {
    if (sampleIndex < sampleMax) {

#ifndef FCB_SENSORS_SCILAB_CALIB
      /* store sample */
      calibrationSamples[sampleIndex][X_IDX] = sXYZMagVector[X_IDX];
      calibrationSamples[sampleIndex][Y_IDX] = sXYZMagVector[Y_IDX];
      calibrationSamples[sampleIndex][Z_IDX] = sXYZMagVector[Z_IDX];
#endif
      sampleIndex++;
    } else if (sampleIndex == sampleMax) {
#ifndef FCB_SENSORS_SCILAB_CALIB
      /* run Gauss-Newton Least Sphere fit algorithm */

      /* ISSUE2_TODO do the offset & scaling calculations */

      /* ISSUE2_TODO store the offsets (in flash) & scaling */

      /* calibration done */
#else
      /* - copy values from CLI to SciLab data file
       * - run SciLab Gauss-Newton
       * - insert calibration values manually into code
       */
#endif
      magMode = MAGMTR_FETCHING;
      sampleIndex = 0;
    }
  }
}

void GetAcceleration(int16_t * xDotDot, int16_t * yDotDot, int16_t * zDotDot) {
  *xDotDot = sXYZDotDot[X_IDX];
  *yDotDot = sXYZDotDot[Y_IDX];
  *zDotDot = sXYZDotDot[Z_IDX];
}

void GetMagVector(float32_t * x, float32_t * y, float32_t * z) {
  *x = sXYZMagVector[X_IDX];
  *y = sXYZMagVector[Y_IDX];
  *z = sXYZMagVector[Z_IDX];
}


void PrintAccelerometerValues(void) {
  static char sampleString[ACCMAG_SAMPLING_MAX_STRING_SIZE];

  snprintf((char*) sampleString, ACCMAG_SAMPLING_MAX_STRING_SIZE,
      "Accelerometer readings [m/(s * s)]:\nAccX: %f\nAccY: %f\nAccZ: %f\n\r\n",
      sXYZDotDot[X_IDX],
      sXYZDotDot[Y_IDX],
      sXYZDotDot[Z_IDX]);

  USBComSendString(sampleString);
}


#ifdef ACCMAG_TODO
void GaussNewtonLeastSphereFit(void) {

  /**
   *  Following the implementation developed by Rolfe Schmidt at the below links
   * N = number of samples
   *
   * 1) Naive implementation, fundamentals of Gauss-Newton computation
   *    - https://chionophilous.wordpress.com/2012/09/01/implementing-the-gauss-newton-algorithm-for-sphere-fitting-1-of-3/
   *    This the memory complexity of the naive implementation is O(34N + 156) according to RS
   *    due to large Jacobian matrices whose size is proportional to N. So he develops a
   *    refined version below:
   * 2) This version does not scale memory use with respect to N:
   *    - https://chionophilous.wordpress.com/2012/09/08/implementing-the-gauss-newton-algorithm-for-sphere-fitting-2-of-3/
   *      but it still ...
   *      - requires all N samples to be stored in memory.
   *      - requires multiple passes over observation data.
   * 3) So the final version he develops is a one-pass streaming algorithm:
   *     - https://chionophilous.wordpress.com/2012/09/15/implementing-the-gauss-newton-algorithm-for-sphere-fitting-3-of-3/
   */

  // N samples
  // of 3-tuple values
  // each value is scaled & offset
  float32_t r[N];   //residual vector
  float32_t J[N][6]; // Jacobian matrix N rows, 6 columns. 6 = 2 for each vector dimension (3)
  float32_t x[N][3]; //the observations - N by 3-tuple
  float32_t JtJ[21]; /* The left-hand side of equation 2. It is symmetric so
                      * we only need to store 21 of the 36 matrix entires.
                      */
  float32_t JtR[6]; //The right-hand side of equation 2.
}
#endif
