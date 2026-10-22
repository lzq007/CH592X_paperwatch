/********************************** (C) COPYRIGHT *******************************
 * File Name          : BUZZER.c
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2026/08/20
 * Description        : 无源蜂鸣器驱动（PB4 -> AO3400 -> 蜂鸣器）
 *
 *                      硬件连接：PB4(PWM7) -- AO3400 MOS 栅极 -- 无源蜂鸣器
 *                      原理：PWM7 输出 ~2.7kHz 方波，经 AO3400 放大驱动蜂鸣器发声
 *********************************************************************************/

/******************************************************************************/
/* 头文件包含 */
#include "HAL.h"
#include "BUZZER.h"

/* 蜂鸣器事件：用于 TMOS 定时调度"滴滴"两声 */
#define BUZZER_BEEP_EVENT   0x0001

/* 鸣响时间与间隔（ms） */
#define BUZZER_SHORT_MS     100    /* 每声鸣响时长 */
#define BUZZER_GAP_MS       120    /* 两声之间间隔 */
#define BUZZER_TIMES        2      /* "滴滴"两声 */

/* 蜂鸣器任务 ID */
static tmosTaskID buzzerTaskID;

/* 剩余鸣响次数 */
static uint8_t buzzerBeepLeft;

/* 当前阶段：0=鸣响，1=静音间隔，2=收尾 */
static uint8_t buzzerStage;

/* 是否有鸣响序列正在进行（含静音间隔）。阻塞式墨水屏刷新会推迟 PWM 关断、
 * 把"滴"声拉长，调用方可据此把刷新推迟到鸣响结束。 */
static volatile uint8_t buzzerActive;

/* 蜂鸣器 TMOS 事件处理 */
static tmosEvents BUZZER_ProcessEvent(tmosTaskID task_id, tmosEvents events);

/*******************************************************************************
 * @fn      BUZZER_ProcessEvent
 *
 * @brief   蜂鸣器 TMOS 事件处理：交替"鸣响"与"静音间隔"，完成"滴滴"两声
 *
 *          序列：鸣(100ms) → 停(120ms) → 鸣(100ms) → 停(120ms) → 结束
 *          用 buzzerBeepLeft 记录剩余鸣响次数，>0 时鸣响，==0 时静音间隔；
 *          静音间隔后若还有剩余轮次则继续，否则彻底停止。
 *
 * @param   task_id - TMOS task ID
 * @param   events  - events bitmap
 *
 * @return  events not processed
 *******************************************************************************/
static tmosEvents BUZZER_ProcessEvent(tmosTaskID task_id, tmosEvents events)
{
    if(events & BUZZER_BEEP_EVENT)
    {
        if(buzzerStage == 0)
        {
            /* 鸣响阶段：打开 PWM 发声 */
            PWMX_ACTOUT(BUZZER_PWM_CH, 128, High_Level, ENABLE);
            if(buzzerBeepLeft > 0)
            {
                buzzerBeepLeft--;
            }
            if(buzzerBeepLeft > 0)
            {
                /* 还有下一声，先静音间隔 */
                buzzerStage = 1;
                tmos_start_task(buzzerTaskID, BUZZER_BEEP_EVENT, MS1_TO_SYSTEM_TIME(BUZZER_SHORT_MS));
            }
            else
            {
                /* 全部鸣响完毕，最后再静音收尾 */
                buzzerStage = 2;
                tmos_start_task(buzzerTaskID, BUZZER_BEEP_EVENT, MS1_TO_SYSTEM_TIME(BUZZER_SHORT_MS));
            }
        }
        else if(buzzerStage == 1)
        {
            /* 静音间隔阶段：关闭 PWM，切回鸣响 */
            PWMX_ACTOUT(BUZZER_PWM_CH, 128, High_Level, DISABLE);
            buzzerStage = 0;
            tmos_start_task(buzzerTaskID, BUZZER_BEEP_EVENT, MS1_TO_SYSTEM_TIME(BUZZER_GAP_MS));
        }
        else
        {
            /* 收尾：关闭 PWM，彻底结束 */
            PWMX_ACTOUT(BUZZER_PWM_CH, 128, High_Level, DISABLE);
            buzzerStage = 0;
            buzzerActive = 0;
        }
        return (events ^ BUZZER_BEEP_EVENT);
    }
    return 0;
}

/*******************************************************************************
 * @fn      BUZZER_Init
 *
 * @brief   初始化蜂鸣器：配置 PWM7 时钟与占空比，注册 TMOS 任务
 *
 * @param   None.
 *
 * @return  None.
 *******************************************************************************/
void BUZZER_Init(void)
{
    /* 注册 TMOS 任务，用于"滴滴"两声的定时调度 */
    buzzerTaskID = TMOS_ProcessEventRegister(BUZZER_ProcessEvent);
    buzzerBeepLeft = 0;
    buzzerStage = 0;
    buzzerActive = 0;

    /* 配置 PWM7 输出：频率 ~2.7kHz
     * 系统时钟 60MHz，PWM 基准时钟 = d * Tsys = 85 * (1/60MHz)
     * PWM 周期 = 256 * 85 * (1/60MHz) ≈ 362us  => 频率 ≈ 2758Hz
     */
    PWMX_CLKCfg(85);
    PWMX_CycleCfg(PWMX_Cycle_256);

    /* PB4 配置为 PWM7 推挽输出 */
    GPIOB_ModeCfg(GPIO_Pin_4, GPIO_ModeOut_PP_5mA);

    /* 默认静音 */
    PWMX_ACTOUT(BUZZER_PWM_CH, 128, High_Level, DISABLE);
}

/*******************************************************************************
 * @fn      BUZZER_On
 *
 * @brief   蜂鸣器持续鸣响
 *
 * @param   ms - 鸣响时长(ms)，0 表示持续鸣响直到调用 BUZZER_Off
 *
 * @return  None.
 *******************************************************************************/
void BUZZER_On(uint16_t ms)
{
    PWMX_ACTOUT(BUZZER_PWM_CH, 128, High_Level, ENABLE);
    if(ms)
    {
        tmos_start_task(buzzerTaskID, BUZZER_BEEP_EVENT, MS1_TO_SYSTEM_TIME(ms));
    }
}

/*******************************************************************************
 * @fn      BUZZER_Off
 *
 * @brief   蜂鸣器静音
 *
 * @param   None.
 *
 * @return  None.
 *******************************************************************************/
void BUZZER_Off(void)
{
    PWMX_ACTOUT(BUZZER_PWM_CH, 128, High_Level, DISABLE);
}

/*******************************************************************************
 * @fn      BUZZER_Stop
 *
 * @brief   彻底停止蜂鸣器：关闭 PWM 并复位"滴滴"状态机，取消挂起的事件
 *          （用于屏幕刷新等阻塞操作前，避免 PWM 冻结在开启状态）
 *
 * @param   None.
 *
 * @return  None.
 *******************************************************************************/
void BUZZER_Stop(void)
{
    PWMX_ACTOUT(BUZZER_PWM_CH, 128, High_Level, DISABLE);
    buzzerBeepLeft = 0;
    buzzerStage = 0;
    buzzerActive = 0;
    tmos_stop_task(buzzerTaskID, BUZZER_BEEP_EVENT);
}

/*******************************************************************************
 * @fn      BUZZER_NotifyBeep
 *
 * @brief   消息通知提醒：发出"滴滴"两声短促提示
 *
 * @param   None.
 *
 * @return  None.
 *******************************************************************************/
void BUZZER_NotifyBeep(void)
{
    /* 两声：鸣-停-鸣-停 */
    BUZZER_Stop();
    buzzerBeepLeft = BUZZER_TIMES;
    buzzerStage = 0;
    buzzerActive = 1;
    tmos_start_task(buzzerTaskID, BUZZER_BEEP_EVENT, 0);
}

uint8_t BUZZER_IsBeeping(void)
{
    return buzzerActive;
}

/******************************** endfile @ BUZZER ******************************/
