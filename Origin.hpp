// ============================================================================
//  出身（Origin）—— 需求 11
//
//  开局身份不再一律「南疆六转蛊仙」，而是按出生五域各设数种身份。
//  不同身份带来不同的起始修为、人脉、蛊虫、传承线索与血脉。
//
//  其中两种身份【当日开窍】，须演出开窍场景，且开窍后为一转初阶：
//    · 南疆·百家寨孤儿（姓名必须为「百」姓）
//    · 北原·黄金家族弟子（血脉加持，场景更北原化）
//
//  开窍一节的原著依据（第一卷·学堂家老讲述，可核验）：
//    · 开窍大典上开辟空窍，凝聚真元海，自此为一转蛊师
//    · 蛊师九大境界，每转又分初阶、中阶、高阶、巅峰
//    · 资质以元海占空窍的比例定：
//        丁等 两三成 → 通常止于一转二转
//        丙等 四五成 → 通常止于二转，少数侥幸三转初阶
//        乙等 六七成 → 可至三转，甚至四转
//        甲等 八九成 → 可至五转
//    · 刚开窍者皆为一转初阶
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"
#include "gr/core/CanonNumbers.hpp"

#include <string>
#include <vector>

namespace gr {

enum class OriginId : int {
    // —— 南疆 ——
    NanJiang_ShangMerchant  = 0,   // 寄居商家寨的商人
    NanJiang_BaiOrphan      = 1,   // 百家寨孤儿，当日开窍，姓百
    NanJiang_Rogue          = 2,   // 野生散修
    // —— 东海 ——
    DongHai_RogueNoSea      = 3,   // 无自己海域的散修
    DongHai_RogueWithSea    = 4,   // 有一小片海域的散修
    // —— 北原 ——
    BeiYuan_ChuDisciple     = 5,   // 楚门（楚家）弟子
    BeiYuan_GoldenBlood     = 6,   // 黄金家族弟子，当日开窍
    BeiYuan_Beastman        = 7,   // 兽人族人
    // —— 西漠 ——
    XiMo_ShangClan          = 8,   // 商家族人
    XiMo_TangClan           = 9,   // 唐家人
    XiMo_OtherClan          = 10,  // 其它家族人
    // —— 中洲 ——
    ZhongZhou_TenSects      = 11,  // 十大宗门弟子
    ZhongZhou_Rogue         = 12,  // 野生散修
};

//  开窍资质沿用 core/CanonNumbers.hpp 中已有的 Aptitude（丁/丙/乙/甲），
//  此处不再重复定义 —— 两套资质会令数值口径打架。
//  元海占空窍比例：
//    丁等 两三成 / 丙等 四五成 / 乙等 六七成 / 甲等 八九成（第一卷可核验）

// 开窍场景描述（供界面演出）
struct AwakeningScene {
    bool     enabled = false;
    std::string where;      // 何地开窍
    std::string rite;       // 仪式
    std::string detail;     // 过程
    std::string result;     // 结果
    std::string source;     // 溯源
};

struct OriginDef {
    OriginId    id       = OriginId::NanJiang_ShangMerchant;
    Domain      domain   = Domain::NanJiang;
    std::string name;               // 身份名
    std::string shortDesc;          // 一句话
    std::string desc;               // 详述
    Rank        startRank = Rank::R1;   // 起始修为

    // 姓名约束：为空 = 不限；否则为必须以此字起始的姓氏
    std::string requiredSurname;

    // 是否当日开窍（须演出开窍场景）
    bool        awakens = false;
    // 开窍身份的可选家族（为空则无）
    std::vector<std::string> optionalClans;  // 可选家族（如北原黄金家族择一）
    canon::Aptitude aptitude = canon::Aptitude::Bing;

    // —— 起始所得 ——
    std::vector<std::string> startIntel;    // 已知情报（传承线索等）
    std::vector<std::string> startGuNames;  // 起始蛊虫（按名匹配模板）
    std::vector<std::string> startSites;    // 起始拥有的地域/海域
    bool        hasBloodline = false;       // 巨阳血脉
    std::string bloodlineNote;
    int         socialTie = 0;              // 城内人脉（0 = 无）
    std::string startSiteId;                // 起始所在地点

    std::string source;     // 溯源：原著可核验 / 需求设定
    bool        canon = true;
};

// 全部身份
const std::vector<OriginDef>& allOrigins();
const OriginDef* origin(OriginId id);
std::vector<const OriginDef*> originsIn(Domain d);

// 开窍场景（依身份生成；awakens=false 时 enabled=false）
AwakeningScene awakeningSceneFor(OriginId id, const std::string& playerName);

inline const char* to_string(OriginId id) {
    switch (id) {
        case OriginId::NanJiang_ShangMerchant: return "寄居商家寨的商人";
        case OriginId::NanJiang_BaiOrphan:     return "百家寨孤儿（当日开窍）";
        case OriginId::NanJiang_Rogue:         return "南疆野生散修";
        case OriginId::DongHai_RogueNoSea:     return "无海域的东海散修";
        case OriginId::DongHai_RogueWithSea:   return "有小片海域的东海散修";
        case OriginId::BeiYuan_ChuDisciple:    return "楚门弟子";
        case OriginId::BeiYuan_GoldenBlood:    return "黄金家族弟子（当日开窍）";
        case OriginId::BeiYuan_Beastman:       return "兽人族人";
        case OriginId::XiMo_ShangClan:         return "商家族人";
        case OriginId::XiMo_TangClan:          return "唐家人";
        case OriginId::XiMo_OtherClan:         return "西漠其它家族人";
        case OriginId::ZhongZhou_TenSects:     return "十大宗门弟子";
        case OriginId::ZhongZhou_Rogue:        return "中洲野生散修";
    }
    return "？";
}

} // namespace gr
