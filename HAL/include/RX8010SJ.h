#ifndef __RX8010SJ_H__
#define __RX8010SJ_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "CH59x_common.h"

/* RTC 后端选择：
 *   1 = 外部 RTC 芯片 RX8010SJ（I2C，默认）
 *   0 = 使用 MCU 片内 RTC（32.768kHz LSE 计数）
 * 上层统一调用 RX8010SJ_* 接口，切换只需改这一个宏。 */
#ifndef USE_EXT_RTC
#define USE_EXT_RTC    1
#endif

typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t weekday; /* 0 = Sunday, 6 = Saturday */
} RX8010SJ_Time;

void RX8010SJ_Init(void);
uint8_t RX8010SJ_Begin(RX8010SJ_Time *time);
uint8_t RX8010SJ_ReadTime(RX8010SJ_Time *time);
uint8_t RX8010SJ_SetTime(const RX8010SJ_Time *time);

#ifdef __cplusplus
}
#endif

#endif /* __RX8010SJ_H__ */
