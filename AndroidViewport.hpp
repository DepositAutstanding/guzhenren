// ============================================================================
//  AndroidViewport —— Android 呈现层中与平台无关的【纯逻辑】
//
//  为什么要单独抽出来：
//
//    游戏界面的布局是按 1440×900 设计并逐像素调过的（面板宽度 320、
//    图例高 136、炼蛊台只压主区……）。若让 Android 直接按物理分辨率
//    走一遍布局，等于把全部布局重做一次，且两边必然对不齐。
//
//    所以 Android 上仍渲染 1440×900 的【虚拟画面】，再整幅缩放贴到
//    物理屏上，留黑边（letterbox）。布局代码一行不改，两边永远一致。
//
//    于是「怎么缩放、怎么留黑边、触摸坐标怎么反算回去」就成了全部
//    平台差异，也正好是可以脱离 NDK 单独测的纯数学。放在这里，
//    便能在 Linux 上编译并写单元测试；真正依赖 <android/*.h> 的部分
//    在 android/app/src/main/cpp/AndroidBackend.cpp，只在 NDK 下编。
//
//  本文件不含任何平台头文件，宿主与 Android 通用。
// ============================================================================
#pragma once

#include <cstddef>

namespace gr::android {

//  虚拟画面尺寸：界面布局的基准分辨率
constexpr int kVirtualW = 1440;
constexpr int kVirtualH = 900;

// ---------------------------------------------------------------------------
//  视口：虚拟画面在物理屏上的位置与缩放
//
//  采用「等比缩放 + 居中留边」—— 不拉伸变形。
//  例：手机横屏 2400×1080
//      scale = min(2400/1440, 1080/900) = min(1.667, 1.200) = 1.200
//      画面 1728×1080 居中，左右各留 336 黑边。
// ---------------------------------------------------------------------------
struct Viewport {
    int screenW = 0;   // 物理屏宽（像素）
    int screenH = 0;   // 物理屏高
    int dstX = 0;      // 画面左上角在屏上的位置
    int dstY = 0;
    int dstW = 0;      // 画面在屏上的实际尺寸
    int dstH = 0;

    //  依物理屏尺寸重算 letterbox。sw/sh 非正时全部归零。
    void fit(int sw, int sh);

    float scale() const {
        if (dstW <= 0 || dstH <= 0) return 0.0f;
        return static_cast<float>(dstW) / static_cast<float>(kVirtualW);
    }

    //  物理屏坐标 → 虚拟画面坐标（黑边内返回 false，此时 vx/vy 不写）
    bool toVirtual(float sx, float sy, float& vx, float& vy) const;

    //  虚拟画面坐标 → 物理屏坐标
    void toScreen(float vx, float vy, float& sx, float& sy) const;
};

// ---------------------------------------------------------------------------
//  双指捏合 → 滚轮增量
//
//  地图缩放原本靠滚轮（滚轮以光标所在格为锚点）。触屏没有滚轮，
//  故以双指距离变化模拟：张开即放大，捏合即缩小。
//
//  单指移动【不】产生滚轮 —— 否则拖动地图时会误触发缩放。
// ---------------------------------------------------------------------------
class PinchTracker {
public:
    //  每变化多少像素算一格滚轮。手机屏上 24 像素约等于一格手感。
    static constexpr float kPixelsPerStep = 24.0f;

    void clear() { active_ = false; dist_ = 0.0f; }

    //  传入当前触点（最多取前两个），返回本帧滚轮增量。
    //  count < 2 时返回 0 并重置 —— 手指离开后再次按下不应瞬间缩放。
    float feed(int count, float x0, float y0, float x1, float y1);

    float distance() const { return dist_; }
    bool  active()  const { return active_; }

private:
    bool  active_ = false;
    float dist_   = 0.0f;
};

// ---------------------------------------------------------------------------
//  贴屏：把虚拟画面（RGBA8888）缩放并居中写进目标缓冲
//
//  参数中的 vp 已含 letterbox 结果，故黑边由本函数一并填黑 ——
//  ANativeWindow_lock 拿到的缓冲内容未定义，不清会残留上一帧或噪声。
//
//  stride 单位是【像素数】而非字节数（ANativeWindow_Buffer.stride 即如此）。
//
//  缩放取最近邻：手机上每帧要处理百万级像素，双线性在软渲染下吃不消，
//  而放大倍率通常在 1.2~2.0，最近邻的锯齿在静态界面上可接受。
// ---------------------------------------------------------------------------
void blitToRGBA8888(const unsigned char* src, int srcW, int srcH,
                    unsigned char* dst, int dstW, int dstH, int dstStride,
                    const Viewport& vp);

void blitToRGB565(const unsigned char* src, int srcW, int srcH,
                  unsigned char* dst, int dstW, int dstH, int dstStride,
                  const Viewport& vp);

} // namespace gr::android
