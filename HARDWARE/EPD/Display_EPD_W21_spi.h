#ifndef _DISPLAY_EPD_W21_SPI_
#define _DISPLAY_EPD_W21_SPI_

#include "CH59x_common.h"

/* EPD 引脚定义（CH592X）
 * EPD_BUSY  = PA6  (输入)
 * EPD_RST   = PA4  (输出)
 * EPD_DC    = PA5  (输出)
 * EPD_CS    = PA12 (输出)
 * SPI_SCLK  = PA13 (输出)
 * SPI_MOSI  = PA14 (输出)
 */
#define EPD_BUSY_PIN    GPIO_Pin_6    /* PA6  */
#define EPD_RST_PIN     GPIO_Pin_4    /* PA4  */
#define EPD_DC_PIN      GPIO_Pin_5    /* PA5  */
#define EPD_CS_PIN      GPIO_Pin_12   /* PA12 */
#define EPD_CLK_PIN     GPIO_Pin_13   /* PA13 */
#define EPD_MOSI_PIN    GPIO_Pin_14   /* PA14 */

#define EPD_W21_MOSI_0  GPIOA_ResetBits(EPD_MOSI_PIN)
#define EPD_W21_MOSI_1  GPIOA_SetBits(EPD_MOSI_PIN)

#define EPD_W21_CLK_0   GPIOA_ResetBits(EPD_CLK_PIN)
#define EPD_W21_CLK_1   GPIOA_SetBits(EPD_CLK_PIN)

#define EPD_W21_CS_0    GPIOA_ResetBits(EPD_CS_PIN)
#define EPD_W21_CS_1    GPIOA_SetBits(EPD_CS_PIN)

#define EPD_W21_DC_0    GPIOA_ResetBits(EPD_DC_PIN)
#define EPD_W21_DC_1    GPIOA_SetBits(EPD_DC_PIN)

#define EPD_W21_RST_0   GPIOA_ResetBits(EPD_RST_PIN)
#define EPD_W21_RST_1   GPIOA_SetBits(EPD_RST_PIN)

/* BUSY：高电平表示忙，低电平表示空闲 */
#define isEPD_W21_BUSY  GPIOA_ReadPortPin(EPD_BUSY_PIN)


void SPI_Write(unsigned char value);
void EPD_W21_WriteDATA(unsigned char datas);
void EPD_W21_WriteCMD(unsigned char command);
void EPD_GPIO_Init(void);

#endif  //#ifndef _MCU_SPI_H_
