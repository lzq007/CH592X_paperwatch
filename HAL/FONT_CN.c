/********************************** (C) COPYRIGHT *******************************
 * File Name          : FONT_CN.c
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2026/08/21
 * Description        : 中文字库渲染模块（GB2312 一级汉字 16x16 加粗点阵）
 *
 *                      字模数据编译进固件（g_gb2312_16_font[]），
 *                      通过 GB2312 区/位码直接寻址读取。
 *
 *                      GB2312 编码：
 *                      区字节 0xB0~0xD7（16~55区）
 *                      位字节 0xA1~0xFE（1~94位）
 *                      偏移 = ((区-0xB0)*94 + (位-0xA1)) * 32
 *********************************************************************************/

/******************************************************************************/
/* 头文件包含 */
#include "CH59x_common.h"
#include "GUI_Paint.h"
#include "font_gb2312_16.h"
#include "uni_to_gb2312.h"
#include "FONT_CN.h"

#define FONT_CN_ASCII_Y_OFFSET  2

/*******************************************************************************
 * @fn      FONT_CN_Init
 *
 * @brief   初始化中文字库（预留，当前无初始化需求）
 *
 * @param   None.
 *
 * @return  None.
 *******************************************************************************/
void FONT_CN_Init(void)
{
    /* 字模为 const 编译进固件，无需运行时初始化 */
}

/*******************************************************************************
 * @fn      FONT_CN_GetGlyph
 *
 * @brief   获取一个 GB2312 汉字的 16x16 字模
 *
 * @param   qu  - 区字节（0xB0~0xD7）
 * @param   wei - 位字节（0xA1~0xFE）
 * @param   out - 输出缓冲区（≥32 字节）
 *
 * @return  1=找到字模，0=字不在字库范围
 *******************************************************************************/
uint8_t FONT_CN_GetGlyph(uint8_t qu, uint8_t wei, uint8_t *out)
{
    uint32_t idx;

    if(qu < GB2312_Q_MIN || qu > GB2312_Q_MAX) return 0;
    if(wei < GB2312_W_MIN || wei > GB2312_W_MAX) return 0;

    idx = (uint32_t)(qu - GB2312_Q_MIN) * 94 + (wei - GB2312_W_MIN);

    /* 压缩存储：仅前 COMMON_COUNT 常用字有真实字模，其余返回空白 */
    if(idx >= GB2312_COMMON_COUNT)
    {
        tmos_memset(out, 0xFF, GB2312_CHAR_BYTES);   /* 空白占位 */
        return 0;
    }

    tmos_memcpy(out, &g_gb2312_16_font[idx * GB2312_CHAR_BYTES], GB2312_CHAR_BYTES);
    return 1;
}

/*******************************************************************************
 * @fn      FONT_CN_DrawString
 *
 * @brief   绘制中英混排字符串（UTF-8 输入，内部转 GB2312 查字模）
 *
 * @param   x,y       - 起始坐标
 * @param   str       - UTF-8 字符串
 * @param   fg        - 前景色（文字色）
 * @param   max_chars - 最多绘制字符数（0=不限）
 *
 * @return  绘制后的 X 位置（用于多行排版）
 *******************************************************************************/
uint16_t FONT_CN_DrawString(uint16_t x, uint16_t y, const char *str,
                            UWORD fg, uint16_t max_chars)
{
    uint16_t cur_x = x;
    uint16_t cnt = 0;
    uint8_t glyph[GB2312_CHAR_BYTES];

    if(!str) return x;

    while(*str)
    {
        if(max_chars && cnt >= max_chars) break;

        if((uint8_t)*str < 0x80)
        {
            /* ASCII 字符：改用 Font16，并略微下移以和汉字底部对齐 */
            Paint_DrawChar(cur_x, (uint16_t)(y + FONT_CN_ASCII_Y_OFFSET),
                           *str, &Font16, WHITE, fg);
            cur_x += Font16.Width;
            str++;
        }
        else if((uint8_t)*str >= 0xE0)
        {
            /* UTF-8 3 字节（汉字）→ GB2312 */
            uint8_t b0 = *str; str++;
            if(!*str) break;
            uint8_t b1 = *str; str++;
            if(!*str) break;
            uint8_t b2 = *str; str++;

            /* UTF-8 → Unicode */
            uint16_t uni = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);

            /* Unicode → GB2312（查表） */
            uint8_t qu, wei;
            if(FONT_CN_UniToGB2312(uni, &qu, &wei))
            {
                if(FONT_CN_GetGlyph(qu, wei, glyph))
                {
                    FONT_CN_DrawGlyph(cur_x, y, glyph, fg);
                }
            }
            cur_x += GB2312_FONT_W;
            cnt++;
        }
        else
        {
            /* 其他编码跳过 */
            str++;
        }
    }
    return cur_x;
}

/*******************************************************************************
 * @fn      FONT_CN_DrawGlyph
 *
 * @brief   绘制单个 16x16 汉字字模
 *
 * @param   x,y   - 左上角坐标
 * @param   glyph - 32 字节字模（横向 MSB 优先）
 * @param   fg    - 前景色
 *
 * @return  None.
 *******************************************************************************/
void FONT_CN_DrawGlyph(uint16_t x, uint16_t y, const uint8_t *glyph, UWORD fg)
{
    uint16_t i, j;
    const uint8_t *p = glyph;

    for(j = 0; j < GB2312_FONT_H; j++)
    {
        for(i = 0; i < GB2312_FONT_W; i++)
        {
            if(*p & (0x80 >> (i % 8)))
            {
                Paint_SetPixel(x + i, y + j, fg);
            }
            if(i % 8 == 7) p++;
        }
    }
}

/*******************************************************************************
 * @fn      FONT_CN_UniToGB2312
 *
 * @brief   Unicode 码点转 GB2312 区/位码（二分查找）
 *
 * @param   uni - Unicode 码点（U+4E00~U+9FFF 汉字区）
 * @param   qu  - 输出区字节
 * @param   wei - 输出位字节
 *
 * @return  1=转换成功，0=不支持
 *******************************************************************************/
uint8_t FONT_CN_UniToGB2312(uint16_t uni, uint8_t *qu, uint8_t *wei)
{
    uint16_t lo = 0, hi = UNI_TO_GB2312_COUNT - 1;
    uint16_t mid;

    if(!qu || !wei) return 0;

    /* 二分查找 */
    while(lo <= hi)
    {
        mid = (lo + hi) / 2;
        if(g_uni_to_gb2312[mid] == uni)
        {
            *qu = g_uni_gb2312_qu[mid];
            *wei = g_uni_gb2312_wei[mid];
            return 1;
        }
        else if(g_uni_to_gb2312[mid] < uni)
        {
            lo = mid + 1;
        }
        else
        {
            hi = (mid > 0) ? (mid - 1) : 0;
        }
    }
    return 0;
}

/******************************** endfile @ FONT_CN ****************************/
