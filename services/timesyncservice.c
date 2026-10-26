#include "CONFIG.h"
#include "timesyncservice.h"
#include "time_units.h"

static const uint8_t timeSyncServUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(TIME_SYNC_SERVICE_UUID), HI_UINT16(TIME_SYNC_SERVICE_UUID)
};

static const uint8_t timeSyncCharUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(TIME_SYNC_CHAR_UUID), HI_UINT16(TIME_SYNC_CHAR_UUID)
};

static const uint8_t timeSyncStateUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(TIME_SYNC_STATE_UUID), HI_UINT16(TIME_SYNC_STATE_UUID)
};

static uint8_t timeSyncStateProps = GATT_PROP_READ;
static uint8_t timeSyncStateValue = 0;

static const gattAttrType_t timeSyncService = {
    ATT_BT_UUID_SIZE, (uint8_t *)timeSyncServUUID
};

static uint8_t timeSyncCharProps = GATT_PROP_WRITE | GATT_PROP_WRITE_NO_RSP;
static uint8_t timeSyncCharValue[TIME_SYNC_PACKET_LEN];

static gattAttribute_t timeSyncAttrTbl[] = {
    {
        {ATT_BT_UUID_SIZE, primaryServiceUUID},
        GATT_PERMIT_READ,
        0,
        (uint8_t *)&timeSyncService
    },
    {
        {ATT_BT_UUID_SIZE, characterUUID},
        GATT_PERMIT_READ,
        0,
        &timeSyncCharProps
    },
    {
        {ATT_BT_UUID_SIZE, timeSyncCharUUID},
        GATT_PERMIT_WRITE,
        0,
        timeSyncCharValue
    },
    {
        {ATT_BT_UUID_SIZE, characterUUID},
        GATT_PERMIT_READ,
        0,
        &timeSyncStateProps
    },
    {
        {ATT_BT_UUID_SIZE, timeSyncStateUUID},
        GATT_PERMIT_READ,
        0,
        &timeSyncStateValue
    },
};

static tmosTaskID timeSyncTaskID;
static volatile uint8_t timeSyncWindowOpen;
static volatile uint8_t timeSyncPendingValid;
static time_sync_request_t timeSyncPending;

static uint8_t time_sync_is_late(void)
{
    return (uint8_t)(!timeSyncWindowOpen);
}

static void time_sync_queue_request(time_sync_request_type_t type,
                                    const RX8010SJ_Time *time)
{
    timeSyncPending.type = type;
    if(time)
    {
        timeSyncPending.time = *time;
    }
    timeSyncPendingValid = 1;

    if(timeSyncTaskID)
    {
        tmos_set_event(timeSyncTaskID, TIME_SYNC_EVT);
    }
}

static uint8_t TimeSync_WriteAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                                    uint8_t *pValue, uint16_t len, uint16_t offset,
                                    uint8_t method)
{
    RX8010SJ_Time time;
    uint16_t year;
    time_sync_request_type_t type;

    (void)connHandle;
    (void)pAttr;
    (void)method;

    /* 只有停在「等待蓝牙广播时间同步」页面时才允许写入时间。
     * 否则直接返回 WRITE_NOT_PERMITTED，手机端会收到失败，
     * 而不是"写入成功但固件悄悄丢弃"这种误导性结果。 */
    if(time_sync_is_late())
    {
        PRINT("timesync: rejected, window closed\n");
        return ATT_ERR_WRITE_NOT_PERMITTED;
    }

    if(offset != 0)
    {
        time_sync_queue_request(TIME_SYNC_REQ_INVALID, NULL);
        return ATT_ERR_INVALID_OFFSET;
    }

    if(len != TIME_SYNC_PACKET_LEN)
    {
        time_sync_queue_request(TIME_SYNC_REQ_INVALID, NULL);
        return ATT_ERR_INVALID_VALUE_SIZE;
    }

    year = BUILD_UINT16(pValue[0], pValue[1]);
    time.year = year;
    time.month = pValue[2];
    time.day = pValue[3];
    time.hour = pValue[4];
    time.minute = pValue[5];
    time.second = pValue[6];
    time.weekday = pValue[7];
    type = TIME_SYNC_REQ_TIME;

    time_sync_queue_request(type, &time);
    return SUCCESS;
}

static gattServiceCBs_t timeSyncCBs = {
    NULL,
    TimeSync_WriteAttrCB,
    NULL
};

void TimeSync_Init(tmosTaskID task_id)
{
    timeSyncTaskID = task_id;
}

void TimeSync_SetWindowOpen(uint8_t open)
{
    uint8_t value = open ? 1 : 0;

    if(value == timeSyncWindowOpen)
    {
        return;
    }

    timeSyncWindowOpen = value;
    /* 同步状态特征，手机端可主动读取判断当前能否写入时间 */
    timeSyncStateValue = value;
}

uint8_t TimeSync_PopRequest(time_sync_request_t *req)
{
    if(!req || !timeSyncPendingValid)
    {
        return 0;
    }

    *req = timeSyncPending;
    timeSyncPendingValid = 0;
    return 1;
}

bStatus_t TimeSync_AddService(void)
{
    return GATTServApp_RegisterService(timeSyncAttrTbl,
                                       GATT_NUM_ATTRS(timeSyncAttrTbl),
                                       7,
                                       &timeSyncCBs);
}
