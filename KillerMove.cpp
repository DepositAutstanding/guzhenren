// 杀招结算实现
#include "gr/gu/KillerMove.hpp"

#include <algorithm>
#include <unordered_map>

namespace gr {

Ratio KillerMoveResolver::collapseChance(int gapLevels, double stability) {
    if (gapLevels >= 0) return 0.0;
    double raw = 0.22 * static_cast<double>(-gapLevels);
    raw *= (1.0 - stability * 0.6);   // 道境越高，组合越稳定，越不易崩解
    return std::min(0.95, raw);
}

MoveOutcome KillerMoveResolver::resolve(const KillerMoveDef& def,
                                        DaoLevel casterDao,
                                        Rank     casterRank,
                                        double   casterEssence,
                                        const std::vector<GuInstance>& availableGu,
                                        const std::vector<GuTemplate>& guTemplates,
                                        double   rngRoll) {
    MoveOutcome out;

    // ---------- 仙凡鸿沟：转数不足，根本催不动 ----------
    if (rank_value(casterRank) < rank_value(def.requiredRank)) {
        out.err    = Err::ImmortalMortalGap;
        out.detail = "仙凡鸿沟：" + std::string(to_string(casterRank)) +
                     " 无法催动 " + to_string(def.requiredRank) + " 级杀招";
        return out;
    }

    // ---------- 前置校验：组成蛊虫是否齐备 ----------
    // 仅对资料库已收录且已建立 GuTemplate 的组成蛊做校验。
    // 若 components 为空（资料库标注「组成不详」或组成蛊未单列条目），
    // 则不因无法核验而阻碍催动——不能把「资料缺失」误判为「蛊虫不齐」。
    std::unordered_map<GuId, const GuTemplate*> tpl;
    for (const auto& t : guTemplates) tpl[t.id] = &t;

    std::vector<const GuInstance*> used;
    for (GuId need : def.components) {
        const GuInstance* found = nullptr;
        for (const auto& g : availableGu) {
            if (g.templateId == need && g.usable()) { found = &g; break; }
        }
        if (!found) {
            out.err    = Err::MaterialMissing;
            out.detail = "杀招「" + def.name + "」组成蛊虫不齐，无法催动";
            return out;
        }
        used.push_back(found);
    }

    // ---------- 道境系数 ----------
    DaoCoefficients c = dao_coefficients(casterDao);
    const int gap = dao_gap(casterDao, def.requiredDao);

    out.power       = def.basePower * c.power;
    out.essenceCost = def.baseEssence * c.cost;
    out.backlash    = def.baseBacklash * c.backlash;

    // ---------- 道境不足：反噬暴涨 + 杀招残缺 ----------
    if (gap < 0) {
        double deficitMul = 1.0 + 0.75 * static_cast<double>(-gap);  // 缺口每级 +75%
        out.backlash   *= deficitMul;
        out.power      *= (1.0 / (1.0 + 0.35 * static_cast<double>(-gap)));
        out.incomplete  = true;

        // 大概率直接崩碎蛊虫
        const double chance = collapseChance(gap, c.stability);
        if (rngRoll < chance) {
            out.collapsed = true;
            // 崩碎：优先碎掉完整度最低的组成蛊。
            // 若该杀招的组成蛊未见于资料库（components 为空），
            // 仍须体现「崩解毁蛊」——退而碎掉随身完整度最低的一只蛊。
            const GuInstance* weakest = nullptr;
            for (const auto* g : used) {
                if (!weakest || g->integrity < weakest->integrity) weakest = g;
            }
            if (!weakest) {
                for (const auto& g : availableGu) {
                    if (!g.usable()) continue;
                    if (!weakest || g.integrity < weakest->integrity) weakest = &g;
                }
            }
            if (weakest) out.shatteredGu.push_back(weakest->instanceId);
            out.power    *= 0.25;
            out.backlash *= 1.5;
            out.err    = Err::MoveUnstable;
            out.detail = "道境不足（缺 " + std::to_string(-gap) +
                         " 级）：反噬暴涨、杀招残缺，蛊虫崩碎！";
            out.executed = true;
            return out;
        }
        out.detail = "道境不足（缺 " + std::to_string(-gap) +
                     " 级）：反噬暴涨、杀招残缺，勉强催动";
        out.executed = true;
        return out;
    }

    // ---------- 仙元不足 ----------
    if (casterEssence < out.essenceCost) {
        out.err    = Err::InsufficientEssence;
        out.detail = "仙元不足，杀招未能成形";
        return out;
    }

    out.executed = true;
    out.detail   = std::string("道境·") + to_string(casterDao) +
                   "：威力×" + std::to_string(c.power) +
                   "，消耗×" + std::to_string(c.cost) +
                   "，反噬×" + std::to_string(c.backlash);
    return out;
}

// ---------------------------------------------------------------------------
//  资料库「杀招」表（共 101 条）的核验样例
//  转数、流派、组成蛊虫一律照抄资料库原文，不臆造未收录蛊的转数与流派。
//  componentNames 保存原文；components 只填资料库已收录且已建模板的蛊。
// ---------------------------------------------------------------------------
std::vector<KillerMoveDef> build_canon_killer_moves() {
    std::vector<KillerMoveDef> m;
    auto add = [&m](KillerMoveDef d) { m.push_back(std::move(d)); };

    // 资料库：三转 / 魂道、音道复合 / 心音蛊＋和声蛊＋飞魂蛊＋魂链蛊＋巨灵心蛊
    // 其中 飞魂蛊、魂链蛊 为凡蛊总表已收录之四转魂道蛊；
    // 心音蛊、和声蛊、巨灵心蛊 资料库未单列条目，故只记录其名。
    { KillerMoveDef d; d.id=1; d.name="三心合魂"; d.dao=Dao::Soul;
      d.requiredRank=Rank::R3; d.requiredDao=DaoLevel::Entry;
      d.componentNames={"心音蛊","和声蛊","飞魂蛊","魂链蛊","巨灵心蛊"};
      d.components={12, 13};      // 飞魂蛊、魂链蛊（已建模板，参与齐备性校验）
      d.componentNote="心音蛊、和声蛊、巨灵心蛊 未见于资料库单列条目，转数待考";
      d.basePower=90; d.baseEssence=8; d.baseBacklash=0.05;
      d.source="杀招表：三转 魂道、音道复合"; add(d); }

    // 资料库：三转 / 力道 / 巨灵身蛊＋巨灵心蛊＋巨灵意蛊（三者资料库均未单列）
    { KillerMoveDef d; d.id=2; d.name="巨灵变"; d.dao=Dao::Strength;
      d.requiredRank=Rank::R3; d.requiredDao=DaoLevel::Entry;
      d.componentNames={"巨灵身蛊","巨灵心蛊","巨灵意蛊"};
      d.componentNote="组成蛊资料库均未单列条目，转数待考";
      d.basePower=110; d.baseEssence=10; d.baseBacklash=0.05;
      d.source="杀招表：三转 力道"; add(d); }

    // 资料库：七转 / 剑道 / 浪剑仙蛊＋水道仙蛊＋五百凡蛊
    { KillerMoveDef d; d.id=3; d.name="剑浪三叠"; d.dao=Dao::Sword;
      d.requiredRank=Rank::R7; d.requiredDao=DaoLevel::Small;
      d.componentNames={"浪剑仙蛊","水道仙蛊","五百凡蛊"};
      d.componentNote="浪剑仙蛊、水道仙蛊 资料库未单列条目；五百凡蛊为数量泛指";
      d.basePower=420; d.baseEssence=60; d.baseBacklash=0.08;
      d.source="杀招表：七转 剑道"; add(d); }

    // 资料库：七转 / 人道（奴力合流）/ 刃蛊＋浮生火＋饮刃酒＋正反狙神针等
    // 方源以之分身体系的核心杀招；后炼成「万我仙蛊」简化催动。
    { KillerMoveDef d; d.id=4; d.name="万我"; d.dao=Dao::Human;
      d.requiredRank=Rank::R7; d.requiredDao=DaoLevel::Great;
      d.componentNames={"刃蛊","浮生火","饮刃酒","正反狙神针"};
      d.componentNote="组成蛊资料库未单列条目；后以“万我仙蛊”为蛊方炼成简化核心";
      d.basePower=520; d.baseEssence=60; d.baseBacklash=0.12;
      d.source="杀招表：七转 人道（奴力合流）"; add(d); }

    // 资料库：七转 / 人道 / 万我杀招＋自力更生蛊＋全力以赴蛊＋长毛炼道大阵等
    // 全力以赴蛊 为凡蛊总表已收录之五转辅助·兽力蛊。
    { KillerMoveDef d; d.id=5; d.name="万我仙蛊（杀招固化）"; d.dao=Dao::Human;
      d.requiredRank=Rank::R7; d.requiredDao=DaoLevel::Great;
      d.componentNames={"万我杀招","自力更生蛊","全力以赴蛊","长毛炼道大阵"};
      d.components={17};          // 全力以赴蛊（已建模板）
      d.componentNote="自力更生蛊、长毛炼道大阵 资料库未单列条目";
      d.basePower=560; d.baseEssence=18; d.baseBacklash=0.06;   // 固化后仙元损耗大减
      d.source="杀招表：七转 人道（万我仙蛊·杀招固化）"; add(d); }

    // 资料库：八转 / 律道 / 坚持＋挽澜＋逆流河底蕴＋万我根基
    { KillerMoveDef d; d.id=6; d.name="逆流护身印"; d.dao=Dao::Rule;
      d.requiredRank=Rank::R8; d.requiredDao=DaoLevel::Foundation;
      d.componentNames={"坚持","挽澜","逆流河底蕴","万我根基"};
      d.componentNote="资料库原文写作「坚持」「挽澜」，对应条目待考；持续消耗逆流河水";
      d.basePower=680; d.baseEssence=120; d.baseBacklash=0.10;
      d.source="杀招表：八转 律道（万我变招第二式）"; add(d); }

    // 资料库：六转 / 运道、智道复合 / 运筹仙蛊＋不详
    { KillerMoveDef d; d.id=7; d.name="运筹帷幄"; d.dao=Dao::Luck;
      d.requiredRank=Rank::R6; d.requiredDao=DaoLevel::Small;
      d.componentNames={"运筹仙蛊"};
      d.componentNote="辅蛊不详";
      d.basePower=200; d.baseEssence=40; d.baseBacklash=0.05;
      d.source="杀招表：六转 运道、智道复合"; add(d); }

    // 资料库：六转 / 梦道 / 组成不详
    { KillerMoveDef d; d.id=8; d.name="纯梦求真变"; d.dao=Dao::Dream;
      d.requiredRank=Rank::R6; d.requiredDao=DaoLevel::Small;
      d.componentNames={};
      d.componentNote="组成不详";
      d.basePower=260; d.baseEssence=50; d.baseBacklash=0.07;
      d.source="杀招表：六转 梦道"; add(d); }

    // 气绝逢生：资料库杀招表未收录，出自第六卷卷初研究报告——
    // 气海分身以气绝魔仙重生法为原型，用「气绝逢生」炼化气功果。
    { KillerMoveDef d; d.id=9; d.name="气绝逢生"; d.dao=Dao::Qi;
      d.requiredRank=Rank::R8; d.requiredDao=DaoLevel::Foundation;
      d.componentNames={};
      d.componentNote="资料库杀招表未收录；出处为第六卷卷初研究报告（气海老祖炼化气功果）";
      d.canon = false;
      d.basePower=900; d.baseEssence=200; d.baseBacklash=0.20;
      d.source="第六卷卷初研究报告（非资料库杀招表条目）"; add(d); }

    return m;
}

} // namespace gr
