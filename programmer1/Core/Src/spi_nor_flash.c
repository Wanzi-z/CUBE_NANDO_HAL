/*********************************************************************************************************
*
* File                : spi_nor_flash.c
* Hardware Environment:
* Build Environment   : STM32CUBEIDE  Version: 1.13.2
* Version             : V1.0
* By                  :
*
*                                  (c) Copyright 2005-2011, WaveShare
*                                       http://www.waveshare.net
*                                          All Rights Reserved
*
*********************************************************************************************************/

#include "spi_nor_flash.h"
#include "spi.h"
#include "gpio.h"
#include <stm32f4xx.h>

/**SPI1 GPIO Configuration
  PA6     ------> SPI1_MISO
  PA7     ------> SPI1_MOSI
  PB3     ------> SPI1_SCK
  PB4     ------> SPI1_CS
*/

#define SPI_FLASH_MISO_PIN SPI1_MISO_Pin
#define SPI_FLASH_MOSI_PIN SPI1_MOSI_Pin
#define SPI_FLASH_SCK_PIN SPI1_SCK_Pin
#define SPI_FLASH_CS_PIN SPI1_CS_Pin

#define FLASH_DUMMY_BYTE 0xA5

/* 第一地址周期 */
#define ADDR_1st_CYCLE(ADDR) (uint8_t)((ADDR)& 0xFF)
/* 第二地址周期 */
#define ADDR_2nd_CYCLE(ADDR) (uint8_t)(((ADDR)& 0xFF00) >> 8)
/* 第三地址周期 */
#define ADDR_3rd_CYCLE(ADDR) (uint8_t)(((ADDR)& 0xFF0000) >> 16)
/* 第四地址周期 */
#define ADDR_4th_CYCLE(ADDR) (uint8_t)(((ADDR)& 0xFF000000) >> 24)

#define UNDEFINED_CMD 0xFF

extern SPI_HandleTypeDef hspi1;

typedef struct __attribute__((__packed__))
{
    uint8_t page_offset;    // 页偏移量
    uint8_t read_cmd;       // 读取指令
    uint8_t read_id_cmd;    // 读取ID指令
    uint8_t write_cmd;      // 写入指令
    uint8_t write_en_cmd;   // 写使能指令
    uint8_t erase_cmd;      // 擦除指令
    uint8_t status_cmd;     // 状态指令
    uint8_t busy_bit;       // 忙状态位
    uint8_t busy_state;     // 忙状态值
    uint32_t freq;          // SPI频率
} spi_conf_t;

static spi_conf_t spi_conf;

// 初始化SPI Flash的GPIO引脚
static void spi_flash_gpio_init()
{
	HAL_SPI_MspInit(&hspi1);
}

// 取消初始化SPI Flash的GPIO引脚
static void spi_flash_gpio_uninit()
{
	HAL_SPI_MspDeInit(&hspi1);
}

static inline void spi_flash_select_chip()
{
//    GPIO_ResetBits(GPIOA, SPI_FLASH_CS_PIN);
   	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI_FLASH_CS_PIN, GPIO_PIN_RESET);
}

static inline void spi_flash_deselect_chip()
{
//    GPIO_SetBits(GPIOA, SPI_FLASH_CS_PIN);
  	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI_FLASH_CS_PIN, GPIO_PIN_SET);
}

static uint16_t spi_flash_get_baud_rate_prescaler(uint32_t spi_freq_khz)
{
    uint32_t system_clock_khz = SystemCoreClock / 1000;

    if (spi_freq_khz >= system_clock_khz / 2)
         return SPI_BAUDRATEPRESCALER_2;
    else if (spi_freq_khz >= system_clock_khz / 4)
         return SPI_BAUDRATEPRESCALER_4;
    else if (spi_freq_khz >= system_clock_khz / 8)
         return SPI_BAUDRATEPRESCALER_8;
    else if (spi_freq_khz >= system_clock_khz / 16)
         return SPI_BAUDRATEPRESCALER_16;
    else if (spi_freq_khz >= system_clock_khz / 32)
         return SPI_BAUDRATEPRESCALER_32;
    else if (spi_freq_khz >= system_clock_khz / 64)
          return SPI_BAUDRATEPRESCALER_64;
    else if (spi_freq_khz >= system_clock_khz / 128)
          return SPI_BAUDRATEPRESCALER_128;
    else
          return SPI_BAUDRATEPRESCALER_256;
}

// 初始化SPI Flash
static int spi_flash_init(void *conf, uint32_t conf_size)
{
    SPI_InitTypeDef spi_init;

    if (conf_size < sizeof(spi_conf_t))
        return -1; 

    spi_conf = *(spi_conf_t *)conf;

    spi_flash_gpio_init();

    spi_flash_deselect_chip();

    /* 配置SPI */
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
    hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = spi_flash_get_baud_rate_prescaler(spi_conf.freq);
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 7;
    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
      Error_Handler();
    }  // 根据以上配置初始化SPI1

    /* 使能SPI */
//    SPI_Cmd(SPI1, ENABLE);
    __HAL_SPI_ENABLE(&hspi1); // 初始化SPI Flash芯片

    return 0;
}

// 取消初始化SPI Flash
static void spi_flash_uninit()
{
    spi_flash_gpio_uninit();
    /* 禁用SPI */
    __HAL_SPI_DISABLE(&hspi1);
}

// 发送一个字节到SPI Flash并返回接收到的字节
static uint8_t spi_flash_send_byte(uint8_t byte)
{
  uint32_t timeout = 0x1000000;
  uint8_t rx_byte = 0X00;

  if(HAL_SPI_TransmitReceive(&hspi1, &byte, &rx_byte, 1, timeout) != HAL_OK)
   {
	   rx_byte = 0XFF;
   }

  return rx_byte;
}

// 从SPI Flash中读取一个字节
static inline uint8_t spi_flash_read_byte()
{
    return spi_flash_send_byte(FLASH_DUMMY_BYTE);
}

// 读取SPI Flash的状态寄存器值
static uint32_t spi_flash_read_status()
{
    uint8_t status;
    uint32_t flash_status = FLASH_STATUS_READY;

    spi_flash_select_chip();

    spi_flash_send_byte(spi_conf.status_cmd);

    status = spi_flash_read_byte();

    if (spi_conf.busy_state == 1 && (status & (1 << spi_conf.busy_bit)))
        flash_status = FLASH_STATUS_BUSY;
    else if (spi_conf.busy_state == 0 && !(status & (1 << spi_conf.busy_bit)))
        flash_status = FLASH_STATUS_BUSY;

    spi_flash_deselect_chip();

    return flash_status;
}

// 获取SPI Flash的状态，等待操作完成或超时
static uint32_t spi_flash_get_status()
{
    uint32_t status, timeout = 0x1000000;

    status = spi_flash_read_status();

    /* 等待操作完成或超时 */
    while (status == FLASH_STATUS_BUSY && timeout)
    {
        status = spi_flash_read_status();
        timeout --;
    }

    if (!timeout)
        status = FLASH_STATUS_TIMEOUT;

    return status;
}

// 读取SPI Flash的ID
static void spi_flash_read_id(chip_id_t *chip_id)
{
    spi_flash_select_chip();

    spi_flash_send_byte(spi_conf.read_id_cmd);
    spi_flash_send_byte(0x00);  // 发送读取厂商ID的指令

    chip_id->maker_id = spi_flash_read_byte();
    chip_id->device_id = spi_flash_read_byte();
    chip_id->third_id = spi_flash_read_byte();
    chip_id->fourth_id = spi_flash_read_byte();
    chip_id->fifth_id = spi_flash_read_byte();
    chip_id->sixth_id = spi_flash_read_byte();

    spi_flash_deselect_chip();
}

// 启用SPI Flash的写使能
static void spi_flash_write_enable()
{
    if (spi_conf.write_en_cmd == UNDEFINED_CMD)
        return;

    spi_flash_select_chip();
    spi_flash_send_byte(spi_conf.write_en_cmd);
    spi_flash_deselect_chip();
}

// 异步写入SPI Flash的一页数据
static void spi_flash_write_page_async(uint8_t *buf, uint32_t page, uint32_t page_size)
{
    uint32_t i;

    spi_flash_write_enable();

    spi_flash_select_chip();

    spi_flash_send_byte(spi_conf.write_cmd);

    page = page << spi_conf.page_offset;

    spi_flash_send_byte(ADDR_3rd_CYCLE(page));
    spi_flash_send_byte(ADDR_2nd_CYCLE(page));
    spi_flash_send_byte(ADDR_1st_CYCLE(page));

    for (i = 0; i < page_size; i++)
        spi_flash_send_byte(buf[i]);

    spi_flash_deselect_chip();
}

// 从指定地址读取数据到缓冲区
static uint32_t spi_flash_read_data(uint8_t *buf, uint32_t page, uint32_t page_offset, uint32_t data_size)
{
    uint32_t i, addr = (page << spi_conf.page_offset) + page_offset;

    spi_flash_select_chip();

    spi_flash_send_byte(spi_conf.read_cmd);

    spi_flash_send_byte(ADDR_3rd_CYCLE(addr));
    spi_flash_send_byte(ADDR_2nd_CYCLE(addr));
    spi_flash_send_byte(ADDR_1st_CYCLE(addr));

    /* AT45DB要求在地址后写入虚拟字节 */
    spi_flash_send_byte(FLASH_DUMMY_BYTE);

    for (i = 0; i < data_size; i++)
        buf[i] = spi_flash_read_byte();

    spi_flash_deselect_chip();

    return FLASH_STATUS_READY;
}

// 从指定页读取数据到缓冲区
static uint32_t spi_flash_read_page(uint8_t *buf, uint32_t page, uint32_t page_size)
{
    return spi_flash_read_data(buf, page, 0, page_size);
}

// 从指定页的偏移量读取备用数据到缓冲区
static uint32_t spi_flash_read_spare_data(uint8_t *buf, uint32_t page, uint32_t offset, uint32_t data_size)
{
    return FLASH_STATUS_INVALID_CMD;
}

// 擦除指定块
static uint32_t spi_flash_erase_block(uint32_t page)
{
    uint32_t addr = page << spi_conf.page_offset;

    spi_flash_write_enable();

    spi_flash_select_chip();

    spi_flash_send_byte(spi_conf.erase_cmd);

    spi_flash_send_byte(ADDR_3rd_CYCLE(addr));
    spi_flash_send_byte(ADDR_2nd_CYCLE(addr));
    spi_flash_send_byte(ADDR_1st_CYCLE(addr));

    spi_flash_deselect_chip();

    return spi_flash_get_status();
}

// 检查是否支持坏块管理
static inline bool spi_flash_is_bb_supported()
{
    return false;
}

// SPI NOR Flash的硬件抽象层
flash_hal_t hal_spi_nor =
{
    .init = spi_flash_init,
    .uninit = spi_flash_uninit,
    .read_id = spi_flash_read_id,
    .erase_block = spi_flash_erase_block,
    .read_page = spi_flash_read_page,
    .read_spare_data = spi_flash_read_spare_data, 
    .write_page_async = spi_flash_write_page_async,
    .read_status = spi_flash_read_status,
    .is_bb_supported = spi_flash_is_bb_supported
};
