cmake_minimum_required(VERSION 3.20)

set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})
set(WAFERLOG_ROOT ${APP_PATH}/..)
set(WAFERLOG_TUYAOPEN_PATCH ${APP_PATH}/patches/tuyaopen_lvgl_t5ai_flicker.patch)

find_program(WAFERLOG_GIT_EXECUTABLE git REQUIRED)
execute_process(
    COMMAND ${WAFERLOG_GIT_EXECUTABLE} -C ${CMAKE_SOURCE_DIR} apply --check ${WAFERLOG_TUYAOPEN_PATCH}
    RESULT_VARIABLE WAFERLOG_PATCH_CHECK
    OUTPUT_QUIET
    ERROR_QUIET
)
if(WAFERLOG_PATCH_CHECK EQUAL 0)
    execute_process(
        COMMAND ${WAFERLOG_GIT_EXECUTABLE} -C ${CMAKE_SOURCE_DIR} apply ${WAFERLOG_TUYAOPEN_PATCH}
        RESULT_VARIABLE WAFERLOG_PATCH_APPLY
    )
    if(NOT WAFERLOG_PATCH_APPLY EQUAL 0)
        message(FATAL_ERROR "Failed to apply the T5AI display stability patch")
    endif()
else()
    execute_process(
        COMMAND ${WAFERLOG_GIT_EXECUTABLE} -C ${CMAKE_SOURCE_DIR} apply --reverse --check ${WAFERLOG_TUYAOPEN_PATCH}
        RESULT_VARIABLE WAFERLOG_PATCH_PRESENT
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(NOT WAFERLOG_PATCH_PRESENT EQUAL 0)
        message(FATAL_ERROR "TuyaOpen LVGL display sources are incompatible with the WaferLog patch")
    endif()
endif()

set(APP_SRCS
    ${APP_PATH}/src/tuya_main.c
    ${APP_PATH}/src/tuya_lvgl.c
    ${WAFERLOG_ROOT}/src/waferlog_ui.c
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
        ${WAFERLOG_ROOT}
        ${WAFERLOG_ROOT}/src
        ${WAFERLOG_ROOT}/src/hal
)

target_compile_definitions(${EXAMPLE_LIB}
    PRIVATE
        WAFERLOG_T5AI=1
        LV_LVGL_H_INCLUDE_SIMPLE=1
        NOTE_STROKE_LIMIT=1024
)

target_compile_options(${EXAMPLE_LIB}
    PRIVATE
        -Wno-deprecated-declarations
        -Wno-unused-function
)

