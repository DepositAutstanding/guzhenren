# ============================================================================
#  《蛊真人》Android 端 CMake
#
#  构建产物：libguzhenren.so
#  入口：android_main.cpp（android_native_app_glue）
#
#  说明：这里复用核心层的全部源码（SRCS + GUI_SRCS），
#  只把「窗口 / 输入 / 呈现」换成 Android 后端。
#  核心不含任何平台依赖，故同一套源码在 Linux 与 Android 上都能编译。
# ============================================================================
cmake_minimum_required(VERSION 3.22.1)
project(guzhenren LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------- 源码根目录 ----------
# 本文件位于 android/app/src/main/cpp/
get_filename_component(CPP_DIR ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)
get_filename_component(MAIN_DIR ${CPP_DIR}/.. ABSOLUTE)          # .../main
get_filename_component(APP_DIR  ${MAIN_DIR}/.. ABSOLUTE)         # .../app
get_filename_component(AND_DIR  ${APP_DIR}/.. ABSOLUTE)          # .../android
set(ROOT ${AND_DIR}/..)                                          # 工程根

# ---------- 核心源码（与桌面版共享） ----------
# 这里显式列出，避免把 tests/ 与 main_gui.cpp（含 Linux 专用入口）编进来。
file(GLOB_RECURSE CORE_SRCS
    ${ROOT}/src/ai/*.cpp
    ${ROOT}/src/battle/*.cpp
    ${ROOT}/src/core/*.cpp
    ${ROOT}/src/cultivator/*.cpp
    ${ROOT}/src/gu/*.cpp
    ${ROOT}/src/net/*.cpp
    ${ROOT}/src/sim/*.cpp
    ${ROOT}/src/ui/*.cpp
    ${ROOT}/src/world/*.cpp
)
# 桌面版入口、Win32 后端与测试不含在内
list(FILTER CORE_SRCS EXCLUDE REGEX ".*main_gui\\.cpp$")
list(FILTER CORE_SRCS EXCLUDE REGEX ".*main\\.cpp$")
list(FILTER CORE_SRCS EXCLUDE REGEX ".*Win32Backend\\.cpp$")
list(FILTER CORE_SRCS EXCLUDE REGEX ".*/tests/.*")

# ---------- ImGui（4 个文件） ----------
set(IMGUI_DIR ${ROOT}/third_party/imgui)
set(IMGUI_SRCS
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
)

# ---------- Android 端源码 ----------
set(ANDROID_SRCS
    ${CPP_DIR}/android_main.cpp
    ${CPP_DIR}/AndroidBackend.cpp
    ${CPP_DIR}/jni_bridge.cpp
    ${ROOT}/src/ui/AndroidViewport.cpp
)

# ---------- android_native_app_glue ----------
# 由 NDK 提供（含 ANativeActivity_onCreate，是 android_main 的调用方）。
#
# 注意：不同 NDK 版本里这个目录的位置与存在性并不一致
#  （部分新版本已移除或改路径）。这里在多个候选位置里查找，
#  找不到就给出明确报错 —— 否则症状是 APK 编出来却没有 .so，
#  运行时才在 System.loadLibrary 处抛 UnsatisfiedLinkError，
#  很难反推到 CMake 这一步。
set(GLUE_DIR "")
foreach(candidate
        "${ANDROID_NDK}/sources/android/native_app_glue"
        "${ANDROID_NDK_ROOT}/sources/android/native_app_glue"
        "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")
    if(EXISTS "${candidate}/android_native_app_glue.c")
        set(GLUE_DIR "${candidate}")
        break()
    endif()
endforeach()

if(GLUE_DIR STREQUAL "")
    message(FATAL_ERROR
        "未找到 android_native_app_glue.c（已查找 ANDROID_NDK / "
        "ANDROID_NDK_ROOT / CMAKE_ANDROID_NDK 下的 sources/android/native_app_glue）。"
        "请确认 NDK 已安装，或把该目录路径填入 CMakeLists.txt 的 GLUE_DIR。")
endif()
message(STATUS "native_app_glue: ${GLUE_DIR}")

add_library(native_app_glue STATIC ${GLUE_DIR}/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC ${GLUE_DIR})

# ---------- 主库 ----------
add_library(guzhenren SHARED
    ${CORE_SRCS}
    ${IMGUI_SRCS}
    ${ANDROID_SRCS}
)

target_include_directories(guzhenren PRIVATE
    ${ROOT}/include
    ${IMGUI_DIR}
    ${CPP_DIR}
)

target_compile_definitions(guzhenren PRIVATE
    GZR_ANDROID=1
)

# ---------- 链接 ----------
target_link_libraries(guzhenren
    android
    native_app_glue
    EGL
    GLESv2
    log
    z
)

# ---------- 发布版裁剪 ----------
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET guzhenren POST_BUILD
        COMMAND ${CMAKE_STRIP} $<TARGET_FILE:guzhenren>)
endif()
