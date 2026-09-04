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
static uint8_t attr_message_size[ATTR_DATA_SIZE];
static uint8_t attr_date[ATTR_DATA_SIZE];
static uint8_t attr_posaction[ATTR_DATA_SIZE];
static uint8_t attr_negaction[ATTR_DATA_SIZE];
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

#define CLOCK_POLL_MS           1000
#define TIME_SYNC_STATUS_MS      1500

typedef enum
{
    APP_SCREEN_CLOCK = 0,
    APP_SCREEN_NOTIFICATION_DETAIL = 1,
} app_screen_t;

static tmosTaskID appTaskID;
static volatile uint8_t clock_force_refresh = 1;
static volatile uint8_t clock_force_partial_once = 0;
static volatile uint8_t clock_refresh_blocked = 0;
static volatile uint8_t notification_beep_pending = 0;
static app_screen_t app_screen = APP_SCREEN_CLOCK;
static uint8_t notification_unread = 0;
static uint8_t notification_pending_during_detail = 0;
/* 消息图标是否已显示：通知未读且图标未显示时，CLOCK_TICK 会持续重试刷新，
 * 避免通知到达时覆盖层锁定/详情页导致图标刷新被跳过、直到分钟变化才恢复。 */
static uint8_t msg_icon_shown = 0;
static uint8_t clock_time_valid = 0;
static uint8_t clock_error_shown = 0;
static RX8010SJ_Time clock_time_last;
static volatile uint8_t time_sync_session_active = 0;
static volatile uint8_t time_sync_restore_to_waiting = 0;
static volatile uint8_t time_sync_status_visible = 0;

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

    clock_time_last = *now;
    clock_time_valid = 1;
    clock_force_refresh = 0;
    clock_error_shown = 0;
}

static void time_sync_begin_session(void)
{
    tmos_stop_task(appTaskID, TIME_SYNC_RESTORE_EVT);
    time_sync_session_active = 1;
    time_sync_restore_to_waiting = 0;
    time_sync_status_visible = 1;
    TimeSync_SetWindowOpen(1);
    EPD_SetOverlayLock(1);
    EPD_Show_TimeSyncWaiting();
}

static void time_sync_show_clock(void)
{
    RX8010SJ_Time now;

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
    /* 强制下一次时钟 tick 立即局部刷新，确保消息图标（右上角）及时出现，
     * 而不必等到分钟变化或其它刷新条件。 */
    if(app_screen == APP_SCREEN_CLOCK && !EPD_IsOverlayLocked())
    {
        clock_force_partial_once = 1;
    }
    tmos_set_event(appTaskID, EPD_REFRESH_EVT);
}

static tmosEvents Main_ProcessEvent(tmosTaskID task_id, tmosEvents events)
{
    if (events & EPD_REFRESH_EVT)
    {
        /* 收到通知时立即独立刷新右上角消息图标（不重绘整个时钟，刷新快）。
         * 仅在时钟界面时刷新：EPD_UpdateClockMessageIcon 直接操作 EPD_Image，
         * 若在详情页刷新会破坏详情页帧。 */
        if (notification_unread && app_screen == APP_SCREEN_CLOCK &&
            !time_sync_session_active && !time_sync_restore_to_waiting &&
            !time_sync_status_visible && !EPD_IsOverlayLocked())
        {
            EPD_UpdateClockMessageIcon(1);
            msg_icon_shown = 1;
        }

        if (time_sync_session_active || time_sync_restore_to_waiting || time_sync_status_visible)
        {
            if (notification_beep_pending)
            {
                notification_beep_pending = 0;
                BUZZER_NotifyBeep();
            }
            return events ^ EPD_REFRESH_EVT;
        }

        if (ancs_pairing_success_refresh_pending)
        {
            app_screen = APP_SCREEN_CLOCK;
            clock_force_partial_once = 1;
            clock_force_refresh = 0;
        }

        if (app_screen == APP_SCREEN_CLOCK &&
            (notification_unread || ancs_pairing_success_refresh_pending))
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
                EPD_Show_TimeSyncSyncing();

                if(RX8010SJ_SetTime(&req.time))
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
            else if(req.type == TIME_SYNC_REQ_INVALID)
            {
                EPD_SetOverlayLock(1);
                time_sync_status_visible = 1;
                EPD_Show_TimeSyncFail();
                if(time_sync_session_active)
                {
                    time_sync_schedule_restore(1);
                }
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

    if (events & CLOCK_TICK_EVT)
    {
        RX8010SJ_Time now;

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
            return events ^ CLOCK_TICK_EVT;
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
            return events ^ CLOCK_TICK_EVT;
        }

        clock_error_shown = 0;

        if (clock_refresh_blocked)
        {
            clock_force_refresh = 1;
            clock_refresh_blocked = 0;
        }

        if (app_screen == APP_SCREEN_CLOCK &&
            (clock_force_partial_once || clock_force_refresh || clock_time_changed(&now) ||
             ancs_pairing_success_refresh_pending))
        {
            clock_render(&now);
            if (ancs_pairing_success_refresh_pending)
            {
                ancs_pairing_success_refresh_pending = 0;
            }
        }

        return events ^ CLOCK_TICK_EVT;
    }

    return 0;
}




static void notif_print(struct bt_ancs_evt_notif *notif)
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
        notification_latest = *notif;
        notif_print(&notification_latest);

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

        notification_unread = 1;
        notification_pending_during_detail =
            (uint8_t)(app_screen == APP_SCREEN_NOTIFICATION_DETAIL);
        notification_beep_pending = 1;
        /* 新通知到达，清空旧内容防止详情页显示残留 */
        epd_pending.appid[0] = '\0';
        epd_pending.title[0] = '\0';
        epd_pending.message[0] = '\0';
        /* 图标未显示，等待时钟 tick 重试刷新 */
        msg_icon_shown = 0;
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
        if(current_time - k_s.timestamp_key2 > 100){
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

    if(keys & HAL_KEY_SW_3){
        uint32_t current_time = get_current_time();
        /* 简单防抖，忽略 100ms 内的重复触发 */
        if(current_time - k_s.timestamp_key3 > 100){
            PRINT("\n== Notification attrs dump ==\n");
            notif_attr_print(&notif_attr_latest);
        }
        k_s.timestamp_key3 = current_time;
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
            time_sync_begin_session();
        }
        else
        {
            EPD_Show_Notification("Clock", "RX8010SJ", "RTC init fail");
            clock_error_shown = 1;
            PRINT("RX8010SJ init fail\n");
        }

        tmos_start_reload_task(appTaskID, CLOCK_TICK_EVT, MS1_TO_SYSTEM_TIME(CLOCK_POLL_MS));
    }

    HalKeyConfig(button_changed);
    ancs_init();
    ancs_subscribe_cb_register(&subscribe);
    GAPRole_PeripheralInit();
    peripheral_ancs_client_init();
    Main_Circulation();
}

/******************************** endfile @ main ******************************/
