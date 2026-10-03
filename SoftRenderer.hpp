// ============================================================================
//  ImGui 软件光栅化后端（headless 环境专用）
//
//  为什么需要它：ImGui 核心是平台无关的，只负责生成 ImDrawData（顶点+索引+命令），
//  真正的绘制由后端完成。官方后端依赖 OpenGL / DirectX / Metal，
//  而 CI / 容器等 headless 环境没有这些，也就无法编译运行、无法截图验证。
//
//  本后端用纯 CPU 光栅化把 ImDrawData 画进一张 RGBA 位图：
//    · 不依赖 OpenGL、GLFW、X11 中的任何一个
//    · 支持裁剪矩形、顶点色、alpha 混合、字体图集采样
//    · 输出 PPM 便于外部工具转 PNG
//
//  它不是给玩家用的渲染器（性能远不及 GPU），而是工程验证设施：
//  让 UI 布局在无显示器的机器上也能被自动化检查与人工审阅。
// ============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ImDrawData;

namespace gr {
namespace soft {

struct Bitmap {
    int width = 0, height = 0;
    std::vector<std::uint8_t> pixels;   // RGBA8，行优先，长度 = w*h*4

    void resize(int w, int h) {
        width = w; height = h;
        pixels.assign(static_cast<std::size_t>(w) * h * 4, 0);
    }
    void clear(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);
    void setPixel(int x, int y, std::uint8_t r, std::uint8_t g,
                  std::uint8_t b, std::uint8_t a);
};

// 把 ImGui 一帧的绘制数据光栅化到 bmp
//  fontPixels / fontW / fontH 为字体图集（Alpha8）
void renderDrawData(ImDrawData* drawData, Bitmap& bmp,
                    const std::uint8_t* fontPixels, int fontW, int fontH);

// 写出 PPM（P6）。PPM 无需压缩库，便于后续用外部工具转 PNG。
bool writePPM(const std::string& path, const Bitmap& bmp);

// 写出 BMP（自带格式，可直接查看，无需转换）
bool writeBMP(const std::string& path, const Bitmap& bmp);

} // namespace soft
} // namespace gr
