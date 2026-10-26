/********************************** (C) COPYRIGHT *******************************
 * File Name          : KEY.c
 * Author             : WCH
 * Version            : V1.2
 * Date               : 2022/01/18
 * Description        :
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/******************************************************************************/
/* ͷ�ļ����� */
#include "HAL.h"

/**************************************************************************************************
 *                                        GLOBAL VARIABLES
 **************************************************************************************************/

static uint8_t halKeySavedKeys; /* 上一次采样到的按键状态，用于查询是否有键值变化 */

/* 中断里只置标志，实际处理仍在 TMOS 任务上下文执行：
 * 按键回调包含 30ms 阻塞鸣响和墨水屏刷屏，绝不能放在中断里。 */
static volatile uint8_t halKeyIrqPending;

/**************************************************************************************************
 *                                        FUNCTIONS - Local
 **************************************************************************************************/
static halKeyCBack_t pHalKeyProcessFunction; /* callback function */

/**************************************************************************************************
 * @fn      HAL_KeyInit
 *
 * @brief   Initilize Key Service
 *
 * @param   none
 *
 * @return  None
 **************************************************************************************************/
void HAL_KeyInit(void)
{
    /* Initialize previous key to 0 */
    halKeySavedKeys = 0;
    halKeyIrqPending = 0;
    /* Initialize callback function */
    pHalKeyProcessFunction = NULL;
    /* 三个按键配置为浮空输入（外置上拉电阻，按下低电平）：
     * GPIO_ModeIN_Floating = 关闭内部上拉/下拉 + 方向输入 */
    GPIOB_ModeCfg(GPIO_Pin_22, GPIO_ModeIN_Floating);  /* KEY1: PB22 */
    GPIOB_ModeCfg(GPIO_Pin_13, GPIO_ModeIN_Floating);  /* KEY2: PB13 */
    GPIOB_ModeCfg(GPIO_Pin_12, GPIO_ModeIN_Floating);  /* KEY3: PB12 */

    /* 改为边沿中断触发（按下为上升沿），不再靠 100ms 轮询。
     * 轮询会在阻塞刷屏期间漏采按键；中断不受刷屏影响。 */
    GPIOB_ITModeCfg(GPIO_Pin_22 | GPIO_Pin_13 | GPIO_Pin_12,
                    GPIO_ITMode_RiseEdge);
    GPIOB_ClearITFlagBit(GPIO_Pin_22 | GPIO_Pin_13 | GPIO_Pin_12);
    PFIC_EnableIRQ(GPIO_B_IRQn);
}

/**************************************************************************************************
 * @fn      HalKeyConfig
 *
 * @brief   Configure the Key serivce
 *
 * @param   cback - pointer to the CallBack function
 *
 * @return  None
 **************************************************************************************************/
void HalKeyConfig(halKeyCBack_t cback)
{
    /* Register the callback fucntion */
    pHalKeyProcessFunction = cback;

    /* 松开检测仍需周期任务：边沿中断只能告诉我们"按下了"，
     * 知道"已松开"要靠采样。周期放宽到 50ms，只读 IO 不做别的事。 */
    tmos_start_task(halTaskID, HAL_KEY_EVENT, HAL_KEY_POLLING_VALUE);
}

/**************************************************************************************************
 * GPIOB 中断：按键按下（上升沿）时置标志并投递任务事件
 **************************************************************************************************/
__INTERRUPT
void GPIOB_IRQHandler(void)
{
    uint32_t flags = GPIOB_ReadITFlagBit(GPIO_Pin_22 | GPIO_Pin_13 | GPIO_Pin_12);

    if(flags)
    {
        GPIOB_ClearITFlagBit(flags);
        halKeyIrqPending = 1;
        /* 中断里只投递 TMOS 事件，由任务上下文执行按键回调 */
        tmos_set_event(halTaskID, HAL_KEY_IRQ_EVENT);
    }
}

/**************************************************************************************************
 * @fn      HalKeyRead
 *
 * @brief   Read the current value of a key
 *
 * @param   None
 *
 * @return  keys - current keys status
 **************************************************************************************************/
uint8_t HalKeyRead(void)
{
    uint8_t keys = 0;

    if(HAL_PUSH_BUTTON1())
    { //������1
        keys |= HAL_KEY_SW_1;
    }
    if(HAL_PUSH_BUTTON2())
    { //������1
        keys |= HAL_KEY_SW_2;
    }
    if(HAL_PUSH_BUTTON3())
    { //������1
        keys |= HAL_KEY_SW_3;
    }
    if(HAL_PUSH_BUTTON4())
    { //������1
        keys |= HAL_KEY_SW_4;
    }
    return keys;
}

/**************************************************************************************************
 * @fn      HAL_KeyPoll
 *
 * @brief   Called by hal_driver to poll the keys
 *
 * @param   None
 *
 * @return  None
 **************************************************************************************************/
/**************************************************************************************************
 * 处理中断投递的按键事件：读一次当前状态并回调（与轮询共用去重逻辑）
 **************************************************************************************************/
void HAL_KeyDispatch(void)
{
    uint8_t keys = 0;
    if(HAL_PUSH_BUTTON1())
    {
        keys |= HAL_KEY_SW_1;
    }
    if(HAL_PUSH_BUTTON2())
    {
        keys |= HAL_KEY_SW_2;
    }
    if(HAL_PUSH_BUTTON3())
    {
        keys |= HAL_KEY_SW_3;
    }
    if(HAL_PUSH_BUTTON4())
    {
        keys |= HAL_KEY_SW_4;
    }

    halKeyIrqPending = 0;

    if(keys && (pHalKeyProcessFunction))
    {
        (pHalKeyProcessFunction)(keys);
    }
}

uint8_t HAL_KeyIrqPending(void)
{
    return halKeyIrqPending;
}

void HAL_KeyPoll(void)
{
    uint8_t keys = 0;
    if(HAL_PUSH_BUTTON1())
    {
        keys |= HAL_KEY_SW_1;
    }
    if(HAL_PUSH_BUTTON2())
    {
        keys |= HAL_KEY_SW_2;
    }
    if(HAL_PUSH_BUTTON3())
    {
        keys |= HAL_KEY_SW_3;
    }
    if(HAL_PUSH_BUTTON4())
    {
        keys |= HAL_KEY_SW_4;
    }
    if(keys == halKeySavedKeys)
    { /* Exit - since no keys have changed */
        return;
    }
    halKeySavedKeys = keys; /* Store the current keys for comparation next time */
    /* Invoke Callback if new keys were depressed */
    if(keys && (pHalKeyProcessFunction))
    {
        (pHalKeyProcessFunction)(keys);
    }
}

/******************************** endfile @ key ******************************/
