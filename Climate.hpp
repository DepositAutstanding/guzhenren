// ============================================================================
//  1.3 天罡罡风全新规则（修复旧版错误）
//    · 天罡罡风并非永久持续刮动
//    · 白天罡风分为：平静期 / 弱风期 / 暴风灾期，动态轮换
//    · 平静期：蛊仙可安全穿行白天洞天之间
//    · 暴风灾期：罡风狂暴，普通八转以下会被撕裂重创
//    · 罡风变化为世界自然动态天气，随世界时间自主切换，非永久固定
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"

#include <string>

namespace gr {

enum class GalePhase : std::uint8_t {
    Calm    = 0,  // 平静期
    Weak    = 1,  // 弱风期
    Disaster = 2  // 暴风灾期
};

inline const char* to_string(GalePhase p) {
    switch (p) {
        case GalePhase::Calm:     return "平静期";
        case GalePhase::Weak:     return "弱风期";
        case GalePhase::Disaster: return "暴风灾期";
    }
    return "？";
}

// 罡风状态机：由世界时间驱动自主轮换
class TiangangGale {
public:
    // 各阶段持续刻度（可调，供策划配表）
    int calmTicks     = 30;
    int weakTicks     = 18;
    int disasterTicks = 12;

    GalePhase phase() const { return phase_; }
    int       elapsed() const { return elapsed_; }
    int       remain() const;

    // 推进一个世界刻度，返回是否发生阶段切换
    bool advance();

    void  reset(GalePhase p = GalePhase::Calm);

    // 该阶段下，某转数单位是否会被「撕裂重创」
    // 暴风灾期：八转以下必被撕裂重创；八转以上仍有阻力但不致命
    bool isLethalTo(Rank r) const;

    // 通行难度系数 0.0(畅通) ~ 1.0(绝境)
    double traverseDifficulty() const;

    // 穿行消耗倍率（仙元/道痕损耗）
    double traverseCostMultiplier() const;

private:
    GalePhase phase_   = GalePhase::Calm;
    int       elapsed_ = 0;
};

// ---------------------------------------------------------------- 世界气候
// 北原严寒的触发条件载体：极夜 / 暴风雪 / 深夜（且非暖季）
enum class DayPhase : std::uint8_t { Dawn = 0, Day, Dusk, Night, DeepNight };

inline const char* to_string(DayPhase p) {
    switch (p) {
        case DayPhase::Dawn:      return "清晨";
        case DayPhase::Day:       return "白昼";
        case DayPhase::Dusk:      return "黄昏";
        case DayPhase::Night:     return "夜晚";
        case DayPhase::DeepNight: return "深夜";
    }
    return "？";
}

struct WorldClimate {
    bool     polarNight = false;      // 极夜
    bool     blizzard   = false;      // 暴风雪
    bool     warmSeason = true;       // 暖季
    DayPhase phase      = DayPhase::Day;

    // 北原严寒触发判定：极夜 / 暴风雪 / 深夜极寒时段（暖季白昼不触发）
    bool triggersBeiYuanCold() const {
        return polarNight || blizzard || (phase == DayPhase::DeepNight && !warmSeason);
    }
};

} // namespace gr

// ============================================================================
//  两天洞天天灵状态（地理研究文档：断更时刻的两天已非静态两层）
//
//  A 级事实（第六卷断更节点）：
//    · 幽魂吞噬黑天天灵，代表黑天，试图吞并白天
//    · 幽魂冲入太阳、太阳爆裂
//    · 两天洞天的白天天灵与天脉节点遭到破坏
//    · 太阳被巨阳评为「全天下最大的天脉节点，也是两天中最大的光脉炎脉节点」
//      —— 不是可简单消灭的普通天体
//
//  B 级推论：两天洞天由此进入规则级重组。
//  明确排除的 C 级说法：「黑天赢下白天后五域立即黑暗」
//  「幽魂成为新太阳」—— 因幽魂神志异常，方源、巨阳、星宿三方仍在博弈，
//  太阳本身也爆散为光炎碎片。
//
//  由此，代码中「两天 = 静态两层」的旧模型在第六卷已过时。
// ============================================================================

enum class HeavenSpiritState : std::uint8_t {
    Intact  = 0,   // 天灵完好（旧稳态）
    Damaged = 1,   // 受损
    Devoured= 2,   // 被吞噬
    Shattered = 3  // 爆散
};

inline const char* to_string(HeavenSpiritState s) {
    switch (s) {
        case HeavenSpiritState::Intact:    return "完好";
        case HeavenSpiritState::Damaged:   return "受损";
        case HeavenSpiritState::Devoured:  return "被吞噬";
        case HeavenSpiritState::Shattered: return "爆散";
    }
    return "？";
}

class TwoHeavensState {
public:
    // 开局（第六卷卷初）状态，依地理研究文档设定
    void setVolumeSixOpening() {
        blackSpirit_ = HeavenSpiritState::Devoured;  // 黑天天灵被幽魂吞噬
        whiteSpirit_ = HeavenSpiritState::Damaged;   // 白天天灵受损
        sunNode_     = HeavenSpiritState::Shattered; // 太阳爆裂，光炎脉节点遭破坏
        stable_      = false;
    }

    HeavenSpiritState blackSpirit() const { return blackSpirit_; }
    HeavenSpiritState whiteSpirit() const { return whiteSpirit_; }
    HeavenSpiritState sunNode()     const { return sunNode_; }

    // 两天是否仍维持旧稳态 —— 第六卷开局起为 false
    bool isStable() const { return stable_; }

    // 幽魂是否掌握黑天（神志异常，故不等于黑天已获胜）
    bool youHunHoldsBlack() const {
        return blackSpirit_ == HeavenSpiritState::Devoured;
    }

    // 太阳是「全天下最大的天脉节点」—— 不可当作普通天体消灭
    bool sunIsLargestNode() const { return true; }

    // 天脉节点破坏度：影响两天内洞天的稳定性与通行
    double nodeDisruption() const;

    // 描述当前状态（供演示与日志）
    std::string describe() const;

private:
    HeavenSpiritState blackSpirit_ = HeavenSpiritState::Intact;
    HeavenSpiritState whiteSpirit_ = HeavenSpiritState::Intact;
    HeavenSpiritState sunNode_     = HeavenSpiritState::Intact;
    bool stable_ = true;
};
