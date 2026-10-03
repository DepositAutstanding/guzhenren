// 三气平衡系统实现
#include "gr/cultivator/ThreeQi.hpp"
#include "gr/cultivator/Cultivator.hpp"

#include <algorithm>
#include <cmath>

namespace gr {

bool ThreeQiSystem::isInsideOwnParadise(const Cultivator& c, const WorldMap& world) {
    (void)world;   // 只需判定「是否身处自己掌控的洞天内部」，无需查询世界
    if (!c.location.isParadiseInterior) return false;
    if (c.location.siteId.empty())      return false;
    return c.owns(c.location.siteId);
}

Result<ThreeQiPool> ThreeQiSystem::gather(const Cultivator& c, const WorldMap& world,
                                          double effort) {
    if (c.threeQi.inSeclusion) {
        return Result<ThreeQiPool>::fail(Err::LockedBySeclusion,
                                         "闭关调息期间无法外出搜集三气");
    }
    if (!requiresBalance(c.rank)) {
        return Result<ThreeQiPool>::fail(Err::RankTooLow,
                                         "六转以下无三气平衡需求，无需搜集三气");
    }
    WorldMap::QiYield y = world.collectQi(c.location, effort);
    ThreeQiPool got{y.heaven, y.earth, y.human};
    return Result<ThreeQiPool>::success(got,
        std::string("于 ") + to_string(c.location.layer) + " 搜集三气（天气+" +
        std::to_string(y.heaven) + "，地气+" + std::to_string(y.earth) +
        "，人气+" + std::to_string(y.human) + "）");
}

// 对外语义：闭关期间无法移动 / 无法战斗 / 无法外出
bool ThreeQiSystem::canMove(const Cultivator& c)   { return !c.threeQi.inSeclusion; }
bool ThreeQiSystem::canBattle(const Cultivator& c) { return !c.threeQi.inSeclusion; }
bool ThreeQiSystem::canLeave(const Cultivator& c)  { return !c.threeQi.inSeclusion; }

Result<void> ThreeQiSystem::beginSeclusion(Cultivator& c, const WorldMap& world,
                                           const ThreeQiConfig&) {
    if (!requiresBalance(c.rank)) {
        return Result<void>::fail(Err::RankTooLow,
                                  "仅六转及以上蛊仙需要三气静修平衡");
    }
    if (c.threeQi.inSeclusion) {
        return Result<void>::fail(Err::LockedBySeclusion, "已在闭关中");
    }
    if (!isInsideOwnParadise(c, world)) {
        return Result<void>::fail(Err::NotInOwnParadise,
            "维持三气平衡必须居于自身仙窍内部 / 自己掌控的福地洞天内部");
    }
    c.threeQi.inSeclusion    = true;
    c.threeQi.seclusionTicks = 0;
    return Result<void>::success(
        "进入定点闭关调和三气：期间无法移动、无法战斗、无法外出");
}

Result<void> ThreeQiSystem::endSeclusion(Cultivator& c) {
    if (!c.threeQi.inSeclusion) {
        return Result<void>::fail(Err::LockedBySeclusion, "未处于闭关状态");
    }
    c.threeQi.inSeclusion    = false;
    c.threeQi.seclusionTicks = 0;
    return Result<void>::success("平衡完成，解除闭关，恢复行动");
}

ThreeQiSystem::TickReport ThreeQiSystem::tick(Cultivator& c, const ThreeQiConfig& cfg) {
    TickReport rep;
    if (!requiresBalance(c.rank)) return rep;

    rep.deviationBefore = c.qi.deviation();

    // ---- 自然消耗：转数越高，三气消耗越快 ----
    const double upkeep = cfg.upkeepPerTick * (1.0 + 0.35 * (rank_value(c.rank) - 6));
    double drain = std::min(upkeep, c.qi.total() / 3.0);
    c.qi.heaven = std::max(0.0, c.qi.heaven - drain);
    c.qi.earth  = std::max(0.0, c.qi.earth  - drain);
    c.qi.human  = std::max(0.0, c.qi.human  - drain);
    rep.qiConsumed = drain * 3.0;

    // ---- 闭关调和：定点不动，把三气向均值收敛 ----
    if (c.threeQi.inSeclusion) {
        ++c.threeQi.seclusionTicks;

        if (c.essence < cfg.essencePerTick) {
            // 仙元耗尽，被迫中断闭关
            endSeclusion(c);
            rep.note = "仙元耗尽，闭关被迫中断";
        } else {
            c.essence -= cfg.essencePerTick;
            rep.essenceSpent = cfg.essencePerTick;
            rep.reconciling  = true;

            const double mean = c.qi.mean();
            auto pull = [&](double& v) {
                const double diff = v - mean;
                const double step = std::min(std::abs(diff), cfg.reconcileRate);
                v += (diff >= 0 ? -step : step);
            };
            pull(c.qi.heaven); pull(c.qi.earth); pull(c.qi.human);
        }
    }

    rep.deviationAfter = c.qi.deviation();

    // ---- 平衡 / 失衡判定 ----
    const bool balanced = isBalanced(c.qi, cfg.tolerance);
    if (balanced && c.threeQi.lastBalanceTick < 0) {
        rep.becameBalanced = true;
        c.threeQi.lastBalanceTick = 0;
    } else if (!balanced) {
        c.threeQi.lastBalanceTick = -1;
    }

    const bool overLimit = c.qi.deviation() > cfg.imbalanceLimit;
    if (overLimit && !c.threeQi.unbalanced) {
        c.threeQi.unbalanced = true;
        rep.becameUnbalanced = true;

        StatusEffect s;
        s.kind      = StatusKind::ThreeQiImbalance;
        s.name      = "三气失衡";
        s.magnitude = std::min(1.0, (c.qi.deviation() - cfg.imbalanceLimit) / 60.0);
        apply_status(c.statuses, s);
    } else if (!overLimit && c.threeQi.unbalanced) {
        c.threeQi.unbalanced = false;
    }
    return rep;
}

} // namespace gr
