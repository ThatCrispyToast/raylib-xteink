set(X4_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
get_filename_component(X4_ROOT "${X4_ROOT}" ABSOLUTE)
set(X4_RAYLIB_SOURCES
    ${X4_ROOT}/vendor/raylib/src/rcore.c
    ${X4_ROOT}/vendor/raylib/src/rshapes.c
    ${X4_ROOT}/vendor/raylib/src/rtextures.c
    ${X4_ROOT}/vendor/raylib/src/rtext.c
    ${X4_ROOT}/src/x4_display.c
    ${X4_ROOT}/src/x4_font.c)
function(x4_configure target)
    # Resolve rlsw's self-include after __FILE__ path remapping.
    target_include_directories(${target} PUBLIC ${X4_ROOT}/include ${X4_ROOT}/vendor/raylib/src
        PRIVATE ${X4_ROOT}/src ${X4_ROOT})
    target_compile_options(${target} PRIVATE -include ${X4_ROOT}/src/x4_config.h
        -ffunction-sections -fdata-sections
        "-fmacro-prefix-map=${X4_ROOT}/=")
    target_compile_features(${target} PUBLIC c_std_99)
endfunction()
