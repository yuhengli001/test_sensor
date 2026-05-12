cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME test_sensor)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
    # === A121 lib ===
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/rss/lib
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/radar_sensor.c

    # ====================
    # === algorithms ===
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/algorithms/acc_algorithm.c

    # === getting started ===
    # ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/examples/getting_started/example_detector_distance.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/examples/getting_started/example_bring_up.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/examples/getting_started/example_detector_distance_M33.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/examples/getting_started/fall_detector.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/examples/getting_started/vital_signs.c

    # === processing ===
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/examples/processing/example_processing_amplitude.c
    
    # === example apps ===
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/use_cases/example_apps/example_vibration.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/use_cases/example_apps/example_vibration_main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/use_cases/example_apps/example_surface_velocity.c

    # === reference apps ===
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/use_cases/reference_apps/ref_app_breathing.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/use_cases/reference_apps/ref_app_breathing_main.c
    
    # ==================== 
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/integration/acc_hal_integration_stm32cube_xe121_single_sensor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/integration/acc_integration_stm32.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/integration/acc_integration_log.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/rss/include
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/integration
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/algorithms
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/examples/getting_started
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/examples/processing
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/examples/helper
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/use_cases/example_apps
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/use_cases/reference_apps
    ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m33_gcc/exploration_server
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
    # === A121 lib ===
    acc_detector_distance_a121
    acc_detector_presence_a121
    # acconeer_exploration_server_a121
    # The core library MUST be last
    acconeer_a121
    m
)

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -Wl,-u,_printf_float
    -Wl,-z,noexecstack
)
