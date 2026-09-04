/********************************** (C) COPYRIGHT *******************************
 * File Name          : BUZZER.h
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2026/08/20
 * Description        : 无源蜂鸣器驱动（PB4 -> AO3400 -> 蜂鸣器），使用 PWM7 输出
 *********************************************************************************/

#ifndef __BUZZER_H
#define __BUZZER_H

#ifdef __cplusplus
extern "C" {
#endif

/* 蜂鸣器引脚：PB4 对应 PWM7 通道 */
#define BUZZER_PWM_CH       CH_PWM7

/**
 * @brief   初始化蜂鸣器（配置 PB4 为 PWM7 输出）
 */
void BUZZER_Init(void);

/**
 * @brief   蜂鸣器响一段时间
 *
 * @param   ms  - 鸣响时长(ms)
 */
void BUZZER_On(uint16_t ms);

/**
 * @brief   蜂鸣器静音
 */
void BUZZER_Off(void);

/**
 * @brief   彻底停止蜂鸣器：关闭 PWM 并复位"滴滴"状态机
 *          （用于屏幕刷新等阻塞操作前，避免 PWM 冻结在开启状态）
 */
void BUZZER_Stop(void);

/**
 * @brief   收到消息通知时提醒：发出"滴滴"两声短促提示
 *          （鸣 100ms - 停 120ms - 鸣 100ms，使用 TMOS 定时任务，不阻塞主流程）
 */
void BUZZER_NotifyBeep(void);

#ifdef __cplusplus
}
#endif

#endif /* __BUZZER_H */
