// 蛊界数值口径实现 —— 逐项对应《蛊真人数值口径资料库》
#include "gr/core/CanonNumbers.hpp"

namespace gr {
namespace canon {

// ---------------------------------------------------------------------------
//  真元 / 仙元名称与颜色
// ---------------------------------------------------------------------------
EssenceSpec essence_spec(Rank r) {
    switch (r) {
        case Rank::R1:
            return {"青铜真元", "初阶翠绿，逐阶加深（苍绿→深绿→墨绿）",
                    "酷我第057集；细分色为百科整理，原作正文明确称「青铜真元」", true};
        case Rank::R2:
            return {"赤铁真元", "浅红→暗红",
                    "酷我第102集：方源晋升后出现赤铁真元海", true};
        case Rank::R3:
            return {"白银真元", "白银雾气凝成白银真元海",
                    "酷我第102集", true};
        case Rank::R4:
            return {"黄金真元", "—",
                    "世界观拆解2（流传设定，需二次核验）", false};
        case Rank::R5:
            return {"紫晶真元", "—",
                    "世界观拆解2（流传设定，需二次核验）", false};
        case Rank::R6:
            return {"青提仙元", "—",
                    "百度百科「蛊仙」：仙窍本源产出，可与真元本质区分", true};
        case Rank::R7:
            return {"红枣仙元", "—", "百度百科「蛊仙」", true};
        case Rank::R8:
            return {"白荔仙元", "—",
                    "快懂百科「九转蛊尊」：成尊前置要求本源已产出白荔仙元", true};
        case Rank::R9:
            return {"黄杏仙元", "橙黄色、杏子大小",
                    "快懂百科「九转蛊尊」；第249节设定：突破天道封锁后本源质变所得", true};
    }
    return {"？", "—", "—", false};
}

// ---------------------------------------------------------------------------
//  流派境界：吞窍要求与道境映射
// ---------------------------------------------------------------------------
FlowLevel swallow_requirement(Rank r) {
    switch (r) {
        case Rank::R6: return FlowLevel::Master;      // 六转大师
        case Rank::R7: return FlowLevel::Grandmaster; // 七转宗师
        case Rank::R8: return FlowLevel::Great;       // 八转大宗师
        case Rank::R9: return FlowLevel::Supreme;     // 九转无上大宗师
        default:       return FlowLevel::Ordinary;
    }
}

// 道境六阶 → 流派境界（工程映射：道境圆满以上方可触及大宗师层级）
FlowLevel flow_level_of(DaoLevel d) {
    switch (d) {
        case DaoLevel::Entry:      return FlowLevel::QuasiMaster;
        case DaoLevel::Small:      return FlowLevel::Master;
        case DaoLevel::Great:      return FlowLevel::Grandmaster;
        case DaoLevel::Perfection: return FlowLevel::QuasiGreat;
        case DaoLevel::Foundation: return FlowLevel::Great;
        case DaoLevel::MarkFusion: return FlowLevel::Supreme;
    }
    return FlowLevel::Ordinary;
}

DaoLevel dao_level_of(FlowLevel f) {
    if (f >= FlowLevel::Supreme)          return DaoLevel::MarkFusion;
    if (f >= FlowLevel::Great)            return DaoLevel::Foundation;
    if (f >= FlowLevel::QuasiGreat)       return DaoLevel::Perfection;
    if (f >= FlowLevel::Grandmaster)      return DaoLevel::Great;
    if (f >= FlowLevel::Master)           return DaoLevel::Small;
    return DaoLevel::Entry;
}

// ---------------------------------------------------------------------------
//  道痕量级：世界观概数 vs 读者平均估算（两套口径，不混算）
// ---------------------------------------------------------------------------
DaoMarkScale dao_mark_scale(Rank r) {
    switch (r) {
        case Rank::R6:
            return {200.0, 1000.0, 0.0, 9000.0,
                    "世界观整理[1]：数百至不足1000；读者估算0—9000（非官方逐人数据）"};
        case Rank::R7:
            return {2000.0, 10000.0, 10000.0, 30000.0,
                    "世界观整理[1]：数千至不足1万；读者估算10000—30000"};
        case Rank::R8:
            return {30000.0, 40000.0, 100000.0, 300000.0,
                    "世界观整理[1]：约3—4万，5万以上稀少；"
                    "常规积累约10万—30万（与成尊30万阈值衔接，但不等同所有八转已达30万）"};
        case Rank::R9:
            return {kVenerableDaoMarks, kUnknown, kUnknown, kUnknown,
                    "成尊主修道痕门槛≥30万；历史尊者实际通常高于此门槛"};
        default:
            return {kUnknown, kUnknown, kUnknown, kUnknown, "凡俗阶段不适用道痕量级"};
    }
}

// ---------------------------------------------------------------------------
//  灾劫周期与道痕收益
// ---------------------------------------------------------------------------
static const TribulationCycle kCyclesR6[] = {
    {TribulationKind::EarthCalamity,  10},   // 六转：地灾每10年
    {TribulationKind::HeavenCalamity, 100},  //       天劫每100年（通常3次后晋七转）
};
static const TribulationCycle kCyclesR7[] = {
    {TribulationKind::EarthCalamity,  10},   // 七转：地灾每10年
    {TribulationKind::HeavenCalamity, 50},   //       天劫每50年（较六转缩短）
    {TribulationKind::GreatCalamity,  100},  //       浩劫每100年（通常3次后晋八转）
};
static const TribulationCycle kCyclesR8[] = {
    {TribulationKind::HeavenCalamity, 10},   // 八转：天劫每10年
    {TribulationKind::GreatCalamity,  50},   //       浩劫每50年
    {TribulationKind::MyriadCalamity, 100},  //       万劫每100年（通常3次后满足部分成尊条件）
};
static const TribulationCycle kCyclesR9[] = {
    {TribulationKind::GreatCalamity,  10},   // 九转：浩劫每10年（灾劫层级整体上移）
    {TribulationKind::MyriadCalamity, 50},   //       万劫每50年
    {TribulationKind::ChaosMajor,     100},  //       混沌大难每100年
};

TribulationProfile tribulation_profile(Rank r) {
    switch (r) {
        case Rank::R6:
            return {Rank::R6, kCyclesR6, 2, "《蛊真人》世界观整理[1]"};
        case Rank::R7:
            return {Rank::R7, kCyclesR7, 3, "《蛊真人》世界观整理[1]"};
        case Rank::R8:
            return {Rank::R8, kCyclesR8, 3, "《蛊真人》世界观整理[1]"};
        case Rank::R9:
            return {Rank::R9, kCyclesR9, 3, "《蛊真人》世界观整理[1]"};
        default:
            return {r, nullptr, 0, "凡俗阶段无仙窍灾劫"};
    }
}

double tribulation_mark_gain(TribulationKind k) {
    switch (k) {
        case TribulationKind::EarthCalamity:  return 250.0;    // 普通地灾
        case TribulationKind::HeavenCalamity: return 750.0;    // 普通天劫
        case TribulationKind::GreatCalamity:  return 7250.0;   // 普通浩劫
        case TribulationKind::MyriadCalamity: return 86750.0;  // 普通万劫
        default: return kUnknown;   // 混沌小难／大难：资料未给出
    }
}

// ---------------------------------------------------------------------------
//  寿元
// ---------------------------------------------------------------------------
double lifespan_gu_gain(Rank r) {
    switch (r) {
        case Rank::R2: return 10.0;    // 二转寿蛊：十年
        case Rank::R3: return 100.0;   // 三转寿蛊：百年
        case Rank::R4: return 1000.0;  // 四转寿蛊：千年
        default: return kUnknown;      // 一转、五转及以上完整序列未核验
    }
}

const char* sarira_gu_name(Rank r) {
    switch (r) {
        case Rank::R1: return "青铜舍利蛊";
        case Rank::R2: return "赤铁舍利蛊";
        case Rank::R3: return "白银舍利蛊";
        case Rank::R4: return "黄金舍利蛊";
        case Rank::R5: return "紫晶舍利蛊";
        default: return nullptr;   // 六转及以上无舍利蛊
    }
}

// ---------------------------------------------------------------------------
//  资质与元海
// ---------------------------------------------------------------------------
AptitudeSpec aptitude_spec(Aptitude a) {
    switch (a) {
        case Aptitude::Ding:
            return {Aptitude::Ding, 0.20, 0.30, kUnknown,
                    "通常最高约一转至二转",
                    "百度百科「古月方源」；原文未给出精确容量公式，视为设定口径"};
        case Aptitude::Bing:
            return {Aptitude::Bing, 0.40, 0.59, 0.4 / kFullYuanHai,
                    "通常可到二转，极少数到三转初阶",
                    "第159节整理：丙等精确范围40%—59%；方源44%；恢复0.4成/小时"};
        case Aptitude::Yi:
            return {Aptitude::Yi, 0.60, 0.70, kUnknown,
                    "多可至三转，部分至四转",
                    "百度百科「古月方源」"};
        case Aptitude::Jia:
            return {Aptitude::Jia, 0.80, 0.90, 0.8 / kFullYuanHai,
                    "可修至五转（非保证必达）",
                    "百度百科「古月方源」；同一炼化场景对照：甲等0.8成/小时"};
    }
    return {Aptitude::Ding, kUnknown, kUnknown, kUnknown, "—", "—"};
}

// ---------------------------------------------------------------------------
//  福地洞天
// ---------------------------------------------------------------------------
static const ParadiseScale kParadiseSmall{
    "小福地", kUnknown, 3000000.0, 10.0,
    "第三卷第228节整理[5]：至多300万亩，引动光阴小脉支流，资源贫瘠，产10余颗"};
static const ParadiseScale kParadiseMid{
    "中福地", 4000000.0, 6000000.0, 20.0,
    "第三卷第228节整理[5]：400万—600万亩，引动光阴中脉支流，物产较丰富，产20余颗"};
static const ParadiseScale kParadiseUpper{
    "上福地", 7000000.0, 9000000.0, 30.0,
    "第三卷第228节整理[5]：700万—900万亩，引动大脉支流；天地二气残留多，"
    "可自然将凡蛊炼成仙蛊，产超30颗"};

ParadiseScale paradise_scale(int grade) {
    switch (grade) {
        case 0: return kParadiseSmall;
        case 1: return kParadiseMid;
        default: return kParadiseUpper;
    }
}

} // namespace canon
} // namespace gr
