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
void EPD_SetOverlayLock(uint8_t locked);
uint8_t EPD_IsOverlayLocked(void);

#ifdef __cplusplus
}
#endif

#endif /* __EPD_DISPLAY_H */
