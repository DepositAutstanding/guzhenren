// ============================================================================
//  ImGui 前端入口
//
//  两种运行方式，共用同一套界面代码：
//
//    1) 真机窗口（默认，需 GLFW + OpenGL3）
//         ./gu_gui
//
//    2) 离屏渲染（headless，无需任何图形库）
//         ./gu_gui --headless --out frame.ppm
//       用软件光栅化后端把界面画成图片，用于在无显示器环境下验证布局。
//
//  编译期由 HAVE_GLFW 宏决定是否编入 GLFW 后端代码，
//  因此该源文件在两种环境下都能通过编译。
// ============================================================================

#include "imgui.h"
#include "gr/ui/GameSession.hpp"
#include "gr/ui/ImGuiApp.hpp"
#include "gr/ui/SoftRenderer.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef HAVE_GLFW
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#endif

#ifdef _WIN32
#include "gr/ui/Win32Backend.hpp"
#endif

namespace {

// 中文字体：ImGui 默认图集不含汉字，不加载则全文显示为方块。
// 按优先级依次尝试，全部失败时回退默认字体（此时中文不可读，但程序仍可运行）。
const char* const kCjkFonts[] = {
#ifdef _WIN32
    // Windows：优先微软雅黑（界面字体，字形清晰），回退黑体 / 宋体
    "C:/Windows/Fonts/msyh.ttc",
    "C:/Windows/Fonts/msyhl.ttc",
    "C:/Windows/Fonts/simhei.ttf",
    "C:/Windows/Fonts/simsun.ttc",
#endif
    "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
    "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
    "/usr/share/fonts/truetype/misans/MiSans-Heavy.ttf",
    "/usr/share/fonts/truetype/alibaba-puhuiti/AlibabaPuHuiTi-2-75-SemiBold.ttf",
    "/usr/share/fonts/opentype/noto/NotoSerifCJK-Bold.ttc",
};

void setupStyle() {
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding    = 4.0f;
    st.FrameRounding     = 3.0f;
    st.GrabRounding      = 3.0f;
    st.WindowTitleAlign  = ImVec2(0.02f, 0.5f);
    st.ItemSpacing       = ImVec2(8, 5);

    ImVec4* c = st.Colors;
    c[ImGuiCol_WindowBg]    = ImVec4(0.09f, 0.10f, 0.13f, 1.00f);
    c[ImGuiCol_TitleBg]     = ImVec4(0.13f, 0.15f, 0.19f, 1.00f);
    c[ImGuiCol_TitleBgActive]=ImVec4(0.17f, 0.20f, 0.26f, 1.00f);
    c[ImGuiCol_MenuBarBg]   = ImVec4(0.13f, 0.15f, 0.19f, 1.00f);
    c[ImGuiCol_Header]      = ImVec4(0.20f, 0.24f, 0.31f, 1.00f);
    c[ImGuiCol_HeaderHovered]=ImVec4(0.26f, 0.31f, 0.40f, 1.00f);
    c[ImGuiCol_HeaderActive]= ImVec4(0.31f, 0.37f, 0.47f, 1.00f);
    c[ImGuiCol_Button]      = ImVec4(0.20f, 0.24f, 0.31f, 1.00f);
    c[ImGuiCol_ButtonHovered]=ImVec4(0.28f, 0.34f, 0.44f, 1.00f);
    c[ImGuiCol_ButtonActive]= ImVec4(0.34f, 0.41f, 0.53f, 1.00f);
    c[ImGuiCol_FrameBg]     = ImVec4(0.15f, 0.17f, 0.22f, 1.00f);
    c[ImGuiCol_Tab]         = ImVec4(0.16f, 0.19f, 0.25f, 1.00f);
    c[ImGuiCol_TabActive]   = ImVec4(0.24f, 0.29f, 0.37f, 1.00f);
    c[ImGuiCol_Border]      = ImVec4(0.22f, 0.25f, 0.31f, 1.00f);
}

// 返回是否成功加载中文字体
bool loadCjkFont(float sizePx) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    bool loaded = false;
    for (const char* path : kCjkFonts) {
        if (FILE* f = std::fopen(path, "rb")) {
            std::fclose(f);
            ImFontConfig cfg;
            cfg.OversampleH = 2;
            cfg.OversampleV = 2;
            cfg.PixelSnapH  = true;
            // 先加载拉丁字符，再叠加中文，保证英文数字用清晰字形
            static const ImWchar latin[] = {
                0x0020, 0x00FF,   // 基本拉丁 + 拉丁补充
                0x2000, 0x206F,   // 通用标点
                0x3000, 0x303F,   // CJK 标点
                0,
            };
            io.Fonts->AddFontFromFileTTF(path, sizePx, &cfg, latin);
            cfg.MergeMode = true;
            io.Fonts->AddFontFromFileTTF(path, sizePx, &cfg,
                                         io.Fonts->GetGlyphRangesChineseFull());
            loaded = true;
            break;   // 静默成功：不污染 stdout
        }
    }
    if (!loaded) {
        io.Fonts->AddFontDefault();
        std::fprintf(stderr, "[警告] 未找到中文字体，界面中文将显示为方块\n");
    }
    io.Fonts->Build();
    return loaded;
}

// 让玩家先做几个动作，使界面（日志、三气、坐标库）有内容可看
void seedSession(gr::GameSession& s) {
    s.pushLog(true, "—— 第六卷开局：疯魔窟大战结束后 ——");
    s.advance(3);

    // 观察两处地点，为定仙游积累坐标
    auto sites = s.siteViews();
    int observed = 0;
    for (const auto& site : sites) {
        if (observed >= 3) break;
        if (site.isHome || site.known) continue;
        gr::Command c;
        c.kind = gr::CmdKind::Observe;
        c.siteId = site.siteId;
        s.execute(c);
        ++observed;
    }

    // 搜集一次三气，让三气条出现数值
    {
        gr::Command c;
        c.kind = gr::CmdKind::GatherQi;
        c.ticks = 1;
        s.execute(c);
    }
    s.advance(2);
}

// 演示脚本：模拟玩家一轮完整操作，用于离屏渲染出「操作之后」的界面。
// 顺序刻意覆盖三气闭环：外出搜集 → 返回福地 → 闭关调和 → 解除闭关。
// --cave <siteId>：指定演示进入哪处洞天（出样图时用）
static std::string g_caveId;
//  --startstep <0..3>：开局流程直接跳到该步出样图
//    0=选出生地 1=选身份（含降生之地）2=开窍 3=定名
static int g_startStep = -1;
static bool g_refineDemo = false;
static std::string g_gotoLandmark;   // --goto <地标id>：直接站到该地标（出样图用）

void runDemoScript(gr::GameSession& s) {
    s.pushLog(true, "=== 演示：一轮三气闭环 ===");

    auto sites = s.siteViews();

    // ① 观察若干地点，扩充定仙游坐标库
    int observed = 0;
    for (const auto& site : sites) {
        if (observed >= 4) break;
        if (site.known || site.isHome) continue;
        gr::Command c; c.kind = gr::CmdKind::Observe; c.siteId = site.siteId;
        s.execute(c);
        ++observed;
    }

    // ② 对首个已知坐标尝试定仙游（验证「只能跳已知坐标」的规则）
    for (const auto& site : sites) {
        if (!site.known) continue;
        gr::Command c; c.kind = gr::CmdKind::Jump; c.siteId = site.siteId;
        s.execute(c);
        break;
    }

    // ③ 外出搜集三气
    {
        gr::Command c; c.kind = gr::CmdKind::GatherQi; c.ticks = 3;
        s.execute(c);
    }

    // ④ 返回自家福地闭关
    {
        gr::Command c; c.kind = gr::CmdKind::EnterSeclusion;
        s.execute(c);
    }

    // ⑤ 闭关中推进调和（闭关期间移动类指令应被锁死）
    {
        gr::Command c; c.kind = gr::CmdKind::Cultivate; c.ticks = 6;
        s.execute(c);
    }

    // ⑥ 验证闭关锁：此时移动应被拒绝
    for (const auto& site : sites) {
        if (!site.known) continue;
        gr::Command c; c.kind = gr::CmdKind::MoveTo; c.siteId = site.siteId;
        s.execute(c);
        break;
    }

    // ⑦ 解除闭关
    {
        gr::Command c; c.kind = gr::CmdKind::LeaveSeclusion;
        s.execute(c);
    }

    // ⑧ 推进世界，让 NPC 自主行动
    s.advance(5);

    // ⑨ 在二维地图上探索若干处，揭示周边地形
    //    （地表探索与洞天列表是两套并行的坐标体系）
    const int W = s.tileMap().width(), H = s.tileMap().height();
    const int stops[][2] = {
        {W / 2, H / 2},           // 中洲
        {W / 2, H / 6},           // 北原
        {W / 6, H / 2},           // 西漠
        {W * 5 / 6, H / 2},       // 东海
        {W / 2, H * 5 / 6},       // 南疆
    };
    for (const auto& st : stops) {
        gr::Command c; c.kind = gr::CmdKind::MoveOnMap;
        c.mapX = st[0]; c.mapY = st[1];
        s.execute(c);
    }

    // ⑨b 炼蛊台演示：开炉 → 候炉 2 秒 → 切地图盖灰层
    if (g_refineDemo) {
        for (const gr::GuRecipe* r : s.world().refinery().recipes()) {
            if (!r || r->isImmortal()) continue;    // 凡蛊方开局即懂
            gr::Command c; c.kind = gr::CmdKind::RefineOpen; c.refineRecipe = r->id;
            auto rr = s.execute(c);
            if (rr.ok) break;
        }
        for (int i = 0; i < 6; ++i) s.tickRefinePrepare(0.5);   // 走完 2 秒候炉
    }

    // ⑨‑b 站到指定地标（出样图用：展示聚落建筑面板）
    if (!g_gotoLandmark.empty()) {
        const gr::Landmark* lm = nullptr;
        //
        //  既认 id（shangliangshan）也认中文名（商家城）。
        //  原先只比 id，而出样图脚本传的是中文名，
        //  于是 --goto 静默失效 —— 拍出来的仍是默认世界地图，
        //  几张「聚落」「出生点」样图因此长得一模一样。
        //
        for (const auto& x : s.tileMap().landmarks())
            if (x.id == g_gotoLandmark || x.name == g_gotoLandmark) { lm = &x; break; }
        if (lm) {
            gr::Command mv; mv.kind = gr::CmdKind::MoveOnMap;
            mv.mapX = lm->x; mv.mapY = lm->y;
            s.execute(mv);
        }
    }

    // ⑩ 进入一处洞天：洞天内部是另一张地图（秘境）
    //    疯魔窟九层嵌套，最能体现「洞天自有天地」
    // --cave <siteId> 可指定进入哪处洞天（出样图时用）
    if (!g_caveId.empty()) {
        s.enterCave(g_caveId);
    } else if (!s.enterCave("cn_fengmo_ku").ok) {
        s.enterCave("nj_huxian_fudi");
    }

    // ⑪ 自创炼蛊：以手中之蛊推演前所未有之蛊，成则命名存档
    //    成功率取决于炼道造诣，失败会炸炉 —— 故多试几次
    {
        gr::Cultivator* pl = s.world().player();
        if (pl) {
            for (int attempt = 0; attempt < 60; ++attempt) {
                while (pl->carriedGu.size() < 2) {
                    gr::GuInstance g;
                    g.templateId = 1; g.holder = pl->id;
                    pl->carriedGu.push_back(g);
                }
                if (pl->essence < 300.0) pl->essence = 2000.0;
                gr::Command c; c.kind = gr::CmdKind::Innovate;
                c.components = {0, 1};
                s.execute(c);
                if (s.pendingInnovation().pending) break;
            }
            if (s.pendingInnovation().pending) {
                gr::Command n; n.kind = gr::CmdKind::NameGu;
                n.newName = "元初一气蛊";
                s.execute(n);
            }
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    bool headless = false;
    bool demo = false;
    bool reveal = false;
    bool devMode = false;
    bool showLog = false;   // 见闻录：默认收起，--log 打开
    bool skipNaming = false;   // 跳过开局取名（仅出样图时用）
    bool verbose = false;
    std::string outPath = "frame.ppm";
    std::string tabName = "World";
    int frames = 1;


    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--headless")) headless = true;
        else if (!std::strcmp(argv[i], "--demo")) demo = true;
        else if (!std::strcmp(argv[i], "--reveal")) reveal = true;
        else if (!std::strcmp(argv[i], "--dev")) devMode = true;
        else if (!std::strcmp(argv[i], "--log")) showLog = true;
        // 跳过开局取名（出样图时用，免得界面被取名窗挡住）
        else if (!std::strcmp(argv[i], "--skip-naming")) skipNaming = true;
        else if (!std::strcmp(argv[i], "--cave") && i + 1 < argc) g_caveId = argv[++i];
        else if (!std::strcmp(argv[i], "--goto") && i + 1 < argc) g_gotoLandmark = argv[++i];
        else if (!std::strcmp(argv[i], "--startstep") && i + 1 < argc) g_startStep = atoi(argv[++i]);
        // --refine：演示炼蛊台（开炉 → 候炉 → 备料界面）
        else if (!std::strcmp(argv[i], "--refine")) g_refineDemo = true;
        else if (!std::strcmp(argv[i], "-v") || !std::strcmp(argv[i], "--verbose"))
            verbose = true;
        else if (!std::strcmp(argv[i], "--out") && i + 1 < argc) outPath = argv[++i];
        else if (!std::strcmp(argv[i], "--tab") && i + 1 < argc) tabName = argv[++i];
        else if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--help")) {
            std::printf(
                "用法：%s [--headless] [--out <file>] [--tab World|Cave|Gu|Dao|Help]"
                " [--demo] [--reveal] [--mortal] [--dev] [--log] [-v] [--frames <n>]\n", argv[0]);
            return 0;
        }
    }

    // ---------------------------------------------------------------- 世界
    // 默认以六转蛊仙开局：三气平衡、闭关、定仙游、成尊等核心机制
    // 均为六转以上内容，一转凡人开局时这些系统在界面上不可操作。
    // 想还原「新天外之魔」彭达的凡人起点，可加 --mortal。
    gr::Rank startRank = gr::Rank::R6;
    for (int i = 1; i < argc; ++i)
        if (!std::strcmp(argv[i], "--mortal")) startRank = gr::Rank::R1;

    gr::GameSession session(20240906);
    session.world().createDefaultPlayer("彭达", gr::Domain::NanJiang, startRank);
    session.placePlayerAtStart();
    seedSession(session);
    if (demo) runDemoScript(session);
    if (reveal) {
        // 出样图用：揭示全图，便于展示地形与地标分布
        session.tileMap().revealAll();
        session.revealCurrentRealm();
    }

    // ---------------------------------------------------------------- ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;   // 不落盘 imgui.ini，保持运行可复现
    setupStyle();
    loadCjkFont(17.0f);

    gr::gui::App app(session);
    app.setDevMode(devMode);
    app.setShowLog(showLog);   // 默认 false：开局隐藏见闻录，菜单可打开
    app.setNamingDone(skipNaming);   // 默认 false：开局先取名
    if (g_startStep >= 0) app.setStartStep(g_startStep);   // 直接跳到该步出样图

    // ---------------------------------------------------------------- headless
    if (headless) {
        const float W = 1440.0f, H = 900.0f;
        io.DisplaySize = ImVec2(W, H);
        io.Fonts->TexID = (ImTextureID)(intptr_t)1;   // 软件后端据此启用纹理采样

        // 取出字体图集（Alpha8）供软件后端采样
        int fw = 0, fh = 0;
        unsigned char* pixels = nullptr;
        io.Fonts->GetTexDataAsAlpha8(&pixels, &fw, &fh);

        // 解析页签
        struct TabDef { const char* n; gr::gui::App::Tab t; };
        const TabDef tabs[] = {
            {"World", gr::gui::App::Tab::World}, {"Cave", gr::gui::App::Tab::Cave},
            {"Gu",    gr::gui::App::Tab::Gu},    {"Dao",  gr::gui::App::Tab::Dao},
            {"Quest", gr::gui::App::Tab::Quest}, {"Help", gr::gui::App::Tab::Help},
            {"Realm", gr::gui::App::Tab::Realm},
            {"Bag",   gr::gui::App::Tab::Bag},
            {"Npc",   gr::gui::App::Tab::Npc},
            {"Beast", gr::gui::App::Tab::Beast},
        };
        for (const auto& t : tabs) {
            if (tabName == t.n) { app.tab() = t.t; break; }
        }

        gr::soft::Bitmap bmp;
        for (int f = 0; f < frames; ++f) {
            ImGui::NewFrame();
            app.setDisplaySize(W, H);
            session.tickRefinePrepare(0.5);   // headless 每帧推进 0.5 秒
            app.buildFrame();
            ImGui::Render();

            gr::soft::renderDrawData(ImGui::GetDrawData(), bmp, pixels, fw, fh);
        }

        if (!gr::soft::writePPM(outPath, bmp)) {
            std::fprintf(stderr, "写出失败：%s\n", outPath.c_str());
            ImGui::DestroyContext();
            return 1;
        }
        if (verbose)
            std::printf("[离屏渲染] %s（%dx%d，%d 帧）\n",
                        outPath.c_str(), bmp.width, bmp.height, frames);
        ImGui::DestroyContext();
        return 0;
    }

    // ------------------------------------------------------------- Win32
    //
    //  Windows 原生后端：Win32 窗口 + 软件光栅化 + GDI 呈现。
    //  不依赖 GLFW / OpenGL —— 交叉编译环境下没有现成的 Windows 版 GLFW，
    //  与其引入外部二进制，不如直接走系统 API，编出的 exe 双击即可运行。
#ifdef _WIN32
    {
        gr::win32::Backend wb;
        if (!gr::win32::create(wb, 1440, 900, "蛊真人 开放世界")) {
            std::fprintf(stderr, "无法创建窗口（Win32）\n");
            ImGui::DestroyContext();
            return 1;
        }
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->TexID = (ImTextureID)(intptr_t)1;   // 软件后端据此启用纹理采样
        int fw = 0, fh = 0;
        unsigned char* pixels = nullptr;
        io.Fonts->GetTexDataAsAlpha8(&pixels, &fw, &fh);

        gr::soft::Bitmap bmp;
        double prev = gr::win32::nowSeconds();

        while (gr::win32::pumpEvents(wb)) {
            const double now = gr::win32::nowSeconds();
            io.DeltaTime = std::max(1.0 / 240.0, std::min(now - prev, 0.1));
            prev = now;

            io.DisplaySize = ImVec2((float)wb.width, (float)wb.height);
            ImGui::NewFrame();
            app.setDisplaySize((float)wb.width, (float)wb.height);
            session.tickRefinePrepare(io.DeltaTime);
            app.buildFrame();
            ImGui::Render();

            gr::soft::renderDrawData(ImGui::GetDrawData(), bmp, pixels, fw, fh);
            gr::win32::present(wb, bmp.pixels.data(), bmp.width, bmp.height);

            // 界面在本帧产生的指令：交回会话层判定
            auto pc = app.takeCommand();
            if (pc.has) {
                auto r = session.execute(pc.cmd);
                app.setStatus(r.ok ? r.title : ("被拒：" + r.title));
                for (const auto& fx : r.effects) app.emitEffect(fx);
            }
        }
        gr::win32::destroy(wb);
        ImGui::DestroyContext();
        return 0;
    }
#endif

    // ---------------------------------------------------------------- GLFW
#ifdef HAVE_GLFW
    if (!glfwInit()) {
        std::fprintf(stderr, "glfwInit 失败\n");
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    GLFWwindow* win = glfwCreateWindow(1440, 900, "蛊真人 开放世界", nullptr, nullptr);
    if (!win) {
        std::fprintf(stderr, "无法创建窗口\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    unsigned char* texPixels = nullptr;
    int tw = 0, th = 0;
    io.Fonts->GetTexDataAsRGBA32(&texPixels, &tw, &th);
    GLuint fontTex = 0;
    glGenTextures(1, &fontTex);
    glBindTexture(GL_TEXTURE_2D, fontTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, texPixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    io.Fonts->TexID = (ImTextureID)(intptr_t)fontTex;

    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int dw = 0, dh = 0;
        glfwGetWindowSize(win, &dw, &dh);
        app.setDisplaySize(static_cast<float>(dw), static_cast<float>(dh));
        // 炼蛊「原地候炉」按真实秒推进，须每帧调用
        session.tickRefinePrepare(io.DeltaTime);
        app.buildFrame();

        // 执行界面在本帧产生的指令
        auto pc = app.takeCommand();
        if (pc.has) {
            auto r = session.execute(pc.cmd);
            app.setStatus(r.ok ? r.title : ("被拒：" + r.title));
            // 将规则层结算出的特效交给界面播放。
            // 特效由【结算】产生，不由界面凭空生成 ——
            // 否则会出现「催动失败却仍放出特效」的脱节。
            for (const auto& fx : r.effects) app.emitEffect(fx);
        }

        ImGui::Render();
        glViewport(0, 0, dw, dh);
        glClearColor(0.05f, 0.06f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(win);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
#else
    std::fprintf(stderr,
        "本程序未编入窗口后端（当前环境无 GLFW/OpenGL，且非 Windows 原生构建）。\n"
        "可用 --headless 离屏渲染界面图片：\n"
        "    %s --headless --out frame.ppm\n", argv[0]);
    ImGui::DestroyContext();
    return 2;
#endif
}
