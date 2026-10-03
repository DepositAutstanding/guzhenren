// ============================================================================
//  游戏会话层 —— 指令接口与 UI 只读视图
//
//  存在的理由：原 GameWorld 是「演示驱动」的（main.cpp 写死一串动作再打印），
//  没有状态机、没有指令入口，界面层无从下手。本层把它改造成：
//
//      UI ──execute(Command)──▶ GameSession ──▶ 各子系统
//      UI ◀──playerView()────── GameSession
//
//  界面层只做两件事：把世界快照画出来、把玩家输入包装成 Command。
//  所有规则判定仍留在原子系统，本层不复制任何规则。
// ============================================================================
#pragma once

#include "gr/ai/NpcInteraction.hpp"
#include "gr/core/Types.hpp"
#include "gr/core/IntelSystem.hpp"
#include "gr/core/Rng.hpp"
#include "gr/sim/GameWorld.hpp"
#include "gr/sim/QuestSystem.hpp"
#include "gr/world/TileMap.hpp"
#include "gr/world/Beast.hpp"
#include "gr/world/CaveRealm.hpp"
#include "gr/battle/GuEffect.hpp"
#include "gr/gu/RefineBench.hpp"
#include "gr/core/Origin.hpp"

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <deque>

namespace gr {

// ------------------------------------------------------------------ 指令
enum class CmdKind : std::uint8_t {
    Idle = 0,
    MoveTo,           // 移动到指定地点
    Jump,             // 定仙游跳跃（仅限坐标库中已有目标）
    Observe,          // 亲眼见过 / 感知过 → 记入定仙游坐标库
    GatherQi,         // 外出搜集三气
    EnterSeclusion,   // 进入闭关（须在自家仙窍/福地）
    Cultivate,        // 闭关中推进调和
    LeaveSeclusion,   // 解除闭关
    Breakthrough,     // 突破（九转须满足成尊四条件）
    Refine,           // 炼蛊（无蛊方绝对无法炼制）
    Advance,          // 推进世界（NPC 自主行动）
    Rename,           // 改名（玩家自定名号）
    AcceptQuest,      // 接取支线
    MoveOnMap,        // 在二维地图上移动（探索）
    Inquire,          // 打探情报（得知方源持有定仙游等传闻）
    Innovate,         // 自创炼蛊：以已有蛊虫试炼前所未有的新蛊
    NameGu,           // 为刚炼成的新蛊命名（命名后才正式归档）
    CastMove,         // 催动杀招（蛊虫攻击）—— 结算后发出特效请求
    // —— 炼蛊台（需求 8）——
    RefineOpen,       // 打开炼蛊台（进入 2 秒原地等待）
    RefinePick,       // 选 / 弃一件原材料（点一次选中、两次放弃）
    RefineTechnique,  // 选择炼蛊手法
    RefineBegin,      // 确定 —— 开始炼制
    RefineCancel,     // 取消 —— 放弃本炉
    RefineTerminate,  // 终止炼蛊（稳定态出半成品，不稳定则失败）
    RefineTimeGu,     // 投入光阴蛊加速
    RefineMulti,      // 一心多用 —— 再开一炉
    RefineClose,      // 关闭炼蛊界面
    // —— 背包 ——
    BagGather,        // 就地采集（材料按所处地形产出）
    BagDrop,          // 丢弃物品
    BagUse,           // 使用消耗品
    // —— 喂养 ——
    FeedGu,           // 喂蛊：专食（背包物品）恢复至满，真元喂养只到 0.8
    // —— 聚落建筑 ——
    EnterBuilding,    // 进入建筑：按其用途执行（交易/求学/查方/休整…）
    // —— NPC 互动 ——
    NpcTalk,          // 交谈 —— 交换见闻，可得情报
    NpcTrade,         // 交易 —— 买卖蛊虫与材料
    NpcLearn,         // 请教 —— 学炼蛊手法
    NpcBorrow,        // 求借 —— 借仙蛊（定仙游支线）
    NpcDuel,          // 挑战 —— 出手相斗
    // —— 狩猎 ——
    Hunt,             // 猎兽：异兽可战，荒兽（蛊仙级）不可敌
};

inline const char* to_string(CmdKind k) {
    switch (k) {
        case CmdKind::Idle:           return "待机";
        case CmdKind::MoveTo:         return "移动";
        case CmdKind::Jump:           return "定仙游";
        case CmdKind::Observe:        return "观察";
        case CmdKind::GatherQi:       return "搜集三气";
        case CmdKind::EnterSeclusion: return "闭关";
        case CmdKind::Cultivate:      return "调和";
        case CmdKind::LeaveSeclusion: return "出关";
        case CmdKind::Breakthrough:   return "突破";
        case CmdKind::Refine:         return "炼蛊";
        case CmdKind::Advance:        return "推进世界";
        case CmdKind::Rename:         return "改名";
        case CmdKind::AcceptQuest:    return "接取支线";
        case CmdKind::MoveOnMap:      return "地图移动";
        case CmdKind::Inquire:        return "打探情报";
        case CmdKind::Innovate:       return "自创炼蛊";
        case CmdKind::NameGu:         return "命名新蛊";
        case CmdKind::CastMove:       return "催动杀招";
        case CmdKind::RefineOpen:     return "开炉炼蛊";
        case CmdKind::RefinePick:     return "取放材料";
        case CmdKind::RefineTechnique:return "选定手法";
        case CmdKind::RefineBegin:    return "起炉";
        case CmdKind::RefineCancel:   return "撤炉";
        case CmdKind::RefineTerminate:return "终止炼蛊";
        case CmdKind::RefineTimeGu:   return "投入光阴蛊";
        case CmdKind::RefineMulti:    return "一心多用";
        case CmdKind::RefineClose:    return "收起炼蛊台";
        case CmdKind::BagGather:      return "就地采集";
        case CmdKind::BagDrop:        return "丢弃物品";
        case CmdKind::BagUse:         return "使用";
        case CmdKind::FeedGu:         return "喂蛊";
        case CmdKind::EnterBuilding:  return "入建筑";
        case CmdKind::NpcTalk:        return "交谈";
        case CmdKind::NpcTrade:       return "交易";
        case CmdKind::NpcLearn:       return "请教";
        case CmdKind::NpcBorrow:      return "求借";
        case CmdKind::NpcDuel:        return "挑战";
        case CmdKind::Hunt:           return "狩猎";
    }
    return "？";
}

struct Command {
    CmdKind     kind = CmdKind::Idle;
    std::string siteId;      // MoveTo / Jump / Observe 的目标
    std::string npcId;       // Npc* 系列：互动对象
    int         ticks = 1;   // Cultivate / GatherQi / Advance 的刻度数
    MoveId      moveId = 0;  // 预留：使用杀招
    std::size_t recipeIdx = 0; // Refine 的蛊方索引
    std::string newName;     // Rename 的新名号
    bool asAlias = false;        // Rename：游戏中改名 = 给自己取假名 / 代号
    bool changeSprite = false;   // Rename：是否同时更换形象（现阶段仅预留，不作图）
    int         mapX = 0, mapY = 0;  // MoveOnMap 的目标格
    // Innovate：参与炼制的蛊虫（carriedGu 索引）与目标流派
    std::vector<std::size_t> components;
    int         targetDao = -1;
    // —— 炼蛊台参数 ——
    // —— 背包参数 ——
    std::string itemName;            // BagDrop / BagUse：物品名
    double      itemAmount = 1.0;    // 数量

    RecipeId    refineRecipe = 0;    // RefineOpen / RefineMulti
    GuId        refineMat    = 0;    // RefinePick：蛊实例 id
    int         refineTech   = 0;    // RefineTechnique：手法 id
    int         refineSlot   = -1;   // 槽位（-1 = 当前槽）

    std::string buildingId;          // EnterBuilding：建筑 id
    std::string settlementId;        // EnterBuilding：所属聚落 id

    GuId        feedGuId   = 0;      // FeedGu：要喂的蛊实例 id
    bool        feedByEssence=false; // FeedGu：true = 以真元喂（上限 0.8）

    MoveId      castMoveId = 0;      // CastMove：要催动的杀招
    int         castToX = -1, castToY = -1;   // CastMove：目标格（地表地图）
};

struct CommandResult {
    bool        ok = false;
    CmdKind     kind = CmdKind::Idle;
    std::string title;                  // 一句话结论
    std::vector<std::string> notes;     // 细节

    // 蛊虫攻击特效：由【规则层】在结算时填入，UI 只负责播放。
    //
    //  为什么放在这里而不是让 UI 自己造：特效是杀招结算的结果，
    //  威力、是否崩解、用了哪些蛊 —— 这些只有规则层知道。
    //  若由 UI 凭空生成特效，就会出现「没催动成功却放了特效」的脱节。
    std::vector<GuEffectRequest> effects;

    static CommandResult fail(CmdKind k, std::string t) {
        CommandResult r; r.kind = k; r.title = std::move(t); return r;
    }
    static CommandResult succeed(CmdKind k, std::string t) {
        CommandResult r; r.ok = true; r.kind = k; r.title = std::move(t); return r;
    }
};

// ------------------------------------------------------------------ 日志
struct LogEntry {
    Tick        tick = 0;
    bool        success = true;
    std::string text;

    LogEntry() = default;
    LogEntry(Tick t, bool s, std::string x) : tick(t), success(s), text(std::move(x)) {}
};

// ------------------------------------------------------------------ UI 只读视图
//  界面层只读取这些结构，不直接触碰子系统，避免 UI 反过来污染规则。
struct SiteView {
    std::string siteId;
    std::string name;
    std::string region;
    std::string domainName;
    std::string className;     // 五域地表 / 两天之上 / 洞天内部 / 域外规则空间
    std::string layerName;
    std::string ownerName;     // 方源已吞并 / 天庭已占领 / 异族自有 …
    std::string ownerTag;
    bool        isHome = false;     // 玩家名下
    bool        known  = false;     // 已在定仙游坐标库
    double      qiYield = 0.0;
};

// ---------------------------------------------------------------- NPC 接触视图
//
//  界面只读。态度与可做的互动皆由规则层算好，界面不再自行判断 ——
//  避免出现「界面显示可交易、规则却拒绝」的脱节。
//
struct NpcContactView {
    std::string  npcId;
    std::string  name;
    std::string  rankName;
    std::string  factionName;
    std::string  intentName;     // 当前在做什么
    bool         visible = false;
    NpcAttitude  attitude = NpcAttitude::Invisible;
    std::string  attitudeName;
    std::string  line;               // 当面所说
    std::string  reason;             // 为何如此
    std::vector<NpcAction> actions;  // 可做的互动
    std::string  occupation;    // 凡俗之人的职业
    bool         isCommoner = false;
    std::string  whereName;     // 所在聚落
    bool         here = false;  // 是否就在左近（走得到才遇得到）
    int          affinity = 0;
    bool         met = false;
    std::string  source;
    bool         canon = false;
};

struct PlayerView {
    bool     exists = false;
    std::string name;
    std::string rankName;
    std::string domainName;
    std::string locationName;
    std::string demonName;

    // 凡人用「真元」，蛊仙用「仙元」—— 名称随修为切换，不可统称
    std::string essenceName;      // 「真元」/「仙元」
    double   essence = 0.0, maxEssence = 0.0;
    double   health = 0.0;

    // 道痕：未过显形极点前不予显示（凡人时期道痕极稀，顶着大数字出戏）
    bool     daoMarksVisible = false;
    double   daoMarks = 0.0, mainDaoMarks = 0.0;
    double   daoMarksThreshold = 0.0;
    std::string mainDaoName;
    std::string mainFlowName;

    // 三气：凡人时期无此概念，界面应整块隐藏
    bool     showThreeQi = false;
    double   qiHeaven = 0.0, qiEarth = 0.0, qiHuman = 0.0;
    bool     balanced = false;
    bool     inSeclusion = false;
    int      seclusionTicks = 0;

    // 定仙游：仙蛊唯一，玩家只可能「借用」
    std::string dingXianYouState;   // 「未持有」/「借用中」
    bool     canUseDingXianYou = false;
    int      dingXianYouTicksLeft = 0;

    // 能力开关（闭关时锁死，UI 应置灰）
    bool     canMove = false, canBattle = false, canLeave = false;

    std::size_t carriedGu = 0;
    std::size_t knownCoords = 0;   // 定仙游坐标库（count()）
    std::size_t ownedSites = 0;

    // 成尊进度（仅八转以上有意义）
    bool     showVenerable = false;
    bool     vBaiLi = false, vMarks = false, vFlow = false, vSeal = false;
};

// ------------------------------------------------------------------ 会话
//  地图尺度：改这两个常量即可整体缩放，分块/外存/迷雾/域界自动适配
inline constexpr int kMapW = 2048;
inline constexpr int kMapH = 1536;

class GameSession {
public:
    // cacheDir 为空串 = 不落外存（单元测试隔离用；
    // 否则同一目录下多个用例会互相读到对方的探索进度）
    explicit GameSession(std::uint64_t seed = 20240906,
                         std::string cacheDir = "cache/tilemap");
    // 声明了析构函数后移动构造不会隐式生成，而 CaveRealm 是 unique_ptr 成员
    // （不可拷贝）—— 缺了移动语义，按值返回会话的代码就编译不过。
    GameSession(GameSession&&) noexcept = default;
    GameSession& operator=(GameSession&&) noexcept = default;
    // 析构前把地图探索状态写回外存。
    // 此前只有「块被 LRU 淘汰」或「显式 revealAll」时才落盘，
    // 直接退出游戏则本次探索可能全部丢失。
    ~GameSession();

    GameWorld&       world()       { return world_; }
    const GameWorld& world() const { return world_; }

    // 执行一条指令。全部规则判定仍在子系统内，此处只做分发与记录。
    // —— 炼蛊台 ——
    RefineBench&       bench()       { return bench_; }
    const RefineBench& bench() const { return bench_; }
    DisplaySettings&   display()     { return display_; }
    // 推动炼蛊计时（世界推进时调用，按世界刻度）
    void tickRefining(Tick dt);
    // 推动「原地等待」阶段（按真实秒，由界面每帧调用）
    void tickRefinePrepare(double dtSeconds);
    // 背包中是否持有光阴蛊（用于加速炼蛊）
    bool hasTimeGu() const;
    // 背包中是否持有「一心 N 用」系列蛊
    bool hasMultiTaskGu() const;

    // —— 出身（需求 11）——
    //  以指定身份开局：创建角色并套用该身份的起始修为、人脉、
    //  蛊虫、传承线索、海域与血脉。
    CommandResult startWithOrigin(OriginId id, const std::string& name,
                                  const std::string& clan = {},
                                  const std::string& spawnLandmarkId = {});
    // 当前出身（未开局则为「南疆·商家商人」之外的空值）
    const OriginDef* originDef() const { return originId_ ? ::gr::origin(*originId_) : nullptr; }
    // 开窍场景（供界面演出；非开窍身份则 enabled=false）
    AwakeningScene awakeningScene() const;
    // 背包视图（界面用）
    std::vector<const ItemStack*> bagItems() const;
    const Cultivator* player() const { return world_.player(); }
    //  兽类：当前所在格的兽情，以及已遭遇过的兽
    BeastEncounter beastHere() const;
    const std::vector<std::string>& beastsSeen() const { return beastsSeen_; }

    CommandResult execute(const Command& cmd);

    // 便捷：推进世界若干刻度
    CommandResult advance(int ticks = 1);

    // UI 只读快照
    PlayerView           playerView() const;
    std::vector<SiteView> siteViews() const;

    // 情报：得先知道，才谈得上做
    IntelSystem&       intel()       { return intel_; }
    const IntelSystem& intel() const { return intel_; }
    // 蛊方视图：未得蛊方则涂黑，只留「未得此方」
    std::vector<RecipeView> recipeViews() const;

    // ---------------- 自创蛊 ----------------
    //
    //  原著规定：蛊虫非凭空而生，炼制前所未有之蛊极难 ——
    //  需炼道造诣，失败则炸炉、材料损毁，甚至反噬其身。
    //  成功者与失败者皆记于此处，供界面展示与存档。
    struct Innovation {
        bool        pending = false;    // 炼成待命名
        GuTemplate  proto;              // 炼成的蛊虫原型（未命名）
        bool        success = false;
        std::string attemptDetail;      // 本次尝试的经过
    };
    const Innovation& pendingInnovation() const { return pending_; }
    // 玩家自创的蛊虫（已命名并归档）
    const std::vector<GuTemplate>& customGu() const { return customGu_; }
    // 自创炼蛊：components 为 carriedGu 索引
    CommandResult doInnovate(const Command& c);
    CommandResult doNameGu(const Command& c);
    CommandResult doRefineOpen(const Command& c);
    CommandResult doRefinePick(const Command& c);
    CommandResult doRefineTech(const Command& c);
    CommandResult doRefineBegin(const Command& c);
    CommandResult doRefineCancel(const Command& c);
    CommandResult doRefineTerminate(const Command& c);
    CommandResult doRefineTimeGu(const Command& c);
    CommandResult doRefineMulti(const Command& c);
    CommandResult doRefineClose(const Command& c);
    CommandResult doBagGather(const Command& c);
    CommandResult doBagDrop(const Command& c);
    CommandResult doBagUse(const Command& c);
    CommandResult doFeedGu(const Command& c);
    CommandResult doEnterBuilding(const Command& c);
    void harvestRefined(int slot);
    // 存档：自创蛊持久化
    void saveCustomGu() const;
    void loadCustomGu();
    std::string saveDir() const;

    // 秘境：洞天 / 福地内部地图（进入洞天后是另一张图）
    bool inCaveRealm() const { return realm_ != nullptr; }
    CaveRealm* currentRealm() { return realm_.get(); }
    const CaveRealm* currentRealm() const { return realm_.get(); }
    // 进入 / 离开洞天
    CommandResult enterCave(const std::string& siteId);

    // ---------------- NPC 互动 ----------------
    //
    //  原著第一约束不是「能否互动」，而是【能否见到】——
    //  九转尊者行踪莫测，凡人根本无从得见；六转蛊仙视凡人如蝼蚁。
    //  见不到、或对方不屑，则互动一概不成立。
    //
    std::vector<NpcContactView> npcContacts() const;
    CommandResult doNpcTalk(const Command& c);
    CommandResult doNpcTrade(const Command& c);
    CommandResult doNpcLearn(const Command& c);
    CommandResult doNpcBorrow(const Command& c);
    CommandResult doNpcDuel(const Command& c);
    //  情谊：交谈可累积，但不会凭空而来
    int npcAffinity(const std::string& npcId) const;
    CommandResult leaveCave();
    // 在秘境内移动
    CommandResult moveInCave(int nx, int ny);
    // 切换层数（多层结构如疯魔窟）
    CommandResult gotoCaveLevel(int lv);
    // 揭示当前秘境全图（出样图 / 开发者辅助用）
    void revealCurrentRealm();

    // 二维地图（供界面绘制与探索）
    TileMap&       tileMap()       { return tileMap_; }
    const TileMap& tileMap() const { return tileMap_; }
    // 初始定位：把玩家置于出身域地标并揭示周边
    //
    //  初始定位：把玩家放到出身域的一处地标，并揭示周边。
    //  preferLandmarkId 为空时由出身定夺（百家寨优先）；
    //  非空则落到玩家自选之地（须未毁、且在该出身域内）。
    //
    //  此前有两个重载（无参 / 带默认参数），同时存在会让
    //  无参调用产生二义性 —— 编译器不知该选哪个。故合并为一个。
    //
    bool placePlayerAtStart(const std::string& preferLandmarkId = "");
    //  某域内可作降生之地的地标（未毁、有聚落）
    std::vector<std::pair<std::string,std::string>> spawnCandidates(Domain d) const;

    // 支线
    QuestSystem&       quests()       { return quests_; }
    const QuestSystem& quests() const { return quests_; }
    std::vector<QuestView> questViews() const;

    // 开发者选项：影不影响逻辑，仅控制 UI 是否展示后台信息
    bool  devMode() const { return devMode_; }
    void  setDevMode(bool b) { devMode_ = b; }
    // 后台信息（开发者面板用）
    struct DevInfo {
        std::size_t caves = 0, npcs = 0, aliveNpcs = 0, guRegistered = 0;
        std::size_t landmarks = 0, tiles = 0;
        double exploredRatio = 0.0;
        int         terrainKinds = 0;
        int         landmarkBad  = 0;
        Tick   now = 0;
        std::string mapGenerator;
        int mapW = 0, mapH = 0;
        std::vector<std::string> extras;
    };
    DevInfo devInfo() const;

    // 供测试比对：读取后台信息不应改变任何状态
    std::string snapshotForTest() const;

    const std::deque<LogEntry>& log() const { return log_; }
    void pushLog(bool ok, const std::string& text);

    Tick now() const { return world_.world().now(); }
    std::size_t logLimit() const { return logLimit_; }

private:
    CommandResult doMoveTo(const Command& c);
    CommandResult doJump(const Command& c);
    CommandResult doObserve(const Command& c);
    CommandResult doGatherQi(const Command& c);
    CommandResult doEnterSeclusion(const Command& c);
    CommandResult doCultivate(const Command& c);
    CommandResult doLeaveSeclusion(const Command& c);
    CommandResult doBreakthrough(const Command& c);
    CommandResult doRefine(const Command& c);
    CommandResult doCastMove(const Command& c);

    void record(const CommandResult& r);

    CommandResult doRename(const Command& c);
    CommandResult doAcceptQuest(const Command& c);
    CommandResult doMoveOnMap(const Command& c);
    CommandResult doInquire(const Command& c);
    CommandResult doHunt(const Command& c);

    GameWorld             world_;
    QuestSystem           quests_;
    IntelSystem           intel_;
    Innovation        pending_;                 // 刚炼成、尚未命名的新蛊
    std::vector<GuTemplate> customGu_;          // 玩家自创并命名的蛊虫
    std::unique_ptr<CaveRealm> realm_;   // 当前所处的秘境（未进入则为空）

    // 炼蛊台：跨帧的炼制过程（含备料、手法、倒计时）
    RefineBench     bench_;
    GuId            nextGuInstanceId_ = 800000;   // 半成品等自建实例用

    // 出身：开局选定的身份（未选则为空）
    bool                hasOrigin_ = false;
    OriginId            originIdVal_ = OriginId::NanJiang_ShangMerchant;
    const OriginId*     originId_ = nullptr;
    std::string         originClan_;
    AwakeningScene      awakening_;

    //  NPC 情谊：交谈可累积，但不会凭空而来
    std::map<std::string, NpcRelation> npcRel_;
    //  玩家所属势力。Cultivator 不带势力字段 ——
    //  势力是「站位」，不是修士的固有属性，故由会话持有。
    //  未加入任何势力时为 Neutral，此时与谁都不构成敌对。
    FactionId           playerFaction_ = FactionId::Neutral;

    DisplaySettings display_;
    std::string           realmReturnSite_;  // 离开秘境后返回的外界地点
    std::vector<std::string> beastsSeen_;    // 已遭遇过的兽类（见闻录）
    TileMap               tileMap_;
    std::deque<LogEntry>  log_;
    bool                  devMode_ = false;
    static const std::size_t logLimit_ = 300;
};

} // namespace gr
