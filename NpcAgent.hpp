// ============================================================================
//  十二、NPC 完整 AI 体系（保留 + 适配新地图规则）
//    1. 所有原著人物独立 AI、自主行动、不随玩家转动
//    2. NPC 定仙游同样「只能跳转已探索点位」
//    3. NPC 三气平衡同样需要回自己洞天闭关维持
//    4. NPC 会自主搜集三气、处理万劫、争夺异人洞天
//    5. 方源、天庭已占领白天部分异人洞天，AI 行为匹配开局状态
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"
#include "gr/core/Rng.hpp"
#include "gr/battle/Battle.hpp"
#include "gr/cultivator/Cultivator.hpp"
#include "gr/ai/Faction.hpp"
#include "gr/world/WorldMap.hpp"

#include <string>
#include <vector>

namespace gr {

// NPC 当前意图
enum class NpcIntent : std::uint8_t {
    Idle         = 0,
    GatherHeavenQi,   // 前往黑天 / 天道秘境搜集天气
    GatherEarthQi,    // 前往五域地脉 / 福地搜集地气
    GatherHumanQi,    // 前往白天 / 人道势力布局积累人气
    ReturnHome,       // 返回自家洞天
    Seclude,          // 定点闭关调和三气
    HandleTribulation,// 处理万劫 / 灾劫
    ContestCave,      // 争夺异人洞天
    AbsorbQiGongGuo,  // 炼化气功果（研究报告 4.2 气海分身扩张链）
    Hunt,             // 狩猎（如幽魂狩猎）
    Scheme,           // 幕后布局（方源式：分身顶替、情报网络）
    Battle
};

inline const char* to_string(NpcIntent i) {
    switch (i) {
        case NpcIntent::Idle:              return "待机";
        case NpcIntent::GatherHeavenQi:    return "搜集天气";
        case NpcIntent::GatherEarthQi:     return "搜集地气";
        case NpcIntent::GatherHumanQi:     return "搜集人气";
        case NpcIntent::ReturnHome:        return "返回自家洞天";
        case NpcIntent::Seclude:           return "闭关调和三气";
        case NpcIntent::HandleTribulation: return "处理万劫";
        case NpcIntent::ContestCave:       return "争夺异人洞天";
        case NpcIntent::AbsorbQiGongGuo:   return "炼化气功果";
        case NpcIntent::Hunt:              return "狩猎";
        case NpcIntent::Scheme:            return "幕后布局";
        case NpcIntent::Battle:            return "交战";
    }
    return "？";
}

struct NpcAgent {
    Cultivator   self;
    FactionId    faction = FactionId::Neutral;
    NpcIntent    intent  = NpcIntent::Idle;
    std::string  homeSite;                 // 自家洞天 siteId
    std::vector<std::string> targetSites;  // 争夺目标
    int          decisionCooldown = 0;
    bool         isFenShen = false;        // 是否为分身（如吴帅、气海老祖）
    std::string  masterId;                 // 分身的本体

    // ---- 凡俗众生 ----
    //
    //  五域之内不只有尊者与蛊仙。山寨有猎户、草原有牧民、
    //  沙漠有商队、海上有渔夫、宗门有外门弟子 ——
    //  这才是玩家真正日日打交道的世间。
    //
    bool        isCommoner = false;
    std::string occupation;        // 职业：猎户、牧民、商队向导…
    std::string homeSettlement;    // 所属聚落 id（空 = 无固定居所的散人）
    //  居于洞天／福地之内的异人：须入洞天方得相见
    bool        livesInCave = false;
    //  可遇之地：地标 id。玩家走到此处才遇得到。
    std::string whereId;

    // ---- 气功果体系（研究报告 4.2：气道生命/资源/炸弹/复活容器的多态设定）----
    // 一颗普通气功果完全吸收约增六万气道道痕；气海分身诞生时已有八十多万。
    // 两天洞天中的气功果已膨胀至接近自爆边缘 —— 卷初核心变量。
    double qiGongGuoStored = 0.0;   // 已储备的气功果当量（颗）
    double qiGuoExplosionRisk = 0.0;// 自爆风险 0~1，随囤积上升

    // ---- 至尊仙窍吞并（研究报告 2.2／7.2）----
    // 至尊仙胎体可无常规流派壁垒地吞并其他仙窍：
    // 「联盟—资源—洞天—至尊仙窍」的扩张链条。
    bool   hasZhiZunXianQiao = false;   // 是否拥有至尊仙窍
    int    swallowedCaves = 0;          // 已吞并洞天数

    const Cultivator& asCultivator() const { return self; }
};

// ---------------------------------------------------------------------------
//  NPC AI：自主行动，不随玩家转动
// ---------------------------------------------------------------------------
class NpcAi {
public:
    // 决策：依据三气状态、万劫倒计时、势力关系、已知坐标自主选择意图
    static NpcIntent decide(const NpcAgent& agent, const WorldMap& world,
                            const FactionSystem& factions);

    // 执行一个世界刻度
    struct TickReport {
        NpcIntent intent = NpcIntent::Idle;
        std::vector<std::string> notes;
        bool jumped = false;
        Location jumpedTo;
    };
    static TickReport tick(NpcAgent& agent, WorldMap& world,
                           const FactionSystem& factions, Rng& rng);

    // NPC 也遵守：只能跳转已探索点位
    static JumpResult jump(NpcAgent& agent, const Location& to, const WorldMap& world);
};

// ---------------------------------------------------------------------------
//  原著人物开局数据（第六卷卷初 / 断更时刻）
// ---------------------------------------------------------------------------
std::vector<NpcAgent> build_canon_npcs(const WorldMap& world);

} // namespace gr
