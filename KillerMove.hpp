// ============================================================================
//  杀招：蛊虫的组合运用
//    · 组成蛊虫来自凡蛊 / 仙蛊总表
//    · 道境不足强行催动高阶杀招：反噬暴涨、杀招残缺、大概率直接崩碎蛊虫
//    · 道境越高：威力增幅、消耗降低、反噬降低、组合更稳定
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"
#include "gr/gu/GuWorm.hpp"
#include "gr/cultivator/DaoRealm.hpp"

#include <string>
#include <vector>

namespace gr {

using MoveId = std::uint64_t;

struct KillerMoveDef {
    MoveId      id = 0;
    std::string name;
    Dao         dao = Dao::Refine;
    Rank        requiredRank = Rank::R1;
    DaoLevel    requiredDao  = DaoLevel::Entry;  // 道境门槛

    // 组成蛊虫：忠实抄录资料库「杀招」表的「组成蛊虫」列原文。
    // 资料库该列大量引用未单独收录的蛊名（如心音蛊、和声蛊、巨灵心蛊），
    // 故此处以「名」记录，不臆造其转数与流派。
    std::vector<std::string> componentNames;
    std::string componentNote;    // 资料库原文中的补充说明（如「辅蛊不详」）

    // 资料库已收录、且已建立 GuTemplate 的组成蛊，才填入模板 id 参与运行时校验；
    // 为空表示「组成不详 / 含未收录蛊」，运行时不做组件齐备性校验。
    std::vector<GuId> components;

    double      basePower    = 100.0;
    double      baseEssence  = 10.0;             // 基础消耗
    double      baseBacklash = 0.10;             // 基础反噬（占最大道痕比例）
    bool        canon = true;                    // 原著有载
    std::string source;                          // 资料溯源
};

// 催动杀招的结算结果
struct MoveOutcome {
    bool   executed   = false;
    Err    err        = Err::Ok;
    double power      = 0.0;    // 实际威力
    double essenceCost= 0.0;    // 实际消耗
    double backlash   = 0.0;    // 实际反噬
    bool   incomplete = false;  // 杀招残缺
    bool   collapsed  = false;  // 杀招崩解
    std::vector<GuId> shatteredGu;  // 崩碎的蛊虫实例
    std::string detail;
};

// ============================================================================
//  杀招结算器（纯函数式，便于单元测试）
// ============================================================================
class KillerMoveResolver {
public:
    // 道境不足时的崩解概率：缺口每级 +22%，并受稳定度抑制
    static Ratio collapseChance(int gapLevels, double stability);

    // 计算一次催动的完整结果；rngRoll 为 [0,1) 随机值（由调用方提供，保证可复现）
    static MoveOutcome resolve(const KillerMoveDef& def,
                               DaoLevel casterDao,
                               Rank     casterRank,
                               double   casterEssence,
                               const std::vector<GuInstance>& availableGu,
                               const std::vector<GuTemplate>& guTemplates,
                               double   rngRoll);
};

// 原著/资料库可核验的杀招样例
std::vector<KillerMoveDef> build_canon_killer_moves();

} // namespace gr
