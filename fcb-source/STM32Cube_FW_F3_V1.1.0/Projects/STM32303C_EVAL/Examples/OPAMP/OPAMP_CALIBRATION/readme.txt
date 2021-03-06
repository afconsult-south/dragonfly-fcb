/**
  @page OPAMP_CALIBRATION & amplify an applied signal example  
  @verbatim
  ******************** (C) COPYRIGHT 2014 STMicroelectronics *******************
  * @file    OPAMP/OPAMP_CALIBRATION/readme.txt 
  * @author  MCD Application Team
  * @version V1.0.0
  * @date    18-June-2014
  * @brief   Description of the DAC Signals generation example.
  ******************************************************************************
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  @endverbatim

@par Example Description 

On OPAMP1, this example shows how to 
- Get factory calibration (trimming) settings.
- Enable/disable OPAMP
- Modify parameters (gain) on the OPAMP on the fly (OPAMP in on in PGA mode).
- Calibrate OPAMP peripheral before and after use of OPAMP.
  Hence changes can be monitored (versus factory settings or in case of 
  temperature impact for instance).

on OPAMP1 non inverting input using the built-in PGA.
OPAMP1 is configured as following:
   - PGA mode enabled with gain from 2 to 16
   - Non Inverting input is connected to PA5
   - Output is available on PA2  

In this configuration the OPAMP1 amplifies the signal on PA5.
In this example the DAC peripheral is configured to generate a weak sine wave signal
on DAC_OUT2 (PA5) which will be amplified by  either 2, 4, 8, 16 by use of OPAMP1.
lED1 is blinking (rapidly or slowly) as per calibration results. 

- Connect an oscilloscope to DAC_OUT2 (PA5) to display the sine wave signal
  generated by the DAC. 
- Connect an oscilloscope to OPAMP1 Out (PA2) 
  -> to display the amplified (by 2, 4; 8, 16)sine wave signal.
  -> Monitor enabling/disabling of OPAMP.
- Push the Key Push button (connected to PE6 pin) to switch from one step to the 
next one.
- Key push button steps are:
  -> 1st Push: 
     + Factory trimming retrieved 
     + User trimming set according to self calibration (1st run)
     + Factory trimming and user trimming are compared
       - If The LED blinks rapidly: Calibrated trimming are different from Factory Trimming
       - ................. slowly : Calibrated trimming same as Factory Trimming 
  -> 2nd Push: 
     + OPAMP start with gain = 2
     + User of just calibrated user Trimming 
  -> 3rd Push: 
     + OPAMP gain changed on the fly to 4
  -> 4th Push: 
     + OPAMP gain changed on the fly to 8
  -> 5th Push: 
     + OPAMP gain changed on the fly to 16
  -> 6th Push: 
     + OPAMP stopped    
  -> 7th Push: 
     + Self calibration (2nd run)
     + results compare to 1st run.
       - If The LED blinks rapidly: New calibrated trimming are different from ones calibrated before run
       - ................. slowly : New calibrated trimming are same as ones calibrated before run 
  -> 8th Push: end of test
    
@par Directory contents 

  - OPAMP/OPAMP_PGA/Inc/stm32f3xx_hal_conf.h    HAL configuration file
  - OPAMP/OPAMP_PGA/Inc/stm32f3xx_it.h          DMA interrupt handlers header file
  - OPAMP/OPAMP_PGA/Inc/main.h                        Header for main.c module  
  - OPAMP/OPAMP_PGA/Src/stm32f3xx_it.c          DMA interrupt handlers
  - OPAMP/OPAMP_PGA/Src/main.c                        Main program
  - OPAMP/OPAMP_PGA/Src/stm32f3xx_hal_msp.c     HAL MSP file
  - OPAMP/OPAMP_PGA/Src/system_stm32f3xx.c      STM32F3xx system source file

@par Hardware and Software environment  
  
  - This example runs on STM32F303xC devices.
    
  - This example has been tested with STM32303C-EVAL RevC board and can be
    easily tailored to any other supported device and development board.

  - STM32303C-EVAL RevC Set-up  
    - Connect an oscilloscope to PA5 pin to display waveform.
    - Connect an oscilloscope to PA7 pin to display waveform.
    - Connect an oscilloscope to PA2 pin to display waveform.

@par How to use it ? 

In order to make the program work, you must do the following :
 - Open your preferred toolchain 
 - Rebuild all files and load your image into target memory
 - Run the example
  
 * <h3><center>&copy; COPYRIGHT STMicroelectronics</center></h3>
 */

