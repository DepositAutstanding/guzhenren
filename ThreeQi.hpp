// ============================================================================
//  五、三气平衡系统（重大补全、修复空白机制）
//    5.1 补全：六转蛊仙维持三气平衡的唯一正确操作方式
//    5.2 全新完整机制
//      1. 突破六转后，强制解锁「三气静修平衡模式」
//      2. 维持平衡期间：
//         · 必须居于自身仙窍内部 / 自己掌控的福地洞天内部
//         · 无法移动、无法战斗、无法外出
//         · 属于「定点闭关调和大道」行为
//      3. 三气获取、调和完整闭环：
//         天气积累：提前前往黑天、天道秘境搜集储存，带回自家洞天
//         地气积累：提前占领五域地脉、福地、储存地脉元气
//         人气积累：提前在白天、人道势力布局、积累气运香火
//         真正维持流程：
//           a. 外出搜集三气资源储存
//           b. 返回自家仙窍 / 福地闭关
//           c. 定点不动调和三气差值
//           d. 完成平衡，解除闭关，恢复行动
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"
#include "gr/core/Status.hpp"
#include "gr/world/WorldMap.hpp"

#include <algorithm>
#include <string>

namespace gr {

struct Cultivator;  // 前向声明，避免循环依赖

// ---------------------------------------------------------------- 三气池
struct ThreeQiPool {
    double heaven = 0.0;  // 天气
    double earth  = 0.0;  // 地气
    double human  = 0.0;  // 人气

    double total()  const { return heaven + earth + human; }
    double max()    const { return std::max(heaven, std::max(earth, human)); }
    double min()    const { return std::min(heaven, std::min(earth, human)); }
    double deviation() const { return max() - min(); }
    double mean()   const { return total() / 3.0; }
};

// ---------------------------------------------------------------- 配置
struct ThreeQiConfig {
    double tolerance      = 8.0;    // 可接受的差值上限（<= 视为平衡）
    double imbalanceLimit = 30.0;   // 超过即失衡，触发三气失衡 DEBUFF 与灾劫风险
    double upkeepPerTick  = 0.35;   // 每 tick 三气自然消耗基数
    double reconcileRate  = 4.0;    // 闭关时每 tick 向均值收敛的量
    double essencePerTick = 1.2;    // 闭关时每 tick 仙元消耗
    double seclusionTicksMin = 5;   // 最短闭关刻度（防止瞬间开关）
};

// ---------------------------------------------------------------- 闭关状态
struct ThreeQiState {
    bool inSeclusion      = false;
    int  seclusionTicks   = 0;
    int  lastBalanceTick  = -1;
    bool unbalanced       = false;
};

// ---------------------------------------------------------------- 系统
class ThreeQiSystem {
public:
    // 六转及以上强制解锁三气静修平衡模式
    static bool requiresBalance(Rank r) { return is_immortal_rank(r); }

    // 三气差值
    static double deviation(const ThreeQiPool& p) { return p.deviation(); }
    static bool  isBalanced(const ThreeQiPool& p, double tolerance) {
        return p.deviation() <= tolerance;
    }

    // 是否处于「自身仙窍内部 / 自己掌控的福地洞天内部」
    static bool isInsideOwnParadise(const Cultivator& c, const WorldMap& world);

    // ---------------- 流程 a：外出搜集三气资源 ----------------
    // 仅在非闭关状态可用（闭关期间无法外出）
    static Result<ThreeQiPool> gather(const Cultivator& c, const WorldMap& world,
                                      double effort = 1.0);

    // ---------------- 流程 b：返回自家仙窍 / 福地闭关 ----------------
    static Result<void> beginSeclusion(Cultivator& c, const WorldMap& world,
                                       const ThreeQiConfig& cfg = {});

    // ---------------- 流程 d：完成平衡，解除闭关 ----------------
    static Result<void> endSeclusion(Cultivator& c);

    // ---------------- 流程 c：定点不动调和三气差值 ----------------
    // 每个世界刻度调用一次
    struct TickReport {
        bool reconciling  = false;
        bool becameBalanced = false;
        bool becameUnbalanced = false;
        double deviationBefore = 0;
        double deviationAfter  = 0;
        double qiConsumed = 0;
        double essenceSpent = 0;
        std::string note;
    };
    static TickReport tick(Cultivator& c, const ThreeQiConfig& cfg = {});

    // ---------------- 对外语义：行动锁 ----------------
    // 闭关期间：无法移动、无法战斗、无法外出（定义见 ThreeQi.cpp）
    static bool canMove(const Cultivator& c);
    static bool canBattle(const Cultivator& c);
    static bool canLeave(const Cultivator& c);
};

} // namespace gr
