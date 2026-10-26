/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH
 * Version            : V1.1
 * Date               : 2020/08/06
 * Description        :
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/******************************************************************************/
/* 头文件包�? */
#include <time_units.h>
#include <string.h>
#include "CONFIG.h"
#include "HAL.h"
#include "ancs.h"
#include "ancs_client.h"
#include "ancs_client_internal.h"
#include "time_units.h"
#include "timesyncservice.h"
#include "EPD_Display.h"
#include "RX8010SJ.h"


static void bt_ancs_notification_source_handler(struct bt_ancs_client *ancs_c,
        int err, const struct bt_ancs_evt_notif *notif);
static void bt_ancs_data_source_handler(struct bt_ancs_client *ancs_c,
        const struct bt_ancs_attr_response *response);
static void bt_ancs_write_response_handler(struct bt_ancs_client *ancs_c,
                       uint8_t err);

__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

#if(defined(BLE_MAC)) && (BLE_MAC == TRUE)
const uint8_t MacAddr[6] = {0x84, 0xC2, 0xE4, 0x03, 0x02, 0x02};
#endif

#define ATTR_DATA_SIZE BT_ANCS_ATTR_DATA_MAX

#define KEY_REQ_NOTI_ATTR   (1<<0)
#define KEY_REQ_APP_ATTR    (1<<1)
#define KEY_POS_ACTION      (1<<0)
#define KEY_NEG_ACTION      (1<<0)

static struct bt_ancs_subscribe_cb subscribe = {
    .data_source = bt_ancs_data_source_handler,
    .notification_source = bt_ancs_notification_source_handler,
    .write_response = bt_ancs_write_response_handler,
};


/* Local copy to keep track of the newest arriving notifications. */
static struct bt_ancs_evt_notif notification_latest;
/* Local copy of the newest notification attribute. */
static struct bt_ancs_attr notif_attr_latest;
/* Local copy of the newest app attribute. */
static struct bt_ancs_attr notif_attr_app_id_latest;

/* Buffers to store attribute data. */
static uint8_t attr_appid[ATTR_DATA_SIZE];
static uint8_t attr_title[ATTR_DATA_SIZE];
static uint8_t attr_message[ATTR_DATA_SIZE];
/* 当前未使用：这些 ANCS 属性未在 ancs_init 中注册，暂不接收 */
/* static uint8_t attr_message_size[ATTR_DATA_SIZE]; */
/* static uint8_t attr_date[ATTR_DATA_SIZE]; */
/* static uint8_t attr_posaction[ATTR_DATA_SIZE]; */
/* static uint8_t attr_negaction[ATTR_DATA_SIZE]; */
static uint8_t attr_disp_name[ATTR_DATA_SIZE];

/* String literals for the iOS notification categories.
 * Used then printing to UART.
 */
static const char *lit_catid[BT_ANCS_CATEGORY_ID_COUNT] = {
    "Other",
    "Incoming Call",
    "Missed Call",
    "Voice Mail",
    "Social",
    "Schedule",
    "Email",
    "News",
    "Health And Fitness",
    "Business And Finance",
    "Location",
    "Entertainment"
};

/* String literals for the iOS notification event types.
 * Used then printing to UART.
 */
static const char *lit_eventid[BT_ANCS_EVT_ID_COUNT] = { "Added",
                             "Modified",
                             "Removed" };

/* String literals for the iOS notification attribute types.
 * Used when printing to UART.
 */
static const char *lit_attrid[BT_ANCS_NOTIF_ATTR_COUNT] = {
    "App Identifier",
    "Title",
    "Subtitle",
    "Message",
    "Message Size",
    "Date",
    "Positive Action Label",
    "Negative Action Label"
};

static const char *lit_appid[BT_ANCS_APP_ATTR_COUNT] = { "Display Name" };

/* EPD work is deferred to a TMOS task so BLE callbacks stay short. */
#define EPD_REFRESH_EVT         0x0001
#define CLOCK_TICK_EVT          0x0002
#define TIME_SYNC_RESTORE_EVT   0x0008
#define NOTIFICATION_ICON_EVT   0x0010
#define TIME_SET_RESTORE_EVT    0x0020
#define UI_APPLY_EVT            0x0040

#define TIME_SYNC_STATUS_MS      1500

typedef enum
{
    APP_SCREEN_BOOT_MENU = 0,
    APP_SCREEN_TIME_SET = 1,
    APP_SCREEN_CLOCK = 2,
    APP_SCREEN_NOTIFICATION_DETAIL = 3,
    APP_SCREEN_SET_MENU = 4,
} app_screen_t;

static tmosTaskID appTaskID;
static volatile uint8_t clock_force_refresh = 1;
static volatile uint8_t clock_force_partial_once = 0;
static volatile uint8_t clock_refresh_blocked = 0;
static volatile uint8_t notification_beep_pending = 0;
static app_screen_t app_screen = APP_SCREEN_BOOT_MENU;
/* 开机选择页当前高亮项：0=手动设定时间，1=蓝牙时间同步 */
static uint8_t boot_menu_bt_highlight = 0;
/* 手动设时：可编辑副本 + 当前字段（epd_timeset_field_t） */
static RX8010SJ_Time time_set_value;
static uint8_t time_set_field = 0;
/* 设时页编辑态：0=导航（键1/2 选高亮项），1=编辑当前项（键1/2 调值） */
static uint8_t time_set_editing = 0;
/* 设置页：当前选中项（epd_set_item_t）与蜂鸣器开关（掉电保存） */
static uint8_t set_menu_sel = 0;
static uint8_t buzzer_setting_on = 1;
/* 设时页来源：1=从设置页进入（保存/返回后回到设置页），0=开机选择页 */
static uint8_t time_set_from_settings = 0;
static uint8_t notification_unread = 0;
static uint8_t notification_pending_during_detail = 0;
/* Removed 事件可能在覆盖层或详情页显示期间到达，延后到时钟界面清除图标。 */
static uint8_t notification_icon_clear_pending = 0;
/* 消息图标是否已显示：通知未读且图标未显示时，CLOCK_TICK 会持续重试刷新，
 * 避免通知到达时覆盖层锁定/详情页导致图标刷新被跳过、直到分钟变化才恢复。 */
static uint8_t msg_icon_shown = 0;
static uint8_t clock_time_valid = 0;
static uint8_t clock_error_shown = 0;
static RX8010SJ_Time clock_time_last;
static volatile uint8_t time_sync_session_active = 0;
static volatile uint8_t time_sync_restore_to_waiting = 0;
static volatile uint8_t time_sync_status_visible = 0;
/* 上一次打印过的 RTC 秒值：细粒度轮询下按秒节流串口输出 */
static uint8_t clock_print_last_second = 0xFF;

static struct {
    uint8_t appid[ATTR_DATA_SIZE];
    uint8_t title[ATTR_DATA_SIZE];
    uint8_t message[ATTR_DATA_SIZE];
} epd_pending;

static uint8_t clock_time_changed(const RX8010SJ_Time *now)
{
    if(!clock_time_valid)
    {
        return 1;
    }

    if(now->year != clock_time_last.year ||
       now->month != clock_time_last.month ||
       now->day != clock_time_last.day ||
       now->hour != clock_time_last.hour ||
       now->minute != clock_time_last.minute)
    {
        return 1;
    }

    return 0;
}

static uint8_t clock_date_changed(const RX8010SJ_Time *now)
{
    if(!clock_time_valid)
    {
        return 1;
    }

    return (uint8_t)(now->year != clock_time_last.year ||
                     now->month != clock_time_last.month ||
                     now->day != clock_time_last.day ||
                     now->weekday != clock_time_last.weekday);
}

static void clock_render(const RX8010SJ_Time *now)
{
    /* 蓝牙图标状态变化时强制一次局刷：覆盖层/日期变化会走全刷波形，屏幕呈现的
     * 是旧图标，与 EPD_Image_prev 基准错位后快刷无法再擦除，必须借局刷把新帧
     * 与基准对齐。 */
    if(EPD_IsBluetoothIconDirty())
    {
        clock_force_partial_once = 1;
        clock_force_refresh = 0;
    }

    if(clock_force_partial_once)
    {
        EPD_Show_ClockPartial(now, notification_unread);
        clock_force_partial_once = 0;
    }
    else if(clock_force_refresh || clock_date_changed(now))
    {
        EPD_Show_Clock(now, notification_unread);
    }
    else
    {
        EPD_Show_ClockPartial(now, notification_unread);
    }

    /* 时钟重绘已包含消息图标，同步图标显示状态（若通知未读则已显示） */
    msg_icon_shown = (uint8_t)(notification_unread ? 1 : 0);
    if(!notification_unread)
    {
        notification_icon_clear_pending = 0;
    }

    clock_time_last = *now;
    clock_time_valid = 1;
    clock_force_refresh = 0;
    clock_error_shown = 0;
}

/* 时间同步等待/状态页是否正占据屏幕（此时不显示时钟界面）。
 * 用于屏蔽通知提醒音：等待广播时间同步期间蜂鸣器不应鸣响。 */
static uint8_t time_sync_ui_active(void)
{
    return (uint8_t)(app_screen == APP_SCREEN_BOOT_MENU ||
                     app_screen == APP_SCREEN_TIME_SET ||
                     app_screen == APP_SCREEN_SET_MENU ||
                     time_sync_session_active ||
                     time_sync_restore_to_waiting ||
                     time_sync_status_visible);
}

static void time_sync_begin_session(void)
{
    tmos_stop_task(appTaskID, TIME_SYNC_RESTORE_EVT);
    time_sync_session_active = 1;
    time_sync_restore_to_waiting = 0;
    time_sync_status_visible = 1;
    TimeSync_SetWindowOpen(1);
    /* 只有进入「蓝牙时间同步」页面才开广播，其余页面设备不可被搜索 */
    Ancs_SetAdvertising(1);
    EPD_SetOverlayLock(1);
    EPD_Show_TimeSyncWaiting();
}

static void time_sync_show_clock(void)
{
    RX8010SJ_Time now;

    app_screen = APP_SCREEN_CLOCK;
    /* 进入时钟页即恢复正常广播：ANCS 通知需要手机能随时重连 */
    Ancs_SetAdvertising(1);
    EPD_SetOverlayLock(0);
    time_sync_status_visible = 0;
    clock_refresh_blocked = 0;
    clock_force_refresh = 1;

    if(RX8010SJ_ReadTime(&now))
    {
        clock_render(&now);
    }
    else if(clock_time_valid)
    {
        clock_render(&clock_time_last);
    }
}

static void time_sync_schedule_restore(uint8_t waiting)
{
    time_sync_restore_to_waiting = waiting ? 1 : 0;
    tmos_start_task(appTaskID, TIME_SYNC_RESTORE_EVT,
                    MS1_TO_SYSTEM_TIME(TIME_SYNC_STATUS_MS));
}

static uint8_t days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if(month == 2 &&
       ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
    {
        return 29;
    }

    return days[(month >= 1 && month <= 12) ? (month - 1) : 0];
}

static void clock_print_time(const char *tag, const RX8010SJ_Time *t);

/* 按键动作只改状态并置 pending，刷新统一在按键松开后做一次。
 * 原因：e-paper 刷新是阻塞的（设时页整屏重绘可达数百毫秒），在回调里刷屏会
 * 让 HAL_KeyPoll 在阻塞期间漏采按键，快速连按/长按都只能响应第一次。 */
typedef enum
{
    UI_PENDING_NONE = 0,
    UI_PENDING_BOOT_MENU,   /* 开机选择页高亮变化 */
    UI_PENDING_TIME_SET,    /* 设时页数值变化 / 切换字段 */
    UI_PENDING_ENTER_TIME_SET,
    UI_PENDING_SAVE_TIME,
    UI_PENDING_SET_MENU,    /* 设置页选中项 / 蜂鸣器状态变化 */
    UI_PENDING_ENTER_SET_MENU,
    UI_PENDING_LEAVE_SET_MENU,
    UI_PENDING_CANCEL_TIME_SET,
} ui_pending_t;

#define TIME_SET_REPEAT_FIRST_MS   400  /* 按住多久后开始连发 */
#define TIME_SET_REPEAT_PERIOD_MS  120  /* 连发间隔 */
/* 连续快按时的刷新节流间隔：距上次刷新不足该间隔时只累积状态，
 * 到点立即刷一次中间态，让高亮切换过程可见；按键本身始终即时响应。 */
#define UI_SETTLE_MS               90
#define KEY3_LONG_PRESS_MS         1200 /* 键3 长按多久返回时间设定选择页 */
/* 按键去抖：只用于滤掉同一次机械抖动产生的重复触发。
 * 值不能大——设时页要"键3 进入编辑、再键3 退出"，两次必须都能识别。 */
#define KEY_DEBOUNCE_MS            60

/* 进入手动设时页时的初始时间 */
#define TIME_SET_DEFAULT_YEAR      2026
#define TIME_SET_DEFAULT_MONTH     10
#define TIME_SET_DEFAULT_DAY       1
#define TIME_SET_DEFAULT_HOUR      12
#define TIME_SET_DEFAULT_MINUTE    0

static volatile uint8_t ui_pending = UI_PENDING_NONE;
/* 下一次允许刷新的最早时刻（节流用） */
static volatile uint32_t ui_settle_time = 0;
/* 按键跟踪：上一轮采样到的按键位、下一次连发时刻、键3 长按起点 */
static uint8_t key_last_keys;
static uint32_t key_repeat_time;
static uint32_t key3_long_start;
static uint8_t key3_long_fired;

/* 记录一次待刷新（节流窗口由 UI_APPLY_EVT 内的 ui_settle_time 控制） */
static void ui_mark_pending(uint8_t pending)
{
    ui_pending = pending;
}

static void time_set_render(void)
{
    EPD_Show_TimeSet(&time_set_value, time_set_field, time_set_editing);
}

static void time_set_adjust(int8_t delta);
static void time_set_move(int8_t dir);

/* 设时页执行一步按键动作（导航移动 / 数值加减）并刷新。
 * 距上次刷新已过节流窗口则立即刷，否则只记 pending 等窗口结束再刷。
 * 立即刷新很重要：否则长按时"按下"这一格不显示，连发第一次就已经是
 * 第二格，看起来像是跳过了下一个选项。 */
static void time_set_step(uint8_t pending, int8_t delta)
{
    uint32_t now_ms = get_current_time();

    if(time_set_editing && time_set_field < EPD_TS_BACK)
    {
        time_set_adjust(delta);
    }
    else if(!time_set_editing)
    {
        time_set_move(delta);
    }
    else
    {
        /* 焦点在按钮行且未进入编辑：移动不适用于数值 */
        return;
    }

    if((int32_t)(now_ms - ui_settle_time) >= 0)
    {
        ui_pending = UI_PENDING_NONE;
        time_set_render();
        ui_settle_time = get_current_time() + UI_SETTLE_MS;
    }
    else
    {
        ui_mark_pending(pending);
    }
}

/* ---- 用户设置持久化（DataFlash，位于 BLE SNV 之外的独立页） ---- */
#define SETTINGS_EEPROM_ADDR   0x0000
#define SETTINGS_MAGIC         0xA5

typedef struct
{
    uint8_t magic;
    uint8_t buzzer_on;
} user_settings_t;

/* 读设置：首次上电 / 数据无效时使用默认值（蜂鸣器开启） */
static void settings_load(void)
{
    __attribute__((aligned(4))) user_settings_t cfg;

    cfg.magic = 0;
    cfg.buzzer_on = 1;

    if(EEPROM_READ(SETTINGS_EEPROM_ADDR, &cfg, sizeof(cfg)) == 0 &&
       cfg.magic == SETTINGS_MAGIC)
    {
        buzzer_setting_on = cfg.buzzer_on ? 1 : 0;
    }
    else
    {
        buzzer_setting_on = 1;
    }

    BUZZER_SetEnabled(buzzer_setting_on);
    PRINT("settings: buzzer=%u\n", (unsigned int)buzzer_setting_on);
}

static void settings_save(void)
{
    __attribute__((aligned(4))) user_settings_t cfg;

    cfg.magic = SETTINGS_MAGIC;
    cfg.buzzer_on = buzzer_setting_on ? 1 : 0;

    EEPROM_ERASE(SETTINGS_EEPROM_ADDR, EEPROM_MIN_ER_SIZE);
    if(EEPROM_WRITE(SETTINGS_EEPROM_ADDR, &cfg, sizeof(cfg)) != 0)
    {
        PRINT("settings save fail\n");
    }
}

/* force_redraw=1 时整页重绘（覆盖层盖住菜单后恢复），否则只做增量的高亮切换 */
static void boot_menu_show(uint8_t force_redraw)
{
    app_screen = APP_SCREEN_BOOT_MENU;
    /* 开机选择页不广播：避免设备在"未开始使用"时被手机搜到 */
    Ancs_SetAdvertising(0);
    EPD_SetOverlayLock(1);
    EPD_Show_BootMenu(boot_menu_bt_highlight, force_redraw);
}

/* 长按键3 返回时间设定选择页：仅用于蓝牙同步等待页。
 * 设时页已改为底部「返回 / 保存」按钮，不再需要长按。 */
static uint8_t key3_return_to_menu_available(void)
{
    return (uint8_t)(app_screen == APP_SCREEN_CLOCK && time_sync_ui_active());
}

/* 返回选择页：关掉蓝牙同步窗口、取消挂起的恢复任务，并整页重绘菜单 */
static void key3_return_to_menu(void)
{
    tmos_stop_task(appTaskID, TIME_SYNC_RESTORE_EVT);
    tmos_stop_task(appTaskID, TIME_SET_RESTORE_EVT);
    TimeSync_SetWindowOpen(0);
    time_sync_session_active = 0;
    time_sync_restore_to_waiting = 0;
    time_sync_status_visible = 0;
    clock_refresh_blocked = 0;
    ui_pending = UI_PENDING_NONE;
    /* 离开同步页即停止广播 */
    Ancs_SetAdvertising(0);
    /* 设时页下次进入必须整页重绘，否则上一页内容会残留在窗口之外 */
    EPD_TimeSet_Invalidate();

    BUZZER_KeyBeep();
    boot_menu_show(1);
}

/* 「蓝牙时间同步」：进入原有的等待广播页面 */
static void boot_enter_bt_sync(void)
{
    app_screen = APP_SCREEN_CLOCK;
    EPD_TimeSet_Invalidate();
    time_sync_begin_session();
}

/* 「手动设定时间」：以 RTC 当前时间（读不到则用编译时间）为初值进入编辑 */
static void time_set_enter(void)
{
    /* 每次进入设时页都从固定的初始时间开始编辑，不读取 RTC 当前值 */
    time_set_value.year = TIME_SET_DEFAULT_YEAR;
    time_set_value.month = TIME_SET_DEFAULT_MONTH;
    time_set_value.day = TIME_SET_DEFAULT_DAY;
    time_set_value.hour = TIME_SET_DEFAULT_HOUR;
    time_set_value.minute = TIME_SET_DEFAULT_MINUTE;
    time_set_value.second = 0;
    /* 星期在保存时会按年月日重新推算，这里只是给个合法值 */
    time_set_value.weekday = 4;

    time_set_field = EPD_TS_YEAR;
    time_set_editing = 0;
    app_screen = APP_SCREEN_TIME_SET;
    /* 设时页也停止广播，避免设置过程中被手机搜到 */
    Ancs_SetAdvertising(0);
    /* 从选择页整屏切换到设时页，走一次全刷避免大面积残影 */
    EPD_RequestFullRefreshOnce();
    ui_pending = UI_PENDING_ENTER_TIME_SET;
}

static void set_menu_render(void)
{
    EPD_Show_SetMenu(set_menu_sel, buzzer_setting_on, 0);
}

/* 进入设置页（时钟页短按键3） */
static void set_menu_enter(void)
{
    set_menu_sel = EPD_SET_TIME;
    app_screen = APP_SCREEN_SET_MENU;
    /* 设置页属正常使用界面，保持广播（手机需要能连着） */
    EPD_SetOverlayLock(1);
    EPD_Show_SetMenu(set_menu_sel, buzzer_setting_on, 1);
}

/* 离开设置页回到时钟页 */
static void set_menu_leave(void)
{
    EPD_SetMenu_Invalidate();
    EPD_SetOverlayLock(0);
    app_screen = APP_SCREEN_CLOCK;
    clock_refresh_blocked = 0;
    clock_force_refresh = 1;
    ui_pending = UI_PENDING_NONE;

    if(RX8010SJ_ReadTime(&clock_time_last))
    {
        clock_time_valid = 1;
    }
    time_sync_show_clock();
}

/* 设置页上按 键3：进入选中项 */
static void set_menu_activate(void)
{
    switch(set_menu_sel)
    {
    case EPD_SET_TIME:
        EPD_SetMenu_Invalidate();
        time_set_from_settings = 1;
        time_set_enter();
        break;

    case EPD_SET_BUZZER:
        buzzer_setting_on = buzzer_setting_on ? 0 : 1;
        BUZZER_SetEnabled(buzzer_setting_on);
        settings_save();
        ui_pending = UI_PENDING_SET_MENU;
        break;

    default:
        /* 返回时钟页：整页重绘交给松开后的 ui_apply_pending */
        ui_pending = UI_PENDING_LEAVE_SET_MENU;
        break;
    }
}

static void time_set_adjust(int8_t delta)
{
    RX8010SJ_Time *t = &time_set_value;

    /* 只有编辑态且选中项是时间字段时才改数值 */
    if(!time_set_editing || time_set_field >= EPD_TS_BACK)
    {
        return;
    }

    switch(time_set_field)
    {
    case EPD_TS_YEAR:
        t->year = (uint16_t)(t->year + delta);
        if(t->year < 2000) t->year = 2099;
        if(t->year > 2099) t->year = 2000;
        break;
    case EPD_TS_MONTH:
        t->month = (uint8_t)(t->month + delta);
        if(t->month < 1) t->month = 12;
        if(t->month > 12) t->month = 1;
        break;
    case EPD_TS_DAY:
    {
        uint8_t last = days_in_month(t->year, t->month);
        t->day = (uint8_t)(t->day + delta);
        if(t->day < 1) t->day = last;
        if(t->day > last) t->day = 1;
        break;
    }
    case EPD_TS_HOUR:
        if(delta > 0)
        {
            t->hour = (uint8_t)((t->hour + 1) % 24);
        }
        else
        {
            t->hour = (uint8_t)((t->hour == 0) ? 23 : (t->hour - 1));
        }
        break;
    default:
        if(delta > 0)
        {
            t->minute = (uint8_t)((t->minute + 1) % 60);
        }
        else
        {
            t->minute = (uint8_t)((t->minute == 0) ? 59 : (t->minute - 1));
        }
        break;
    }

    ui_pending = UI_PENDING_TIME_SET;
}

/* 键3：
 *   编辑态        -> 退出编辑（横杠消失，回到导航）
 *   导航态 · 时间字段 -> 进入编辑（出现横杠）
 *   导航态 · 返回/保存 -> 立刻执行对应动作
 */
static void time_set_confirm(void)
{
    if(time_set_editing)
    {
        time_set_editing = 0;
        time_set_render();
        return;
    }

    if(time_set_field == EPD_TS_BACK)
    {
        ui_pending = UI_PENDING_CANCEL_TIME_SET;
        return;
    }

    if(time_set_field == EPD_TS_SAVE)
    {
        ui_pending = UI_PENDING_SAVE_TIME;
        return;
    }

    time_set_editing = 1;
    time_set_render();
}

/* 导航态左右/上下移动选中项：年份..分钟 -> 返回 -> 保存 -> 年份 */
static void time_set_move(int8_t dir)
{
    if(dir > 0)
    {
        time_set_field = (uint8_t)((time_set_field + 1) % EPD_TS_COUNT);
    }
    else
    {
        time_set_field = (time_set_field == 0)
                         ? (uint8_t)(EPD_TS_COUNT - 1)
                         : (uint8_t)(time_set_field - 1);
    }
}

/* 设时页的统一刷新：数值/字段变化做窗口局刷，保存时整页刷 + 写 RTC */
static void time_set_apply_pending(void)
{
    RX8010SJ_Time verify;
    static const uint8_t month_offset[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    uint16_t weekday_year;

    if(ui_pending == UI_PENDING_ENTER_TIME_SET)
    {
        ui_pending = UI_PENDING_NONE;
        time_set_render();
        return;
    }

    if(ui_pending != UI_PENDING_SAVE_TIME)
    {
        ui_pending = UI_PENDING_NONE;
        time_set_render();
        return;
    }

    ui_pending = UI_PENDING_NONE;

    /* 写入前由年月日重新推算星期，避免编辑日期后星期错位 */
    weekday_year = time_set_value.year;
    if(time_set_value.month < 3)
    {
        weekday_year--;
    }
    time_set_value.weekday = (uint8_t)((weekday_year + weekday_year / 4 -
                                       weekday_year / 100 + weekday_year / 400 +
                                       month_offset[time_set_value.month - 1] +
                                       time_set_value.day) % 7);
    time_set_value.second = 0;

    if(!RX8010SJ_SetTime(&time_set_value))
    {
        EPD_Show_TimeSetFail();
        tmos_start_task(appTaskID, TIME_SET_RESTORE_EVT,
                        MS1_TO_SYSTEM_TIME(TIME_SYNC_STATUS_MS));
        return;
    }

    RX8010SJ_ReadTime(&verify);
    clock_print_time("[timeset] set ", &time_set_value);
    clock_print_time("[timeset] rtc ", &verify);
    clock_time_last = time_set_value;
    clock_time_valid = 1;
    clock_force_refresh = 1;
    clock_error_shown = 0;

    EPD_TimeSet_Invalidate();

    if(time_set_from_settings)
    {
        /* 从设置页进入：保存后回设置页 */
        time_set_from_settings = 0;
        EPD_SetMenu_Invalidate();
        set_menu_sel = EPD_SET_TIME;
        app_screen = APP_SCREEN_SET_MENU;
        EPD_SetOverlayLock(1);
        EPD_Show_SetMenu(set_menu_sel, buzzer_setting_on, 1);
        return;
    }

    app_screen = APP_SCREEN_CLOCK;
    time_sync_show_clock();
}

/* 放弃本次编辑并离开设时页：从未保存的编辑不写 RTC。
 * 返回到进入设时页的那个页面：
 *   从设置页进入 -> 设置页
 *   从开机「时间设定」选择页进入 -> 开机选择页 */
static void time_set_cancel(void)
{
    EPD_TimeSet_Invalidate();
    clock_refresh_blocked = 0;

    if(time_set_from_settings)
    {
        time_set_from_settings = 0;
        EPD_SetMenu_Invalidate();
        set_menu_sel = EPD_SET_TIME;
        app_screen = APP_SCREEN_SET_MENU;
        EPD_SetOverlayLock(1);
        EPD_Show_SetMenu(set_menu_sel, buzzer_setting_on, 1);
        return;
    }

    /* 开机选择页进入：回到「时间设定」选择页，而不是时钟页 */
    boot_menu_show(1);
}

/* 统一处理待刷新项；只在按键全部松开且本轮无待处理刷新时调用，
 * 避免刷屏期间漏采按键。 */
static void ui_apply_pending(void)
{
    if(ui_pending == UI_PENDING_NONE)
    {
        return;
    }

    /* 设置页的进出会整页重绘（阻塞），同样放到按键松开后执行 */
    if(ui_pending == UI_PENDING_ENTER_SET_MENU)
    {
        ui_pending = UI_PENDING_NONE;
        set_menu_enter();
        return;
    }
    if(ui_pending == UI_PENDING_LEAVE_SET_MENU)
    {
        ui_pending = UI_PENDING_NONE;
        set_menu_leave();
        return;
    }
    if(ui_pending == UI_PENDING_CANCEL_TIME_SET)
    {
        ui_pending = UI_PENDING_NONE;
        time_set_cancel();
        return;
    }

    if(app_screen == APP_SCREEN_TIME_SET)
    {
        time_set_apply_pending();
    }
    else if(app_screen == APP_SCREEN_SET_MENU)
    {
        ui_pending = UI_PENDING_NONE;
        set_menu_render();
    }
    else
    {
        ui_pending = UI_PENDING_NONE;
        if(app_screen == APP_SCREEN_BOOT_MENU)
        {
            boot_menu_show(0);
        }
    }
}

/* 常见 App 包名 → 中文名映射表（因 Display Name 请求会打断通知属性回传，改用映射表） */
typedef struct {
    const char *pkg;   /* 包名 */
    const char *name;  /* 中文名 */
} app_name_map_t;

static const app_name_map_t app_name_map[] = {
    {"com.tencent.xin",        "微信"},
    {"com.tencent.mm",         "微信"},
    {"com.tencent.mobileqq",   "QQ"},
    {"com.tencent.qqmusic",    "QQ音乐"},
    {"com.tencent.tim",        "TIM"},
    {"com.taobao.taobao",      "淘宝"},
    {"com.taobao.tmall",       "天猫"},
    {"com.alipay.mobile",      "支付宝"},
    {"com.jingdong.app.mall",  "京东"},
    {"com.sina.weibo",         "微博"},
    {"com.douban.frodo",       "豆瓣"},
    {"com.baidu.searchbox",    "百度"},
    {"com.tencent.qqlive",     "腾讯视频"},
    {"com.youku.phone",        "优酷"},
    {"com.tencent.mtt",        "QQ浏览器"},
    {"com.android.mms",        "短信"},
    {"com.android.phone",      "电话"},
    {"com.apple.mobilemail",   "邮件"},
    {"com.apple.MobileSMS",    "信息"},
    {"com.apple.mobileslideshow", "照片"},
    {NULL, NULL}
};

/* 查询包名对应的中文名，返回匹配的中文名指针；未找到返回 NULL */
static const char *app_name_lookup(const char *pkg)
{
    const app_name_map_t *p;
    if(!pkg) return NULL;
    for(p = app_name_map; p->pkg; p++)
    {
        if(strcmp(p->pkg, pkg) == 0)
        {
            return p->name;
        }
    }
    return NULL;
}

static void epd_queue_refresh(void)
{
    /* App 名：优先查映射表的中文名，没有则回退到包名 */
    const char *cn_name = app_name_lookup((char *)attr_appid);
    if(cn_name)
    {
        strncpy((char *)epd_pending.appid, cn_name, sizeof(epd_pending.appid) - 1);
        epd_pending.appid[sizeof(epd_pending.appid) - 1] = '\0';
    }
    else
    {
        tmos_memcpy(epd_pending.appid, attr_appid, sizeof(epd_pending.appid));
        epd_pending.appid[sizeof(epd_pending.appid) - 1] = '\0';
    }
    tmos_memcpy(epd_pending.title, attr_title, sizeof(epd_pending.title));
    epd_pending.title[sizeof(epd_pending.title) - 1] = '\0';
    tmos_memcpy(epd_pending.message, attr_message, sizeof(epd_pending.message));
    epd_pending.message[sizeof(epd_pending.message) - 1] = '\0';
    notification_unread = 1;
    notification_pending_during_detail =
        (uint8_t)(app_screen == APP_SCREEN_NOTIFICATION_DETAIL);
    tmos_set_event(appTaskID, EPD_REFRESH_EVT);
}

/* 串口时间打印：输出 RTC 读到的完整时间（含秒与星期）。
 * 墨水屏时钟页只画 HH:MM，无法从屏幕上判断秒级偏差，只能靠串口比对。 */
static void clock_print_time(const char *tag, const RX8010SJ_Time *t)
{
    PRINT("%s %04u-%02u-%02u %02u:%02u:%02u w%u\n", tag,
          (unsigned int)t->year, (unsigned int)t->month, (unsigned int)t->day,
          (unsigned int)t->hour, (unsigned int)t->minute, (unsigned int)t->second,
          (unsigned int)t->weekday);
}

/* 时钟轮询节拍。RX8010SJ 是外部 RTC，走自己的晶振，其秒沿与 MCU 定时器没有
 * 相位关系，也无法读取亚秒相位。常规按 1 秒轮询；进入秒值 58 后改为细粒度
 * 轮询，把「发现分钟翻转」的滞后从平均 500ms 压到 CLOCK_TICK_FINE_MS 以内。 */
#define CLOCK_TICK_NORMAL_MS    1000
#define CLOCK_TICK_FINE_MS      20      /* 细粒度轮询周期，即秒沿检测精度 */
#define CLOCK_TICK_FINE_FROM    58      /* 秒值 >= 此值时进入细粒度轮询 */

static void clock_tick_reschedule(const RX8010SJ_Time *now)
{
    uint32_t delay_ms = (now && now->second >= CLOCK_TICK_FINE_FROM)
                        ? CLOCK_TICK_FINE_MS : CLOCK_TICK_NORMAL_MS;

    tmos_start_task(appTaskID, CLOCK_TICK_EVT, MS1_TO_SYSTEM_TIME(delay_ms));
}
static tmosEvents Main_ProcessEvent(tmosTaskID task_id, tmosEvents events)
{
    if (events & NOTIFICATION_ICON_EVT)
    {
        /* 鸣响期间推迟图标刷新：图标刷新会触发阻塞式墨水屏刷新（忙等待自旋），
         * 会推迟蜂鸣器 PWM 的关断、把"滴"声拉长。蜂鸣仅约 320ms，稍后刷新即可。 */
        if (BUZZER_IsBeeping())
        {
            tmos_start_task(appTaskID, NOTIFICATION_ICON_EVT,
                            MS1_TO_SYSTEM_TIME(100));
            return events ^ NOTIFICATION_ICON_EVT;
        }

        /* app_screen and the overlay lock are the actual display ownership
         * state.  The time-sync flags may remain active while the clock is
         * already visible, so using them here can postpone this event until
         * the next minute refresh. */
        if (app_screen == APP_SCREEN_CLOCK && !EPD_IsOverlayLocked())
        {
            if(notification_icon_clear_pending)
            {
                PRINT("  [EPD] clearing removed notification icon\n");
                EPD_UpdateClockMessageIcon(0);
                notification_icon_clear_pending = 0;
                msg_icon_shown = 0;
                PRINT("  [EPD] removed notification icon cleared\n");
            }
            else if(notification_unread && !msg_icon_shown)
            {
                PRINT("  [EPD] showing notification icon\n");
                EPD_UpdateClockMessageIcon(1);
                msg_icon_shown = 1;
                PRINT("  [EPD] notification icon shown\n");
            }
        }
        else if(app_screen == APP_SCREEN_CLOCK &&
                (notification_icon_clear_pending ||
                 (notification_unread && !msg_icon_shown)))
        {
            tmos_start_task(appTaskID, NOTIFICATION_ICON_EVT,
                            MS1_TO_SYSTEM_TIME(100));
        }

        return events ^ NOTIFICATION_ICON_EVT;
    }

    if (events & EPD_REFRESH_EVT)
    {
        if (time_sync_ui_active())
        {
            /* 等待时间同步期间保持等待页，通知提醒音一并丢弃：
             * 该页面只表示"等待蓝牙广播时间同步"，不应因消息通知而鸣响。 */
            notification_beep_pending = 0;
            return events ^ EPD_REFRESH_EVT;
        }

        if (ancs_pairing_success_refresh_pending)
        {
            app_screen = APP_SCREEN_CLOCK;
            clock_force_partial_once = 1;
            clock_force_refresh = 0;
        }

        if (app_screen == APP_SCREEN_CLOCK &&
            ancs_pairing_success_refresh_pending)
        {
            RX8010SJ_Time now;
            if(ancs_pairing_success_refresh_pending && RX8010SJ_ReadTime(&now))
            {
                clock_render(&now);
            }
            else if(clock_time_valid)
            {
                clock_render(&clock_time_last);
            }
            else if(RX8010SJ_ReadTime(&now))
            {
                clock_render(&now);
            }

            if (ancs_pairing_success_refresh_pending)
            {
                ancs_pairing_success_refresh_pending = 0;
            }
        }

        if (notification_beep_pending)
        {
            notification_beep_pending = 0;
            BUZZER_NotifyBeep();
        }

        return events ^ EPD_REFRESH_EVT;
    }

    if (events & TIME_SYNC_EVT)
    {
        time_sync_request_t req;

        if(TimeSync_PopRequest(&req))
        {
            if(req.type == TIME_SYNC_REQ_TIME && time_sync_session_active)
            {
                EPD_SetOverlayLock(1);

                /* Write the RTC before any e-paper refresh.  EPD refreshes are
                 * blocking and can take multiple seconds, which would make
                 * the received second value stale before it reaches the RTC. */
                clock_print_time("[timesync] req ", &req.time);
                uint8_t time_set_ok = RX8010SJ_SetTime(&req.time);
                {
                    /* 回读用于量化写入延迟：与 req 的秒差即手机侧推算到
                     * RTC 落地之间的残余滞后。 */
                    RX8010SJ_Time verify;
                    if(RX8010SJ_ReadTime(&verify))
                    {
                        clock_print_time("[timesync] rtc ", &verify);
                    }
                }

                EPD_Show_TimeSyncSyncing();
                if(time_set_ok)
                {
                    clock_time_last = req.time;
                    clock_time_valid = 1;
                    clock_force_refresh = 1;
                    clock_error_shown = 0;
                    time_sync_session_active = 0;
                    time_sync_status_visible = 1;
                    TimeSync_SetWindowOpen(0);
                    EPD_Show_TimeSyncSuccess();
                    EPD_RequestFullRefreshOnce();
                    time_sync_schedule_restore(0);
                }
                else
                {
                    time_sync_status_visible = 1;
                    EPD_Show_TimeSyncFail();
                    time_sync_schedule_restore(1);
                }
            }
            else if(req.type == TIME_SYNC_REQ_INVALID && time_sync_session_active)
            {
                /* 只在蓝牙同步流程中提示；开机选择页/手动设时期间收到的
                 * 非法写入直接丢弃，不抢占当前页面。 */
                EPD_SetOverlayLock(1);
                time_sync_status_visible = 1;
                EPD_Show_TimeSyncFail();
                time_sync_schedule_restore(1);
            }
        }

        return events ^ TIME_SYNC_EVT;
    }

    if (events & TIME_SYNC_RESTORE_EVT)
    {
        if(time_sync_restore_to_waiting && time_sync_session_active)
        {
            time_sync_restore_to_waiting = 0;
            EPD_SetOverlayLock(1);
            time_sync_status_visible = 1;
            EPD_Show_TimeSyncWaiting();
        }
        else
        {
            time_sync_restore_to_waiting = 0;
            TimeSync_SetWindowOpen(0);
            time_sync_session_active = 0;
            BUZZER_NotifyBeep();
            time_sync_show_clock();
        }

        return events ^ TIME_SYNC_RESTORE_EVT;
    }

    if (events & TIME_SET_RESTORE_EVT)
    {
        /* 写入失败提示结束后，恢复手动设时页继续编辑 */
        if(app_screen == APP_SCREEN_TIME_SET)
        {
            EPD_SetOverlayLock(1);
            time_set_render();
        }

        return events ^ TIME_SET_RESTORE_EVT;
    }

    if (events & UI_APPLY_EVT)
    {
        uint8_t keys = HalKeyRead();
        uint32_t now_ms = get_current_time();

        if (keys == 0)
        {
            /* HAL_KeyPoll 只在按下时回调，松开不会通知，这里清零上一轮按键状态，
             * 否则再次按下同一个键会被误判为"一直按住"而不响应。 */
            key_last_keys = 0;
            key3_long_start = 0;
            key3_long_fired = 0;

            /* 节流：距上次刷新不足 UI_SETTLE_MS 时，把这次刷新推迟到窗口结束，
             * 期间的新按键继续累积状态。窗口一到立刻刷，高亮过程仍然可见。 */
            if (ui_pending != UI_PENDING_NONE &&
                (int32_t)(now_ms - ui_settle_time) < 0)
            {
                int32_t wait_ms = (int32_t)(ui_settle_time - now_ms);
                tmos_start_task(appTaskID, UI_APPLY_EVT,
                                MS1_TO_SYSTEM_TIME((uint32_t)wait_ms));
                return events ^ UI_APPLY_EVT;
            }

            ui_apply_pending();
            ui_settle_time = get_current_time() + UI_SETTLE_MS;
            return events ^ UI_APPLY_EVT;
        }

        /* 键3 长按返回时间设定选择页。轮询周期 50ms，判定精度足够。 */
        if (key3_return_to_menu_available() && (keys & HAL_KEY_SW_3))
        {
            if (!key3_long_fired)
            {
                if (key3_long_start == 0)
                {
                    key3_long_start = now_ms;
                }
                else if ((uint32_t)(now_ms - key3_long_start) >= KEY3_LONG_PRESS_MS)
                {
                    key3_long_fired = 1;
                    key3_return_to_menu();
                }
            }
        }
        else
        {
            key3_long_start = 0;
            key3_long_fired = 0;
        }

        /* 设时页长按连发：
         *   编辑态 -> 连续加减数值
         *   导航态 -> 连续切换选中项
         * 每一步都立刻刷屏，让中间过程可见。刷屏是阻塞的，所以实际连发速度
         * 由刷新耗时决定（比 TIME_SET_REPEAT_PERIOD_MS 慢时以刷新为准）。 */
        if (app_screen == APP_SCREEN_TIME_SET &&
            (keys & (HAL_KEY_SW_1 | HAL_KEY_SW_2)) &&
            (int32_t)(now_ms - key_repeat_time) >= 0)
        {
            int8_t delta = (keys & (HAL_KEY_SW_1)) ? -1 : 1;

            time_set_step(UI_PENDING_TIME_SET, delta);
            key_repeat_time = get_current_time() + TIME_SET_REPEAT_PERIOD_MS;
        }

        /* 仍有键按住：等松开再刷新，保证按键采样不被阻塞刷屏打断 */
        tmos_start_task(appTaskID, UI_APPLY_EVT, MS1_TO_SYSTEM_TIME(50));
        return events ^ UI_APPLY_EVT;
    }

    if (events & CLOCK_TICK_EVT)
    {
        RX8010SJ_Time now;

        /* 鸣响期间推迟一切屏幕刷新：忙等待式刷新会推迟蜂鸣器 PWM 关断，
         * 把"滴"声拉长。时钟最迟在鸣响结束后的下一轮 tick 补刷。 */
        if (BUZZER_IsBeeping())
        {
            clock_tick_reschedule(NULL);
            return events ^ CLOCK_TICK_EVT;
        }

        /* 开机选择页 / 手动设时页不参与时钟刷新，页面重绘由各自逻辑负责。
         * 配对成功若发生在这两个页面上，只重绘当前页，不跳转到时钟页。 */
        if (app_screen != APP_SCREEN_CLOCK)
        {
            if (ancs_pairing_success_refresh_pending)
            {
                ancs_pairing_success_refresh_pending = 0;
                if (app_screen == APP_SCREEN_BOOT_MENU)
                {
                    /* 配对成功覆盖层刚撤下，必须整页重绘 */
                    boot_menu_show(1);
                }
                else if (app_screen == APP_SCREEN_SET_MENU)
                {
                    EPD_SetOverlayLock(1);
                    EPD_Show_SetMenu(set_menu_sel, buzzer_setting_on, 1);
                }
                else
                {
                    EPD_SetOverlayLock(1);
                    time_set_render();
                }
            }
            clock_tick_reschedule(NULL);
            return events ^ CLOCK_TICK_EVT;
        }

        if (ancs_pairing_success_refresh_pending)
        {
            app_screen = APP_SCREEN_CLOCK;
            clock_force_partial_once = 1;
            clock_force_refresh = 0;
            EPD_SetOverlayLock(0);
        }

        if (EPD_IsOverlayLocked())
        {
            clock_refresh_blocked = 1;
            clock_tick_reschedule(NULL);
            return events ^ CLOCK_TICK_EVT;
        }

        /* 蓝牙图标状态变化（连接/断开）后尽快重绘：借局刷让屏幕与 EPD_Image_prev
         * 基准同步，避免图标残留在下一分钟刷新前都不消失。 */
        if (EPD_IsBluetoothIconDirty())
        {
            clock_force_partial_once = 1;
            clock_force_refresh = 0;
        }

        if (notification_icon_clear_pending &&
            app_screen == APP_SCREEN_CLOCK &&
            !EPD_IsOverlayLocked())
        {
            EPD_UpdateClockMessageIcon(0);
            notification_icon_clear_pending = 0;
            msg_icon_shown = 0;
            PRINT("  [EPD] removed notification icon cleared (retry)\n");
        }

        /* 消息图标重试：通知未读但图标尚未显示（此前可能因覆盖层锁定/详情页
         * 导致 EPD_REFRESH_EVT 跳过），这里在每次时钟 tick 尝试补刷，直到显示。 */
        if (notification_unread && !msg_icon_shown &&
            app_screen == APP_SCREEN_CLOCK)
        {
            EPD_UpdateClockMessageIcon(1);
            msg_icon_shown = 1;
        }

        if (!RX8010SJ_ReadTime(&now))
        {
            if (app_screen == APP_SCREEN_CLOCK && !clock_error_shown)
            {
                EPD_Show_Notification("Clock", "RX8010SJ", "RTC read fail");
                clock_error_shown = 1;
            }
            clock_tick_reschedule(NULL);
            return events ^ CLOCK_TICK_EVT;
        }

        clock_error_shown = 0;

        /* 打印 RTC 读数，用于与手机比对分钟翻转。细粒度轮询期间每 20ms 就
         * 触发一次，按秒值节流，避免同一秒重复输出。 */
        if (now.second != clock_print_last_second)
        {
            clock_print_last_second = now.second;
            clock_print_time("[tick]", &now);
        }

        if (clock_refresh_blocked)
        {
            clock_force_refresh = 1;
            clock_refresh_blocked = 0;
        }

        if (app_screen == APP_SCREEN_CLOCK &&
            (clock_force_partial_once || clock_force_refresh ||
             clock_time_changed(&now) || ancs_pairing_success_refresh_pending))
        {
            /* 量化阻塞式刷屏耗时：局刷约 800ms，面板在波形结束后才呈现新画面，
             * 这是细粒度轮询之外剩余的显示延迟。 */
            uint32_t render_start = get_current_time();
            clock_render(&now);
            PRINT("[tick] render %ums\n",
                  (unsigned int)(get_current_time() - render_start));

            if (ancs_pairing_success_refresh_pending)
            {
                ancs_pairing_success_refresh_pending = 0;
            }
        }

        /* 一次性任务而非重载任务：需要按当前秒值改变下一次的间隔。
         * 所有提前返回的分支都已各自重排期，否则时钟会永久停摆。 */
        clock_tick_reschedule(&now);
        return events ^ CLOCK_TICK_EVT;
    }

    return 0;
}




static void notif_print(const struct bt_ancs_evt_notif *notif)
{
    PRINT("\nNotification\n");
    PRINT("Event:       %s\n", lit_eventid[notif->evt_id]);
    PRINT("Category ID: %s\n", lit_catid[notif->category_id]);
    PRINT("Category Cnt:%u\n", (unsigned int)notif->category_count);
    PRINT("UID:         %u\n", (unsigned int)notif->notif_uid);

    PRINT("Flags:\n");
    if (notif->evt_flags.silent) {
        PRINT(" Silent\n");
    }
    if (notif->evt_flags.important) {
        PRINT(" Important\n");
    }
    if (notif->evt_flags.pre_existing) {
        PRINT(" Pre-existing\n");
    }
    if (notif->evt_flags.positive_action) {
        PRINT(" Positive Action\n");
    }
    if (notif->evt_flags.negative_action) {
        PRINT(" Negative Action\n");
    }
}

/**@brief Function for printing iOS notification attribute data.
 *
 * @param[in] attr Pointer to an iOS notification attribute.
 */
static void notif_attr_print(const struct bt_ancs_attr *attr)
{
    if (attr->attr_len != 0) {
        PRINT("%s: %s\n", lit_attrid[attr->attr_id],
               (char *)attr->attr_data);
    } else if (attr->attr_len == 0) {
        PRINT("%s: (N/A)\n", lit_attrid[attr->attr_id]);
    }
}

/**@brief Function for printing iOS notification attribute data.
 *
 * @param[in] attr Pointer to an iOS App attribute.
 */
static void app_attr_print(const struct bt_ancs_attr *attr)
{
    if (attr->attr_len != 0) {
        PRINT("%s: %s\n", lit_appid[attr->attr_id],
               (char *)attr->attr_data);
    } else if (attr->attr_len == 0) {
        PRINT("%s: (N/A)\n", lit_appid[attr->attr_id]);
    }
}

static int ancs_init(void)
{
    int err;

    err = bt_ancs_client_init(&ancs_c);
    if (err) {
        return err;
    }

    err = bt_ancs_register_attr(&ancs_c,
        BT_ANCS_NOTIF_ATTR_ID_APP_IDENTIFIER,
        attr_appid, ATTR_DATA_SIZE);
    if (err) {
        return err;
    }

    err = bt_ancs_register_app_attr(&ancs_c,
        BT_ANCS_APP_ATTR_ID_DISPLAY_NAME,
        attr_disp_name, sizeof(attr_disp_name));
    if (err) {
        return err;
    }

    err = bt_ancs_register_attr(&ancs_c,
        BT_ANCS_NOTIF_ATTR_ID_TITLE,
        attr_title, ATTR_DATA_SIZE);
    if (err) {
        return err;
    }

    err = bt_ancs_register_attr(&ancs_c,
        BT_ANCS_NOTIF_ATTR_ID_MESSAGE,
        attr_message, ATTR_DATA_SIZE);
    if (err) {
        return err;
    }

    return err;
}

static void bt_ancs_notification_source_handler(struct bt_ancs_client *ancs_c,
        int err, const struct bt_ancs_evt_notif *notif)
{
    if (!err) {
        notif_print(notif);

        if(notif->evt_id == BT_ANCS_EVENT_ID_NOTIFICATION_REMOVED)
        {
            if(notification_unread &&
               notification_latest.notif_uid == notif->notif_uid)
            {
                notification_unread = 0;
                notification_pending_during_detail = 0;
                notification_icon_clear_pending = 1;
                tmos_set_event(appTaskID, NOTIFICATION_ICON_EVT);
                PRINT("  [EPD] removed current UID=%lu, clear icon queued\n",
                      (unsigned long)notif->notif_uid);
            }
            else
            {
                PRINT("  [EPD] removed UID=%lu ignored, current UID=%lu unread=%u\n",
                      (unsigned long)notif->notif_uid,
                      (unsigned long)notification_latest.notif_uid,
                      notification_unread);
            }
            return;
        }

        /* 只处理"新增"且非"重连同步的历史通知"：
         * - 重连时 iOS 会同步大量 pre-existing 通知，这些不是新消息，无需显示
         * - 服务发现未完成时 CP 句柄无效，请求属性会失败，跳过 */
        if(notif->evt_id != BT_ANCS_EVENT_ID_NOTIFICATION_ADDED ||
           notif->evt_flags.pre_existing)
        {
            PRINT("  [EPD] filtered: evt=%d preexist=%d\n",
                notif->evt_id, notif->evt_flags.pre_existing);
            return;
        }
        if(!atomic_test_bit(&ancs_c->state, ANCS_BT_DISCOVERY_COMPLETE))
        {
            PRINT("  [EPD] discovery not complete, skip\n");
            return;
        }

        notification_latest = *notif;
        notification_unread = 1;
        notification_pending_during_detail =
            (uint8_t)(app_screen == APP_SCREEN_NOTIFICATION_DETAIL);
        notification_icon_clear_pending = 0;
        /* 等待蓝牙广播时间同步期间不鸣响提醒音 */
        notification_beep_pending = time_sync_ui_active() ? 0 : 1;
        /* 新通知到达，清空旧内容防止详情页显示残留 */
        epd_pending.appid[0] = '\0';
        epd_pending.title[0] = '\0';
        epd_pending.message[0] = '\0';
        /* 使用独立事件刷新图标；图标已显示时无需重复刷新。 */
        tmos_set_event(appTaskID, NOTIFICATION_ICON_EVT);
        tmos_set_event(appTaskID, EPD_REFRESH_EVT);

        /* 收到新消息通知，请求属性详情（App 名/标题/消息全文等），
         * 详情通过 Data Source 回传，收齐后由 data source handler 刷新屏幕 */
        {
            int aerr = bt_ancs_request_attrs(ancs_c, &notification_latest,
                                             bt_ancs_write_response_handler);
            PRINT("  [EPD] req_attrs err=%d\n", aerr);
        }
    }
}

static void bt_ancs_data_source_handler(struct bt_ancs_client *ancs_c,
        const struct bt_ancs_attr_response *response)
{
    PRINT("  [EPD] DS cmd=%d attr_id=%lu\n",
        response->command_id, (unsigned long)response->attr.attr_id);
    /* 触发屏幕刷新的条件：收到 Title 属性即刷新
     * （iOS 对同一 App 的后续通知可能不重复回传 App Identifier，
     *   不能依赖"三个属性收齐"作为触发条件） */
    switch (response->command_id) {
    case BT_ANCS_COMMAND_ID_GET_NOTIF_ATTRIBUTES:
        if(!notification_unread ||
           response->notif_uid != notification_latest.notif_uid)
        {
            PRINT("  [EPD] stale attrs UID=%lu ignored, current UID=%lu unread=%u\n",
                  (unsigned long)response->notif_uid,
                  (unsigned long)notification_latest.notif_uid,
                  notification_unread);
            break;
        }

        notif_attr_latest = response->attr;
        notif_attr_print(&notif_attr_latest);
        if (response->attr.attr_id ==
            BT_ANCS_NOTIF_ATTR_ID_APP_IDENTIFIER) {
            notif_attr_app_id_latest = response->attr;
            /* 注意：不能在通知属性回传期间发起 Display Name 请求，
             * 会打断 iOS 的 Title/Message 回传（详见历史问题）。中文名通过映射表查询。 */
            /* 收到 App Identifier 即刷新，让详情页至少可显示 App 名 */
            epd_queue_refresh();
        } else if (response->attr.attr_id ==
            BT_ANCS_NOTIF_ATTR_ID_TITLE) {
            /* 收到 Title 即刷新，详情页可显示 App 名 + 标题 */
            epd_queue_refresh();
        } else if (response->attr.attr_id ==
            BT_ANCS_NOTIF_ATTR_ID_MESSAGE) {
            /* 等 message 到齐后再刷新，避免 title 先到就阻塞 BLE。 */
            PRINT("  [EPD] message received, queue refresh\n");
            epd_queue_refresh();
        }
        break;

    case BT_ANCS_COMMAND_ID_GET_APP_ATTRIBUTES:
        app_attr_print(&response->attr);
        break;

    default:
        /* No implementation needed. */
        break;
    }
}

static void bt_ancs_write_response_handler(struct bt_ancs_client *ancs_c,
                       uint8_t err)
{
    switch (err) {
    case BT_ATT_ERR_ANCS_NP_UNKNOWN_COMMAND:
        PRINT("Error: Command ID was not recognized by the Notification Provider.\n");
        break;

    case BT_ATT_ERR_ANCS_NP_INVALID_COMMAND:
        PRINT("Error: Command failed to be parsed on the Notification Provider.\n");
        break;

    case BT_ATT_ERR_ANCS_NP_INVALID_PARAMETER:
        PRINT("Error: Parameter does not refer to an existing object on the Notification Provider.\n");
        break;

    case BT_ATT_ERR_ANCS_NP_ACTION_FAILED:
        PRINT("Error: Perform Notification Action Failed on the Notification Provider.\n");
        break;

    default:
        /* 非 ANCS 错误码（如 ATT 层错误），打印原始值便于排查 */
        if (err) {
            PRINT("CP write err: %#x\n", err);
        }
        break;
    }
}

struct key_state {
    uint32_t timestamp_key1;
    uint32_t timestamp_key2;
    uint32_t timestamp_key3;
};

/**
 * 开机选择页：
 *      key1: 高亮「手动设定时间」。
 *      key2: 高亮「蓝牙时间同步」。
 *      key3: 进入当前高亮项。
 *
 * 手动设时页（两级操作）：
 *      导航态（未进入某项）：key1/key2 移动高亮项（年→月→日→时→分→返回→保存），
 *                            key3 进入编辑，或在「返回/保存」上直接执行。
 *      编辑态：key1 减、key2 加（按住连发），key3 退出编辑（横杠消失）。
 *
 * 时钟页：
 * key1(PB22):
 *      single: request attributes for a notification.
 *      double: request attributes for a given app.
 *
 * key2(PB13):
 *      single: toggle notification detail / clock page.
 *
 * key3(PB12):
 *      single: dump the attributes of the latest notification.
 */
void button_changed(uint8_t keys)
{
    int err;
    static struct key_state k_s = {0};
    /* 本轮回调中新按下的键（相对上一轮采样）。长按不会重复触发，
     * 连发由 UI_APPLY_EVT 里的重复定时器负责。 */
    uint8_t pressed = (uint8_t)(keys & (uint8_t)~key_last_keys);

    key_last_keys = keys;

    /* 开机选择页：键1/键2 切换高亮项，键3 进入当前高亮项。
     * 动作只改状态，刷屏和按键提示音都交给 UI_APPLY_EVT 在松开后执行
     * （EPD 刷新函数开头会 BUZZER_Stop，先响会被截断，听感忽长忽短）。 */
    if(app_screen == APP_SCREEN_BOOT_MENU)
    {
        if(pressed & (HAL_KEY_SW_1 | HAL_KEY_SW_2))
        {
            BUZZER_KeyBeep();
            boot_menu_bt_highlight = (pressed & HAL_KEY_SW_1) ? 0 : 1;
            ui_pending = UI_PENDING_BOOT_MENU;
        }
        else if(pressed & HAL_KEY_SW_3)
        {
            BUZZER_KeyBeep();
            if(boot_menu_bt_highlight)
            {
                boot_enter_bt_sync();
            }
            else
            {
                time_set_from_settings = 0;
                time_set_enter();
            }
        }

        tmos_set_event(appTaskID, UI_APPLY_EVT);
        return;
    }

    /* 设置页：键1/键2 上下移动选中项，键3 进入选中项 */
    if(app_screen == APP_SCREEN_SET_MENU)
    {
        if(pressed & (HAL_KEY_SW_1 | HAL_KEY_SW_2))
        {
            BUZZER_KeyBeep();
            if(pressed & HAL_KEY_SW_1)
            {
                set_menu_sel = (set_menu_sel == 0) ? (EPD_SET_COUNT - 1) : (set_menu_sel - 1);
            }
            else
            {
                set_menu_sel = (uint8_t)((set_menu_sel + 1) % EPD_SET_COUNT);
            }
            ui_pending = UI_PENDING_SET_MENU;
        }
        else if(pressed & HAL_KEY_SW_3)
        {
            BUZZER_KeyBeep();
            set_menu_activate();
        }

        tmos_set_event(appTaskID, UI_APPLY_EVT);
        return;
    }

    /* 手动设时页（两级操作）：
     *   导航态：键1/键2 移动高亮项（年份..分钟 / 返回 / 保存），键3 进入编辑或执行
     *   编辑态：键1/键2 调值（按住连发），键3 退出编辑（横杠消失） */
    if(app_screen == APP_SCREEN_TIME_SET)
    {
        uint32_t current_time = get_current_time();

        if(pressed & HAL_KEY_SW_1)
        {
            BUZZER_KeyBeep();
            time_set_step(UI_PENDING_TIME_SET, -1);
            key_repeat_time = get_current_time() + TIME_SET_REPEAT_FIRST_MS;
        }
        else if(pressed & HAL_KEY_SW_2)
        {
            BUZZER_KeyBeep();
            time_set_step(UI_PENDING_TIME_SET, 1);
            key_repeat_time = get_current_time() + TIME_SET_REPEAT_FIRST_MS;
        }

        if(pressed & HAL_KEY_SW_3)
        {
            if(current_time - k_s.timestamp_key3 > KEY_DEBOUNCE_MS)
            {
                BUZZER_KeyBeep();
                time_set_confirm();
            }
            k_s.timestamp_key3 = current_time;
        }

        tmos_set_event(appTaskID, UI_APPLY_EVT);
        return;
    }

    if(keys & HAL_KEY_SW_1){
        uint32_t current_time = get_current_time();
        if(current_time - k_s.timestamp_key1 < 600){
            if (notif_attr_app_id_latest.attr_id ==
                    BT_ANCS_NOTIF_ATTR_ID_APP_IDENTIFIER &&
                notif_attr_app_id_latest.attr_len != 0) {
                PRINT("Request for %s:\n",
                       notif_attr_app_id_latest.attr_data);
                err = bt_ancs_request_app_attr(
                    &ancs_c, notif_attr_app_id_latest.attr_data,
                    notif_attr_app_id_latest.attr_len,
                    bt_ancs_write_response_handler);
                if (err) {
                    PRINT("Failed requesting attributes for a given app (err: %d)\n",
                           err);
                }
            }
        } else {
            err = bt_ancs_request_attrs(&ancs_c, &notification_latest,
                            bt_ancs_write_response_handler);
            if(err){
                PRINT("Failed requesting attributes for a notification (err: %d)\n",
                          err);
            }
        }
        k_s.timestamp_key1 = current_time;
    }

    if(keys & HAL_KEY_SW_2){
        uint32_t current_time = get_current_time();
        if(current_time - k_s.timestamp_key2 > KEY_DEBOUNCE_MS){
            if(app_screen == APP_SCREEN_NOTIFICATION_DETAIL){
                app_screen = APP_SCREEN_CLOCK;
                notification_unread = notification_pending_during_detail ? 1 : 0;
                notification_pending_during_detail = 0;
                EPD_SetOverlayLock(0);
                clock_force_refresh = 1;
                if(clock_time_valid)
                {
                    clock_render(&clock_time_last);
                }
                else
                {
                    RX8010SJ_Time now;
                    if(RX8010SJ_ReadTime(&now))
                    {
                        clock_render(&now);
                    }
                }
            } else if(notification_unread) {
                app_screen = APP_SCREEN_NOTIFICATION_DETAIL;
                notification_pending_during_detail = 0;
                EPD_SetOverlayLock(1);
                EPD_Show_NotificationSilent((char *)epd_pending.appid,
                                            (char *)epd_pending.title,
                                            (char *)epd_pending.message);
            }
        }

        k_s.timestamp_key2 = current_time;
    }

    if(pressed & HAL_KEY_SW_3){
        uint32_t current_time = get_current_time();
        /* 时钟页短按键3 进入设置页；页面处于覆盖层（详情页/同步等待页）时不响应 */
        if(current_time - k_s.timestamp_key3 > KEY_DEBOUNCE_MS){
            if(app_screen == APP_SCREEN_CLOCK && !EPD_IsOverlayLocked())
            {
                BUZZER_KeyBeep();
                ui_pending = UI_PENDING_ENTER_SET_MENU;
                tmos_set_event(appTaskID, UI_APPLY_EVT);
            }
            else
            {
                PRINT("\n== Notification attrs dump ==\n");
                notif_attr_print(&notif_attr_latest);
            }
        }
        k_s.timestamp_key3 = current_time;
    }

    /* 蓝牙同步等待页没有增量刷新需求，但要支持长按 键3 返回选择页：
     * 这里启动 UI_APPLY_EVT 的按住检测循环（松开后循环自行结束）。 */
    if(key3_return_to_menu_available())
    {
        tmos_set_event(appTaskID, UI_APPLY_EVT);
    }
}

/*********************************************************************
 * @fn      Main_Circulation
 *
 * @brief   主循环
 *
 * @return  none
 */
__HIGH_CODE
__attribute__((noinline))
void Main_Circulation()
{
    while(1)
    {
        TMOS_SystemProcess();
    }
}

/*********************************************************************
 * @fn      main
 *
 * @brief   主函数
 *
 * @return  none
 */
int main(void)
{
#if(defined(DCDC_ENABLE)) && (DCDC_ENABLE == TRUE)
    PWR_DCDCCfg(ENABLE);
#endif
    SetSysClock(CLK_SOURCE_PLL_60MHz);
#if(defined(HAL_SLEEP)) && (HAL_SLEEP == TRUE)
    GPIOA_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
    GPIOB_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
#endif
#ifdef DEBUG
    GPIOA_SetBits(bTXD1);
    GPIOA_ModeCfg(bTXD1, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();
#endif
    PRINT("%s\n", VER_LIB);
    CH59x_BLEInit();
    HAL_Init();
    appTaskID = TMOS_ProcessEventRegister(Main_ProcessEvent);
    TimeSync_Init(appTaskID);

    /* 初始化墨水屏并清屏（约 2 秒） */
    EPD_Show_Init();

    /* 读取掉电保存的用户设置（蜂鸣器开关），必须在任何提示音之前完成 */
    settings_load();

    {
        RX8010SJ_Time start_time;

        if(RX8010SJ_Begin(&start_time))
        {
            clock_time_last = start_time;
            clock_time_valid = 1;
            clock_force_refresh = 0;
            clock_error_shown = 0;
            PRINT("RX8010SJ time %04d-%02d-%02d %02d:%02d:%02d\n",
                  start_time.year, start_time.month, start_time.day,
                  start_time.hour, start_time.minute, start_time.second);
        }
        else
        {
            /* 不再在初始化阶段覆盖屏幕：随后进入的开机选择页会接管显示 */
            clock_error_shown = 1;
            PRINT("RX8010SJ init fail\n");
        }

        /* 每次上电先停在时间设定选择页：手动设时，或选择蓝牙同步 */
        boot_menu_show(1);

        /* 时钟轮询改为一次性任务自我重排期（见 clock_tick_reschedule），
         * 以便在分钟翻转前切换成细粒度轮询、压掉显示滞后。 */
        tmos_start_task(appTaskID, CLOCK_TICK_EVT, MS1_TO_SYSTEM_TIME(CLOCK_TICK_NORMAL_MS));
    }

    HalKeyConfig(button_changed);
    ancs_init();
    ancs_subscribe_cb_register(&subscribe);
    GAPRole_PeripheralInit();
    peripheral_ancs_client_init();
    Main_Circulation();
}

/******************************** endfile @ main ******************************/
