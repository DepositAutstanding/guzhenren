// 战斗系统实现：仙凡鸿沟 + 道痕对抗 + 蛊虫损毁/抢夺/献祭闭环
#include "gr/battle/Battle.hpp"
#include "gr/cultivator/DaoTianLegacy.hpp"

#include <algorithm>
#include <cmath>

namespace gr {

bool BattleSystem::isBattleAllowed(const Cultivator& a, const Cultivator& d,
                                   BattleContext ctx) {
    // 4. 闭关调息战斗禁止
    if (a.threeQi.inSeclusion || d.threeQi.inSeclusion) return false;
    if (ctx == BattleContext::Seclusion) return false;
    if (!a.alive || !d.alive) return false;
    return true;
}

BattleContext BattleSystem::contextOf(const Cultivator& a, const WorldMap& world) {
    if (a.threeQi.inSeclusion) return BattleContext::Seclusion;
    if (!a.location.siteId.empty()) {
        if (const CaveParadise* c = world.findCave(a.location.siteId)) {
            if (c->hasEarthSpirit) return BattleContext::EarthSpirit;
            return BattleContext::ParadiseInterior;
        }
    }
    if (isHeavenLayer(a.location.layer)) return BattleContext::Spatial;
    return BattleContext::Open;
}

bool BattleSystem::blockedByImmortalMortalGap(Rank attacker, Rank defender) {
    // 1. 仙凡鸿沟完整保留：五转及以下无法伤及六转及以上
    return !is_immortal_rank(attacker) && is_immortal_rank(defender);
}

BattleReport BattleSystem::engage(Cultivator& attacker, Cultivator& defender,
                                  const KillerMoveDef& atkMove,
                                  const KillerMoveDef& defMove,
                                  const std::vector<GuTemplate>& templates,
                                  const WorldMap& world,
                                  ImmortalGuRegistry& registry,
                                  double rngRollAtk, double rngRollDef,
                                  double rngRollLoot) {
    BattleReport rep;
    const BattleContext ctx = contextOf(attacker, world);

    if (!isBattleAllowed(attacker, defender, ctx)) {
        rep.err    = Err::LockedBySeclusion;
        rep.detail = std::string("战斗被禁止（场景：") + to_string(ctx) + "）";
        return rep;
    }
    rep.occurred = true;

    // ---------------- 场景修正（需求 13：地灵战斗、空间战斗）----------------
    //  地灵战斗：洞天地灵主场作战，外来者受压制；击败地灵方可真正掌控洞天
    //  空间战斗：两天天位面夹层中作战，宇道手段增益、非宇道手段受扰
    double attackerSceneMul = 1.0;
    double defenderSceneMul = 1.0;
    if (ctx == BattleContext::EarthSpirit) {
        // 地灵：守方（洞天内一方）坐拥地灵主场之利
        defenderSceneMul *= 1.35;
        attackerSceneMul *= 0.80;
        rep.detail += "地灵主场（守方增幅、攻方受制）；";
    } else if (ctx == BattleContext::Spatial) {
        // 空间战斗：宇道杀招威力提升，其余流派受位面夹层扰动
        auto spatialBoost = [](const KillerMoveDef& mv) {
            return mv.dao == Dao::Space ? 1.30 : 0.85;
        };
        attackerSceneMul *= spatialBoost(atkMove);
        defenderSceneMul *= spatialBoost(defMove);
        rep.detail += "空间战斗（宇道增益、他道受扰）；";
    }

    // ---------------- 攻方杀招 ----------------
    MoveOutcome atk = KillerMoveResolver::resolve(
        atkMove, attacker.dao.get(atkMove.dao), attacker.rank,
        attacker.essence, attacker.carriedGu, templates, rngRollAtk);
    rep.attackerMove = atk;

    if (atk.executed) {
        attacker.essence -= atk.essenceCost;

        // ---------------- 道痕对抗 ----------------
        DaoCoefficients atkC = dao_coefficients(attacker.dao.get(atkMove.dao));
        DaoCoefficients defC = dao_coefficients(defender.dao.get(atkMove.dao));

        rep.attackerEffectiveMarks = attacker.daoMarks * (1.0 + atkC.power * 0.1);
        rep.defenderResistance     = defender.daoMarks * defC.resistance;

        double dmg = atk.power * attackerSceneMul;
        // 仙凡鸿沟：凡俗对蛊仙无伤
        if (blockedByImmortalMortalGap(attacker.rank, defender.rank)) {
            dmg = 0.0;
            rep.detail += "仙凡鸿沟：凡俗手段无法撼动蛊仙；";
        }
        // 道痕抗性减免
        dmg *= (1.0 - std::min(0.75, defC.resistance));
        // 转数压制
        dmg *= std::pow(1.35, rank_value(attacker.rank) - rank_value(defender.rank));

        rep.damageToDefender = dmg;
        defender.health = std::max(0.0, defender.health - dmg / (100.0 + defender.daoMarks));

        // 反噬
        attacker.health = std::max(0.0, attacker.health - atk.backlash);
        attacker.daoMarks = std::max(0.0, attacker.daoMarks * (1.0 - atk.backlash * 0.3));

        // 崩碎的蛊虫：从攻方身上移除；若为仙蛊则彻底毁灭（释放唯一名额）
        for (GuId id : atk.shatteredGu) {
            auto it = std::find_if(attacker.carriedGu.begin(), attacker.carriedGu.end(),
                                   [id](const GuInstance& g) { return g.instanceId == id; });
            if (it == attacker.carriedGu.end()) continue;

            std::string guName;
            for (const auto& t : templates)
                if (t.id == it->templateId) guName = t.name;
            if (!guName.empty() && registry.lookupByName(guName)) {
                auto e = registry.lookupByName(guName);
                if (e && e->instanceId == it->instanceId) registry.destroy(e->instanceId);
            }
            rep.guTransfers.push_back({id, guName, GuTransfer::Kind::Shattered,
                                       attacker.id, ""});
            attacker.carriedGu.erase(it);
        }
    }

    // ---------------- 守方反击 ----------------
    if (defender.alive && defender.health > 0) {
        MoveOutcome def = KillerMoveResolver::resolve(
            defMove, defender.dao.get(defMove.dao), defender.rank,
            defender.essence, defender.carriedGu, templates, rngRollDef);
        rep.defenderMove = def;
        if (def.executed && !blockedByImmortalMortalGap(defender.rank, attacker.rank)) {
            defender.essence -= def.essenceCost;
            DaoCoefficients atkCR = dao_coefficients(attacker.dao.get(defMove.dao));
            double dmg = def.power * defenderSceneMul
                       * (1.0 - std::min(0.75, atkCR.resistance))
                       * std::pow(1.35, rank_value(defender.rank) - rank_value(attacker.rank));
            rep.damageToAttacker = dmg;
            attacker.health = std::max(0.0, attacker.health - dmg / (100.0 + attacker.daoMarks));
            defender.health = std::max(0.0, defender.health - def.backlash);
        }
    }

    // ---------------- 死亡判定与蛊虫抢夺 ----------------
    if (defender.health <= 0.0 && !defender.carriedGu.empty()) {
        defender.alive = false;
        rep.defenderDied = true;
        // 抢夺：优先夺仙蛊（不清除注册表，只转移持有者）
        for (const auto& g : defender.carriedGu) {
            std::string guName;
            const GuTemplate* tpl = nullptr;
            for (const auto& t : templates) if (t.id == g.templateId) { tpl = &t; break; }
            if (tpl) guName = tpl->name;
            const bool worth = tpl && tpl->isImmortal() && rngRollLoot < 0.6;
            if (!worth) continue;

            auto e = registry.lookupByName(guName);
            if (e && e->instanceId == g.instanceId) {
                registry.transfer(e->instanceId, attacker.id);
                rep.guTransfers.push_back({g.instanceId, guName,
                                           GuTransfer::Kind::Looted,
                                           defender.id, attacker.id});
                attacker.carriedGu.push_back(g);
            }
        }
    }
    if (attacker.health <= 0.0) { attacker.alive = false; rep.attackerDied = true; }

    if (rep.detail.empty())
        rep.detail = std::string("场景：") + to_string(ctx);
    return rep;
}

Result<void> BattleSystem::lootGu(Cultivator& from, Cultivator& to, GuId instanceId,
                                  ImmortalGuRegistry& registry,
                                  const std::vector<GuTemplate>&) {
    auto it = std::find_if(from.carriedGu.begin(), from.carriedGu.end(),
                           [instanceId](const GuInstance& g) {
                               return g.instanceId == instanceId;
                           });
    if (it == from.carriedGu.end())
        return Result<void>::fail(Err::MaterialMissing, "目标未持有该蛊");

    auto reg = registry.transfer(instanceId, to.id);
    if (!reg.ok()) return reg;

    to.carriedGu.push_back(*it);
    from.carriedGu.erase(it);
    return Result<void>::success("抢夺成功（注册表持有者变更，仙蛊唯一不变）");
}

Result<void> BattleSystem::sacrificeGu(Cultivator& owner, GuId instanceId,
                                       ImmortalGuRegistry& registry,
                                       const std::vector<GuTemplate>& templates) {
    auto it = std::find_if(owner.carriedGu.begin(), owner.carriedGu.end(),
                           [instanceId](const GuInstance& g) {
                               return g.instanceId == instanceId;
                           });
    if (it == owner.carriedGu.end())
        return Result<void>::fail(Err::MaterialMissing, "未持有该蛊");

    std::string guName;
    bool immortal = false;
    for (const auto& t : templates)
        if (t.id == it->templateId) { guName = t.name; immortal = t.isImmortal(); break; }

    if (immortal) {
        auto e = registry.lookupByName(guName);
        // 仿伪蛊 / 尊者幻象不占注册表名额
        if (e && e->instanceId == instanceId) registry.destroy(instanceId);
    }
    owner.carriedGu.erase(it);
    return Result<void>::success("献祭「" + guName + "」：彻底毁灭，唯一名额释放");
}

// ---------------------------------------------------------------------------
//  至尊仙窍吞并（研究报告 2.2／7.2）
//  至尊仙胎体允许无常规流派壁垒地吞并其他仙窍。吞并后：
//    · 洞天归吞并者所有，并从「可被争夺的对象」中移除
//    · 吞并过快会带来内部平衡与灾劫压力 —— 故压缩万劫倒计时
// ---------------------------------------------------------------------------
Result<void> BattleSystem::swallowCave(Cultivator& owner, CaveParadise& target,
                                       WorldMap& world) {
    // 吞并不改变世界表结构与分布，仅改写洞天归属与争夺标记
    (void)world;
    if (!owner.alive)
        return Result<void>::fail(Err::RankTooLow, "施术者已陨落");

    // 必须已实际掌控该洞天（攻占在前，吞并在后）
    if (!owner.owns(target.siteId))
        return Result<void>::fail(Err::NotInOwnParadise,
                                  "尚未掌控「" + target.name + "」，无法吞并");

    target.owner    = CaveOwnership::PlayerOwned;
    target.ownerTag = owner.name;
    target.isSwallowedByZhiZunXianQiao = true;
    target.canBeContested = false;          // 已被吞并，不再是争夺目标

    // 吞并带来内部平衡压力：万劫倒计时压缩
    if (target.tribulationTimer > 0)
        target.tribulationTimer = std::max(5, target.tribulationTimer / 2);
    else
        target.tribulationTimer = 40;

    // 洞天产出并入吞并者的三气储备（扩张链条的「资源」环节）
    owner.qi.heaven += target.qiYieldHeaven * 0.5;
    owner.qi.earth  += target.qiYieldEarth  * 0.5;
    owner.qi.human  += target.qiYieldHuman  * 0.5;

    owner.daoMarks += 20000.0;   // 吞并仙窍带来的道痕增益
    return Result<void>::success("「" + target.name +
                                 "」已吞并入至尊仙窍；内部平衡压力上升，万劫倒计时压缩至 " +
                                 std::to_string(target.tribulationTimer) + " 刻");
}

// ---------------------------------------------------------------------------
//  定仙游迁移（需求 3.2：定仙游迁移机制完整保留）
//  以定仙游将整座洞天搬迁至另一坐标 —— 目标坐标同样受
//  「必须亲眼见过 / 抵达过 / 感知过」铁律约束。
// ---------------------------------------------------------------------------
Result<void> BattleSystem::migrateCaveByDingXianYou(Cultivator& mover,
                                                    CaveParadise& target,
                                                    const Location& destination,
                                                    const WorldMap& world) {
    // 坐标合法性由 Cultivator 的坐标库判定；此处 world 仅用于未来扩展
    // （如校验目标层级的胎壁/罡风约束），当前保留参数以维持接口稳定。
    (void)world;
    if (!mover.canUseDingXianYou())
        return Result<void>::fail(Err::DingXianYouNotHeld,
                                  "未持有定仙游，无法迁移洞天");

    // 铁律：迁移目标也必须是已知坐标
    if (!mover.dingXianYou.knows(destination))
        return Result<void>::fail(Err::CoordinateUnknown,
                                  "未探索 / 未抵达 / 未亲眼看见的坐标，无法迁移洞天");

    // 闭关期间无法外出操作
    if (!ThreeQiSystem::canLeave(mover))
        return Result<void>::fail(Err::LockedBySeclusion,
                                  "定点闭关调和三气中，无法迁移洞天");

    if (!mover.owns(target.siteId))
        return Result<void>::fail(Err::NotInOwnParadise,
                                  "未掌控「" + target.name + "」，无法迁移");

    // 世界胎壁：洞天不可被迁入绝对边界
    if (destination.layer == RealmLayer::WorldWombWall)
        return Result<void>::fail(Err::WombWallImpassable,
                                  "世界胎壁为绝对屏障，洞天不可迁入");

    // 迁移消耗：远高于单人挪移（搬的是整座洞天）
    double cost = 300.0 + 40.0 * rank_value(mover.rank);
    if (mover.essence < cost)
        return Result<void>::fail(Err::InsufficientEssence,
                                  "仙元不足以支撑洞天迁移（需 " + std::to_string(cost) + "）");

    mover.essence -= cost;

    // 搬迁：更新洞天层级/域属/地域，并保留地灵与万劫进度
    target.layer  = destination.layer;
    target.domain = destination.domain;
    target.region = destination.region;

    return Result<void>::success("洞天「" + target.name + "」已迁移至 " +
                                 destination.key() + "，仙元 -" +
                                 std::to_string((long long)cost));
}

BattleSystem::DeathResult BattleSystem::handlePlayerDeath(
    Cultivator& player, ImmortalGuRegistry& registry,
    const std::vector<GuTemplate>& templates, bool keepDingXianYouCoordinates) {
    DeathResult r;

    // 蛊虫尽失：仙蛊在注册表中转为「无主/在世」，不清除注册表
    for (const auto& g : player.carriedGu) {
        std::string guName;
        bool immortal = false;
        for (const auto& t : templates)
            if (t.id == g.templateId) { guName = t.name; immortal = t.isImmortal(); break; }
        if (immortal) {
            auto e = registry.lookupByName(guName);
            if (e && e->instanceId == g.instanceId)
                registry.transfer(e->instanceId, "无主");
        }
        r.lostGuNames.push_back(guName);
    }
    player.carriedGu.clear();

    // 定仙游坐标库：默认保留（轮回记忆的一部分）
    if (!keepDingXianYouCoordinates) {
        for (const auto& s : player.dingXianYou.all())
            player.dingXianYou.forget(s.loc);
    } else {
        for (const auto& s : player.dingXianYou.all())
            r.knownSitesPreserved.push_back(s.loc.key());
    }

    // 5. 玩家死亡轮回保留盗天传承记忆（需求 13）
    //    蛊虫尽失，但盗天真传分支与领悟度不失。
    r.keptDaoTianLegacy = DaoTianLegacySystem::preserveOnDeath(player, r.legacy);
    r.legacyBranches.clear();
    for (DaoTianBranch b : r.legacy.branches)
        r.legacyBranches.push_back(to_string(b));
    r.reincarnated = true;

    // 轮回：修为与道痕清空，但道境（大道领悟）与盗天传承记忆保留
    player.rank     = Rank::R1;
    player.daoMarks = 0.0;
    player.essence  = player.maxEssence = 100.0;
    player.health   = 1.0;
    player.alive    = true;
    player.threeQi.inSeclusion = false;
    player.threeQi.seclusionTicks = 0;

    r.detail = "死亡轮回：盗天传承记忆保留；定仙游坐标保留 " +
               std::to_string(r.knownSitesPreserved.size()) + " 处；失去蛊虫 " +
               std::to_string(r.lostGuNames.size()) + " 只";
    return r;
}

} // namespace gr
