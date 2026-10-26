#ifndef TIMESYNCSERVICE_H
#define TIMESYNCSERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "CH59xBLE_LIB.h"
#include "RX8010SJ.h"

#define TIME_SYNC_EVT                 0x0004

#define TIME_SYNC_SERVICE_UUID        0xFFF0
#define TIME_SYNC_CHAR_UUID           0xFFF1
/* 只读状态特征：1=已进入等待蓝牙同步页面（可写入时间），0=不可写入 */
#define TIME_SYNC_STATE_UUID          0xFFF2

#define TIME_SYNC_PACKET_LEN          8

typedef enum
{
    TIME_SYNC_REQ_NONE = 0,
    TIME_SYNC_REQ_TIME,
    TIME_SYNC_REQ_INVALID,
} time_sync_request_type_t;

typedef struct
{
    time_sync_request_type_t type;
    RX8010SJ_Time time;
} time_sync_request_t;

void TimeSync_Init(tmosTaskID task_id);
void TimeSync_SetWindowOpen(uint8_t open);
uint8_t TimeSync_PopRequest(time_sync_request_t *req);
bStatus_t TimeSync_AddService(void);

#ifdef __cplusplus
}
#endif

#endif /* TIMESYNCSERVICE_H */
