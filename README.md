# CUBE_NANDO_HAL
1.使用STM32CUBEIDE编译bootloader，然后使用STLINK V2刷入；

2.使用STM32CUBEIDE编译programmer,然后使用STLINK V2刷入；
（STM32F407VETX_FLASH.ld文件中FLASH    (rx)    : ORIGIN = 0x08004000,   LENGTH = 120K）

3.使用STM32CUBEIDE编译programmer,然后使用STLINK V2刷入；
（修改STM32F407VETX_FLASH.ld文件中FLASH    (rx)    : ORIGIN = 0x08022000,   LENGTH = 120K）

4.其它MCU可使用STM32CUBEMX配置好硬件接口后，从本代码中复制修改相应文件。
