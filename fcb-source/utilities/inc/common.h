/******************************************************************************
 * @file    common.h
 * @author  Dragonfly
 * @version v. 1.0.0
 * @date    2015-06-09
 * @brief   Header file for common functions
 ******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __COMMON_H
#define __COMMON_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f3xx.h"

/* Exported constants --------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/

#define IS_POS(X) ((X) >= 0)

/* Exported function prototypes --------------------------------------------- */
void InitCRC(void);
uint32_t CalculateCRC(const uint8_t* dataBuffer, const uint32_t dataBufferSize);
uint16_t UInt16Mean(const uint16_t* buffer, const uint16_t length);

void InitLEDs(void);
void LEDsOff(void);
void ToggleLEDs(void);

#endif /* __COMMON_H */

/**
 * @}
 */

/**
 * @}
 */
/*****END OF FILE****/