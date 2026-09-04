/********************************** (C) COPYRIGHT *******************************
 * File Name          : FONT_CN.h
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2026/08/21
 * Description        : 中文字库渲染模块接口（GB2312 一级汉字 16x16 加粗）
 *********************************************************************************/

#ifndef __FONT_CN_H
#define __FONT_CN_H

#ifdef __cplusplus
extern "C" {
#endif

/* 中文字模规格（GB2312 一级 16x16 加粗） */
#define FONT_CN_W   16   /* 中文字宽 */
#define FONT_CN_H   16   /* 中文字高 */
#define FONT_CN_BYTES 32 /* 每字字节数 */

/**
 * @brief   初始化中文字库
 */
void FONT_CN_Init(void);

/**
 * @brief   获取一个 GB2312 汉字的 16x16 字模
 *
 * @param   qu  - 区字节（0xB0~0xD7）
 * @param   wei - 位字节（0xA1~0xFE）
 * @param   out - 输出缓冲区（≥32 字节）
 *
 * @return  1=找到，0=不在字库
 */
uint8_t FONT_CN_GetGlyph(uint8_t qu, uint8_t wei, uint8_t *out);

/**
 * @brief   绘制单个 16x16 汉字字模
 *
 * @param   x,y   - 左上角坐标
 * @param   glyph - 32 字节字模
 * @param   fg    - 前景色
 */
void FONT_CN_DrawGlyph(uint16_t x, uint16_t y, const uint8_t *glyph, UWORD fg);

/**
 * @brief   绘制中英混排字符串（UTF-8 输入，ASCII 使用 Font16）
 *
 * @param   x,y       - 起始坐标
 * @param   str       - UTF-8 字符串
 * @param   fg        - 前景色
 * @param   max_chars - 最多绘制汉字数（0=不限）
 *
 * @return  绘制后的 X 位置
 */
uint16_t FONT_CN_DrawString(uint16_t x, uint16_t y, const char *str,
                            UWORD fg, uint16_t max_chars);

/**
 * @brief   Unicode 码点转 GB2312 区/位码
 *
 * @param   uni - Unicode 码点（U+4E00~U+9FFF 汉字区）
 * @param   qu  - 输出区字节
 * @param   wei - 输出位字节
 *
 * @return  1=转换成功，0=不支持
 */
uint8_t FONT_CN_UniToGB2312(uint16_t uni, uint8_t *qu, uint8_t *wei);

#ifdef __cplusplus
}
#endif

#endif /* __FONT_CN_H */
