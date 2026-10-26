#include "RX8010SJ.h"

#if USE_EXT_RTC

#include "CH59x_gpio.h"

#define RX8010SJ_I2C_ADDR_DEFAULT 0x32

#define RX8010SJ_REG_SEC          0x10
#define RX8010SJ_REG_MIN          0x11
#define RX8010SJ_REG_HOUR         0x12
#define RX8010SJ_REG_WDAY         0x13
#define RX8010SJ_REG_MDAY         0x14
#define RX8010SJ_REG_MONTH        0x15
#define RX8010SJ_REG_YEAR         0x16
#define RX8010SJ_REG_RESV17       0x17
#define RX8010SJ_REG_RESV30       0x30
#define RX8010SJ_REG_RESV31       0x31
#define RX8010SJ_REG_IRQ          0x32
#define RX8010SJ_REG_FLAG         0x1E
#define RX8010SJ_REG_CTRL         0x1F

#define RX8010SJ_FLAG_VLF         0x02
#define RX8010SJ_FLAG_AF          0x08
#define RX8010SJ_FLAG_TF          0x10
#define RX8010SJ_FLAG_UF          0x20

#define RX8010SJ_CTRL_AIE         0x08
#define RX8010SJ_CTRL_UIE         0x20
#define RX8010SJ_CTRL_STOP        0x40
#define RX8010SJ_CTRL_TEST        0x80

/*
 * Board wiring:
 *   SDA -> PB15
 *   SCL -> PB14
 * These pins are driven with software I2C because CH592 hardware I2C does
 * not map to PB14/PB15.
 */
#define RX8010SJ_SDA_PIN_DEFAULT  GPIO_Pin_15
#define RX8010SJ_SCL_PIN_DEFAULT  GPIO_Pin_14
#define RX8010SJ_I2C_DELAY_US     8

typedef struct
{
    uint32_t sda_pin;
    uint32_t scl_pin;
} rx8010_bus_config_t;

static const uint8_t rx8010_addr_candidates[] = {0x32, 0x19};
static const rx8010_bus_config_t rx8010_bus_candidates[] = {
    {GPIO_Pin_15, GPIO_Pin_14},
};

static uint8_t rx8010_i2c_addr = RX8010SJ_I2C_ADDR_DEFAULT;
static uint32_t rx8010_sda_pin = RX8010SJ_SDA_PIN_DEFAULT;
static uint32_t rx8010_scl_pin = RX8010SJ_SCL_PIN_DEFAULT;

static void rx8010_delay(void)
{
    DelayUs(RX8010SJ_I2C_DELAY_US);
}

static void rx8010_sda_release(void)
{
    GPIOB_SetBits(rx8010_sda_pin);
    GPIOB_ModeCfg(rx8010_sda_pin, GPIO_ModeIN_PU);
    GPIOB_PinCfg(rx8010_sda_pin, ENABLE);
}

static void rx8010_sda_low(void)
{
    GPIOB_ResetBits(rx8010_sda_pin);
    GPIOB_ModeCfg(rx8010_sda_pin, GPIO_ModeOut_PP_5mA);
    GPIOB_PinCfg(rx8010_sda_pin, ENABLE);
}

static void rx8010_scl_release(void)
{
    GPIOB_SetBits(rx8010_scl_pin);
    GPIOB_ModeCfg(rx8010_scl_pin, GPIO_ModeIN_PU);
    GPIOB_PinCfg(rx8010_scl_pin, ENABLE);
}

static void rx8010_scl_low(void)
{
    GPIOB_ResetBits(rx8010_scl_pin);
    GPIOB_ModeCfg(rx8010_scl_pin, GPIO_ModeOut_PP_5mA);
    GPIOB_PinCfg(rx8010_scl_pin, ENABLE);
}

static uint8_t rx8010_sda_read(void)
{
    return GPIOB_ReadPortPin(rx8010_sda_pin) ? 1 : 0;
}

static void rx8010_i2c_start(void)
{
    rx8010_sda_release();
    rx8010_scl_release();
    rx8010_delay();
    rx8010_sda_low();
    rx8010_delay();
    rx8010_scl_low();
}

static void rx8010_i2c_stop(void)
{
    rx8010_sda_low();
    rx8010_delay();
    rx8010_scl_release();
    rx8010_delay();
    rx8010_sda_release();
    rx8010_delay();
}

static uint8_t rx8010_i2c_write_byte(uint8_t data)
{
    uint8_t i;

    for(i = 0; i < 8; i++)
    {
        if(data & 0x80)
        {
            rx8010_sda_release();
        }
        else
        {
            rx8010_sda_low();
        }

        rx8010_delay();
        rx8010_scl_release();
        rx8010_delay();
        rx8010_scl_low();
        data <<= 1;
    }

    rx8010_sda_release();
    rx8010_delay();
    rx8010_scl_release();
    rx8010_delay();
    i = rx8010_sda_read() ? 0 : 1;
    rx8010_scl_low();
    return i;
}

static uint8_t rx8010_i2c_read_byte(uint8_t ack)
{
    uint8_t i;
    uint8_t data = 0;

    rx8010_sda_release();

    for(i = 0; i < 8; i++)
    {
        data <<= 1;
        rx8010_delay();
        rx8010_scl_release();
        rx8010_delay();
        if(rx8010_sda_read())
        {
            data |= 0x01;
        }
        rx8010_scl_low();
    }

    if(ack)
    {
        rx8010_sda_low();
    }
    else
    {
        rx8010_sda_release();
    }

    rx8010_delay();
    rx8010_scl_release();
    rx8010_delay();
    rx8010_scl_low();
    rx8010_sda_release();

    return data;
}

static uint8_t rx8010_i2c_ping(void)
{
    uint8_t ack;

    rx8010_i2c_start();
    ack = rx8010_i2c_write_byte((uint8_t)(rx8010_i2c_addr << 1));
    rx8010_i2c_stop();

    return ack;
}

static uint8_t rx8010_write_regs(uint8_t reg, const uint8_t *data, uint8_t len)
{
    uint8_t i;

    rx8010_i2c_start();
    if(!rx8010_i2c_write_byte((uint8_t)(rx8010_i2c_addr << 1)))
    {
        rx8010_i2c_stop();
        return 0;
    }
    if(!rx8010_i2c_write_byte(reg))
    {
        rx8010_i2c_stop();
        return 0;
    }

    for(i = 0; i < len; i++)
    {
        if(!rx8010_i2c_write_byte(data[i]))
        {
            rx8010_i2c_stop();
            return 0;
        }
    }

    rx8010_i2c_stop();
    return 1;
}

static uint8_t rx8010_read_regs(uint8_t reg, uint8_t *data, uint8_t len)
{
    uint8_t i;

    rx8010_i2c_start();
    if(!rx8010_i2c_write_byte((uint8_t)(rx8010_i2c_addr << 1)))
    {
        rx8010_i2c_stop();
        return 0;
    }
    if(!rx8010_i2c_write_byte(reg))
    {
        rx8010_i2c_stop();
        return 0;
    }

    rx8010_i2c_start();
    if(!rx8010_i2c_write_byte((uint8_t)((rx8010_i2c_addr << 1) | 0x01)))
    {
        rx8010_i2c_stop();
        return 0;
    }

    for(i = 0; i < len; i++)
    {
        data[i] = rx8010_i2c_read_byte((uint8_t)(i + 1 < len));
    }

    rx8010_i2c_stop();
    return 1;
}

static uint8_t rx8010_read_reg(uint8_t reg, uint8_t *data)
{
    return rx8010_read_regs(reg, data, 1);
}

static uint8_t rx8010_write_reg(uint8_t reg, uint8_t data)
{
    return rx8010_write_regs(reg, &data, 1);
}

static uint8_t rx8010_bcd_to_bin(uint8_t value)
{
    return (uint8_t)(((value >> 4) * 10) + (value & 0x0F));
}

static uint8_t rx8010_bin_to_bcd(uint8_t value)
{
    return (uint8_t)(((value / 10) << 4) | (value % 10));
}

static uint8_t rx8010_weekday_mask_to_index(uint8_t mask)
{
    uint8_t index;

    for(index = 0; index < 7; index++)
    {
        if(mask & (1u << index))
        {
            return index;
        }
    }

    return 0;
}

static uint8_t rx8010_calc_weekday(uint16_t year, uint8_t month, uint8_t day)
{
    static const uint8_t month_offset[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};

    if(month < 3)
    {
        year--;
    }

    return (uint8_t)((year + year / 4 - year / 100 + year / 400 +
                      month_offset[month - 1] + day) % 7);
}

static uint8_t rx8010_month_from_text(const char *month)
{
    static const char months[][4] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    uint8_t index;

    for(index = 0; index < 12; index++)
    {
        if(month[0] == months[index][0] &&
           month[1] == months[index][1] &&
           month[2] == months[index][2])
        {
            return (uint8_t)(index + 1);
        }
    }

    return 1;
}

static void rx8010_get_build_time(RX8010SJ_Time *time)
{
    const char *date = __DATE__;
    const char *clock = __TIME__;

    time->year = (uint16_t)(((date[7] - '0') * 1000) +
                            ((date[8] - '0') * 100) +
                            ((date[9] - '0') * 10) +
                            (date[10] - '0'));
    time->month = rx8010_month_from_text(date);
    time->day = (uint8_t)(((date[4] == ' ') ? 0 : (date[4] - '0')) * 10 +
                          (date[5] - '0'));
    time->hour = (uint8_t)((clock[0] - '0') * 10 + (clock[1] - '0'));
    time->minute = (uint8_t)((clock[3] - '0') * 10 + (clock[4] - '0'));
    time->second = (uint8_t)((clock[6] - '0') * 10 + (clock[7] - '0'));
    time->weekday = rx8010_calc_weekday(time->year, time->month, time->day);
}

static uint8_t rx8010_time_is_valid(const RX8010SJ_Time *time)
{
    if(time->year < 2000 || time->year > 2099)
    {
        return 0;
    }
    if(time->month < 1 || time->month > 12)
    {
        return 0;
    }
    if(time->day < 1 || time->day > 31)
    {
        return 0;
    }
    if(time->hour > 23 || time->minute > 59 || time->second > 59)
    {
        return 0;
    }
    if(time->weekday > 6)
    {
        return 0;
    }

    return 1;
}

/* ==================== 外部 RTC 芯片 RX8010SJ（I2C） ==================== */

void RX8010SJ_Init(void)
{
    uint8_t addr_index;
    uint8_t bus_index;

    /* PB14/PB15 overlap with debug/SPI remap functions on CH592.
     * Release them before probing the software I2C bus. */
    R16_PIN_ALTERNATE |= RB_DEBUG_EN;
    GPIOPinRemap(DISABLE, RB_PIN_SPI0);

    for(bus_index = 0; bus_index < (sizeof(rx8010_bus_candidates) / sizeof(rx8010_bus_candidates[0])); bus_index++)
    {
        rx8010_sda_pin = rx8010_bus_candidates[bus_index].sda_pin;
        rx8010_scl_pin = rx8010_bus_candidates[bus_index].scl_pin;
        rx8010_sda_release();
        rx8010_scl_release();
        rx8010_delay();

        PRINT("RX8010SJ probe bus: SDA=PB%u SCL=PB%u idle=%u/%u\n",
              (rx8010_sda_pin == GPIO_Pin_14) ? 14 : 15,
              (rx8010_scl_pin == GPIO_Pin_14) ? 14 : 15,
              GPIOB_ReadPortPin(rx8010_sda_pin) ? 1 : 0,
              GPIOB_ReadPortPin(rx8010_scl_pin) ? 1 : 0);

        for(addr_index = 0; addr_index < (sizeof(rx8010_addr_candidates) / sizeof(rx8010_addr_candidates[0])); addr_index++)
        {
            rx8010_i2c_addr = rx8010_addr_candidates[addr_index];
            if(rx8010_i2c_ping())
            {
                PRINT("RX8010SJ probe ok: SDA=PB%u SCL=PB%u ADDR=0x%02X\n",
                      (rx8010_sda_pin == GPIO_Pin_14) ? 14 : 15,
                      (rx8010_scl_pin == GPIO_Pin_14) ? 14 : 15,
                      rx8010_i2c_addr);
                return;
            }
        }
    }

    rx8010_sda_pin = RX8010SJ_SDA_PIN_DEFAULT;
    rx8010_scl_pin = RX8010SJ_SCL_PIN_DEFAULT;
    rx8010_i2c_addr = RX8010SJ_I2C_ADDR_DEFAULT;
    rx8010_sda_release();
    rx8010_scl_release();
    PRINT("RX8010SJ probe fail on all candidate bus configs\n");
}

uint8_t RX8010SJ_ReadTime(RX8010SJ_Time *time)
{
    uint8_t raw[7];

    if(!time)
    {
        return 0;
    }

    if(!rx8010_read_regs(RX8010SJ_REG_SEC, raw, sizeof(raw)))
    {
        return 0;
    }

    time->second = rx8010_bcd_to_bin((uint8_t)(raw[0] & 0x7F));
    time->minute = rx8010_bcd_to_bin((uint8_t)(raw[1] & 0x7F));
    time->hour = rx8010_bcd_to_bin((uint8_t)(raw[2] & 0x3F));
    time->weekday = rx8010_weekday_mask_to_index((uint8_t)(raw[3] & 0x7F));
    time->day = rx8010_bcd_to_bin((uint8_t)(raw[4] & 0x3F));
    time->month = rx8010_bcd_to_bin((uint8_t)(raw[5] & 0x1F));
    time->year = (uint16_t)(2000 + rx8010_bcd_to_bin(raw[6]));

    return 1;
}

uint8_t RX8010SJ_SetTime(const RX8010SJ_Time *time)
{
    uint8_t ctrl;
    uint8_t flag;
    uint8_t raw[7];

    if(!time || !rx8010_time_is_valid(time))
    {
        return 0;
    }

    if(!rx8010_read_reg(RX8010SJ_REG_CTRL, &ctrl))
    {
        return 0;
    }

    ctrl &= (uint8_t)~RX8010SJ_CTRL_TEST;
    if(!rx8010_write_reg(RX8010SJ_REG_CTRL, (uint8_t)(ctrl | RX8010SJ_CTRL_STOP)))
    {
        return 0;
    }

    raw[0] = rx8010_bin_to_bcd(time->second);
    raw[1] = rx8010_bin_to_bcd(time->minute);
    raw[2] = rx8010_bin_to_bcd(time->hour);
    raw[3] = (uint8_t)(1u << time->weekday);
    raw[4] = rx8010_bin_to_bcd(time->day);
    raw[5] = rx8010_bin_to_bcd(time->month);
    raw[6] = rx8010_bin_to_bcd((uint8_t)(time->year - 2000));

    if(!rx8010_write_regs(RX8010SJ_REG_SEC, raw, sizeof(raw)))
    {
        return 0;
    }

    if(!rx8010_write_reg(RX8010SJ_REG_CTRL, (uint8_t)(ctrl & ~RX8010SJ_CTRL_STOP)))
    {
        return 0;
    }

    if(!rx8010_read_reg(RX8010SJ_REG_FLAG, &flag))
    {
        return 0;
    }

    flag &= (uint8_t)~(RX8010SJ_FLAG_VLF |
                       RX8010SJ_FLAG_AF |
                       RX8010SJ_FLAG_TF |
                       RX8010SJ_FLAG_UF);

    return rx8010_write_reg(RX8010SJ_REG_FLAG, flag);
}

uint8_t RX8010SJ_Begin(RX8010SJ_Time *time)
{
    RX8010SJ_Time current;
    uint8_t flag;
    uint8_t ctrl;

    RX8010SJ_Init();

    if(!rx8010_write_reg(RX8010SJ_REG_RESV17, 0xD8) ||
       !rx8010_write_reg(RX8010SJ_REG_RESV30, 0x00) ||
       !rx8010_write_reg(RX8010SJ_REG_RESV31, 0x08) ||
       !rx8010_write_reg(RX8010SJ_REG_IRQ, 0x00))
    {
        return 0;
    }

    if(!rx8010_read_reg(RX8010SJ_REG_CTRL, &ctrl))
    {
        return 0;
    }

    ctrl &= (uint8_t)~(RX8010SJ_CTRL_TEST |
                       RX8010SJ_CTRL_AIE |
                       RX8010SJ_CTRL_UIE |
                       RX8010SJ_CTRL_STOP);

    if(!rx8010_write_reg(RX8010SJ_REG_CTRL, ctrl))
    {
        return 0;
    }

    if(!rx8010_read_reg(RX8010SJ_REG_FLAG, &flag))
    {
        return 0;
    }

    if(!RX8010SJ_ReadTime(&current))
    {
        return 0;
    }

    if((flag & RX8010SJ_FLAG_VLF) || !rx8010_time_is_valid(&current))
    {
        rx8010_get_build_time(&current);
        if(!RX8010SJ_SetTime(&current))
        {
            return 0;
        }
    }
    else if(flag & (RX8010SJ_FLAG_AF | RX8010SJ_FLAG_TF | RX8010SJ_FLAG_UF))
    {
        flag &= (uint8_t)~(RX8010SJ_FLAG_AF |
                           RX8010SJ_FLAG_TF |
                           RX8010SJ_FLAG_UF);
        if(!rx8010_write_reg(RX8010SJ_REG_FLAG, flag))
        {
            return 0;
        }
    }

    if(time)
    {
        *time = current;
    }

    return 1;
}

#else  /* !USE_EXT_RTC */

/* ==================== MCU 片内 RTC ==================== */

#include "RTC.h"
#include <string.h>

/* 片内 RTC 上电后从固定时间开始走时（没有后备电池，掉电即丢）。
 * 需要准确时间时用「手动设定时间」或蓝牙同步写入。 */
#define MCU_RTC_START_YEAR   2026
#define MCU_RTC_START_MONTH  1
#define MCU_RTC_START_DAY    1
#define MCU_RTC_START_HOUR   0
#define MCU_RTC_START_MINUTE 0
#define MCU_RTC_START_SECOND 0

static uint8_t mcu_rtc_begin_done = 0;

void RX8010SJ_Init(void)
{
    /* 片内 RTC 的 32.768kHz 时钟由 HAL_TimeInit() 在 HAL_Init() 中配置 */
}

uint8_t RX8010SJ_Begin(RX8010SJ_Time *time)
{
    if(!mcu_rtc_begin_done)
    {
        RTC_InitTime(MCU_RTC_START_YEAR, MCU_RTC_START_MONTH, MCU_RTC_START_DAY,
                     MCU_RTC_START_HOUR, MCU_RTC_START_MINUTE, MCU_RTC_START_SECOND);
        mcu_rtc_begin_done = 1;
        PRINT("MCU RTC start %04u-%02u-%02u %02u:%02u:%02u\n",
              MCU_RTC_START_YEAR, MCU_RTC_START_MONTH, MCU_RTC_START_DAY,
              MCU_RTC_START_HOUR, MCU_RTC_START_MINUTE, MCU_RTC_START_SECOND);
    }

    if(!RX8010SJ_ReadTime(time))
    {
        return 0;
    }

    return 1;
}

uint8_t RX8010SJ_ReadTime(RX8010SJ_Time *time)
{
    static const uint8_t month_offset[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    uint16_t y, mon, d, h, m, sec;
    uint16_t wy;

    if(!time)
    {
        return 0;
    }

    RTC_GetTime(&y, &mon, &d, &h, &m, &sec);

    time->year = y;
    time->month = (uint8_t)mon;
    time->day = (uint8_t)d;
    time->hour = (uint8_t)h;
    time->minute = (uint8_t)m;
    time->second = (uint8_t)sec;

    /* RTC_GetTime 不返回星期，按年月日推算 */
    wy = y;
    if(mon < 3)
    {
        wy--;
    }
    time->weekday = (uint8_t)((wy + wy / 4 - wy / 100 + wy / 400 +
                               month_offset[(mon >= 1 && mon <= 12) ? (mon - 1) : 0] +
                               d) % 7);

    return 1;
}

uint8_t RX8010SJ_SetTime(const RX8010SJ_Time *time)
{
    if(!time)
    {
        return 0;
    }
    if(time->year < 2020 || time->year > 2064 ||
       time->month < 1 || time->month > 12 ||
       time->day < 1 || time->day > 31 ||
       time->hour > 23 || time->minute > 59 || time->second > 59)
    {
        return 0;
    }

    RTC_InitTime(time->year, time->month, time->day,
                 time->hour, time->minute, time->second);
    mcu_rtc_begin_done = 1;
    return 1;
}

#endif /* USE_EXT_RTC */
