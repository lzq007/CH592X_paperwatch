#include "Display_EPD_W21_spi.h"

//E-paper GPIO initialization
void EPD_GPIO_Init(void)
{
    /* CS(PA12) SCK(PA13) MOSI(PA14) DC(PA5) RST(PA4) 输出 */
    GPIOA_ModeCfg(EPD_CS_PIN | EPD_CLK_PIN | EPD_MOSI_PIN |
                  EPD_DC_PIN | EPD_RST_PIN, GPIO_ModeOut_PP_5mA);
    /* BUSY(PA6) 输入，上拉 */
    GPIOA_ModeCfg(EPD_BUSY_PIN, GPIO_ModeIN_PU);
}



//SPI write byte
void SPI_Write(unsigned char value)
{
    unsigned char i;
    EPD_W21_CLK_0;
    for(i=0;i<8;i++)
    {
        if(value&0x80)
            EPD_W21_MOSI_1;
        else
            EPD_W21_MOSI_0;
        EPD_W21_CLK_1;
        EPD_W21_CLK_0;
        value=value<<1;
    }
}

//SPI write command
void EPD_W21_WriteCMD(unsigned char command)
{
    EPD_W21_CS_0;
    EPD_W21_DC_0;  // D/C#   0:command  1:data
    SPI_Write(command);
    EPD_W21_CS_1;
}
//SPI write data
void EPD_W21_WriteDATA(unsigned char datas)
{
    EPD_W21_CS_0;
    EPD_W21_DC_1;  // D/C#   0:command  1:data
    SPI_Write(datas);
    EPD_W21_CS_1;
}
