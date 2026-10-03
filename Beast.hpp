// ============================================================================
//  异兽与荒兽
//
//  原著分级（自下而上）：
//
//    野兽 → 百兽王 → 千兽王 → 万兽王 → 兽皇
//          → 荒兽 → 上古荒兽 → 太古荒兽 → 太古传奇荒兽
//
//  【分级存在分歧，如实记录】
//
//  各来源对「荒兽」的起点说法差一位：
//
//    · 百度百科《世界观(3)》：野兽到兽皇对应一转到五转的蛊师，
//      荒兽到太古荒兽对应六转到八转的蛊仙，太古传奇为八转巅峰以上。
//    · B 站整理稿：普通、二转、三转、四转、五转、六转、七转、八转、
//      八转巅峰及以上（即荒兽 = 六转）。
//    · 知乎整理稿：一转百兽王、二转千兽王、三转万兽王(异兽)、
//      四转兽皇(异兽王)、五转荒兽、六转上古荒兽、七转太古荒兽。
//
//  本项目采用**百度百科 / B 站**一说（两处独立来源一致）：
//  **荒兽属蛊仙级（六转）**，凡人不可敌。
//
//  理由：若荒兽仅为五转，则凡人巅峰可与之周旋，这与
//  「几个六转蛊仙面对上古荒兽没有仙蛊就是一个死字」的体感相悖。
//
//  「万兽王」及以下统称**异兽**（三转起有奇能），
//  「荒兽」及以上为**荒兽**（蛊仙级）。
//
//  另：道兽 —— 纯粹由本流派道痕凝聚成的生灵，无器官要害
//  （如魂兽、影兽、泥怪）。此类单列。
//
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"
#include "gr/world/TileMap.hpp"

#include <string>
#include <vector>

namespace gr {

// ---------------------------------------------------------------- 兽类品阶
enum class BeastRank : std::uint8_t {
    Wild       = 0,   // 野兽：无对应转数
    HundredKing,      // 百兽王：一转
    ThousandKing,     // 千兽王：二转
    MyriadKing,       // 万兽王：三转  ← 异兽起点
    BeastKing,        // 兽皇（异兽王）：四转
    Huang,            // 荒兽：六转（蛊仙级）
    AncientHuang,     // 上古荒兽：七转
    PrimordialHuang,  // 太古荒兽：八转
    LegendaryHuang,   // 太古传奇荒兽：八转巅峰及以上（亚仙尊）
};

inline const char* to_string(BeastRank r) {
    switch (r) {
        case BeastRank::Wild:            return "野兽";
        case BeastRank::HundredKing:     return "百兽王";
        case BeastRank::ThousandKing:    return "千兽王";
        case BeastRank::MyriadKing:      return "万兽王";
        case BeastRank::BeastKing:       return "兽皇";
        case BeastRank::Huang:           return "荒兽";
        case BeastRank::AncientHuang:    return "上古荒兽";
        case BeastRank::PrimordialHuang: return "太古荒兽";
        case BeastRank::LegendaryHuang:  return "太古传奇";
    }
    return "？";
}

//  对应修为（0 = 无对应转数）
inline int rankToRankNumber(BeastRank r) {
    switch (r) {
        case BeastRank::Wild:            return 0;
        case BeastRank::HundredKing:     return 1;
        case BeastRank::ThousandKing:    return 2;
        case BeastRank::MyriadKing:      return 3;
        case BeastRank::BeastKing:       return 4;
        case BeastRank::Huang:           return 6;
        case BeastRank::AncientHuang:    return 7;
        case BeastRank::PrimordialHuang: return 8;
        case BeastRank::LegendaryHuang:  return 9;   // 亚仙尊，示为九转级
    }
    return 0;
}

//  是否属「异兽」（凡人可敌的范围）
inline bool isYiShou(BeastRank r) {
    return r >= BeastRank::MyriadKing && r <= BeastRank::BeastKing;
}

//  是否属「荒兽」（蛊仙级，凡人不可敌）
inline bool isHuangShou(BeastRank r) {
    return r >= BeastRank::Huang;
}

// ---------------------------------------------------------------- 兽种
struct BeastSpecies {
    std::string id;
    std::string name;
    BeastRank   rank = BeastRank::Wild;
    Domain      domain = Domain::None;          // None = 五域皆可
    std::string desc;
    std::string source;                          // 溯源
    bool        canon = false;                   // 是否原著可核验
    bool        isDaoBeast = false;              // 是否道兽（道痕凝聚，无要害）

    //  产出：狩猎可得之物
    std::vector<std::string> drops;

    //  出没地形
    std::vector<Terrain> habitats;
};

//  全表
const std::vector<BeastSpecies>& all_beast_species();

//  按域与地形查可能出现的兽种
std::vector<const BeastSpecies*> beasts_at(Domain d, Terrain t);

//  按名查
const BeastSpecies* beast_species_by_id(const std::string& id);

// ---------------------------------------------------------------- 遭遇
//
//  荒兽级以上极稀，且多在特定定点；寻常地形上遇到的多是野兽与兽王。
//
struct BeastEncounter {
    std::string speciesId;
    std::string name;
    BeastRank   rank = BeastRank::Wild;
    int         count = 1;
    bool        canon = false;
    std::string source;
    //  玩家可否一战：荒兽级一律不可
    bool        fightable = true;
    //  不可敌时的提示（逃为上）
    std::string warning;
};

//  在某格生成遭遇（确定性：同格同种，不因重绘而变）
BeastEncounter make_encounter(Domain d, Terrain t, int x, int y, std::uint64_t seed);

//  狩猎结算
struct HuntOutcome {
    bool   ok = false;
    bool   fled = false;        // 未战先逃
    std::string beastName;
    BeastRank   rank = BeastRank::Wild;
    int    essenceCost = 0;     // 消耗真元
    int    hpLoss = 0;          // 气血损耗
    std::vector<std::string> gained;   // 所得兽材
    std::string line;
};

//  以玩家修为试算胜负（不掷骰，按品阶对比，避免随机性失控）
HuntOutcome resolve_hunt(const BeastSpecies& sp, int playerRank, int playerAttack);

} // namespace gr
