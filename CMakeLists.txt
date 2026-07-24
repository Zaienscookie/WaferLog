cmake_minimum_required(VERSION 3.20)

project(WaferLog VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "Disable LVGL demos for WaferLog" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "Disable LVGL examples for WaferLog" FORCE)

set(WAFERLOG_LOCAL_LVGL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lvgl")
set(WAFERLOG_LOCAL_SDL2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SDL2-2.30.1/cmake")

if(EXISTS "${WAFERLOG_LOCAL_LVGL_DIR}/CMakeLists.txt")
    set(WAFERLOG_LVGL_DIR "${WAFERLOG_LOCAL_LVGL_DIR}" CACHE PATH "LVGL source directory")
else()
    include(FetchContent)
    FetchContent_Declare(
        lvgl
        GIT_REPOSITORY https://github.com/lvgl/lvgl.git
        GIT_TAG release/v9.2
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(lvgl)
    set(WAFERLOG_LVGL_DIR "${lvgl_SOURCE_DIR}" CACHE PATH "LVGL source directory")
endif()

if(NOT TARGET lvgl)
    add_subdirectory("${WAFERLOG_LVGL_DIR}" lvgl)
endif()

if(EXISTS "${WAFERLOG_LOCAL_SDL2_DIR}/sdl2-config.cmake")
    set(SDL2_DIR "${WAFERLOG_LOCAL_SDL2_DIR}" CACHE PATH "SDL2 package directory")
endif()

find_package(SDL2 REQUIRED CONFIG)
if(SDL2_INCLUDE_DIRS)
    target_include_directories(lvgl PUBLIC ${SDL2_INCLUDE_DIRS})
endif()

add_executable(waferlog
    src/main.c
    src/waferlog_ui.c
    src/hal/hal.c
    src/mouse_cursor_icon.c
)

target_compile_definitions(waferlog PRIVATE LV_CONF_INCLUDE_SIMPLE)
target_include_directories(waferlog PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

if(TARGET SDL2::SDL2)
    target_link_libraries(waferlog PRIVATE lvgl SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
    target_link_libraries(waferlog PRIVATE lvgl SDL2::SDL2-static)
else()
    target_include_directories(waferlog PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(waferlog PRIVATE lvgl ${SDL2_LIBRARIES})
endif()

if(WIN32)
    target_link_options(waferlog PRIVATE "-mconsole")
endif()

set_target_properties(waferlog PROPERTIES
    LINKER_LANGUAGE CXX
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_custom_target(run
    COMMAND waferlog
    DEPENDS waferlog
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    USES_TERMINAL
)
