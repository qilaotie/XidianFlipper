#ifndef U8G2_ESP32_HAL_H
#define U8G2_ESP32_HAL_H
/*
 * u8g2_esp32_hal.h
 *
 *  Created on: Feb 12, 2017
 *      Author: kolban
 */

#ifndef U8G2_ESP32_HAL_H_
#define U8G2_ESP32_HAL_H_
#include "u8g2.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"

#define U8G2_ESP32_HAL_UNDEFINED GPIO_NUM_NC

// ESP32-C6 only has I2C_NUM_0; force it
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C61)
#define I2C_MASTER_NUM I2C_NUM_0     //  I2C port number for master dev
#elif SOC_I2C_NUM > 1
#define I2C_MASTER_NUM I2C_NUM_1     //  I2C port number for master dev
#else
#define I2C_MASTER_NUM I2C_NUM_0     //  I2C port number for master dev
#endif

#define I2C_MASTER_TX_BUF_DISABLE 0  //  I2C master do not need buffer
#define I2C_MASTER_RX_BUF_DISABLE 0  //  I2C master do not need buffer
#define I2C_MASTER_FREQ_HZ 700000    //  I2C master clock frequency (SSD1306 700kHz)
#define ACK_CHECK_EN 0x1             //  I2C master will check ack from slave
#define ACK_CHECK_DIS 0x0  //  I2C master will not check ack from slave

/** @public
 * HAL configuration structure.
 */
typedef struct {
  union {
    /* SPI settings. */
    struct {
      /* GPIO num for clock. */
      gpio_num_t clk;
      /* GPIO num for SPI mosi. */
      gpio_num_t mosi;
      /* GPIO num for SPI slave/chip select. */
      gpio_num_t cs;
    } spi;
    /* I2C settings. */
    struct {
      /* GPIO num for I2C data. */
      gpio_num_t sda;
      /* GPIO num for I2C clock. */
      gpio_num_t scl;
    } i2c;
  } bus;
  /* GPIO num for reset. */
  gpio_num_t reset;
  /* GPIO num for DC. */
  gpio_num_t dc;
} u8g2_esp32_hal_t;

/**
 * Construct a default HAL configuration with all fields undefined.
 */
#define U8G2_ESP32_HAL_DEFAULT                                        \
  {                                                                   \
    .bus = {.spi = {.clk = U8G2_ESP32_HAL_UNDEFINED,                  \
                    .mosi = U8G2_ESP32_HAL_UNDEFINED,                 \
                    .cs = U8G2_ESP32_HAL_UNDEFINED}},                 \
    .reset = U8G2_ESP32_HAL_UNDEFINED, .dc = U8G2_ESP32_HAL_UNDEFINED \
  }

/**
 * @brief 查询 OLED 是否可用。
 *
 * I2C 通信首次失败后返回 false，此后所有 U8G2 绘制操作静默跳过。
 * 调用者应在 UI 初始化前检查此标志，避免无屏操作。
 *
 * @return true  OLED 已成功初始化并可通信。
 * @return false OLED 未连接或通信失败。
 */
bool oled_is_available(void);

/**
 * Initialize the HAL with the given configuration.
 *
 * @see u8g2_esp32_hal_t
 * @see U8G2_ESP32_HAL_DEFAULT
 */
void u8g2_esp32_hal_init(u8g2_esp32_hal_t u8g2_esp32_hal_param);
uint8_t u8g2_esp32_spi_byte_cb(u8x8_t* u8x8,
                               uint8_t msg,
                               uint8_t arg_int,
                               void* arg_ptr);
uint8_t u8g2_esp32_i2c_byte_cb(u8x8_t* u8x8,
                               uint8_t msg,
                               uint8_t arg_int,
                               void* arg_ptr);
uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t* u8x8,
                                     uint8_t msg,
                                     uint8_t arg_int,
                                     void* arg_ptr);
#endif /* U8G2_ESP32_HAL_H_ */

#endif