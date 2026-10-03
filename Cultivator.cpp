// 修士系统实现
#include "gr/cultivator/Cultivator.hpp"
#include "gr/gu/Inventory.hpp"
#include "gr/core/CanonNumbers.hpp"
#include "gr/cultivator/ThreeQi.hpp"
#include "gr/cultivator/DingXianYou.hpp"

#include <algorithm>
#include <cmath>

namespace gr {

bool Cultivator::owns(const std::string& siteId) const {
    return std::find(ownedSites.begin(), ownedSites.end(), siteId) != ownedSites.end();
}

Result<void> CultivatorSystem::breakthrough(Cultivator& c, Rank newRank) {
    if (rank_value(newRank) <= rank_value(c.rank)) {
        return Result<void>::fail(Err::RankTooLow, "新修为不高于当前修为");
    }
    const Rank old = c.rank;

    // ------------------------------------------------------------------
    //  成九转（尊位）：必须满足「成尊四条件」，不是修为够就能升
    //  数值口径库（快懂百科「九转蛊尊」）：四项须同时满足，
    //  最后一项令仙窍本源质变为黄杏仙元。
    // ------------------------------------------------------------------
    if (rank_value(newRank) >= 9) {
        auto chk = CultivatorSystem::checkVenerableFitness(c);
        if (!chk.satisfied) {
            return Result<void>::fail(Err::RankTooLow,
                "未满足成尊条件，无法登临九转：\n" + chk.detail);
        }
    }

    c.rank = newRank;

    std::string d = std::string("突破：") + to_string(old) + " → " + to_string(newRank);

    // 5.2.1 突破六转后，强制解锁三气静修平衡模式
    if (is_immortal_rank(newRank) && !is_immortal_rank(old)) {
        // 真元 → 仙元：本质是质变，凡人真元量与蛊仙仙元量不可同日而语。
        // 以 kEssenceConversionRate 折算（真元远多于仙元），并大幅抬高上限。
        const double converted = c.essence * gr::kEssenceConversionRate;
        c.maxEssence = std::max(1000.0, c.maxEssence * gr::kEssenceMaxGrowth);
        c.essence    = std::min(c.maxEssence, converted);
        d += "；真元尽数转化为仙元（" + std::to_string((long long)c.essence) +
             "），强制解锁三气静修平衡模式（需定期回仙窍/福地闭关调和）";
    }
    // 八转：仙窍称洞天（六转七转称福地）
    if (rank_value(newRank) == 8) {
        d += "；仙窍升格为洞天（六转七转称福地，八转九转称洞天）";
    }
    return Result<void>::success(d);
}

// ---------------------------------------------------------------------------
//  成尊四条件校验（数值口径库：快懂百科「九转蛊尊」）
// ---------------------------------------------------------------------------
CultivatorSystem::VenerableFitness
CultivatorSystem::checkVenerableFitness(const Cultivator& c) {
    using namespace canon;
    VenerableFitness f;
    f.requirement = VenerableRequirement{};

    // ① 仙窍本源已产出白荔仙元（即已达八转仙元层级）
    //    口径：要求达到八转仙元层级，而不是仅修到九转。
    f.hasBaiLiSource = (rank_value(c.rank) >= 8);

    // ② 主修流派道痕 ≥ 30 万（必须是主修流派，不是所有流派合计）
    f.mainDaoMarks  = c.mainDaoMarks();
    f.enoughMarks   = (f.mainDaoMarks >= f.requirement.mainDaoMarksThreshold);

    // ③ 主修流派达到无上大宗师
    f.mainFlow      = c.mainFlowLevel();
    f.enoughFlow    = (static_cast<int>(f.mainFlow) >=
                       static_cast<int>(f.requirement.requiredFlowLevel));

    // ④ 突破天道封锁（灾劫、寿命、宿命）—— 外部剧情/世界状态决定，
    //    此处不自作主张判定，交由调用方通过 flag 传入。
    f.brokeHeavenlySeal = c.brokeHeavenlyDaoSeal;

    f.satisfied = f.hasBaiLiSource && f.enoughMarks && f.enoughFlow &&
                  f.brokeHeavenlySeal;

    // 诊断文案
    auto yes = [](bool b) { return b ? "[满足] " : "[未满足] "; };
    f.detail.clear();
    f.detail += std::string(yes(f.hasBaiLiSource)) +
        "①仙窍本源已产出白荔仙元（当前 " + std::string(to_string(c.rank)) + "）\n";
    f.detail += "    " + std::string(yes(f.enoughMarks)) + "②主修流派道痕 ≥ " +
        std::to_string((long long)f.requirement.mainDaoMarksThreshold) +
        "（当前主修 " + std::to_string((long long)f.mainDaoMarks) +
        "，合计 " + std::to_string((long long)c.daoMarks) + "）\n";
    f.detail += "    " + std::string(yes(f.enoughFlow)) + "③主修流派达" +
        std::string(to_string(f.requirement.requiredFlowLevel)) +
        "（当前 " + std::string(to_string(f.mainFlow)) + "）\n";
    f.detail += "    " + std::string(yes(f.brokeHeavenlySeal)) +
                "④突破天道封锁（灾劫、寿命、宿命）";
    return f;
}

Result<TraverseCost> CultivatorSystem::moveTo(Cultivator& c,
                                                         const Location& to,
                                                         const WorldMap& world) {
    // 闭关锁：无法移动
    if (!ThreeQiSystem::canMove(c)) {
        return Result<TraverseCost>::fail(Err::LockedBySeclusion);
    }
    // 胎壁
    if (to.layer == RealmLayer::WorldWombWall) {
        auto r = WorldMap::tryCrossWombWall(WombWallPrivilege::None, c.isPlayer);
        return Result<TraverseCost>::fail(r.err, r.detail);
    }
    // 层级合法性 + 罡风 + 位面压制
    auto cost = world.evaluateTraversal(c.location, to, c.rank);
    if (!cost.ok()) return cost;

    if (c.essence < cost.value.essence) {
        return Result<TraverseCost>::fail(Err::InsufficientEssence,
                                                    "真元/仙元不足以支撑此次穿行");
    }
    c.essence -= cost.value.essence;
    if (cost.value.daoMarks > 0) c.daoMarks = std::max(0.0, c.daoMarks - cost.value.daoMarks);
    c.location = to;

    // 抵达即解锁定仙游坐标（探索地图 → 积累坐标库）
    observe(c, to, SightSource::Arrived, world.now());
    return cost;
}

JumpResult CultivatorSystem::jumpByDingXianYou(Cultivator& c, const Location& to,
                                               const WorldMap& world) {
    return DingXianYou::tryJump(c, to, world, c.canUseDingXianYou());
}

void CultivatorSystem::observe(Cultivator& c, const Location& loc, SightSource src,
                               Tick at) {
    DingXianYou::observe(c, loc, src, at);
}

void CultivatorSystem::applyEnvironment(Cultivator& c, const WorldMap& world) {
    clear_env_statuses(c.statuses);
    auto effects = world.evaluateEnvironment(c.location, c.bornDomain);
    for (const auto& s : effects) {
        StatusEffect applied = s;
        // 位面压制无本土免疫；环境 DEBUFF 已由世界层按本土免疫规则减免
        apply_status(c.statuses, applied);

        // 天地规则压制：凡俗生灵平等承受，持续掉血
        if (s.kind == StatusKind::HeavenSuppression && !c.isImmortal()) {
            c.health = std::max(0.0, c.health - 0.02 * s.magnitude);
        }
        if (s.kind == StatusKind::TiangangGale &&
            world.gale().isLethalTo(c.rank)) {
            c.health = std::max(0.0, c.health - 0.25);
        }
    }
    if (c.health <= 0.0) c.alive = false;
}

Result<ThreeQiPool> CultivatorSystem::gatherQi(const Cultivator& c,
                                               const WorldMap& world, double effort) {
    return ThreeQiSystem::gather(c, world, effort);
}

Result<void> CultivatorSystem::enterSeclusion(Cultivator& c, const WorldMap& world) {
    return ThreeQiSystem::beginSeclusion(c, world);
}

Result<void> CultivatorSystem::leaveSeclusion(Cultivator& c) {
    return ThreeQiSystem::endSeclusion(c);
}

CultivatorSystem::TickReport CultivatorSystem::tick(Cultivator& c, const WorldMap& world) {
    TickReport rep;
    if (!c.alive) return rep;

    // 1) 环境结算（DEBUFF / 位面压制 / 罡风）
    applyEnvironment(c, world);
    if (!c.alive) {
        rep.diedFromEnvironment = true;
        rep.notes.push_back("死于环境压制：" + c.location.key());
        return rep;
    }

    // 2) 三气平衡推进
    rep.threeQi = ThreeQiSystem::tick(c);
    if (rep.threeQi.reconciling)
        rep.notes.push_back("闭关调息中，三气差值 " +
                            std::to_string(rep.threeQi.deviationBefore) + " → " +
                            std::to_string(rep.threeQi.deviationAfter));
    if (rep.threeQi.becameUnbalanced)
        rep.notes.push_back("三气失衡！需立即返回仙窍/福地闭关调和");

    // 3) 状态推进
    tick_statuses(c.statuses, 1);

    // 4) 仙元自然回复（非闭关状态）
    if (!c.threeQi.inSeclusion) {
        c.essence = std::min(c.maxEssence, c.essence + c.maxEssence * 0.01);
    }
    return rep;
}

double CultivatorSystem::combatPower(const Cultivator& c, Dao d) {
    DaoCoefficients co = dao_coefficients(c.dao.get(d));
    double base = 20.0 * std::pow(2.0, rank_value(c.rank) - 1);
    return (base + c.daoMarks) * co.power;
}

} // namespace gr

// ---------------------------------------------------------------------------
//  主修流派判定
//  成尊条件只认「主修流派」：境界最高者；并列时取道痕更多者。
//  绝不可把所有流派道痕合计当作成尊凭据。
// ---------------------------------------------------------------------------
namespace gr {

Dao Cultivator::mainDao() const {
    Dao   best = Dao::Refine;
    int   bestLv = -1;
    double bestMarks = -1.0;

    for (const auto& kv : flowLevels) {
        int lv = static_cast<int>(kv.second);
        double marks = 0.0;
        auto it = daoMarksByDao.find(kv.first);
        if (it != daoMarksByDao.end()) marks = it->second;

        if (lv > bestLv || (lv == bestLv && marks > bestMarks)) {
            best = kv.first; bestLv = lv; bestMarks = marks;
        }
    }
    // 未登记流派境界时，退而以道痕最多者为主修
    if (bestLv < 0) {
        double m = -1.0;
        for (const auto& kv : daoMarksByDao) {
            if (kv.second > m) { m = kv.second; best = kv.first; }
        }
    }
    return best;
}

FlowLevel Cultivator::mainFlowLevel() const {
    return flowOf(mainDao());
}

} // namespace gr
