/********************************** (C) COPYRIGHT *******************************
 * File Name          : EPD_Display.h
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2026/08/20
 * Description        : 1.22 inch EPD display helpers
 *********************************************************************************/

#ifndef __EPD_DISPLAY_H
#define __EPD_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "RX8010SJ.h"

#define EPD_MSG_LINE_LEN  40

/* 手动设定时间的可编辑字段，顺序即按键切换顺序 */
typedef enum
{
    EPD_TS_YEAR = 0,
    EPD_TS_MONTH,
    EPD_TS_DAY,
    EPD_TS_HOUR,
    EPD_TS_MINUTE,
    EPD_TS_BACK,        /* 底部「返回」按钮 */
    EPD_TS_SAVE,        /* 底部「保存」按钮 */
    EPD_TS_COUNT,
} epd_timeset_field_t;

/* 设置页选项顺序 */
typedef enum
{
    EPD_SET_TIME = 0,
    EPD_SET_BUZZER,
    EPD_SET_RETURN,
    EPD_SET_COUNT,
} epd_set_item_t;

void EPD_Show_Init(void);
void EPD_Show_Notification(const char *app_name, const char *title, const char *message);
void EPD_Show_NotificationSilent(const char *app_name, const char *title, const char *message);
void EPD_Show_Clock(const RX8010SJ_Time *time, uint8_t has_message);
void EPD_Show_ClockPartial(const RX8010SJ_Time *time, uint8_t has_message);
void EPD_Show_TimeSyncWaiting(void);
void EPD_Show_TimeSyncSyncing(void);
void EPD_Show_TimeSyncSuccess(void);
void EPD_Show_TimeSyncFail(void);
void EPD_Show_TimeSyncTimeout(void);
void EPD_RequestFullRefreshOnce(void);
void EPD_UpdateClockMessageIcon(uint8_t show);
void EPD_UpdateClockBluetoothIcon(uint8_t show);
uint8_t EPD_IsBluetoothIconDirty(void);
void EPD_Show_Passcode(uint32_t passcode);
void EPD_Show_BluetoothConnected(void);
void EPD_Show_BootMenu(uint8_t bt_highlight, uint8_t force_redraw);
/* field: epd_timeset_field_t；editing=1 表示正在编辑该字段（显示横杠），
 * editing=0 表示导航态（选中项反显高亮）。 */
void EPD_Show_TimeSet(const RX8010SJ_Time *time, uint8_t field, uint8_t editing);
void EPD_Show_TimeSetFail(void);
void EPD_TimeSet_Invalidate(void);
void EPD_Show_SetMenu(uint8_t selection, uint8_t buzzer_on, uint8_t force_redraw);
void EPD_SetMenu_Invalidate(void);
void EPD_SetOverlayLock(uint8_t locked);
uint8_t EPD_IsOverlayLocked(void);

#ifdef __cplusplus
}
#endif

#endif /* __EPD_DISPLAY_H */
