// ============================================================================
//  蛊界数值口径（依据《蛊真人数值口径资料库》）
//
//  本表只收「可量化口径」，与设定资料库（记录「有什么」）互补。
//  严格遵循资料库的「不编造原则」：原著未给出者标 kUnknown 而非填推测值。
//
//  每项均附 origin 溯源，标明出处与置信性质：
//    · 世界观整理 / 百科词条  → 可作设定口径
//    · 读者整理 / 平均估算     → 属估算模型，非官方逐人数据
//    · 待考                    → 公开资料未给出，代码不得依赖
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"

#include <string>

namespace gr {

// ---------------------------------------------------------------------------
//  凡人 / 蛊仙的能量分界（真元 ↔ 仙元）
//
//  原著分野：一转至五转为凡人蛊师，用的是「真元」；
//  六转及以上为蛊仙，真元质变为「仙元」。二者不是同一事物的多少之分，
//  而是性质不同 —— 故界面与文案必须按修为给出正确称谓。
//
//  突破六转时真元按 kEssenceConversionRate 折算为仙元（真元量远大于仙元量），
//  仙元上限按 kEssenceMaxGrowth 抬高。
// ---------------------------------------------------------------------------
// 凡人真元上限参考量级（一转 ~100，五转约 500）
constexpr double kPrimevalEssenceCap    = 100.0;
// 真元 → 仙元的折算率：真元远多于仙元，取 1/10 为量级示意
constexpr double kEssenceConversionRate = 0.10;
// 突破六转后仙元上限的增长倍数
constexpr double kEssenceMaxGrowth      = 10.0;

// ---------------------------------------------------------------------------
//  道痕的「显形极点」
//
//  凡人时期道痕极其稀少（近乎为零），且原著中道痕是蛊仙体系的显性指标。
//  本项目因而设一个极点：累计道痕未过线时界面不予显示（显示「—」），
//  越过该点才揭示数值 —— 既贴合设定，也避免一转角色顶着大数字出戏。
// ---------------------------------------------------------------------------
constexpr double kDaoMarksRevealThreshold = 1000.0;   // 道痕显形极点

// 是否已达「道痕显形」极点
inline bool daoMarksRevealed(double marks) {
    return marks >= kDaoMarksRevealThreshold;
}


namespace canon {

// 标记「原著/公开资料未给出」的哨兵值。
// 任何读到该值的逻辑都应走「不可判定」分支，而不是猜一个数。
constexpr double kUnknown = -1.0;

// ---------------------------------------------------------------------------
//  一、转数与境界（数值口径库「转数与境界」表）
// ---------------------------------------------------------------------------

// 仙凡分界：六转为蛊仙起点。五转及以下为凡人蛊师。
// 六转时空窍吸纳天地人三气化为仙窍。
constexpr int kImmortalRank = 6;

// 境界总层：九大境界，每境分初阶/中阶/高阶/巅峰 4 小境
constexpr int kMaxRank      = 9;
constexpr int kSubLevels    = 4;

inline bool isImmortal(Rank r) { return static_cast<int>(r) >= kImmortalRank; }
inline bool isMortal(Rank r)   { return !isImmortal(r); }

// 小境界：初阶→中阶→高阶→巅峰
enum class SubLevel : std::uint8_t { Early = 0, Middle = 1, Late = 2, Peak = 3 };

inline const char* to_string(SubLevel s) {
    switch (s) {
        case SubLevel::Early:  return "初阶";
        case SubLevel::Middle: return "中阶";
        case SubLevel::Late:   return "高阶";
        case SubLevel::Peak:   return "巅峰";
    }
    return "？";
}

// 凡俗真元 / 仙元名称（数值口径库「真元与仙元」表）
// 一至五转为真元，六转及以上为仙元，名色皆有出处。
struct EssenceSpec {
    const char* name;         // 名称
    const char* colorNote;    // 颜色 / 细分色说明（百科整理，非原文全表）
    const char* origin;       // 溯源
    bool        verified;     // 是否原作正文明确（否则为流传整理）
};

EssenceSpec essence_spec(Rank r);

// 仙元兑换：理论 1（高转）∶100（低转）；
// 低转仙元催动高转蛊虫有损耗，实际约 1∶120—130。
constexpr double kEssenceExchangeTheory = 100.0;
constexpr double kEssenceExchangeActualLow  = 120.0;
constexpr double kEssenceExchangeActualHigh = 130.0;

// 仙元石换算（交易语境）
//   1 颗仙元石 ＝ 1 颗六转青提仙元
//   100 块仙元石 ＝ 1 颗七转红枣仙元
constexpr double kImmortalStonePerQingTi = 1.0;
constexpr double kImmortalStonePerHongZao = 100.0;

// ---------------------------------------------------------------------------
//  二、成尊四条件（数值口径库：快懂百科「九转蛊尊」）
//      四项须同时满足，最后一项令仙窍本源质变为黄杏仙元。
// ---------------------------------------------------------------------------

struct VenerableRequirement {
    // 1) 仙窍本源已产出白荔仙元（八转层级）
    bool   needBaiLiSource = true;
    // 2) 主修流派道痕 ≥ 30 万（必须是主修流派，不是所有流派合计）
    double mainDaoMarksThreshold = 300000.0;
    // 3) 主修流派达到无上大宗师（流派境界，非道境六阶）
    FlowLevel requiredFlowLevel = FlowLevel::Supreme;
    // 4) 突破天道封锁（灾劫、寿命、宿命）
    bool   needBreakHeavenlyDaoSeal = true;
};

// 道主资格：无上大宗师 ＋ 九转 ＋ 当世同流派理解最深
// 同一流派同时通常只有一位道主。
struct DaoLordship {
    bool   requiresNine = true;                    // 九转才可触及
    FlowLevel requiredFlowLevel = FlowLevel::Supreme;
    bool   uniquePerDao = true;                    // 同一流派同时通常一位
};

// ---------------------------------------------------------------------------
//  三、流派境界序列（数值口径库「流派境界与道痕」表：共 10 级）
//
//  重要：这是「流派境界」，与需求说明书七章的「道境六阶」是两套体系，
//  不可合并：
//    · 道境（DaoLevel）：入门→小成→大成→圆满→道基→道痕贯通
//      → 绑定战力机制（威力增幅、消耗降低、反噬降低、杀招稳定）
//    · 流派境界（FlowLevel）：普通→大师→宗师→大宗师→无上大宗师→道主
//      → 绑定成尊条件、吞窍资格、道主身份
//  原作直接锚点为宗师、大宗师、无上大宗师、道主；「准X」阶见于常见整理。
// ---------------------------------------------------------------------------

// FlowLevel 定义于 core/Types.hpp（与 DaoLevel 并列的基础类型）

// 成尊流派境界条件：主修流派达到无上大宗师
constexpr FlowLevel kVenerableFlowLevel = FlowLevel::Supreme;

// 吞窍流派境界要求（须匹配被吞仙窍的对应流派，不是吞窍者主修流派）
//   六转大师、七转宗师、八转大宗师、九转无上大宗师
FlowLevel swallow_requirement(Rank r);

// 道境六阶 × 流派境界十阶的对应（工程映射，非原著明示）
// 用于让同一角色两套体系保持自洽：道境越高，流派境界不应更低。
FlowLevel flow_level_of(DaoLevel d);
DaoLevel  dao_level_of(FlowLevel f);

// ---------------------------------------------------------------------------
//  四、道痕量级（数值口径库「流派境界与道痕」表）
//
//  注意：世界观量级与读者平均估算为两套不同口径，不可混算。
//    · 世界观概数：六转 数百~1000；七转 数千~1万；八转 3~4万（5万以上稀少）
//    · 读者估算：六转 0~9000；七转 1~3万；八转 10~30万（与成尊阈值衔接）
//  代码默认采用「世界观概数」作典型值，估算区间仅用于校验不越界。
// ---------------------------------------------------------------------------
struct DaoMarkScale {
    double typicalLow;      // 世界观典型下限
    double typicalHigh;     // 世界观典型上限
    double estimateLow;     // 读者平均估算下限（可能为 kUnknown）
    double estimateHigh;    // 读者平均估算上限
    const char* origin;
};

DaoMarkScale dao_mark_scale(Rank r);

// 成尊主修道痕门槛：≥ 30 万（历史尊者实际通常高于此门槛）
constexpr double kVenerableDaoMarks = 300000.0;

// ---------------------------------------------------------------------------
//  五、灾劫体系（数值口径库「寿元与灾劫」表）
// ---------------------------------------------------------------------------

enum class TribulationKind : std::uint8_t {
    None = 0,
    EarthCalamity,   // 地灾
    HeavenCalamity,  // 天劫
    GreatCalamity,   // 浩劫
    MyriadCalamity,  // 万劫
    ChaosMinor,      // 混沌小难
    ChaosMajor       // 混沌大难
};

inline const char* to_string(TribulationKind k) {
    switch (k) {
        case TribulationKind::None:           return "无";
        case TribulationKind::EarthCalamity:  return "地灾";
        case TribulationKind::HeavenCalamity: return "天劫";
        case TribulationKind::GreatCalamity:  return "浩劫";
        case TribulationKind::MyriadCalamity: return "万劫";
        case TribulationKind::ChaosMinor:     return "混沌小难";
        case TribulationKind::ChaosMajor:     return "混沌大难";
    }
    return "？";
}

// 灾劫等级序列：地灾 < 天劫 < 浩劫 < 万劫 < 混沌小难 < 混沌大难
// 单个转数只面对其中部分层级。
inline int tribulation_order(TribulationKind k) { return static_cast<int>(k); }

// 灾劫周期（仙窍时间口径，单位：年）
struct TribulationCycle {
    TribulationKind kind;
    int    years;      // 间隔年数
};

// 某转数所面对的灾劫周期表
struct TribulationProfile {
    Rank rank;
    const TribulationCycle* cycles;
    int  count;
    const char* origin;
};

TribulationProfile tribulation_profile(Rank r);

// 灾劫道痕收益（平均口径，受仙窍品质、资源、福分影响，不代表每次恒定）
//   地灾 250／场；天劫 750／场；浩劫 7250／场；万劫 86750／场
//   注：有声书字幕作「八万多的道痕」，86750 与概数「八万多」并存。
double tribulation_mark_gain(TribulationKind k);

// 三次万劫理论道痕 ≈ 30 万（由 3 × 86750 推算）
// 原著有声书明确「三次万劫后道痕平均约三十万」
constexpr int   kMyriadTimesToVenerable = 3;
constexpr double kMyriadTotalMarks      = 300000.0;

// 六转 → 七转：27 次地灾 ＋ 3 次天劫（按十年一地灾、百年一天劫推算，非所有案例）
constexpr int kR6EarthCalamityCount = 27;
constexpr int kR6HeavenCalamityCount = 3;

// ---------------------------------------------------------------------------
//  六、寿元（数值口径库「寿元与灾劫」表）
//      大量转数寿元原著未给出，一律 kUnknown，代码不得依赖。
// ---------------------------------------------------------------------------

// 凡人寿命上限：约 100 岁（无灾无病的人类极限，不等于各转蛊师固定寿元）
constexpr double kMortalLifespan = 100.0;

// 九转蛊尊年龄范围：三千岁 ~ 两万五千岁
// 是词条列出的尊者年龄范围，不是每个九转的固定寿元
constexpr double kVenerableAgeLow  = 3000.0;
constexpr double kVenerableAgeHigh = 25000.0;

// 已知尊者寿元（词条明确数值）
constexpr double kYuanShiLifespan = 25000.0;   // 元始仙尊：寿两万五千岁而终
constexpr double kXingXiuLifespan = 19000.0;   // 星宿仙尊
constexpr double kWuJiLifespan    = 6000.0;    // 无极魔尊「六千岁余」，非精确整数

// 寿蛊增寿（按转数）：二转十年、三转百年、四转千年
// 一转与五转及以上完整序列未核验
double lifespan_gu_gain(Rank r);

// 舍利蛊 ≠ 寿蛊：舍利蛊提升小境界、不延寿；寿蛊直接添寿
// 序列：青铜、赤铁、白银、黄金、紫晶（一至五转；六转及以上无舍利蛊）
const char* sarira_gu_name(Rank r);

// 天机仙蛊反噬：催动失败者损失 10 ~ 70 年寿元
constexpr double kTianJiBacklashLow  = 10.0;
constexpr double kTianJiBacklashHigh = 70.0;

// ---------------------------------------------------------------------------
//  七、资质与元海（数值口径库「资质与元海」表）
//      甲乙丙丁是区间，每等内部仍有详细尺度 —— 不可取固定值。
// ---------------------------------------------------------------------------

enum class Aptitude : std::uint8_t { Ding = 0, Bing = 1, Yi = 2, Jia = 3 };

inline const char* to_string(Aptitude a) {
    switch (a) {
        case Aptitude::Ding: return "丁等";
        case Aptitude::Bing: return "丙等";
        case Aptitude::Yi:   return "乙等";
        case Aptitude::Jia:  return "甲等";
    }
    return "？";
}

struct AptitudeSpec {
    Aptitude grade;
    double   yuanHaiLow;    // 元海占空窍比例下限
    double   yuanHaiHigh;   // 上限
    double   regenPerHour;  // 自然恢复（成/小时）；kUnknown 表示未给出
    const char* typicalCeiling;  // 通常修行上限（百科归纳，非保证）
    const char* origin;
};

AptitudeSpec aptitude_spec(Aptitude a);

// 元海基础量纲：满元海按 10 成计
constexpr double kFullYuanHai = 10.0;

// 方源锚点：初期丙等 44%（丙等中偏下，因冲击二转至少需 55% 墨绿真元而受限）
// 中后期提升至甲等 90%；后得至尊仙胎体，不再适用普通甲乙丙丁容量模型
constexpr double kFangYuanInitialYuanHai = 4.4;
constexpr double kFangYuanLaterYuanHai   = 9.0;
constexpr double kRank2BreakthroughGate  = 5.5;  // 一转巅峰→二转门槛（方源所处条件）

// ---------------------------------------------------------------------------
//  八、福地洞天与仙窍（数值口径库「福地洞天与仙窍」表）
// ---------------------------------------------------------------------------

// 命名口径：六转、七转仙窍称福地；八转、九转称洞天。
// 这是仙窍形态而非固定面积单位；洞天有降格为福地的风险。
inline bool isParadise(Rank r) {
    return static_cast<int>(r) >= 6 && static_cast<int>(r) <= 7;
}
inline bool isCave(Rank r) {
    return static_cast<int>(r) >= 8;
}

// 福地面积（六转，单位：万亩）
struct ParadiseScale {
    const char* grade;
    double   areaLow;      // 万亩
    double   areaHigh;     // 万亩；kUnknown 表示未给出
    double   essenceYield; // 仙元产量（颗）
    const char* origin;
};

ParadiseScale paradise_scale(int grade);   // 0=小 1=中 2=上

// 至尊仙窍（方源，特例，不可当普通生产率）
struct ZhiZunXianQiaoSpec {
    int    layers = 10;                 // 10 层
    double areaPerLayerMu = 50000000.0; // 每层 5000 万亩以上
    double timeRatio = 60.0;            // 仙窍∶外界 = 60∶1
    double essencePerYear = 96.0;       // 按仙窍时间每年 96 颗
    double essencePerOuterDay = 16.0;   // 换算到外界约每天 16 颗
};

// 琅琊福地：原为洞天，故意跌落为福地；灾劫上限为浩劫，不受万劫
// —— 因此道痕收益较少。
struct LangYaSpec {
    TribulationKind maxTribulation = TribulationKind::GreatCalamity;
    bool            immuneToMyriad = true;
    const char* note = "原为洞天，故意跌落为福地，故不受万劫";
};

// 长毛炼道大阵：每 10 年破败倾覆（案例性表述，非所有仙蛊屋固定寿命）
constexpr int kChangMaoArrayLifespanYears = 10;

// 天元宝皇莲：花苞完全成熟后 81 颗莲子皆为对应转数的仙元
constexpr int kTianYuanBaoHuangLianSeeds = 81;

// ---------------------------------------------------------------------------
//  九、蛊虫数量与规模（数值口径库「蛊虫数量与规模」表）
// ---------------------------------------------------------------------------

// 合炼产物：月光蛊＋两只小光蛊 → 月芒蛊（3 只 → 1 只）
// 典型案例，不等于全书通用合炼定额。
constexpr int kYueMangInputCount  = 3;
constexpr int kYueMangOutputCount = 1;

// 夺舍体系：1 只六转夺舍仙蛊 ＋ 3000 多只五转凡蛊
// 特定法门的组成，不是通用合炼上限。
constexpr int kDuoSheImmortalGu = 1;
constexpr int kDuoSheMortalGu   = 3000;

} // namespace canon
} // namespace gr
