include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/raylib-x4.cmake)
if(X4_QEMU_SMOKE)
    idf_component_register(SRCS ${X4_RAYLIB_SOURCES} ../../tests/firmware_smoke.c
        INCLUDE_DIRS ../../src ../../include ../../vendor/raylib/src REQUIRES esp_timer)
    x4_configure(${COMPONENT_LIB})
    target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-variable -Wno-unused-function)
    return()
endif()
if(NOT X4_APP_SOURCE)
    set(X4_APP_SOURCE "${X4_ROOT}/examples/paper.c")
endif()
idf_component_register(SRCS ${X4_RAYLIB_SOURCES} ../../src/x4_epd.c
    ../../src/x4_hal_espidf.c ${X4_APP_SOURCE} app_main.c
    INCLUDE_DIRS ../../src ../../include ../../vendor/raylib/src
    REQUIRES esp_driver_gpio esp_driver_spi esp_adc esp_timer fatfs sdmmc esp_driver_sdspi esp_driver_usb_serial_jtag)
x4_configure(${COMPONENT_LIB})
set_source_files_properties(${X4_APP_SOURCE} PROPERTIES COMPILE_DEFINITIONS "main=x4_application_main")
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-variable -Wno-unused-function -Wno-missing-field-initializers)
