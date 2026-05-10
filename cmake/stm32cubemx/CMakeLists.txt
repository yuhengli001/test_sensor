cmake_minimum_required(VERSION 3.22)
# Enable CMake support for ASM and C languages
enable_language(C ASM)
# STM32CubeMX generated symbols (macros)
set(MX_Defines_Syms 
	USE_FULL_LL_DRIVER 
	BLE_LL 
	BLE_STACK_BASIC_FEATURES 
	PHY_40nm_3_40_a 
	USE_HAL_DRIVER 
	STM32WBA64xx
    $<$<CONFIG:Debug>:DEBUG>
)

# STM32CubeMX generated include paths
set(MX_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Modules
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Config
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Config/Log
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Config/LowPower
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Config/Debug_GPIO
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Config/Flash
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Config/ADC_Ctrl
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Config/CRC_Ctrl
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/App
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/Target
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Inc/Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/trace/adv_trace
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/BasicAES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/Flash
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/MemoryManager
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/RTDebug
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/SerialCmdInterpreter
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/Log
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/misc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/sequencer
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/tim_serv
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/lpm/tiny_lpm
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/link_layer/ll_cmd_lib/config/ble_basic
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/ble/svc/Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/CMSIS/Device/ST/STM32WBAxx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/link_layer/ll_cmd_lib/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/link_layer/ll_cmd_lib/inc/_40nm_reg_files
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/link_layer/ll_cmd_lib/inc/ot_inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/link_layer/ll_sys/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/ble/stack/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/ble/stack/include/auto
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/ble/svc/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/CMSIS/Include
)

# STM32CubeMX generated application sources
set(MX_Application_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/App/app_ble.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/App/control_service.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/App/control_service_app.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/App/vital_sign_service.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/App/vital_sign_service_app.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/App/fall_service.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/App/fall_service_app.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/App/vibration_service.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/App/vibration_service_app.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/Target/bleplat.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/Target/power_table.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/Target/linklayer_plat.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/Target/ll_sys_if.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/Target/host_stack_if.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Modules/ble_timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Interfaces/stm32_lpm_if.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Config/CRC_Ctrl/crc_ctrl_conf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Config/Flash/simple_nvm_arbiter_conf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Config/Debug_GPIO/app_debug.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Config/LowPower/user_low_power_config.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Config/LowPower/peripheral_init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Config/ADC_Ctrl/adc_ctrl_conf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/app_entry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/stm32wbaxx_it.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/stm32wbaxx_hal_msp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/sysmem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/syscalls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../startup_stm32wba64xx.s
)

# STM32 HAL/LL Drivers
set(STM32_Drivers_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/system_stm32wbaxx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_ll_pka.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_adc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_adc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_dma.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_dma_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_rcc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_rcc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_cortex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_flash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_flash_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_exti.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_pwr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_pwr_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_ll_exti.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_cryp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_cryp_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_crc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_crc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_ll_icache.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_ll_gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_pka.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_ramcfg.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_rng.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_rng_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_rtc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_rtc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_spi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_spi_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_ll_tim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_ll_dma.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_uart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_uart_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_ll_adc.c
)

# Drivers Midllewares


set(Utilities_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/trace/adv_trace/stm32_adv_trace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/misc/stm32_mem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/misc/stm32_systime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/misc/stm32_tiny_sscanf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/misc/stm32_tiny_vsnprintf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/sequencer/stm32_seq.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/tim_serv/stm32_timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/lpm/tiny_lpm/stm32_lpm.c
)
set(STM32_WPAN_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/ble/svc/Src/svc_ctl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/link_layer/ll_sys/src/ll_sys_cs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/link_layer/ll_sys/src/ll_sys_dp_slp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/link_layer/ll_sys/src/ll_sys_intf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/link_layer/ll_sys/src/ll_sys_startup.c
)
set(Common_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Startup/stm32wbaxx_ResetHandler_GCC.s
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/Log/log_module.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/SerialCmdInterpreter/serial_cmd_interpreter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/RTDebug/RTDebug.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/RTDebug/RTDebug_dtb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/MemoryManager/advanced_memory_manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/MemoryManager/stm32_mm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/Flash/flash_driver.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/Flash/flash_manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/Flash/rf_timing_synchro.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/Flash/simple_nvm_arbiter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/BasicAES/baes_cmac.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/BasicAES/baes_ccm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/BasicAES/baes_ecb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/adc_ctrl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/app_sys.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/crc_ctrl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/otp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/pka_ctrl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/scm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/stm_list.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/stm_queue.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/temp_measurement.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Interfaces/adv_trace_usart_if.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Interfaces/hw_aes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Interfaces/hw_otp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Interfaces/hw_rng.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Interfaces/timer_if.c
)

# Link directories setup
set(MX_LINK_DIRS

    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/link_layer/ll_cmd_lib/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/ble/stack/lib
)
# Project static libraries
set(MX_LINK_LIBS  
	:stm32wba_ble_stack_basic.a
	:WBA6_LinkLayer_BLE_Basic_lib.a 
    STM32_Drivers
    ${TOOLCHAIN_LINK_LIBRARIES}
    Utilities	STM32_WPAN	Common	
)
# Interface library for includes and symbols
add_library(stm32cubemx INTERFACE)
target_include_directories(stm32cubemx INTERFACE ${MX_Include_Dirs})
target_compile_definitions(stm32cubemx INTERFACE ${MX_Defines_Syms})

# Create STM32_Drivers static library
add_library(STM32_Drivers OBJECT)
target_sources(STM32_Drivers PRIVATE ${STM32_Drivers_Src})
target_link_libraries(STM32_Drivers PUBLIC stm32cubemx)


# Create Utilities static library
add_library(Utilities OBJECT)
target_sources(Utilities PRIVATE ${Utilities_Src})
target_link_libraries(Utilities PUBLIC stm32cubemx)

# Create STM32_WPAN static library
add_library(STM32_WPAN OBJECT)
target_sources(STM32_WPAN PRIVATE ${STM32_WPAN_Src})
target_link_libraries(STM32_WPAN PUBLIC stm32cubemx)

# Create Common static library
add_library(Common OBJECT)
target_sources(Common PRIVATE ${Common_Src})
target_link_libraries(Common PUBLIC stm32cubemx)

# Add STM32CubeMX generated application sources to the project
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MX_Application_Src})

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${MX_LINK_DIRS})

# Add libraries to the project
target_link_libraries(${CMAKE_PROJECT_NAME} -Wl,--start-group ${MX_LINK_LIBS} -Wl,--end-group)

# Add the map file to the list of files to be removed with 'clean' target
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_PROJECT_NAME}.map)

# Validate that STM32CubeMX code is compatible with C standard
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()
