// ============================================================================
//  Android 平台后端 —— 声明
//
//  本文件【不含任何 Android/NDK 头文件】，故可以在 Linux 上被包含，
//  便于把不依赖平台的逻辑（Viewport、PinchTracker）纳入单元测试。
//  NDK 相关的类型以前置声明出现，只在 .cpp 里才真正引入。
// ============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "gr/ui/AndroidViewport.hpp"

//  NDK 类型前置声明 —— 避免本头文件把整个 NDK 拖进来
struct ANativeActivity;
struct ANativeWindow;
struct AInputEvent;
struct AAssetManager;
struct ImFontAtlas;

namespace gr::soft { struct Bitmap; }

namespace gr::android {

class AndroidBackend {
public:
    void init(ANativeActivity* activity);

    //  从 APK 的 assets 读入内存（Android 没有常规文件路径，不能 fopen）
    bool readAsset(const char* path, std::vector<unsigned char>& out);

    //  加载自带的中文字体。
    //  系统字体不含 CJK 字形时中文会显示成方块，故 APK 必须自带字体。
    bool loadFont(ImFontAtlas* atlas, float sizePx);

    bool acquireWindow(ANativeWindow* win);
    void releaseWindow();

    //  把虚拟分辨率位图按 letterbox 贴到物理窗口
    void present(const gr::soft::Bitmap& bmp);

    //  触摸 / 按键 → ImGui。返回 true 表示事件已被消费。
    bool handleMotion(AInputEvent* ev);
    bool handleKey(AInputEvent* ev);

    //  Java 侧中文输入法回调
    void commitText(const char* text);
    void setImeVisible(bool v);
    bool imeVisible() const { return imeVisible_; }

    const Viewport& viewport() const { return viewport_; }

private:
    ANativeActivity* activity_ = nullptr;
    ANativeWindow*   window_   = nullptr;
    AAssetManager*   assets_   = nullptr;

    Viewport      viewport_;
    PinchTracker  pinch_;

    //  字体数据必须长期存活 —— ImGui 的图集只是引用它
    //  （FontDataOwnedByAtlas = false），若此处提前释放会读到野指针。
    std::vector<std::vector<unsigned char>> fontBuffers_;

    bool imeVisible_ = false;
};

} // namespace gr::android
