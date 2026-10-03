// ============================================================================
//  ImGui 的 Win32 + GDI 软件后端（Windows 原生，无需 GLFW / OpenGL）
//
//  为什么自己写后端：
//    官方后端依赖 GLFW + OpenGL3，而 mingw 交叉编译环境下没有现成的
//    Windows 版 GLFW 库可用。与其引入外部二进制依赖，不如直接走 Win32：
//      · 窗口   —— Win32 API（CreateWindowEx）
//      · 绘制   —— 复用已有的软件光栅化器，再用 GDI 的 StretchDIBits 贴到窗口
//      · 输入   —— 把 Win32 消息映射成 ImGui 的键鼠事件
//
//  好处是零外部依赖，编出来的 exe 拷到 Windows 上双击即可运行；
//  代价是性能不及 GPU —— 但本作界面是二维地图与面板，GDI 缩放位图足够。
//
//  本文件在非 Windows 平台编译为空壳，不影响 Linux / headless 构建。
// ============================================================================
#pragma once

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace gr {
namespace win32 {

// 一帧的呈现结果：由软件光栅化器产出，交由 GDI 贴上窗口
struct Backend {
    HWND      hwnd      = nullptr;
    HDC       hdc       = nullptr;
    int       width     = 1280;
    int       height    = 800;
    bool      quit      = false;

    // GDI 位图缓存（BGRA，top-down），避免每帧重新分配
    HBITMAP   dib       = nullptr;
    void*     dibBits   = nullptr;
    int       dibW      = 0;
    int       dibH      = 0;

    double    lastTime  = 0.0;
};

// 初始化：注册窗口类并创建窗口。失败返回 false。
bool create(Backend& b, int width, int height, const char* title);

// 泵一次消息，把键鼠事件喂给 ImGui。返回是否应继续运行。
bool pumpEvents(Backend& b);

// 把 RGBA（行优先、自顶向下）位图呈现到窗口。
//  src 长度须为 w*h*4。
void present(Backend& b, const unsigned char* rgba, int w, int h);

void destroy(Backend& b);

// 高精度计时（秒）
double nowSeconds();

} // namespace win32
} // namespace gr

#endif // _WIN32
