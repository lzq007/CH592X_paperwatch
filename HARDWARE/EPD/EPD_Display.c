/********************************** (C) COPYRIGHT *******************************
 * File Name          : EPD_Display.c
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2026/08/20
 * Description        : EPD display helpers for notifications and clock UI
 *********************************************************************************/

#include "CH59x_common.h"
#include "Display_EPD_W21_spi.h"
#include "Display_EPD_W21.h"
#include "GUI_Paint.h"
#include "fonts.h"
#include "EPD_Display.h"
#include "HAL.h"
#include "FONT_CN.h"
#include "font_gb2312_16_thin.h"
#include <stdio.h>
#include <string.h>

extern volatile uint8_t ancs_ble_connected;

static uint8_t EPD_Image[EPD_ARRAY];
static uint8_t EPD_Image_prev[EPD_ARRAY];

#define EPD_FULL_REFRESH_CNT   5
#define EPD_CLOCK_REFRESH_CNT  5
#define EPD_NOTIFICATION_STRONG_REFRESH  0

static uint8_t epd_refresh_count = 0;
static volatile uint8_t epd_need_full = 0;
static volatile uint8_t epd_overlay_lock = 0;
static volatile uint8_t epd_force_full_refresh_once = 0;

static sFONT *EPD_font_app   = &Font16;
static sFONT *EPD_font_title = &Font20;
static sFONT *EPD_font_body  = &Font16;

#define EPD_UI_MARGIN      4
#define EPD_UI_APP_H       24
#define EPD_UI_TITLE_Y     30
#define EPD_UI_BODY_Y      60
#define EPD_UI_BODY_MAX_H  (EPD_HEIGHT - EPD_UI_BODY_Y - 4)
#define EPD_UI_TEXT_W      (EPD_HEIGHT - EPD_UI_MARGIN * 2)
#define EPD_MSG_ICON_X      (EPD_UI_TEXT_W - 22)
#define EPD_MSG_ICON_W      20
#define EPD_MSG_ICON_H      17
#define EPD_MSG_ICON_WIN_X  (((EPD_MSG_ICON_X) / 8) * 8)
#define EPD_MSG_ICON_WIN_W  24
#define EPD_MSG_ICON_X_BYTE ((EPD_MSG_ICON_WIN_X) / 8)
#define EPD_MSG_ICON_X_LEN  ((EPD_MSG_ICON_WIN_W) / 8)

// 蓝牙连接图标：位于消息图标左侧（右上角），蓝牙连接时显示。
// 图标取模自 蓝牙.png（20x24）。
#define EPD_BT_ICON_W       20
#define EPD_BT_ICON_H       24
#define EPD_BT_ICON_X       (EPD_MSG_ICON_X - EPD_BT_ICON_W - 3)
#define EPD_BT_ICON_Y       4
#define EPD_BT_ICON_WIN_X   (((EPD_BT_ICON_X) / 8) * 8)
#define EPD_BT_ICON_WIN_W   24
#define EPD_BT_ICON_X_BYTE  ((EPD_BT_ICON_WIN_X) / 8)
#define EPD_BT_ICON_X_LEN   ((EPD_BT_ICON_WIN_W) / 8)

// 消息图标与蓝牙图标垂直居中（两图标共享中心线）
#define EPD_MSG_ICON_Y      (EPD_BT_ICON_Y + ((EPD_BT_ICON_H - EPD_MSG_ICON_H) / 2))

#define EPD_CLOCK_FONT      (&Font16)
#define EPD_CLOCK_SCALE     3
#define EPD_CLOCK_DATE_SCALE 2
#define EPD_CLOCK_DATE_GAP  3
#define EPD_CLOCK_TIME_GAP  6
#define EPD_CLOCK_TOP_BIAS  4

#define EPD_STATUS_Y        128
#define EPD_STATUS_H        (EPD_HEIGHT - EPD_STATUS_Y)
#define EPD_STATUS_LABEL_Y  132
#define EPD_STATUS_VALUE_Y  150
#define EPD_STATUS_SINGLE_Y  142

static void epd_fill_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                          UWORD color)
{
    uint16_t x;
    uint16_t y;

    for(y = y0; y <= y1; y++)
    {
        for(x = x0; x <= x1; x++)
        {
            Paint_SetPixel(x, y, color);
        }
    }
}

static void epd_draw_text_clip(uint16_t x, uint16_t y, const char *str,
                               sFONT *font, uint16_t max_w, UWORD fg)
{
    char tmp[EPD_MSG_LINE_LEN];
    uint16_t width = 0;
    uint16_t out = 0;
    const uint8_t *p = (const uint8_t *)str;

    if(!str)
    {
        return;
    }

    while(*p && out < (EPD_MSG_LINE_LEN - 4))
    {
        if(*p < 0x80)
        {
            width += font->Width;
            if(width > max_w)
            {
                break;
            }
            tmp[out++] = (char)(*p++);
        }
        else if(*p >= 0xE0)
        {
            width += FONT_CN_W;
            if(width > max_w)
            {
                break;
            }
            tmp[out++] = (char)(*p++);
            if(*p) tmp[out++] = (char)(*p++);
            if(*p) tmp[out++] = (char)(*p++);
        }
        else
        {
            p++;
        }
    }

    tmp[out] = '\0';

    if(*p && out >= 3)
    {
        out -= 3;
        tmp[out++] = '.';
        tmp[out++] = '.';
        tmp[out++] = '.';
        tmp[out] = '\0';
    }

    FONT_CN_DrawString(x, y, tmp, fg, 0);
}

/* 蓝牙图标取模自 蓝牙.png，20x24，黑=1，每行 3 字节（MSB 在前） */
static const uint8_t epd_bt_icon_bitmap[EPD_BT_ICON_H][3] = {
    {0x00, 0x80, 0x00},
    {0x00, 0xE0, 0x00},
    {0x00, 0xF0, 0x00},
    {0x00, 0xFC, 0x00},
    {0x00, 0xFE, 0x00},
    {0x40, 0xE7, 0x80},
    {0xE0, 0xE3, 0xE0},
    {0xF8, 0xE1, 0xF0},
    {0x3E, 0xE7, 0xC0},
    {0x0F, 0xEF, 0x00},
    {0x07, 0xFE, 0x00},
    {0x01, 0xF8, 0x00},
    {0x01, 0xF8, 0x00},
    {0x07, 0xFE, 0x00},
    {0x0F, 0xEF, 0x00},
    {0x3E, 0xE7, 0xC0},
    {0xF8, 0xE1, 0xF0},
    {0xE0, 0xE3, 0xE0},
    {0x40, 0xEF, 0x80},
    {0x00, 0xFE, 0x00},
    {0x00, 0xFC, 0x00},
    {0x00, 0xF0, 0x00},
    {0x00, 0xE0, 0x00},
    {0x00, 0x80, 0x00},
};

/* 在 (x,y) 绘制 20x24 蓝牙位图图标（黑=1 描点） */
static void epd_draw_bluetooth_icon(uint16_t x, uint16_t y)
{
    uint8_t row;
    uint8_t col;

    for(row = 0; row < EPD_BT_ICON_H; row++)
    {
        for(col = 0; col < EPD_BT_ICON_W; col++)
        {
            if(epd_bt_icon_bitmap[row][col / 8] & (0x80 >> (col % 8)))
            {
                Paint_SetPixel((uint16_t)(x + col), (uint16_t)(y + row), BLACK);
            }
        }
    }
}

/* 消息图标取模自 消息.png，20x17，黑=1，每行 3 字节（MSB 在前） */
static const uint8_t epd_msg_icon_bitmap[EPD_MSG_ICON_H][3] = {
    {0xFF, 0xFF, 0xF0},
    {0xFF, 0xFF, 0xF0},
    {0xC0, 0x00, 0x30},
    {0xC0, 0x00, 0x30},
    {0xC0, 0x00, 0x30},
    {0xC0, 0x00, 0x30},
    {0xCE, 0x67, 0x30},
    {0xC4, 0x62, 0x30},
    {0xC0, 0x00, 0x30},
    {0xC0, 0x00, 0x30},
    {0xC0, 0x00, 0x30},
    {0xC0, 0x00, 0x30},
    {0xFF, 0x0F, 0xF0},
    {0x7F, 0x9F, 0xE0},
    {0x01, 0xF8, 0x00},
    {0x00, 0xF0, 0x00},
    {0x00, 0x60, 0x00},
};

/* 在 (x,y) 绘制 20x17 消息位图图标（黑=1 描点） */
static void epd_draw_message_icon(uint16_t x, uint16_t y)
{
    uint8_t row;
    uint8_t col;

    for(row = 0; row < EPD_MSG_ICON_H; row++)
    {
        for(col = 0; col < EPD_MSG_ICON_W; col++)
        {
            if(epd_msg_icon_bitmap[row][col / 8] & (0x80 >> (col % 8)))
            {
                Paint_SetPixel((uint16_t)(x + col), (uint16_t)(y + row), BLACK);
            }
        }
    }
}

static uint16_t epd_measure_utf8_width(const char *text)
{
    uint16_t width = 0;
    const uint8_t *p = (const uint8_t *)text;

    if(!text)
    {
        return 0;
    }

    while(*p)
    {
        if(*p < 0x80)
        {
            width += Font16.Width;
            p++;
        }
        else if(*p >= 0xE0)
        {
            width += FONT_CN_W;
            p += 3;
        }
        else
        {
            p++;
        }
    }

    return width;
}

static void epd_draw_centered_utf8(uint16_t y, const char *text, UWORD fg)
{
    uint16_t text_w;
    uint16_t x;

    if(!text || !*text)
    {
        return;
    }

    text_w = epd_measure_utf8_width(text);
    if(text_w >= Paint.Width)
    {
        x = 0;
    }
    else
    {
        x = (uint16_t)((Paint.Width - text_w) / 2);
    }

    FONT_CN_DrawString(x, y, text, fg, 0);
}

static void epd_refresh_status_window(void)
{
    EPD_HW_Init_GUI();
    EPD_Display_FastSync(EPD_Image, EPD_Image_prev);
    tmos_memcpy(EPD_Image_prev, EPD_Image, sizeof(EPD_Image));
}

static uint8_t epd_can_full_refresh(void)
{
    /* 蓝牙连接期间避免全刷，减少对连接和实时提醒的影响。 */
    return (ancs_ble_connected == 0);
}

static void epd_draw_scaled_char(uint16_t x, uint16_t y, char ch,
                                 const sFONT *font, uint8_t scale, UWORD fg)
{
    uint16_t page;
    uint16_t column;
    uint16_t bytes_per_row;
    uint32_t offset;
    const uint8_t *ptr;

    if(ch < ' ' || ch > '~' || !font || scale == 0)
    {
        return;
    }

    bytes_per_row = (uint16_t)(font->Width / 8 + (font->Width % 8 ? 1 : 0));
    offset = (uint32_t)(ch - ' ') * font->Height * bytes_per_row;
    ptr = &font->table[offset];

    for(page = 0; page < font->Height; page++)
    {
        for(column = 0; column < font->Width; column++)
        {
            if(*ptr & (0x80 >> (column % 8)))
            {
                uint8_t sx;
                uint8_t sy;

                for(sy = 0; sy < scale; sy++)
                {
                    for(sx = 0; sx < scale; sx++)
                    {
                        Paint_SetPixel((uint16_t)(x + column * scale + sx),
                                       (uint16_t)(y + page * scale + sy),
                                       fg);
                    }
                }
            }

            if(column % 8 == 7)
            {
                ptr++;
            }
        }

        if(font->Width % 8 != 0)
        {
            ptr++;
        }
    }
}

static uint8_t epd_cn_glyph_pixel(const uint8_t *glyph, int row, int col)
{
    int byte_index;
    int bit_index;

    if(!glyph || row < 0 || row >= FONT_CN_H || col < 0 || col >= FONT_CN_W)
    {
        return 0;
    }

    byte_index = row * (FONT_CN_W / 8) + (col / 8);
    bit_index = 7 - (col % 8);
    return (uint8_t)((glyph[byte_index] >> bit_index) & 0x01);
}

static void epd_draw_scaled_cn_glyph(uint16_t x, uint16_t y,
                                     const uint8_t *glyph, uint8_t scale,
                                     UWORD fg)
{
    uint16_t row;
    uint16_t column;

    if(!glyph || scale == 0)
    {
        return;
    }

    for(row = 0; row < FONT_CN_H; row++)
    {
        for(column = 0; column < FONT_CN_W; column++)
        {
            if(epd_cn_glyph_pixel(glyph, (int)row, (int)column))
            {
                uint8_t sx;
                uint8_t sy;

                for(sy = 0; sy < scale; sy++)
                {
                    for(sx = 0; sx < scale; sx++)
                    {
                        Paint_SetPixel((uint16_t)(x + column * scale + sx),
                                       (uint16_t)(y + row * scale + sy),
                                       fg);
                    }
                }
            }
        }
    }
}

static uint8_t epd_date_cn_get_glyph(uint8_t qu, uint8_t wei, uint8_t *out)
{
    uint32_t idx;

    if(!out)
    {
        return 0;
    }

    if(qu < GB2312_Q_MIN || qu > GB2312_Q_MAX ||
       wei < GB2312_W_MIN || wei > GB2312_W_MAX)
    {
        tmos_memset(out, 0xFF, GB2312_16_THIN_CHAR_BYTES);
        return 0;
    }

    idx = (uint32_t)(qu - GB2312_Q_MIN) * 94 + (wei - GB2312_W_MIN);
    if(idx >= GB2312_16_THIN_FONT_COUNT)
    {
        tmos_memset(out, 0xFF, GB2312_16_THIN_CHAR_BYTES);
        return 0;
    }

    tmos_memcpy(out, &g_gb2312_16_thin_font[idx * GB2312_16_THIN_CHAR_BYTES],
                GB2312_16_THIN_CHAR_BYTES);
    return 1;
}

static uint8_t epd_ascii_glyph_bounds(char ch, const sFONT *font,
                                      uint16_t *top, uint16_t *bottom)
{
    uint16_t row;
    uint16_t column;
    uint16_t bytes_per_row;
    uint32_t offset;
    const uint8_t *ptr;

    if(!font || ch < ' ' || ch > '~' || !top || !bottom)
    {
        return 0;
    }

    bytes_per_row = (uint16_t)(font->Width / 8 + (font->Width % 8 ? 1 : 0));
    offset = (uint32_t)(ch - ' ') * font->Height * bytes_per_row;
    ptr = &font->table[offset];

    *top = font->Height;
    *bottom = 0;

    for(row = 0; row < font->Height; row++)
    {
        uint8_t hit = 0;
        const uint8_t *row_ptr = ptr;

        for(column = 0; column < font->Width; column++)
        {
            if(*row_ptr & (0x80 >> (column % 8)))
            {
                hit = 1;
                break;
            }

            if(column % 8 == 7)
            {
                row_ptr++;
            }
        }

        if(hit)
        {
            if(*top == font->Height)
            {
                *top = row;
            }
            *bottom = row;
        }

        ptr += bytes_per_row;
    }

    return (*top <= *bottom);
}

static uint8_t epd_cn_glyph_bounds(const uint8_t *glyph,
                                   uint16_t *top, uint16_t *bottom)
{
    uint16_t row;
    uint16_t col;

    if(!glyph || !top || !bottom)
    {
        return 0;
    }

    *top = FONT_CN_H;
    *bottom = 0;

    for(row = 0; row < FONT_CN_H; row++)
    {
        uint8_t hit = 0;

        for(col = 0; col < FONT_CN_W; col++)
        {
            if(epd_cn_glyph_pixel(glyph, (int)row, (int)col))
            {
                hit = 1;
                break;
            }
        }

        if(hit)
        {
            if(*top == FONT_CN_H)
            {
                *top = row;
            }
            *bottom = row;
        }
    }

    return (*top <= *bottom);
}

static uint16_t epd_measure_scaled_utf8_width(const char *text, uint8_t scale)
{
    uint16_t width = 0;
    const uint8_t *p = (const uint8_t *)text;

    if(!text || scale == 0)
    {
        return 0;
    }

    while(*p)
    {
        if(*p < 0x80)
        {
            width = (uint16_t)(width + Font16.Width * scale);
            p++;
        }
        else if(*p >= 0xE0)
        {
            width = (uint16_t)(width + GB2312_16_THIN_FONT_W * scale);
            if(p[1] == 0 || p[2] == 0)
            {
                break;
            }
            p += 3;
        }
        else
        {
            p++;
        }
    }

    return width;
}

static void epd_draw_centered_scaled_utf8(uint16_t y, const char *text,
                                          uint8_t scale,
                                          UWORD fg)
{
    uint16_t text_w;
    uint16_t x;
    const uint8_t *p;
    uint16_t line_h;

    if(!text || !*text || scale == 0)
    {
        return;
    }

    line_h = (uint16_t)(GB2312_16_THIN_FONT_H * scale);
    text_w = epd_measure_scaled_utf8_width(text, scale);
    if(text_w >= Paint.Width)
    {
        x = 0;
    }
    else
    {
        x = (uint16_t)((Paint.Width - text_w) / 2);
    }

    p = (const uint8_t *)text;
    while(*p)
    {
        if(*p < 0x80)
        {
            uint16_t top;
            uint16_t bottom;
            uint16_t glyph_h;
            uint16_t y_draw;

            y_draw = y;
            if(epd_ascii_glyph_bounds((char)*p, &Font16, &top, &bottom))
            {
                glyph_h = (uint16_t)((bottom - top + 1) * scale);
                if(line_h > glyph_h)
                {
                    y_draw = (uint16_t)(y + (line_h - glyph_h) / 2);
                    if(top > 0)
                    {
                        y_draw = (uint16_t)(y_draw - top * scale);
                    }
                }
            }

            epd_draw_scaled_char(x, y_draw, (char)*p, &Font16, scale, fg);
            x = (uint16_t)(x + Font16.Width * scale);
            p++;
        }
        else if(*p >= 0xE0)
        {
            uint8_t b0;
            uint8_t b1;
            uint8_t b2;
            uint16_t uni;
            uint8_t qu;
            uint8_t wei;
            uint8_t glyph[FONT_CN_BYTES];
            uint16_t top;
            uint16_t bottom;
            uint16_t glyph_h;
            uint16_t y_draw;

            if(p[1] == 0 || p[2] == 0)
            {
                break;
            }

            b0 = *p++;
            b1 = *p++;
            b2 = *p++;
            uni = ((uint16_t)(b0 & 0x0F) << 12) |
                  ((uint16_t)(b1 & 0x3F) << 6) |
                  (uint16_t)(b2 & 0x3F);

            if(FONT_CN_UniToGB2312(uni, &qu, &wei) &&
               epd_date_cn_get_glyph(qu, wei, glyph))
            {
                y_draw = y;
                if(epd_cn_glyph_bounds(glyph, &top, &bottom))
                {
                    glyph_h = (uint16_t)((bottom - top + 1) * scale);
                    if(line_h > glyph_h)
                    {
                        y_draw = (uint16_t)(y + (line_h - glyph_h) / 2);
                        if(top > 0)
                        {
                            y_draw = (uint16_t)(y_draw - top * scale);
                        }
                    }
                }
                epd_draw_scaled_cn_glyph(x, y_draw, glyph, scale, fg);
            }
            x = (uint16_t)(x + GB2312_16_THIN_FONT_W * scale);
        }
        else
        {
            p++;
        }
    }
}

static void epd_draw_scaled_string_centered(uint16_t y, const char *text,
                                            const sFONT *font, uint8_t scale,
                                            UWORD fg)
{
    size_t len;
    uint16_t text_w;
    uint16_t x;
    size_t i;

    if(!text || !*text || !font || scale == 0)
    {
        return;
    }

    len = strlen(text);
    text_w = (uint16_t)(len * font->Width * scale);
    if(text_w >= Paint.Width)
    {
        x = 0;
    }
    else
    {
        x = (uint16_t)((Paint.Width - text_w) / 2);
    }

    for(i = 0; i < len; i++)
    {
        epd_draw_scaled_char((uint16_t)(x + i * font->Width * scale), y,
                             text[i], font, scale, fg);
    }
}

void EPD_UpdateClockMessageIcon(uint8_t show)
{
    Paint_SelectImage(EPD_Image);
    Paint_ClearWindows(EPD_MSG_ICON_WIN_X, EPD_MSG_ICON_Y,
                       (uint16_t)(EPD_MSG_ICON_WIN_X + EPD_MSG_ICON_WIN_W - 1),
                       (uint16_t)(EPD_MSG_ICON_Y + EPD_MSG_ICON_H - 1),
                       WHITE);

    if(show)
    {
        epd_draw_message_icon(EPD_MSG_ICON_X, EPD_MSG_ICON_Y);
    }

    EPD_HW_Init_GUI();
    EPD_Display_FastSync(EPD_Image, EPD_Image_prev);
    tmos_memcpy(EPD_Image_prev, EPD_Image, sizeof(EPD_Image));
}

void EPD_UpdateClockBluetoothIcon(uint8_t show)
{
    Paint_SelectImage(EPD_Image);
    Paint_ClearWindows(EPD_BT_ICON_WIN_X, EPD_BT_ICON_Y,
                       (uint16_t)(EPD_BT_ICON_WIN_X + EPD_BT_ICON_WIN_W - 1),
                       (uint16_t)(EPD_BT_ICON_Y + EPD_BT_ICON_H - 1),
                       WHITE);

    if(show)
    {
        epd_draw_bluetooth_icon(EPD_BT_ICON_X, EPD_BT_ICON_Y);
    }

    EPD_HW_Init_GUI();
    EPD_Display_FastSync(EPD_Image, EPD_Image_prev);
    tmos_memcpy(EPD_Image_prev, EPD_Image, sizeof(EPD_Image));
}

static void epd_refresh_frame(uint8_t force_full, uint8_t notify_buzzer)
{
    if(epd_can_full_refresh() &&
       (force_full || EPD_NOTIFICATION_STRONG_REFRESH))
    {
        EPD_HW_Init_GUI();
        EPD_Display_Full_Sync(EPD_Image);
        epd_need_full = 0;
        epd_refresh_count = 0;
    }
    else
    {
        EPD_HW_Init_GUI();
        EPD_Display_FastSync(EPD_Image, EPD_Image_prev);
        if(epd_refresh_count < 0xFF)
        {
            epd_refresh_count++;
        }
    }

    tmos_memcpy(EPD_Image_prev, EPD_Image, sizeof(EPD_Image));

    if(notify_buzzer)
    {
        BUZZER_NotifyBeep();
    }
}

static void epd_refresh_clock_frame(uint8_t force_full)
{
    if(force_full)
    {
        epd_need_full = 0;
        epd_refresh_count = 0;
    }
    else
    {
        if(epd_refresh_count < 0xFF)
        {
            epd_refresh_count++;
        }
    }

    tmos_memcpy(EPD_Image_prev, EPD_Image, sizeof(EPD_Image));
}

static void epd_prepare_clock_base(uint8_t has_message)
{
    Paint_NewImage(EPD_Image, EPD_WIDTH, EPD_HEIGHT, 270, WHITE);
    Paint_SelectImage(EPD_Image);
    Paint_Clear(WHITE);

    /* 蓝牙连接时在右上角显示蓝牙图标（位于消息图标左侧） */
    if(ancs_ble_connected)
    {
        epd_draw_bluetooth_icon(EPD_BT_ICON_X, EPD_BT_ICON_Y);
    }

    if(has_message)
    {
        epd_draw_message_icon(EPD_MSG_ICON_X, EPD_MSG_ICON_Y);
    }
}

static void epd_show_clock_demo(const RX8010SJ_Time *time, uint8_t has_message,
                                uint8_t force_full)
{
    char hhmm[6];
    char date_utf8[32];
    char week_utf8[24];
    uint16_t block_h;
    uint16_t top_y;
    uint16_t date_y;
    uint16_t week_y;
    uint16_t clock_y;
    static const char *weekday_utf8[] = {
        "日", "一", "二", "三", "四", "五", "六"
    };

    hhmm[0] = (char)('0' + (time->hour / 10));
    hhmm[1] = (char)('0' + (time->hour % 10));
    hhmm[2] = ':';
    hhmm[3] = (char)('0' + (time->minute / 10));
    hhmm[4] = (char)('0' + (time->minute % 10));
    hhmm[5] = '\0';

    snprintf(date_utf8, sizeof(date_utf8), "%d月%02d日",
             time->month, time->day);
    snprintf(week_utf8, sizeof(week_utf8), "星期%s",
             weekday_utf8[(time->weekday <= 6) ? time->weekday : 0]);

    EPD_HW_Init_GUI();
    epd_prepare_clock_base(has_message);
    block_h = (uint16_t)(GB2312_16_THIN_FONT_H * EPD_CLOCK_DATE_SCALE * 2 +
                         EPD_CLOCK_DATE_GAP +
                         EPD_CLOCK_TIME_GAP +
                         EPD_CLOCK_FONT->Height * EPD_CLOCK_SCALE);
    top_y = (block_h >= Paint.Height) ? 0 : (uint16_t)((Paint.Height - block_h) / 2);
    if((uint16_t)(top_y + EPD_CLOCK_TOP_BIAS + block_h) < Paint.Height)
    {
        top_y = (uint16_t)(top_y + EPD_CLOCK_TOP_BIAS);
    }
    date_y = top_y;
    week_y = (uint16_t)(date_y + GB2312_16_THIN_FONT_H * EPD_CLOCK_DATE_SCALE + EPD_CLOCK_DATE_GAP);
    clock_y = (uint16_t)(week_y + GB2312_16_THIN_FONT_H * EPD_CLOCK_DATE_SCALE + EPD_CLOCK_TIME_GAP);

    epd_draw_centered_scaled_utf8(date_y, date_utf8, EPD_CLOCK_DATE_SCALE, BLACK);
    epd_draw_centered_scaled_utf8(week_y, week_utf8, EPD_CLOCK_DATE_SCALE, BLACK);
    epd_draw_scaled_string_centered(clock_y, hhmm, EPD_CLOCK_FONT,
                                    EPD_CLOCK_SCALE, BLACK);

    if((epd_can_full_refresh() || epd_force_full_refresh_once) &&
       (force_full || epd_need_full || epd_refresh_count >= EPD_CLOCK_REFRESH_CNT))
    {
        EPD_Display_Full_Sync(EPD_Image);
        epd_refresh_clock_frame(1);
    }
    else
    {
        EPD_Display_FastSync(EPD_Image, EPD_Image_prev);
        epd_refresh_clock_frame(0);
    }

    tmos_memcpy(EPD_Image_prev, EPD_Image, sizeof(EPD_Image));
    epd_force_full_refresh_once = 0;
}

static void epd_show_sync_status_page(const char *text)
{
    uint16_t y;

    BUZZER_Stop();
    epd_overlay_lock = 1;

    Paint_NewImage(EPD_Image, EPD_WIDTH, EPD_HEIGHT, 270, WHITE);
    Paint_SelectImage(EPD_Image);
    Paint_Clear(WHITE);

    y = (uint16_t)((Paint.Height > FONT_CN_H) ? ((Paint.Height - FONT_CN_H) / 2) : 0);
    epd_draw_centered_utf8(y, text, BLACK);

    epd_refresh_frame(1, 0);
}

void EPD_Show_Init(void)
{
    EPD_GPIO_Init();
    EPD_HW_Init_GUI();
    tmos_memset(EPD_Image, 0xFF, sizeof(EPD_Image));
    tmos_memset(EPD_Image_prev, 0xFF, sizeof(EPD_Image_prev));
    EPD_SetRAMValue_BaseMap(EPD_Image);
    epd_need_full = 0;
    epd_refresh_count = 0;
    epd_overlay_lock = 0;
    epd_force_full_refresh_once = 0;
}

void EPD_SetOverlayLock(uint8_t locked)
{
    epd_overlay_lock = locked ? 1 : 0;
}

uint8_t EPD_IsOverlayLocked(void)
{
    return epd_overlay_lock;
}

static void epd_show_notification_page(const char *app_name, const char *title,
                                       const char *message, uint8_t notify_buzzer)
{
    uint16_t y;

    Paint_NewImage(EPD_Image, EPD_WIDTH, EPD_HEIGHT, 270, WHITE);
    Paint_SelectImage(EPD_Image);
    Paint_Clear(WHITE);

    if(app_name && *app_name)
    {
        epd_fill_rect(0, 0, EPD_UI_TEXT_W, EPD_UI_APP_H - 1, BLACK);
        epd_draw_text_clip(EPD_UI_MARGIN, (uint16_t)((EPD_UI_APP_H - EPD_font_app->Height) / 2),
                           app_name, EPD_font_app,
                           (uint16_t)(EPD_UI_TEXT_W - EPD_UI_MARGIN * 2), WHITE);
    }

    if(title && *title)
    {
        epd_draw_text_clip(EPD_UI_MARGIN, EPD_UI_TITLE_Y, title,
                           EPD_font_title,
                           (uint16_t)(EPD_UI_TEXT_W - EPD_UI_MARGIN * 2), BLACK);
    }

    Paint_DrawLine(EPD_UI_MARGIN, EPD_UI_APP_H - 1,
                   EPD_UI_TEXT_W - EPD_UI_MARGIN, EPD_UI_APP_H - 1,
                   BLACK, LINE_STYLE_SOLID, DOT_PIXEL_1X1);
    Paint_DrawLine(EPD_UI_MARGIN, EPD_UI_BODY_Y - 6,
                   EPD_UI_TEXT_W - EPD_UI_MARGIN, EPD_UI_BODY_Y - 6,
                   BLACK, LINE_STYLE_SOLID, DOT_PIXEL_1X1);

    y = EPD_UI_BODY_Y;
    if(message && *message)
    {
        const uint8_t *p = (const uint8_t *)message;
        char line[EPD_MSG_LINE_LEN];
        uint16_t line_width = 0;
        uint16_t out = 0;

        while(*p && y < (EPD_UI_BODY_Y + EPD_UI_BODY_MAX_H - FONT_CN_H))
        {
            if(*p < 0x80)
            {
                if(line_width + EPD_font_body->Width > EPD_UI_TEXT_W)
                {
                    line[out] = '\0';
                    FONT_CN_DrawString(EPD_UI_MARGIN, y, line, BLACK, 0);
                    y += FONT_CN_H + 2;
                    out = 0;
                    line_width = 0;
                }
                line[out++] = (char)(*p++);
                line_width += EPD_font_body->Width;
            }
            else if(*p >= 0xE0)
            {
                if(line_width + FONT_CN_W > EPD_UI_TEXT_W)
                {
                    line[out] = '\0';
                    FONT_CN_DrawString(EPD_UI_MARGIN, y, line, BLACK, 0);
                    y += FONT_CN_H + 2;
                    out = 0;
                    line_width = 0;
                }
                line[out++] = (char)(*p++);
                if(*p) line[out++] = (char)(*p++);
                if(*p) line[out++] = (char)(*p++);
                line_width += FONT_CN_W;
            }
            else
            {
                p++;
            }

            if(out >= (EPD_MSG_LINE_LEN - 4))
            {
                line[out] = '\0';
                FONT_CN_DrawString(EPD_UI_MARGIN, y, line, BLACK, 0);
                y += FONT_CN_H + 2;
                out = 0;
                line_width = 0;
            }
        }

        if(out > 0 && y < (EPD_UI_BODY_Y + EPD_UI_BODY_MAX_H - FONT_CN_H))
        {
            line[out] = '\0';
            FONT_CN_DrawString(EPD_UI_MARGIN, y, line, BLACK, 0);
        }
    }

    epd_refresh_frame((uint8_t)(epd_need_full || epd_refresh_count >= EPD_FULL_REFRESH_CNT),
                      notify_buzzer);
}

void EPD_Show_Notification(const char *app_name, const char *title, const char *message)
{
    BUZZER_Stop();
    epd_show_notification_page(app_name, title, message, 1);
}

void EPD_Show_NotificationSilent(const char *app_name, const char *title, const char *message)
{
    BUZZER_Stop();
    epd_show_notification_page(app_name, title, message, 0);
}

void EPD_Show_Clock(const RX8010SJ_Time *time, uint8_t has_message)
{
    if(!time)
    {
        return;
    }

    epd_show_clock_demo(time, has_message, 1);
}

void EPD_Show_ClockPartial(const RX8010SJ_Time *time, uint8_t has_message)
{
    if(!time)
    {
        return;
    }

    epd_show_clock_demo(time, has_message, 0);
}

void EPD_Show_Passcode(uint32_t passcode)
{
    char code[8];

    BUZZER_Stop();
    epd_overlay_lock = 1;

    code[0] = (char)('0' + (passcode / 100000) % 10);
    code[1] = (char)('0' + (passcode / 10000) % 10);
    code[2] = (char)('0' + (passcode / 1000) % 10);
    code[3] = (char)('0' + (passcode / 100) % 10);
    code[4] = (char)('0' + (passcode / 10) % 10);
    code[5] = (char)('0' + passcode % 10);
    code[6] = '\0';

    Paint_SelectImage(EPD_Image);
    Paint_ClearWindows(0, EPD_STATUS_Y, EPD_WIDTH, EPD_HEIGHT, WHITE);
    epd_draw_centered_utf8(EPD_STATUS_LABEL_Y, "配对码", BLACK);
    epd_draw_centered_utf8(EPD_STATUS_VALUE_Y, code, BLACK);
    epd_refresh_status_window();
}

void EPD_Show_BluetoothConnected(void)
{
    BUZZER_Stop();

    Paint_SelectImage(EPD_Image);
    Paint_ClearWindows(0, EPD_STATUS_Y, EPD_WIDTH, EPD_HEIGHT, WHITE);
    epd_draw_centered_utf8(EPD_STATUS_SINGLE_Y, "蓝牙连接成功", BLACK);
    epd_refresh_status_window();
}

void EPD_Show_TimeSyncWaiting(void)
{
    epd_show_sync_status_page("等待蓝牙广播时间同步");
}

void EPD_Show_TimeSyncSyncing(void)
{
    epd_show_sync_status_page("正在时间同步");
}

void EPD_Show_TimeSyncSuccess(void)
{
    epd_show_sync_status_page("时间同步成功");
}

void EPD_Show_TimeSyncFail(void)
{
    epd_show_sync_status_page("时间同步失败");
}

void EPD_Show_TimeSyncTimeout(void)
{
    epd_show_sync_status_page("时间同步超时");
}

void EPD_RequestFullRefreshOnce(void)
{
    epd_force_full_refresh_once = 1;
}

