set(X4_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
get_filename_component(X4_ROOT "${X4_ROOT}" ABSOLUTE)
if(NOT DEFINED XTEINK_DEVICE)
    set(XTEINK_DEVICE "x4")
endif()
set(XTEINK_DEVICE "${XTEINK_DEVICE}" CACHE STRING "XTEINK device: x4, x3, x4-classic, x4-pro")
set_property(CACHE XTEINK_DEVICE PROPERTY STRINGS x4 x3 x4-classic x4-pro)
set(_xteink_devices x4 x3 x4-classic x4-pro)
if(NOT XTEINK_DEVICE IN_LIST _xteink_devices)
    message(FATAL_ERROR "Unknown XTEINK_DEVICE '${XTEINK_DEVICE}'. Choose x4, x3, x4-classic, or x4-pro.")
endif()
string(TOUPPER "${XTEINK_DEVICE}" _xteink_device_define)
string(REPLACE "-" "_" _xteink_device_define "${_xteink_device_define}")
set(XTEINK_DEVICE_DEFINE "XTEINK_DEVICE=XTEINK_${_xteink_device_define}")
set(X4_RAYLIB_SOURCES
    ${X4_ROOT}/vendor/raylib/src/rcore.c
    ${X4_ROOT}/vendor/raylib/src/rshapes.c
    ${X4_ROOT}/vendor/raylib/src/rtextures.c
    ${X4_ROOT}/vendor/raylib/src/rtext.c
    ${X4_ROOT}/src/x4_display.c
    ${X4_ROOT}/src/x4_font.c)
function(x4_configure target)
    target_compile_definitions(${target} PUBLIC ${XTEINK_DEVICE_DEFINE})
    # Resolve rlsw's self-include after __FILE__ path remapping.
    target_include_directories(${target} PUBLIC ${X4_ROOT}/include ${X4_ROOT}/vendor/raylib/src
        PRIVATE ${X4_ROOT}/src ${X4_ROOT})
    target_compile_options(${target} PRIVATE -include ${X4_ROOT}/src/x4_config.h
        -ffunction-sections -fdata-sections
        "-fmacro-prefix-map=${X4_ROOT}/=")
    target_compile_features(${target} PUBLIC c_std_99)
endfunction()
