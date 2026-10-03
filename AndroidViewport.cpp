#include "gr/ui/AndroidViewport.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace gr::android {

void Viewport::fit(int sw, int sh) {
    screenW = sw;
    screenH = sh;

    if (sw <= 0 || sh <= 0) {
        dstX = dstY = dstW = dstH = 0;
        return;
    }

    //
    //  等比缩放：取宽高比例的较小者，保证画面不变形。
    //  用整数运算定尺寸，避免浮点误差导致画面超出屏幕一个像素
    //  （超出会让 ANativeWindow 的写入越界）。
    //
    const int numW = sw * kVirtualH;   // 按高铺满时所需宽度
    const int numH = sh * kVirtualW;   // 按宽铺满时所需高度

    if (numW >= numH) {
        //  高度受限：以高为准
        dstH = sh;
        dstW = (sh * kVirtualW) / kVirtualH;
    } else {
        //  宽度受限：以宽为准
        dstW = sw;
        dstH = (sw * kVirtualH) / kVirtualW;
    }

    //  再夹一次：整数除法可能让结果刚好等于边界，不会超，但保险起见
    dstW = std::min(dstW, sw);
    dstH = std::min(dstH, sh);

    dstX = (sw - dstW) / 2;
    dstY = (sh - dstH) / 2;
}

bool Viewport::toVirtual(float sx, float sy, float& vx, float& vy) const {
    if (dstW <= 0 || dstH <= 0) return false;

    const float rx = sx - static_cast<float>(dstX);
    const float ry = sy - static_cast<float>(dstY);
    if (rx < 0.0f || ry < 0.0f) return false;

    //  反算回虚拟坐标。用乘法而非除法，避免除零且精度更稳。
    const float inv = static_cast<float>(kVirtualW) / static_cast<float>(dstW);
    const float tx  = rx * inv;
    const float ty  = ry * inv;

    if (tx < 0.0f || ty < 0.0f ||
        tx >= static_cast<float>(kVirtualW) ||
        ty >= static_cast<float>(kVirtualH)) return false;

    vx = tx;
    vy = ty;
    return true;
}

void Viewport::toScreen(float vx, float vy, float& sx, float& sy) const {
    const float s = scale();
    sx = static_cast<float>(dstX) + vx * s;
    sy = static_cast<float>(dstY) + vy * s;
}

float PinchTracker::feed(int count, float x0, float y0, float x1, float y1) {
    if (count < 2) {
        //  不足两指：清零并复位 —— 避免中途抬起一指后剩余手指被误判
        clear();
        return 0.0f;
    }

    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float d  = std::sqrt(dx * dx + dy * dy);

    if (!active_ || dist_ <= 0.0f) {
        //  第二指刚落下：记基准，本帧不产生增量
        active_ = true;
        dist_   = d;
        return 0.0f;
    }

    const float delta = d - dist_;
    dist_ = d;
    return delta / kPixelsPerStep;
}

namespace {

//  最近邻缩放 + letterbox 贴屏。
//  Write 为写单个像素的可调用对象，两种像素格式共用同一套定位逻辑 ——
//  否则缩放与留边的坐标算式要写两遍，改一处漏一处。
template <typename Write>
void blitGeneric(const unsigned char* src, int srcW, int srcH,
                 unsigned char* dst, int dstW, int dstH, int dstStride,
                 const Viewport& vp, const Write& write) {
    if (!src || !dst || srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) return;
    if (vp.dstW <= 0 || vp.dstH <= 0) return;

    //  先把整屏填黑：ANativeWindow 缓冲内容未定义，且黑边本就该是黑的
    std::memset(dst, 0, static_cast<std::size_t>(dstStride) * dstH * write.bytesPerPixel);

    //  源 → 目标的缩放步长（16.16 定点，避免每像素浮点除法）
    const int sxStep = (srcW << 16) / vp.dstW;
    const int syStep = (srcH << 16) / vp.dstH;

    int syFp = 0;
    for (int j = 0; j < vp.dstH; ++j) {
        const int syi = syFp >> 16;
        syFp += syStep;
        if (syi < 0 || syi >= srcH) continue;

        const unsigned char* srcRow = src + static_cast<std::size_t>(syi) * srcW * 4;
        unsigned char* dstRow = dst + static_cast<std::size_t>((vp.dstY + j) * dstStride
                                                               + vp.dstX) * write.bytesPerPixel;

        int sxFp = 0;
        for (int i = 0; i < vp.dstW; ++i) {
            const int sxi = sxFp >> 16;
            sxFp += sxStep;
            if (sxi < 0 || sxi >= srcW) continue;

            const unsigned char* p = srcRow + static_cast<std::size_t>(sxi) * 4;
            write(dstRow + static_cast<std::size_t>(i) * write.bytesPerPixel,
                  p[0], p[1], p[2], p[3]);
        }
    }
}

struct WriteRGBA8888 {
    static constexpr int bytesPerPixel = 4;
    void operator()(unsigned char* d, unsigned char r, unsigned char g,
                    unsigned char b, unsigned char a) const {
        d[0] = r; d[1] = g; d[2] = b; d[3] = a;
    }
};

struct WriteRGB565 {
    static constexpr int bytesPerPixel = 2;
    void operator()(unsigned char* d, unsigned char r, unsigned char g,
                    unsigned char b, unsigned char /*a*/) const {
        //  Android 为小端：低字节在前，故按 R 在高位、B 在低位的 16 位值
        //  以本机字节序写入即为 RGB565 所需的布局。
        const std::uint16_t v = static_cast<std::uint16_t>(
            ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
        d[0] = static_cast<unsigned char>(v & 0xFF);
        d[1] = static_cast<unsigned char>((v >> 8) & 0xFF);
    }
};

} // namespace

void blitToRGBA8888(const unsigned char* src, int srcW, int srcH,
                    unsigned char* dst, int dstW, int dstH, int dstStride,
                    const Viewport& vp) {
    blitGeneric(src, srcW, srcH, dst, dstW, dstH, dstStride, vp, WriteRGBA8888{});
}

void blitToRGB565(const unsigned char* src, int srcW, int srcH,
                  unsigned char* dst, int dstW, int dstH, int dstStride,
                  const Viewport& vp) {
    blitGeneric(src, srcW, srcH, dst, dstW, dstH, dstStride, vp, WriteRGB565{});
}

} // namespace gr::android
