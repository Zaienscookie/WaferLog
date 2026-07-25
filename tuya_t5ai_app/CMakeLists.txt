cmake_minimum_required(VERSION 3.20)

set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})
set(WAFERLOG_ROOT ${APP_PATH}/..)

set(APP_SRCS
    ${APP_PATH}/src/tuya_main.c
    ${APP_PATH}/src/tuya_lvgl.c
    ${WAFERLOG_ROOT}/src/waferlog_ui.c
    ${WAFERLOG_ROOT}/src/fonts/waferlog_font_14.c
    ${WAFERLOG_ROOT}/src/fonts/waferlog_font_16.c
    ${WAFERLOG_ROOT}/src/hal/waferlog_services.c
)

add_library(${EXAMPLE_LIB})

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRCS}
)

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_PATH}/src
        ${WAFERLOG_ROOT}/src
        ${WAFERLOG_ROOT}/src/hal
)

if(EXISTS ${APP_PATH}/src/waferlog_local_config.h)
    target_compile_definitions(${EXAMPLE_LIB} PRIVATE WAFERLOG_HAS_LOCAL_CONFIG=1)
endif()

target_compile_definitions(${EXAMPLE_LIB}
    PRIVATE
        WAFERLOG_T5AI=1
        WAFERLOG_SERVER_HOST=\"116.62.141.17\"
        WAFERLOG_SERVER_PORT=80
        LV_LVGL_H_INCLUDE_SIMPLE=1
        NOTE_STROKE_LIMIT=1024
)

target_compile_options(${EXAMPLE_LIB}
    PRIVATE
        -Wno-deprecated-declarations
        -Wno-unused-function
)
