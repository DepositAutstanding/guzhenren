// ============================================================================
//  ImGui 界面层
//
//  设计原则：本文件只调用 ImGui API，不含任何 GLFW / OpenGL 代码。
//  渲染后端可自由替换（软件光栅化用于 headless 验证，GLFW+OpenGL3 用于真机运行），
//  界面布局在两种后端下完全一致。
//
//  界面只读 session 的视图结构，不直接触碰子系统；所有操作都包装成 Command
//  交回 GameSession 执行 —— 规则仍然只有一处实现。
// ============================================================================
#pragma once

#include "gr/ui/GameSession.hpp"

#include "gr/battle/GuEffect.hpp"

#include <string>
#include <vector>

namespace gr {
namespace gui {

class App {
public:
    explicit App(GameSession& s) : session_(&s) {}

    // 窗口尺寸由后端设定后回填
    void setDisplaySize(float w, float h) { dispW_ = w; dispH_ = h; }

    // 构建一帧界面
    void buildFrame();

    // 主题页签
    enum class Tab : int { World = 0, Cave = 1, Gu = 2, Dao = 3, Quest = 4, Help = 5,
                         Realm = 6, Bag = 7, Npc = 8, Beast = 9 };
    Tab& tab() { return tab_; }

    struct PendingCommand {
        bool        has = false;
        Command     cmd;
    };
    // 取出本帧由界面产生的指令（后端负责执行）
    PendingCommand takeCommand() {
        PendingCommand p = pending_;
        pending_.has = false;
        return p;
    }

    void setStatus(const std::string& s) { status_ = s; }
    // 播放一条由规则层结算出的蛊虫攻击特效
    void emitEffect(const GuEffectRequest& r) { effects_.emit(r); }
    void setDevMode(bool b) { devMode_ = b; }
    // 见闻录：开局隐藏，需要时可打开
    void setShowLog(bool b) { showLog_ = b; }
    void setShowPanel(bool b) { showPanel_ = b; panelSlide_ = b ? 320.0f : 0.0f; }
    // 开局取名：false = 弹出取名界面（默认）；true = 直接进入游戏
    void setNamingDone(bool b) { namingDone_ = b; }
    bool namingDone() const { return namingDone_; }

    //  开局流程直接跳到某一步（出样图用：0=选域 1=选身份 2=开窍 3=定名）
    void setStartStep(int s) { startStep_ = std::clamp(s, 0, 3); }
    // 预留：接入玩家形象图片（现阶段仅登记路径，绘制仍走几何分支）
    void setPlayerSprite(const std::string& p) { playerSpritePath_ = p; }

private:
    void drawMenuBar();
    void drawPlayerPanel();
    void drawWorldTab();
    void drawCaveTab();
    void drawGuTab();
    void drawDaoTab();
    void drawHelpTab();
    void drawLogPanel();
    void drawCommandBar();
    void clampCamera(const class TileMap& tm);
    void drawDevPanel();
    void drawQuestPanel();
    void drawRealmTab();
    // 底部固定区：图例 + 地理说明（不随页签切换）
    void drawLegendBar();
    // 开局取名界面（模态，未定名则挡住主区）
    void drawStartNaming();          // 开局流程：出身 → 开窍 → 定名
    void drawStartOrigin();          // 出身选择（需求 11）
    void drawInnovatePanel();
    // —— 炼蛊台（需求 8）——
    void drawRefineBench();          // 覆盖层：材料 / 手法 / 起炉三区
    void drawRefineMaterials();      // 第一部分：原材料
    void drawRefineTechnique();      // 第二部分：手法与进度
    void drawRefineActions();        // 第三部分：确定取消 / 终止一心多用
    void drawRefineBag();            // 背包弹窗（选材料、按方自动取料）
    void drawBagTab();
    void drawNpcTab();
    void drawBeastTab();
    void drawNpcRow(const gr::NpcContactView& cv);               // 背包页签（需求 12）：囊中蛊 + 物品
    void drawSettingsWindow();       // 设置 — 显示设置（覆盖层透明度）

    void queue(CmdKind k, const std::string& siteId = {}, int ticks = 1);
    // 直接下达完整指令（自创炼蛊等带复杂参数的场景）
    void queue(const Command& c);

    GameSession* session_ = nullptr;   // 由构造函数注入，界面只读其视图
    float  dispW_ = 1440.0f, dispH_ = 900.0f;
    Tab    tab_ = Tab::World;
    int    selectedCave_ = 0;
    int    advanceTicks_ = 1;
    std::string status_ = "就绪";
    bool   tmRevealAll_ = false;
    // 地图视口相机（地图放大后只绘制局部）
    float  camX_ = 0.0f, camY_ = 0.0f;   // 视口中心（地图格坐标）
    float  zoom_ = 8.0f;                 // 每格像素
    bool   camInit_ = false;
    // 秘境视口
    float  rzoom_ = 10.0f;
    // 自创炼蛊：选中的组成蛊虫索引
    std::vector<char> innovateSel_;   // vector<bool> 的代理无法取址，故用 char
    char    guNameBuf_[64] = {0};
    bool   renameOpen_ = false;
    char   renameBuf_[64] = "";
    bool   devMode_ = false;      // 开发者选项：显示后台信息
    // 见闻录：开局隐藏，需要时再打开 —— 主区因此能占满宽度
    bool   showLog_ = false;

    // 个人面板：可关闭。关闭后顶栏出现「个人面板」字样，点击再从左侧滑出。
    bool   showPanel_   = true;
    float  panelSlide_  = 320.0f;   // 当前宽度，动画插值；0 = 完全收起

    // 蛊虫攻击特效（交互预留）：战斗结算 emit，此处逐帧推进与绘制
    GuEffectSystem effects_;

    // WASD 移动：按住不放则以固定间隔连续走，避免每帧一格快到看不清。
    //  返回 true 表示本帧应当走一步，dx/dy 为方向。
    bool pollWASD(int& dx, int& dy);
    float wasdRepeat_ = 0.0f;    // 按住时的重复计时

    // 目标点选：催动杀招前在地图上点一格作目标。
    //  没有这一步，特效只能原地爆发，看不出「从玩家飞向目标」，
    //  「攻击特效的交互」就名不副实。
    // 主区（世界舆图）矩形：炼蛊台覆盖层须与之对齐 ——
    //  需求写的是「炼蛊台窗口只覆盖中间的世界舆图」，
    //  不是铺满整屏压住左面板与图例。
    float  mainX_ = 0.0f, mainY_ = 0.0f, mainW_ = 0.0f, mainH_ = 0.0f;

    // 炼蛊台界面状态
    bool   refineBagOpen_    = false;   // 背包（选原材料）弹窗
    bool   refineBagTab_     = false;   // false=囊中蛊 true=蛊方
    bool   settingsOpen_     = false;   // 设置窗口
    bool   techComboOpen_    = false;   // 手法下拉是否展开
    int    techHover_        = 0;       // 下拉中当前高亮项（滚轮滚动）

    bool targeting_      = false;
    MoveId targetMoveId_ = 0;
    std::string targetMoveName_;

    // 形象：预留接入图片资源；现阶段仍以几何图形表示。
    // playerSpriteReady_ 为真时，绘制处改走图片分支（待接纹理）。
    std::string playerSpritePath_;
    bool   playerSpriteReady_ = false;
    bool   changeSpriteOpen_  = false;

    // 开局取名：正式开始游戏之前进行，故须先定名再放行世界推进。
    // namingDone_ 为假时，主区被取名界面盖住，玩家改不了也走不动。
    // —— 细化地形图：显示开关（设置 → 显示设置）——
    bool   showHillshade_ = true;    // 山体阴影
    bool   showContour_   = true;    // 等高线
    bool   namingDone_ = false;
    // —— 开局出身流程状态（需求 11）——
    int          startStep_      = 0;    // 0=选域 1=选身份 2=开窍/确认 3=定名
    Domain       startDomain_    = Domain::NanJiang;
    int          startOriginIdx_ = 0;
    int          startClanIdx_   = 0;
    int          startSpawnIdx_  = 0;   // 降生之地在候选表中的序号
    std::string  startClan_;
    char   startNameBuf_[64] = "";
    std::string startNameErr_;
    PendingCommand pending_;

    std::vector<SiteView> cachedSites_;
    int cachedSitesTick_ = -1;
};

} // namespace gui
} // namespace gr
