// ============================================================================
//  android_main —— Android 入口（android_native_app_glue 的 standard 入口）
//
//  主循环照搬桌面版 main_gui.cpp 的模式，只把「窗口 / 输入 / 呈现」
//  换成 Android 后端：
//
//    ImGui::NewFrame
//      → app.setDisplaySize(1440, 900)
//      → session.tickRefinePrepare(dt)
//      → app.buildFrame()
//      → ImGui::Render
//      → soft::renderDrawData(bmp)      软件光栅化到 1440×900 RGBA
//      → backend.present(bmp)           letterbox 贴到物理窗口
//      → app.takeCommand() → session.execute()
//
//  虚拟分辨率固定 1440×900：界面布局（左 320px 面板、下 136px 图例等）
//  是按这个尺寸排的，固定下来就不必为手机重写一套布局。
// ============================================================================
#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/native_window.h>

#include <chrono>
#include <string>

#include "imgui.h"
#include "gr/ui/SoftRenderer.hpp"
#include "gr/ui/ImGuiApp.hpp"
#include "gr/ui/GameSession.hpp"
#include "gr/ui/AndroidViewport.hpp"
#include "AndroidBackend.hpp"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "GuZhenRen", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "GuZhenRen", __VA_ARGS__)

namespace {

//  与桌面版一致的虚拟分辨率
constexpr int   kVW = gr::android::kVirtualW;   // 1440
constexpr int   KVH = gr::android::kVirtualH;   // 900

void setupImGui(ImGuiIO& io) {
    io.DisplaySize = ImVec2(static_cast<float>(kVW), static_cast<float>(KVH));
    io.IniFilename = nullptr;       // 不写 imgui.ini（Android 无家目录概念）
    io.LogFilename = nullptr;
    //  手机上没有鼠标，但需要鼠标驱动 ImGui 的控件
    io.MouseDrawCursor = false;
    //  软件渲染路径下，字体图集由 SoftRenderer 采样，TexID 给个非 0 值即可
    io.Fonts->TexID = (ImTextureID)(intptr_t)1;
}

//  提取字体图集的 Alpha8 像素，供软件光栅化器采样
//  （与桌面版 main_gui.cpp 的处理一致）
void buildFontAtlas(const std::uint8_t*& pixels, int& w, int& h) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Build();
    io.Fonts->GetTexDataAsAlpha8(&pixels, &w, &h);
}

} // namespace

void android_main(struct android_app* app) {
    LOGI("蛊真人：Android 入口启动");

    gr::android::AndroidBackend backend;
    backend.init(app->activity);

    //  外存目录：探索进度、存档落在应用私有目录，随卸载清除
    std::string cacheDir = app->activity->internalDataPath
                         ? std::string(app->activity->internalDataPath) + "/tilemap"
                         : std::string("cache/tilemap");

    //  ImGui 初始化
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    setupImGui(io);

    //  字体：Android 只能从 assets 内存加载
    if (!backend.loadFont(io.Fonts, 22.0f)) {
        LOGE("字体加载失败，中文将无法显示");
    }

    const std::uint8_t* fontPixels = nullptr;
    int fontW = 0, fontH = 0;
    buildFontAtlas(fontPixels, fontW, fontH);

    //  会话层
    gr::GameSession session(20240906, cacheDir);
    gr::gui::App    gui(session);
    gui.setDisplaySize(static_cast<float>(kVW), static_cast<float>(KVH));

    gr::soft::Bitmap bmp;
    bmp.resize(kVW, KVH);

    //  等待窗口就绪
    while (app->window == nullptr) {
        int events = 0;
        struct android_poll_source* src = nullptr;
        const int id = ALooper_pollAll(-1, nullptr, &events,
                                       reinterpret_cast<void**>(&src));
        if (id >= 0 && src) src->process(app, src);
        if (app->destroyRequested) { ImGui::DestroyContext(); return; }
    }
    backend.acquireWindow(app->window);
    LOGI("窗口就绪 %dx%d", ANativeWindow_getWidth(app->window),
         ANativeWindow_getHeight(app->window));

    app->userData = &backend;   // 供 Java 侧 JNI 回调查找

    using Clock = std::chrono::steady_clock;
    auto last = Clock::now();

    //  ---------------- 主循环 ----------------
    while (!app->destroyRequested) {
        int events = 0;
        struct android_poll_source* src = nullptr;

        //  非阻塞取事件；取完就渲染一帧
        while (ALooper_pollAll(0, nullptr, &events,
                               reinterpret_cast<void**>(&src)) >= 0) {
            if (src) src->process(app, src);

            //  窗口变更：重建 / 旋转 / 尺寸变化
            if (app->window != nullptr &&
                ANativeWindow_getWidth(app->window) > 0) {
                //  acquireWindow 是幂等的：重复调用只会重新协商缓冲格式
                backend.acquireWindow(app->window);
            }
        }

        const auto now = Clock::now();
        const float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        io.DeltaTime = dt > 0.0f ? dt : 1.0f / 60.0f;

        //  一帧
        ImGui::NewFrame();
        session.tickRefinePrepare(dt);
        gui.buildFrame();
        ImGui::Render();

        bmp.clear(16, 19, 25, 255);
        gr::soft::renderDrawData(ImGui::GetDrawData(), bmp,
                                 fontPixels, fontW, fontH);
        backend.present(bmp);

        //  界面产生的指令交给规则层执行
        if (auto pc = gui.takeCommand(); pc.has) {
            session.execute(pc.cmd);
        }
    }

    backend.releaseWindow();
    ImGui::DestroyContext();
    LOGI("蛊真人：退出");
}
