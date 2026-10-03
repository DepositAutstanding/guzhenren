// ============================================================================
// 第三章 洞天福地全新分布大修正（重点修复原著错位）
//   1. 异人洞天、异族洞天「大量分布在白天」（旧版完全错误，现已修复）
//   2. 白天构成：人道天庭洞天集群 + 大量古老异族/异人洞天
//      · 部分异人洞天被方源吞并掌控（第六卷开局已存在）
//      · 部分异人洞天被天庭攻占、奴役、驻守
//   3. 黑天：以天道原生秘境、天道洞天、少量异族洞天为主
//   4. 原著有名洞天福地全部固定唯一实例，不随机生成
//   5. 太古七天碎片、幽天魔渊洞天可随机生成
//   6. 全部散落于两天五域全域，无虚空空域
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"
#include "gr/core/Status.hpp"
#include "gr/core/CanonNumbers.hpp"

#include <string>
#include <vector>
#include <optional>

namespace gr {

class Rng;

enum class CaveKind : std::uint8_t {
    RenownedParadise = 0,  // 原著有名洞天福地：固定唯一实例，不随机生成
    YirenCave,             // 异人洞天
    YizuCave,              // 异族洞天
    HeavenDaoSecret,       // 天道原生秘境（黑天为主）
    HeavenDaoCave,         // 天道洞天
    TaiguSevenFragment,    // 太古七天碎片：可随机生成
    YouTianMoyuan,         // 幽天魔渊洞天：可随机生成
    TianTingCluster        // 人道天庭洞天集群
};

inline const char* to_string(CaveKind k) {
    switch (k) {
        case CaveKind::RenownedParadise:  return "原著有名洞天福地";
        case CaveKind::YirenCave:         return "异人洞天";
        case CaveKind::YizuCave:          return "异族洞天";
        case CaveKind::HeavenDaoSecret:   return "天道原生秘境";
        case CaveKind::HeavenDaoCave:     return "天道洞天";
        case CaveKind::TaiguSevenFragment:return "太古七天碎片";
        case CaveKind::YouTianMoyuan:     return "幽天魔渊洞天";
        case CaveKind::TianTingCluster:   return "人道天庭洞天集群";
    }
    return "未知洞天";
}

// 地点分类（地理研究文档：必须区分「五域内坐标」与「洞天内坐标」）
enum class SiteClass : std::uint8_t {
    MortalSurface = 0,   // ①五域地表
    TwoHeavens,          // ②五域上方两天（太古白天/黑天）
    CaveInterior,        // ③洞天 / 仙窍内部
    OuterRuleSpace       // ④宙道 / 域外规则空间（如光阴长河）
};

inline const char* to_string(SiteClass s) {
    switch (s) {
        case SiteClass::MortalSurface:  return "五域地表";
        case SiteClass::TwoHeavens:     return "两天之上";
        case SiteClass::CaveInterior:   return "洞天内部";
        case SiteClass::OuterRuleSpace: return "域外规则空间";
    }
    return "？";
}

// 洞天归属状态（第六卷开局）
enum class CaveOwnership : std::uint8_t {
    Unclaimed = 0,   // 无主
    PlayerOwned,     // 玩家掌控
    FangYuanSwallowed, // 已被方源吞并掌控
    TianTingOccupied,  // 已被天庭攻占/奴役/驻守
    NpcFaction,        // 其他 NPC 势力
    NativeRace         // 原住异人/异族自持
};

inline const char* to_string(CaveOwnership o) {
    switch (o) {
        case CaveOwnership::Unclaimed:        return "无主";
        case CaveOwnership::PlayerOwned:      return "玩家掌控";
        case CaveOwnership::FangYuanSwallowed:return "方源吞并";
        case CaveOwnership::TianTingOccupied: return "天庭占领";
        case CaveOwnership::NpcFaction:       return "NPC势力占据";
        case CaveOwnership::NativeRace:       return "原住异族自持";
    }
    return "？";
}

// ============================================================================
//  福地等级（升仙所得仙窍的规模）
//
//  原著明载（第三卷 291 节、百度百科「蛊仙」词条同源）：
//
//    下等福地：方圆至多三百万亩，引动光阴小脉支流，
//              每年产仙元十余颗，资源贫瘠。
//    中等福地：方圆四百万到六百万亩，引动光阴中脉支流，
//              年产仙元二十余颗，物产丰富。
//    上等福地：方圆七百万到九百万亩，引动光阴大脉支流，
//              年产仙元三十余颗，天地二气残留得多，
//              相互交感可将凡蛊自然点化为仙蛊。
//    特等福地：方圆一千万到两千万亩，年产仙元五十余颗，
//              时间流速极快 —— 唯【十绝体】升仙可得。
//
//  【必须澄清的一处口径】
//
//  常有人以为「资质（甲乙丙丁）决定福地大小」，原著并非如此。
//  百度百科「蛊仙」词条写得明白：
//      「吸收的天地二气越多，升仙后的福地面积就越大。」
//  即决定因素是【升仙时纳气所得天地二气的量】，属操作与机遇，
//  不是资质本身。
//
//  资质影响的是另一条线：元海占空窍的比例（丁两三成、丙四五成、
//  乙六七成、甲八九成），决定真元总量与修行快慢。
//
//  唯一的例外是十绝体：黑楼兰以大力真武体升仙即得【特等福地】 ——
//  这是体质带来的，可算「资质影响福地」的唯一明载情形。
//
//  故本模块按原著实现：面积由纳气量定等，十绝体直取特等；
//  不把甲乙丙丁资质直接映射为福地大小。
//
//  面积直接决定洞天地图尺寸 —— 福地越大，进去后的地图越大。
// ============================================================================
enum class CaveGrade : std::uint8_t {
    Lower    = 0,   // 下等
    Middle   = 1,   // 中等
    Upper    = 2,   // 上等
    Special  = 3,   // 特等（十绝体）
};

inline const char* to_string(CaveGrade g) {
    switch (g) {
        case CaveGrade::Lower:   return "下等福地";
        case CaveGrade::Middle:  return "中等福地";
        case CaveGrade::Upper:   return "上等福地";
        case CaveGrade::Special: return "特等福地";
    }
    return "？";
}

struct CaveGradeInfo {
    CaveGrade   grade      = CaveGrade::Lower;
    double      minMu      = 0.0;      // 面积下限（万亩）
    double      maxMu      = 300.0;    // 面积上限（万亩）
    int         essencePerYear = 12;   // 年产仙元
    double      timeRatio  = 1.0;      // 光阴流速（相对外界倍数）
    int         mapW       = 48;       // 洞天地图宽（格）
    int         mapH       = 36;       // 洞天地图高（格）
    std::string source;
};

//  依等级取规格
CaveGradeInfo cave_grade_info(CaveGrade g);

//  依面积（万亩）反推等级
CaveGrade cave_grade_by_area(double mu);

struct CaveParadise {
    std::string siteId;
    std::string name;
    CaveKind       kind       = CaveKind::RenownedParadise;
    //  福地等级与面积：等级定等，面积（万亩）由升仙纳气量落在等级区间内取值
    CaveGrade      grade      = CaveGrade::Lower;
    double         areaMu     = 300.0;      // 面积（万亩）
    bool           tenJueBody = false;      // 十绝体升仙 → 直取特等
    RealmLayer     layer      = RealmLayer::WhiteHeaven;
    Domain         domain     = Domain::None;
    std::string    region;                 // 地表/地下/黑天/白天中的落点描述
    CaveOwnership  owner      = CaveOwnership::Unclaimed;
    std::string    ownerTag;               // 具体归属者，如「方源」「天庭」

    bool  fixedInstance = true;            // true=原著固定唯一实例；false=可随机生成
    bool  hasEarthSpirit = false;          // 地灵
    int   tribulationTimer = 0;            // 万劫/灾劫倒计时（<=0 未计时）
    // 将至的灾劫类型。道痕收益依类型而定（地灾250/天劫750/浩劫7250/万劫86750）
    canon::TribulationKind pendingTribulation = canon::TribulationKind::None;
    double qiYieldHeaven = 0.0;            // 天气产出（黑天/天道秘境型）
    double qiYieldEarth  = 0.0;            // 地气产出（五域地脉/福地型）
    double qiYieldHuman  = 0.0;            // 人气产出（白天/人道势力型）

    // 资料溯源：区分「原著/研究报告定点」与「按需求文档构造的填充实例」，
    // 避免把工程占位当作原著事实引用。
    std::string source;

    // ---- 地点分类（地理研究文档第五章：A 级区分）----
    //  最易犯的错误是把「仙窍内部坐标」当作「五域地表省份」。
    //  例：荡魂山、落魄谷在琅琊福地内；逆流河在至尊仙窍小东海；
    //      光阴长河属宙道至高空间，不等同五域任何河流。
    SiteClass siteClass = SiteClass::MortalSurface;

    // 嵌套层数（仅九层结构类地点使用，如疯魔窟 9 层；其余为 0）
    int floors = 0;

    // ---- 福地／洞天的可量化口径（数值口径库「福地洞天与仙窍」表）----
    // 原著未给出者一律 canon::kUnknown，不可填推测值。
    double essenceYieldPerYear = canon::kUnknown;  // 仙元产量（颗/年，仙窍时间）
    double outerEssencePerDay  = canon::kUnknown;  // 换算到外界（颗/天）
    double timeRatio           = canon::kUnknown;  // 仙窍∶外界 时间流速比
    int    layers              = 0;                // 分层数（至尊仙窍 10 层）

    // 至尊仙窍吞并（研究报告 7.2）：吞并后不再是可被争夺的对象
    bool isSwallowedByZhiZunXianQiao = false;
    bool canBeContested = true;

    Location location() const {
        Location l;
        l.layer  = layer;
        l.domain = domain;
        l.region = region;
        l.siteId = siteId;
        l.isParadiseInterior = true;
        return l;
    }
};

// ---------------------------------------------------------------------------
//  洞天实例构建
// ---------------------------------------------------------------------------
// 第六卷开局（疯魔窟大战结束后）原著洞天实例表：固定唯一，绝不随机生成
std::vector<CaveParadise> build_canon_cave_table();

// 随机散落生成：仅太古七天碎片、幽天魔渊洞天允许随机；散落于两天五域全域
std::vector<CaveParadise> generate_random_caves(Rng& rng, int count,
                                                const std::string& prefix = "rnd");

// ---------------------------------------------------------------- 分布合法性
// 硬性铁律：白天存在大量异人洞天；黑天以天道秘境/洞天为主，仅少量异族洞天
struct DistributionRule {
    // 判断某类洞天是否允许落在某层级
    static bool kindAllowedInLayer(CaveKind k, RealmLayer l);

    // 是否允许随机生成（太古七天碎片、幽天魔渊洞天可随机；原著有名实例不可）
    static bool isRandomizable(CaveKind k) {
        return k == CaveKind::TaiguSevenFragment || k == CaveKind::YouTianMoyuan;
    }
};

} // namespace gr
