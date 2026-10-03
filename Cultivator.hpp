// 修士主体：玩家与 NPC 共用同一套数据结构与规则
#pragma once

#include "gr/core/Types.hpp"
#include "gr/core/Status.hpp"
#include "gr/gu/GuWorm.hpp"
#include "gr/gu/Inventory.hpp"
#include "gr/gu/KillerMove.hpp"
#include "gr/cultivator/DaoRealm.hpp"
#include "gr/cultivator/ThreeQi.hpp"
#include "gr/cultivator/DingXianYou.hpp"
#include "gr/ai/Detection.hpp"
#include "gr/cultivator/DaoTianLegacy.hpp"
#include "gr/core/CanonNumbers.hpp"
#include "gr/world/WorldMap.hpp"

#include <string>
#include <vector>

namespace gr {

// 天外之魔身份（完整天外之魔 / 普通新穿越者 / 本土）
enum class DemonIdentity : std::uint8_t {
    Native          = 0,
    NewTransmigrator = 1,   // 如彭达：携带天外道痕但尚未深度嵌合
    CompleteDemon   = 2     // 完整天外之魔：如方源
};

inline const char* to_string(DemonIdentity d) {
    switch (d) {
        case DemonIdentity::Native:           return "本土生灵";
        case DemonIdentity::NewTransmigrator: return "新天外之魔";
        case DemonIdentity::CompleteDemon:    return "完整天外之魔";
    }
    return "？";
}

// 定仙游的持有形态。仙蛊唯一，故「拥有」只有方源一人；
// 他人最多是「借用」—— 这是支线「方源将定仙游借给你」的落点。
enum class DingXianYouPossession : std::uint8_t {
    None = 0,       // 未持有
    Owned,          // 自有（仅方源）
    Lent,           // 借用中（支线授予，可有期限）
};

inline const char* to_string(DingXianYouPossession p) {
    switch (p) {
        case DingXianYouPossession::None:  return "未持有";
        case DingXianYouPossession::Owned: return "自有";
        case DingXianYouPossession::Lent:  return "借用中";
    }
    return "？";
}

struct Cultivator {
    std::string id;
    std::string name;
    bool        isPlayer = false;

    Domain      bornDomain = Domain::None;   // 本域出生 → 本域环境高额免疫
    Rank        rank = Rank::R1;
    Location    location;

    // ---------------- 资源 ----------------
    // 能量池随修为改变性质，不可混称：
    //   · 一转~五转（凡人蛊师）→ 真元 primeval essence
    //   · 六转及以上（蛊仙）  → 仙元 immortal essence
    // 数值仍用单一池，但对外必须按修为给出正确名称，突破时按比例转化。
    double essence    = 100.0;
    double maxEssence = 100.0;
    double daoMarks   = 0.0;     // 道痕总量
    double health     = 1.0;     // 0~1
    bool   alive      = true;

    // 当前修为下能量的正确称谓
    const char* essenceName() const { return is_immortal_rank(rank) ? "仙元" : "真元"; }
    bool usesPrimevalEssence() const { return !is_immortal_rank(rank); }

    // ---------------- 修行 ----------------
    DaoProficiency dao;
    ThreeQiPool    qi;
    ThreeQiState   threeQi;

    // ---------------- 蛊与杀招 ----------------
    std::vector<GuInstance> carriedGu;

    // ---------------- 背包 ----------------
    //  材料、消耗品、天材地宝等无生命之物。
    //  蛊虫是活的、有唯一性，另存于 carriedGu —— 两者不可混同。
    Inventory bag;
    std::vector<MoveId>     knownMoves;

    // ---------------- 炼蛊 ----------------
    //  魂魄强度：决定天然可同时驾驭几炉蛊（一心多用）。
    //  不足时须借助「一心多用」系列蛊（一心二用蛊、一心三用蛊……）。
    int soulStrength = 30;

    //  已学会的炼蛊手法（存 RefineTechnique::id）。
    //  手法可经阅读书籍、拜师等方式习得；「无」(id=0) 恒可用。
    std::vector<int> knownTechniques;
    bool knowsTechnique(int id) const {
        if (id == 0) return true;    // 「无」无需学习
        return std::find(knownTechniques.begin(), knownTechniques.end(), id)
               != knownTechniques.end();
    }
    void learnTechnique(int id) {
        if (!knowsTechnique(id)) knownTechniques.push_back(id);
    }

    // 四、定仙游坐标库：只增于亲眼见过 / 抵达过 / 感知过
    DingXianYouCoordinates dingXianYou;

    // 定仙游是仙蛊、世间唯一，原著中归方源所有。
    // 玩家不得天然持有 —— 只能通过支线「方源将定仙游借给你」获得临时使用权。
    // 借用期间仍受同一铁律约束：只能跳往亲眼见过 / 抵达过 / 感知过的坐标。
    DingXianYouPossession dingXianYouPossession = DingXianYouPossession::None;
    int dingXianYouLendTicks = 0;   // 借用剩余刻度（0 = 不限期）

    // 兼容旧调用：是否当前可催动定仙游
    bool holdsDingXianYou = false;
    bool canUseDingXianYou() const {
        return dingXianYouPossession != DingXianYouPossession::None;
    }

    // ---------------- 领地 ----------------
    // 自身仙窍 / 自己掌控的福地洞天 —— 三气闭关的必要场所
    std::vector<std::string> ownedSites;

    // ---------------- 状态 ----------------
    std::vector<StatusEffect> statuses;

    // ---------------- 天外之魔 ----------------
    DemonIdentity demonIdentity = DemonIdentity::Native;
    DemonExposure exposure;

    // ---------------- 情报 ----------------
    // 某些事须先「听说」才可能去做。典型即定仙游：
    // 玩家得先得知「方源持有定仙游」这条情报，支线才会出现 ——
    // 否则无从开口求借，支线根本无从触发。
    std::vector<std::string> knownIntel;
    bool knowsIntel(const std::string& id) const {
        return std::find(knownIntel.begin(), knownIntel.end(), id) != knownIntel.end();
    }
    void learnIntel(const std::string& id) {
        if (!knowsIntel(id)) knownIntel.push_back(id);
    }

    // ---------------- 盗天传承 ----------------
    // 需求十六·一期「盗天传承」；研究报告 4.3、Table 5
    // 死亡轮回保留（需求 13）
    DaoTianLegacy daoTianLegacy;

    // ---------------- 数值口径相关 ----------------
    // 流派境界：与「道境六阶」分列，按流派各记一份
    // （成尊要求的是「主修流派」境界，不是所有流派合计）
    std::unordered_map<Dao, FlowLevel> flowLevels;
    // 各流派道痕分项（成尊只认主修流派那一项）
    std::unordered_map<Dao, double>    daoMarksByDao;
    // ④突破天道封锁：由世界事件/剧情推进置位，不在修炼系统内部自判
    bool brokeHeavenlyDaoSeal = false;

    FlowLevel flowOf(Dao d) const {
        auto it = flowLevels.find(d);
        return it == flowLevels.end() ? FlowLevel::Ordinary : it->second;
    }
    void setFlow(Dao d, FlowLevel f) { flowLevels[d] = f; }

    // 主修流派 = 流派境界最高者（并列时取道痕更多者）
    Dao mainDao() const;
    FlowLevel mainFlowLevel() const;
    double mainDaoMarks() const {
        auto it = daoMarksByDao.find(mainDao());
        return it == daoMarksByDao.end() ? 0.0 : it->second;
    }
    void addDaoMarks(Dao d, double v) {
        daoMarks += v;
        daoMarksByDao[d] += v;
    }

    // ---------------- 派生查询 ----------------
    bool isImmortal() const { return is_immortal_rank(rank); }
    bool isOtherworldlyDemon() const {
        return demonIdentity != DemonIdentity::Native;
    }
    bool owns(const std::string& siteId) const;
};

// ---------------------------------------------------------------------------
//  修士行为系统
// ---------------------------------------------------------------------------
class CultivatorSystem {
public:
    // 突破：六转强制解锁三气静修平衡模式；九转须满足成尊四条件
    static Result<void> breakthrough(Cultivator& c, Rank newRank);

    // 成尊四条件校验（数值口径库「转数与境界」表）
    struct VenerableFitness {
        bool      satisfied = false;
        bool      hasBaiLiSource = false;    // ①仙窍本源已产出白荔仙元
        bool      enoughMarks    = false;    // ②主修流派道痕 ≥ 30 万
        bool      enoughFlow     = false;    // ③主修流派达无上大宗师
        bool      brokeHeavenlySeal = false; // ④突破天道封锁
        double    mainDaoMarks = 0.0;
        FlowLevel mainFlow = FlowLevel::Ordinary;
        canon::VenerableRequirement requirement;
        std::string detail;                  // 逐条诊断
    };
    static VenerableFitness checkVenerableFitness(const Cultivator& c);

    // 移动（受闭关锁、胎壁、罡风、位面压制约束）
    static Result<TraverseCost> moveTo(Cultivator& c, const Location& to,
                                                 const WorldMap& world);

    // 定仙游跳转
    static JumpResult jumpByDingXianYou(Cultivator& c, const Location& to,
                                        const WorldMap& world);

    // 探索：抵达/看见/感知 → 扩充定仙游坐标库
    static void observe(Cultivator& c, const Location& loc, SightSource src, Tick at);

    // 环境结算：把世界 DEBUFF 应用到修士身上（含本土免疫）
    static void applyEnvironment(Cultivator& c, const WorldMap& world);

    // 三气
    static Result<ThreeQiPool> gatherQi(const Cultivator& c, const WorldMap& world,
                                        double effort = 1.0);
    static Result<void> enterSeclusion(Cultivator& c, const WorldMap& world);
    static Result<void> leaveSeclusion(Cultivator& c);

    // 每个世界刻度推进
    struct TickReport {
        ThreeQiSystem::TickReport threeQi;
        bool diedFromEnvironment = false;
        std::vector<std::string> notes;
    };
    static TickReport tick(Cultivator& c, const WorldMap& world);

    // 战力评估（道境 + 转数 + 道痕）
    static double combatPower(const Cultivator& c, Dao dao);
};

} // namespace gr
