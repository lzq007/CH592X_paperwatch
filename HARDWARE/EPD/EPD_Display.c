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
/* 蓝牙图标状态：
 * epd_bt_icon_target   —— 应当显示的目标状态，随 ancs_ble_connected 同步
 * epd_bt_icon_rendered —— 上一帧时钟页实际画到屏幕上的状态
 * epd_bt_icon_dirty    —— 目标状态变化，待 main 的 CLOCK_TICK 触发重绘 */
static uint8_t epd_bt_icon_target = 0;
static uint8_t epd_bt_icon_rendered = 0;
static volatile uint8_t epd_bt_icon_dirty = 0;
/* 开机选择页：当前已呈现的高亮项，以及页面是否已整页绘制 */
static uint8_t epd_boot_menu_highlight = 0;
static uint8_t epd_boot_menu_drawn = 0;
/* 设时页是否已整页绘制（之后只做数值区局刷） */
static uint8_t epd_time_set_drawn = 0;

static sFONT *EPD_font_app   = &Font16;
static sFONT *EPD_font_title = &Font20;
static sFONT *EPD_font_body  = &Font16;

/* 屏幕逻辑方向：ROTATE_0 为横屏 192x176（文字右转 90°），
 * ROTATE_270 为原来的竖屏 176x192。所有界面坐标都按逻辑尺寸推导。 */
#define EPD_UI_ROTATE      ROTATE_0
#if (EPD_UI_ROTATE == ROTATE_90) || (EPD_UI_ROTATE == ROTATE_270)
#define EPD_UI_W           EPD_HEIGHT
#define EPD_UI_H           EPD_WIDTH
#else
#define EPD_UI_W           EPD_WIDTH
#define EPD_UI_H           EPD_HEIGHT
#endif

#define EPD_UI_MARGIN      4
#define EPD_UI_APP_H       24
#define EPD_UI_TITLE_Y     30
#define EPD_UI_BODY_Y      60
#define EPD_UI_BODY_MAX_H  (EPD_UI_H - EPD_UI_BODY_Y - 4)
#define EPD_UI_TEXT_W      (EPD_UI_W - EPD_UI_MARGIN * 2)
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

/* 开机选择页两个选项按钮的行高与间距 */
#define BOOT_MENU_ROW_H      44
#define BOOT_MENU_ROW_GAP    10
#define BOOT_MENU_ROW0_Y     30
#define BOOT_MENU_ROW1_Y     (BOOT_MENU_ROW0_Y + BOOT_MENU_ROW_H + BOOT_MENU_ROW_GAP)
#define BOOT_MENU_BOX_X0     16
#define BOOT_MENU_BOX_X1     (EPD_UI_W - 17)
/* 高亮切换时的窗口局刷范围：字节列 2~21（像素 16~175），行 26~129 */
#define BOOT_MENU_WIN_X      2
#define BOOT_MENU_WIN_W      20
#define BOOT_MENU_WIN_Y      26
#define BOOT_MENU_WIN_H      104

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

/* 等宽 ASCII 串按字宽计算后的居中起始 X */
static uint16_t epd_scaled_text_x(const char *text, const sFONT *font, uint8_t scale)
{
    uint16_t text_w;

    if(!text || !font || scale == 0)
    {
        return 0;
    }

    text_w = (uint16_t)(strlen(text) * font->Width * scale);
    if(text_w >= Paint.Width)
    {
        return 0;
    }

    return (uint16_t)((Paint.Width - text_w) / 2);
}

static void epd_draw_scaled_string_centered(uint16_t y, const char *text,
                                            const sFONT *font, uint8_t scale,
                                            UWORD fg)
{
    size_t len;
    uint16_t x;
    size_t i;

    if(!text || !*text || !font || scale == 0)
    {
        return;
    }

    len = strlen(text);
    x = epd_scaled_text_x(text, font, scale);

    for(i = 0; i < len; i++)
    {
        epd_draw_scaled_char((uint16_t)(x + i * font->Width * scale), y,
                             text[i], font, scale, fg);
    }
}

/* 由 main 的 CLOCK_TICK 调用：读取并清除图标待刷新标记 */
uint8_t EPD_IsBluetoothIconDirty(void)
{
    uint8_t dirty = epd_bt_icon_dirty;

    if(dirty)
    {
        epd_bt_icon_dirty = 0;
    }

    return dirty;
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

/* 蓝牙图标状态切换（连接=1/断开=0）时调用：只置标记，实际刷新交给
 * main 的下一次 CLOCK_TICK（clock_render 走局刷，按 ancs_ble_connected 重画）。
 * 不在此处直接刷屏：连接成功时覆盖层「蓝牙连接成功」仍锁定屏幕，直接刷会被
 * 随后的时钟重绘覆盖成旧状态。 */
void EPD_UpdateClockBluetoothIcon(uint8_t show)
{
    /* 目标状态已与屏幕一致则无需刷新。调用方在赋值 ancs_ble_connected 后立即
     * 调用，因此比较的是「当前已呈现」的状态。 */
    if((show ? 1 : 0) == epd_bt_icon_target)
    {
        return;
    }

    epd_bt_icon_target = show ? 1 : 0;
    epd_bt_icon_dirty = 1;
}

static void epd_refresh_frame(uint8_t force_full, uint8_t notify_buzzer)
{
    if(epd_can_full_refresh() &&
       (force_full || EPD_NOTIFICATION_STRONG_REFRESH ||
        epd_force_full_refresh_once))
    {
        EPD_HW_Init_GUI();
        EPD_Display_Full_Sync(EPD_Image);
        epd_need_full = 0;
        epd_refresh_count = 0;
        epd_force_full_refresh_once = 0;
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
    Paint_NewImage(EPD_Image, EPD_WIDTH, EPD_HEIGHT, EPD_UI_ROTATE, WHITE);
    Paint_SelectImage(EPD_Image);
    Paint_Clear(WHITE);

    /* 蓝牙连接时在右上角显示蓝牙图标（位于消息图标左侧） */
    if(epd_bt_icon_target)
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
    uint8_t epd_needs_full;
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

    /* 本帧是否本应走全刷。epd_force_full_refresh_once 是显式请求，必须能
     * 单独触发全刷（例如蓝牙同步成功后从状态页切到时钟页，此时 BLE 已连接、
     * epd_can_full_refresh() 为假，且调用方走的是 Partial 接口）。 */
    epd_needs_full = (uint8_t)((epd_can_full_refresh() ||
                                epd_force_full_refresh_once) &&
                               (force_full || epd_need_full ||
                                epd_force_full_refresh_once ||
                                epd_refresh_count >= EPD_CLOCK_REFRESH_CNT));

    /* 移除蓝牙图标必须走局刷：全刷波形会把当前屏幕（含旧图标）当作新基准写入
     * 0x26，之后差分局刷再无法擦除图标。若本应全刷则推迟一帧补上，避免残影累积。 */
    if(epd_bt_icon_rendered && !epd_bt_icon_target)
    {
        EPD_Display_FastSync(EPD_Image, EPD_Image_prev);
        epd_refresh_clock_frame(0);
        if(epd_needs_full)
        {
            /* 推迟到下一帧：同时保留显式全刷请求，否则会被这一次局刷吃掉 */
            epd_need_full = 1;
            epd_force_full_refresh_once = 1;
        }
    }
    else if(epd_needs_full)
    {
        EPD_Display_Full_Sync(EPD_Image);
        epd_refresh_clock_frame(1);
        epd_force_full_refresh_once = 0;
    }
    else
    {
        EPD_Display_FastSync(EPD_Image, EPD_Image_prev);
        epd_refresh_clock_frame(0);
    }

    epd_bt_icon_rendered = epd_bt_icon_target;
    tmos_memcpy(EPD_Image_prev, EPD_Image, sizeof(EPD_Image));
}

static void epd_show_sync_status_page(const char *text)
{
    uint16_t y;

    BUZZER_Stop();
    epd_overlay_lock = 1;

    Paint_NewImage(EPD_Image, EPD_WIDTH, EPD_HEIGHT, EPD_UI_ROTATE, WHITE);
    Paint_SelectImage(EPD_Image);
    Paint_Clear(WHITE);

    y = (uint16_t)((Paint.Height > FONT_CN_H) ? ((Paint.Height - FONT_CN_H) / 2) : 0);
    epd_draw_centered_utf8(y, text, BLACK);

    epd_refresh_frame(1, 0);
}

/* 蓝牙同步等待页：状态文字上移，底部加一行"长按3返回"提示 */
static void epd_show_sync_waiting_page(const char *text)
{
    BUZZER_Stop();
    epd_overlay_lock = 1;

    Paint_NewImage(EPD_Image, EPD_WIDTH, EPD_HEIGHT, EPD_UI_ROTATE, WHITE);
    Paint_SelectImage(EPD_Image);
    Paint_Clear(WHITE);

    epd_draw_centered_utf8(64, text, BLACK);
    epd_draw_centered_utf8(140, "长按3返回", BLACK);

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
    epd_bt_icon_target = 0;
    epd_bt_icon_rendered = 0;
    epd_bt_icon_dirty = 0;
    epd_boot_menu_drawn = 0;
    epd_boot_menu_highlight = 0;
    epd_time_set_drawn = 0;
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

    Paint_NewImage(EPD_Image, EPD_WIDTH, EPD_HEIGHT, EPD_UI_ROTATE, WHITE);
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
    Paint_ClearWindows(0, EPD_STATUS_Y, EPD_UI_W, EPD_UI_H, WHITE);
    epd_draw_centered_utf8(EPD_STATUS_LABEL_Y, "配对码", BLACK);
    epd_draw_centered_utf8(EPD_STATUS_VALUE_Y, code, BLACK);
    epd_refresh_status_window();
}

void EPD_Show_BluetoothConnected(void)
{
    BUZZER_Stop();

    Paint_SelectImage(EPD_Image);
    Paint_ClearWindows(0, EPD_STATUS_Y, EPD_UI_W, EPD_UI_H, WHITE);
    epd_draw_centered_utf8(EPD_STATUS_SINGLE_Y, "蓝牙连接成功", BLACK);
    epd_refresh_status_window();
}

void EPD_Show_TimeSyncWaiting(void)
{
    epd_show_sync_waiting_page("等待蓝牙广播时间同步");
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

static void epd_draw_scaled_string_left(uint16_t x, uint16_t y, const char *text,
                                        const sFONT *font, uint8_t scale,
                                        UWORD fg)
{
    size_t i;

    if(!text || !font || scale == 0)
    {
        return;
    }

    for(i = 0; text[i]; i++)
    {
        epd_draw_scaled_char((uint16_t)(x + i * font->Width * scale), y,
                             text[i], font, scale, fg);
    }
}

/* 绘制开机选择页的单个选项按钮：高亮项黑底白字，非高亮项白底黑框黑字 */
static void epd_boot_menu_draw_row(uint8_t bt_highlight, uint8_t row)
{
    uint16_t row_y = row ? BOOT_MENU_ROW1_Y : BOOT_MENU_ROW0_Y;
    uint16_t row_text_y = (uint16_t)(row_y + (BOOT_MENU_ROW_H - FONT_CN_H) / 2);
    uint8_t active = (uint8_t)((row == 1) ? bt_highlight : !bt_highlight);
    UWORD fg = active ? WHITE : BLACK;

    Paint_DrawRectangle(BOOT_MENU_BOX_X0, row_y,
                        BOOT_MENU_BOX_X1, (uint16_t)(row_y + BOOT_MENU_ROW_H - 1),
                        BLACK, active ? DRAW_FILL_FULL : DRAW_FILL_EMPTY,
                        DOT_PIXEL_1X1);

    epd_draw_centered_utf8(row_text_y,
                           (row == 0) ? "手动设定时间" : "蓝牙时间同步", fg);
}

/* 开机选择页：手动设定时间 / 蓝牙时间同步，高亮项用黑底白字反显。
 * 首次绘制整页走全刷；仅高亮项变化时只重刷两个选项行区域（窗口局刷）。
 * force_redraw=1 用于页面被其它覆盖层（配对码、连接成功）盖住后整页恢复。 */
void EPD_Show_BootMenu(uint8_t bt_highlight, uint8_t force_redraw)
{
    uint8_t highlight = bt_highlight ? 1 : 0;
    uint8_t row;

    BUZZER_Stop();
    epd_overlay_lock = 1;

    if(!epd_boot_menu_drawn || force_redraw)
    {
        Paint_NewImage(EPD_Image, EPD_WIDTH, EPD_HEIGHT, EPD_UI_ROTATE, WHITE);
        Paint_SelectImage(EPD_Image);
        Paint_Clear(WHITE);

        epd_draw_centered_utf8(6, "时间设定", BLACK);

        for(row = 0; row < 2; row++)
        {
            epd_boot_menu_draw_row(highlight, row);
        }

        epd_draw_centered_utf8(134, "键1/2选择 键3确认", BLACK);
        epd_draw_centered_utf8(154, "长按3返回", BLACK);

        epd_boot_menu_drawn = 1;
        epd_boot_menu_highlight = highlight;
        epd_refresh_frame(1, 0);
        return;
    }

    if(highlight == epd_boot_menu_highlight)
    {
        return;
    }

    /* 只有高亮项切换：先把窗口区域清白（否则旧的黑底会留在帧缓存里，
     * 失去高亮的行仍是黑的），再重画两个选项按钮，只刷新该矩形区域。
     * 窗口按字节列对齐（像素 16~175），避开标题和底部提示文字。 */
    Paint_SelectImage(EPD_Image);
    Paint_ClearWindows(BOOT_MENU_WIN_X * 8, BOOT_MENU_WIN_Y,
                       BOOT_MENU_WIN_X * 8 + BOOT_MENU_WIN_W * 8,
                       BOOT_MENU_WIN_Y + BOOT_MENU_WIN_H, WHITE);
    for(row = 0; row < 2; row++)
    {
        epd_boot_menu_draw_row(highlight, row);
    }

    EPD_HW_Init_GUI();
    EPD_PartRefresh_Window(BOOT_MENU_WIN_X, BOOT_MENU_WIN_Y,
                           EPD_Image, EPD_Image_prev,
                           BOOT_MENU_WIN_W, BOOT_MENU_WIN_H);

    tmos_memcpy(EPD_Image_prev, EPD_Image, sizeof(EPD_Image));
    epd_boot_menu_highlight = highlight;
    epd_refresh_count++;
}

/* 设时页数值区窗口：只刷新日期/时间/下划线，不动标题和底部提示。
 * 像素 x 41~151 → 字节列 4~20；行 40~108 覆盖两行文字与下划线。 */
#define TIME_SET_WIN_X      4
#define TIME_SET_WIN_W      17
#define TIME_SET_WIN_Y      40
#define TIME_SET_WIN_H      69

/* 只画数值区（日期、时间、当前字段下划线），调用方负责选好画布 */
static void epd_time_set_draw_values(const RX8010SJ_Time *time, uint8_t field)
{
    uint16_t date_y = 44;
    uint16_t time_y = 66;
    uint16_t date_x;
    uint16_t time_x;
    uint16_t ul_x0;
    uint16_t ul_x1;
    uint16_t ul_y;
    char date_str[16];
    char time_str[8];

    snprintf(date_str, sizeof(date_str), "%04u-%02u-%02u",
             (unsigned int)time->year, (unsigned int)time->month,
             (unsigned int)time->day);
    snprintf(time_str, sizeof(time_str), "%02u:%02u",
             (unsigned int)time->hour, (unsigned int)time->minute);

    /* 日期行与时间行都按各自文本宽度居中，下划线跟随居中后的字符位置 */
    date_x = epd_scaled_text_x(date_str, &Font16, 1);
    time_x = epd_scaled_text_x(time_str, &Font16, 2);

    epd_draw_scaled_string_left(date_x, date_y, date_str, &Font16, 1, BLACK);
    epd_draw_scaled_string_left(time_x, time_y, time_str, &Font16, 2, BLACK);

    /* 按字段换算下划线位置：日期行字号 1，时间行字号 2 */
    switch(field)
    {
    case EPD_TS_YEAR:
        ul_x0 = date_x;
        ul_x1 = (uint16_t)(date_x + Font16.Width * 4);
        ul_y = (uint16_t)(date_y + Font16.Height + 2);
        break;
    case EPD_TS_MONTH:
        ul_x0 = (uint16_t)(date_x + Font16.Width * 5);
        ul_x1 = (uint16_t)(date_x + Font16.Width * 7 - 1);
        ul_y = (uint16_t)(date_y + Font16.Height + 2);
        break;
    case EPD_TS_DAY:
        ul_x0 = (uint16_t)(date_x + Font16.Width * 8);
        ul_x1 = (uint16_t)(date_x + Font16.Width * 10 - 1);
        ul_y = (uint16_t)(date_y + Font16.Height + 2);
        break;
    case EPD_TS_HOUR:
        ul_x0 = time_x;
        ul_x1 = (uint16_t)(time_x + Font16.Width * 2 * 2 - 1);
        ul_y = (uint16_t)(time_y + Font16.Height * 2 + 2);
        break;
    default:
        ul_x0 = (uint16_t)(time_x + Font16.Width * 2 * 3);
        ul_x1 = (uint16_t)(time_x + Font16.Width * 2 * 5 - 1);
        ul_y = (uint16_t)(time_y + Font16.Height * 2 + 2);
        break;
    }

    epd_fill_rect(ul_x0, ul_y, ul_x1, (uint16_t)(ul_y + 2), BLACK);
}

/* 手动设时页：逐项调整，当前项下方画一条粗下划线。
 * 首次进入整页绘制；之后调整数值只做数值区窗口局刷，缩短阻塞时间。 */
void EPD_Show_TimeSet(const RX8010SJ_Time *time, uint8_t field)
{
    if(!time)
    {
        return;
    }

    BUZZER_Stop();
    epd_overlay_lock = 1;

    if(!epd_time_set_drawn)
    {
        Paint_NewImage(EPD_Image, EPD_WIDTH, EPD_HEIGHT, EPD_UI_ROTATE, WHITE);
        Paint_SelectImage(EPD_Image);
        Paint_Clear(WHITE);

        epd_draw_centered_utf8(12, "时间设定", BLACK);
        epd_time_set_draw_values(time, field);
        epd_draw_centered_utf8(124, "键1减 键2加 键3确认", BLACK);
        epd_draw_centered_utf8(146, "长按3返回", BLACK);

        epd_time_set_drawn = 1;
        epd_refresh_frame(0, 0);
        return;
    }

    /* 仅数值变化：清窗口后重画数值区，局部刷新 */
    Paint_SelectImage(EPD_Image);
    Paint_ClearWindows(TIME_SET_WIN_X * 8, TIME_SET_WIN_Y,
                       TIME_SET_WIN_X * 8 + TIME_SET_WIN_W * 8,
                       TIME_SET_WIN_Y + TIME_SET_WIN_H, WHITE);
    epd_time_set_draw_values(time, field);

    EPD_HW_Init_GUI();
    EPD_PartRefresh_Window(TIME_SET_WIN_X, TIME_SET_WIN_Y,
                           EPD_Image, EPD_Image_prev,
                           TIME_SET_WIN_W, TIME_SET_WIN_H);

    /* 显式全刷请求已由本次局刷消费 */
    tmos_memcpy(EPD_Image_prev, EPD_Image, sizeof(EPD_Image));
    epd_force_full_refresh_once = 0;
    epd_refresh_count++;
}

void EPD_Show_TimeSetFail(void)
{
    epd_show_sync_status_page("保存时间失败");
}

/* 离开设时页后调用：下次进入必须整页重绘。否则增量窗口局刷只覆盖数值区，
 * 上一页（选择页/时钟页）的内容会残留在窗口之外。 */
void EPD_TimeSet_Invalidate(void)
{
    epd_time_set_drawn = 0;
}

void EPD_RequestFullRefreshOnce(void)
{
    epd_force_full_refresh_once = 1;
}

