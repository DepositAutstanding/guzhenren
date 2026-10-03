// ============================================================================
//  Android 平台后端
//
//  职责：把 Android 的窗口、触摸、资源、软键盘接到游戏的会话层与 ImGui 上。
//
//  为什么单独拆这一层：
//    游戏核心（GameSession / ImGuiApp / SoftRenderer）是纯 C++，
//    不认识任何平台。所有依赖 NDK 的东西都收在本文件里，
//    这样核心逻辑可以在 Linux 上编译、测试（见 tests/test_android.cpp
//    对 AndroidViewport 的覆盖），而不必等到真机才能发现问题。
//
//  渲染路线：
//    复用已有的 soft::Bitmap 软件光栅化器 —— 不引入 OpenGL。
//    ImGui 产出绘制指令 → renderDrawData 光栅化到 1440×900 的 RGBA 位图
//    → 按 letterbox 等比缩放贴到 ANativeWindow 的像素缓冲。
//    虚拟分辨率固定为 1440×900（与桌面版一致），
//    界面布局因此无需为手机重写一套。
// ============================================================================
#include "gr/ui/AndroidBackend.hpp"

#include <android/input.h>
#include <android/native_window.h>
#include <android/asset_manager.h>
#include <android/looper.h>
#include <android/log.h>

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "imgui.h"
#include "gr/ui/SoftRenderer.hpp"
#include "gr/ui/AndroidViewport.hpp"

#define GZR_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "GuZhenRen", __VA_ARGS__)
#define GZR_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "GuZhenRen", __VA_ARGS__)

namespace gr::android {
namespace {

//  ANativeWindow_Buffer.format 的常见取值（不同 NDK 版本常量名略有出入，
//  这里直接写数值，避免依赖特定头文件版本）
constexpr int WINDOW_FORMAT_RGBA_8888 = 1;
constexpr int WINDOW_FORMAT_RGBX_8888 = 2;
constexpr int WINDOW_FORMAT_RGB_565   = 4;

} // namespace

void AndroidBackend::init(ANativeActivity* activity) {
    activity_ = activity;
    if (activity_) assets_ = activity->assetManager;
}

// ---------------------------------------------------------------------------
//  资源读取
//
//  Android 的 assets 打包在 APK 里，没有常规文件路径，
//  必须走 AAssetManager；fopen("/assets/...") 一定失败。
//  故字体不能沿用桌面版的 AddFontFromFileTTF。
// ---------------------------------------------------------------------------
bool AndroidBackend::readAsset(const char* path, std::vector<unsigned char>& out) {
    if (!assets_) return false;
    AAsset* a = AAssetManager_open(assets_, path, AASSET_MODE_BUFFER);
    if (!a) { GZR_LOGE("资源缺失: %s", path); return false; }

    const off_t len = AAsset_getLength(a);
    out.resize(static_cast<std::size_t>(len));
    if (len > 0) {
        const int got = AAsset_read(a, out.data(), static_cast<size_t>(len));
        if (got != static_cast<int>(len)) {
            AAsset_close(a);
            out.clear();
            GZR_LOGE("资源读取不完整: %s", path);
            return false;
        }
    }
    AAsset_close(a);
    return true;
}

// ---------------------------------------------------------------------------
//  字体加载
//
//  桌面版从文件路径加载；Android 只能从内存加载。
//  另外 ImGui 的中文显示必须有 CJK 字形 —— 系统字体不带中文会全是方块，
//  所以 APK 里必须自带一份字体。
// ---------------------------------------------------------------------------
bool AndroidBackend::loadFont(ImFontAtlas* atlas, float sizePx) {
    //  字体文件复制到 assets/fonts/ 下（由构建脚本完成）
    const char* candidates[] = {
        "fonts/AlibabaPuHuiTi-2-105-Heavy.ttf",
        "fonts/MiSans-Heavy.ttf",
    };
    for (const char* p : candidates) {
        std::vector<unsigned char> data;
        if (!readAsset(p, data)) continue;
        if (data.empty()) continue;

        ImFontConfig cfg;
        cfg.FontDataOwnedByAtlas = false;   // data 由本对象持有，不能让图集释放
        ImFont* f = atlas->AddFontFromMemoryTTF(
            data.data(), static_cast<int>(data.size()), sizePx, &cfg,
            atlas->GetGlyphRangesChineseFull());
        if (f) {
            fontBuffers_.push_back(std::move(data));   // 必须长期存活
            GZR_LOGI("已加载字体 %s (%.1f px)", p, sizePx);
            return true;
        }
        GZR_LOGE("字体解析失败: %s", p);
    }
    GZR_LOGE("未找到任何可用中文字体");
    return false;
}

// ---------------------------------------------------------------------------
//  窗口准备
// ---------------------------------------------------------------------------
bool AndroidBackend::acquireWindow(ANativeWindow* win) {
    if (!win) return false;
    window_ = win;
    //  固定按 RGBA8888 申请；部分设备不支持时由 setBuffersGeometry 协商
    ANativeWindow_setBuffersGeometry(win, 0, 0, WINDOW_FORMAT_RGBA_8888);
    return true;
}

void AndroidBackend::releaseWindow() {
    if (window_) {
        ANativeWindow_release(window_);
        window_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
//  呈现：把虚拟分辨率位图按 letterbox 贴到物理窗口
// ---------------------------------------------------------------------------
void AndroidBackend::present(const soft::Bitmap& bmp) {
    if (!window_ || bmp.pixels.empty()) return;

    ANativeWindow_Buffer nb;
    //  注意：region 传 nullptr 表示锁定整块缓冲
    if (ANativeWindow_lock(window_, &nb, nullptr) < 0) {
        GZR_LOGE("锁定窗口缓冲失败");
        return;
    }
    if (!nb.bits) {
        ANativeWindow_unlockAndPost(window_);
        return;
    }

    viewport_.fit(nb.width, nb.height);

    if (nb.format == WINDOW_FORMAT_RGB_565) {
        blitToRGB565(bmp.pixels.data(), bmp.width, bmp.height,
                     static_cast<unsigned char*>(nb.bits),
                     nb.width, nb.height, nb.stride, viewport_);
    } else {
        //  RGBA8888 / RGBX8888 均为每像素 4 字节，走同一条路径
        blitToRGBA8888(bmp.pixels.data(), bmp.width, bmp.height,
                       static_cast<unsigned char*>(nb.bits),
                       nb.width, nb.height, nb.stride, viewport_);
    }

    ANativeWindow_unlockAndPost(window_);
}

// ---------------------------------------------------------------------------
//  触摸事件 → ImGui
//
//  屏幕物理坐标要先反变换回 1440×900 的虚拟坐标，
//  否则界面布局（左侧 320px 面板等）会与手指位置对不上。
// ---------------------------------------------------------------------------
bool AndroidBackend::handleMotion(AInputEvent* ev) {
    if (!ev) return false;

    const int action = AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_MASK;
    const size_t count = AMotionEvent_getPointerCount(ev);

    //  双指捏合 → 滚轮增量（地图缩放）
    if (count >= 2) {
        const float x0 = AMotionEvent_getX(ev, 0), y0 = AMotionEvent_getY(ev, 0);
        const float x1 = AMotionEvent_getX(ev, 1), y1 = AMotionEvent_getY(ev, 1);
        const float wheel = pinch_.feed(2, x0, y0, x1, y1);
        ImGuiIO& io = ImGui::GetIO();
        io.MouseWheel += wheel;
        //  双指时不同步鼠标位置，避免焦点乱跳
        return true;
    }

    //  单指：按下 / 移动 / 抬起
    const float sx = AMotionEvent_getX(ev, 0);
    const float sy = AMotionEvent_getY(ev, 0);

    float vx = 0, vy = 0;
    const bool inside = viewport_.toVirtual(sx, sy, vx, vy);

    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(vx, vy);

    switch (action) {
        case AMOTION_EVENT_ACTION_DOWN:
            //  点黑边不触发 —— 否则点画面外也会命中画面内的某个控件
            if (inside) io.MouseDown[0] = true;
            return true;
        case AMOTION_EVENT_ACTION_UP:
            io.MouseDown[0] = false;
            pinch_.feed(1, 0, 0, 0, 0);   // 抬指重置捏合基准
            return true;
        case AMOTION_EVENT_ACTION_MOVE:
            io.MouseDown[0] = inside;     // 移出画面即视为松开
            return true;
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
//  键盘事件 → ImGui
//
//  注意：NativeActivity 的原生 IME 不支持中文输入。
//  中文（开局取名、假名）由 Java 侧的 EditText 覆盖层输入，
//  通过 nativeOnCommitText 回调送进来。
// ---------------------------------------------------------------------------
bool AndroidBackend::handleKey(AInputEvent* ev) {
    if (!ev) return false;
    ImGuiIO& io = ImGui::GetIO();

    const int32_t keyCode = AKeyEvent_getKeyCode(ev);
    const int32_t action  = AKeyEvent_getAction(ev);
    const bool down = (action == AKEY_EVENT_ACTION_DOWN);

    //  返回键：先让 ImGui 消费（关闭弹窗），否则退出
    if (keyCode == AKEYCODE_BACK) {
        if (down) io.AddKeyEvent(ImGuiKey_Escape, true);
        else     io.AddKeyEvent(ImGuiKey_Escape, false);
        return true;
    }
    //  删除键
    if (keyCode == AKEYCODE_DEL) {
        if (down) io.AddKeyEvent(ImGuiKey_Backspace, true);
        else     io.AddKeyEvent(ImGuiKey_Backspace, false);
        return true;
    }
    //  回车
    if (keyCode == AKEYCODE_ENTER) {
        if (down) io.AddKeyEvent(ImGuiKey_Enter, true);
        else     io.AddKeyEvent(ImGuiKey_Enter, false);
        return true;
    }
    //  方向键 —— 手机上没有，但外接键盘/模拟器可用，顺手映射
    auto mapDir = [](int32_t kc) -> ImGuiKey {
        switch (kc) {
            case AKEYCODE_DPAD_LEFT:  return ImGuiKey_LeftArrow;
            case AKEYCODE_DPAD_RIGHT: return ImGuiKey_RightArrow;
            case AKEYCODE_DPAD_UP:    return ImGuiKey_UpArrow;
            case AKEYCODE_DPAD_DOWN:  return ImGuiKey_DownArrow;
            default: return ImGuiKey_None;
        }
    };
    if (const ImGuiKey k = mapDir(keyCode); k != ImGuiKey_None) {
        io.AddKeyEvent(k, down);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
//  中文输入回调（由 Java 侧调用）
// ---------------------------------------------------------------------------
void AndroidBackend::commitText(const char* text) {
    if (!text || !*text) return;
    ImGuiIO& io = ImGui::GetIO();
    //  逐字符加入，AddInputCharactersUTF8 会正确处理 UTF-8 多字节序列
    io.AddInputCharactersUTF8(text);
}

void AndroidBackend::setImeVisible(bool v) { imeVisible_ = v; }

} // namespace gr::android
