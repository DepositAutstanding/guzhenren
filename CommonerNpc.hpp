// ============================================================================
//  凡俗众生
//
//  五域之内不只有尊者与蛊仙。
//
//  山寨里有猎户与药农，草原有牧民与骑手，沙漠有商队向导与矿工，
//  海上有渔夫与采珠人，宗门有外门弟子与执事 ——
//  这些才是玩家日日打交道的世间。
//
//  本模块按【聚落】生成居民：一处聚落有若干常住者，
//  职业与姓氏随域而异。玩家须走到该地，方得相见。
//
//  原著依据：
//    · 南疆山寨：古月、白、熊三寨，各有族老、家老、蛊师、猎户（第一卷）
//    · 北原黄金家族：黑、东方、刘、关、慕容、耶律、廿二诸家（地点总表）
//    · 西漠十四家族：房、万、左丘、萧、田、董、唐、秦、孙、莫、
//      石、龚、林、拓跋（地点总表）
//    · 东海八大家族：青岳、宋、华、夏、蔡、苏、南宫、解（地点总表）
//    · 中洲十大古派外门弟子众多（地点总表）
//    · 石人矿工「上十万」见于万矿戈壁（地点总表）
//
//  人物本身为工程生成，非原著具名角色；姓氏与职业分布依上述为据。
// ============================================================================
#pragma once

#include "gr/ai/NpcAgent.hpp"
#include "gr/ai/NpcInteraction.hpp"
#include "gr/world/Settlement.hpp"

#include <string>
#include <vector>

namespace gr {

// ---------------------------------------------------------------- 职业
//
//  职业决定可提供的互动：
//    蛊师 / 教习 → 可请教（手法）
//    商贾 / 贩子 → 可交易
//    猎户 / 渔夫 / 矿工 → 可交易（产出之物）
//    其余 → 只可交谈
//
enum class Trade : std::uint8_t {
    Hunter  = 0,   // 猎户 / 渔夫：贩卖所获
    Herder,        // 牧民
    Merchant,      // 商贾 / 贩子
    Miner,         // 矿工 / 采珠人
    GuMaster,      // 蛊师：可请教手法
    Teacher,       // 教习 / 执事：可请教手法
    Elder,         // 族老 / 长老：知晓旧闻，可打探
    Guard,         // 护卫 / 战士
    Craftsman,     // 工匠
    Farmer,        // 农户
    Rogue          // 散修 / 沙行者
};

inline const char* to_string(Trade t) {
    switch (t) {
        case Trade::Hunter:    return "猎户";
        case Trade::Herder:    return "牧民";
        case Trade::Merchant:  return "商贾";
        case Trade::Miner:     return "矿工";
        case Trade::GuMaster:  return "蛊师";
        case Trade::Teacher:   return "教习";
        case Trade::Elder:     return "族老";
        case Trade::Guard:     return "护卫";
        case Trade::Craftsman: return "工匠";
        case Trade::Farmer:    return "农户";
        case Trade::Rogue:     return "散修";
    }
    return "？";
}

//  按聚落生成居民
//  settlements 为空时返回空表（不臆造无处可居之人）
std::vector<NpcAgent> build_commoner_npcs(const SettlementRegistry& reg);

//  某职业可提供的互动
bool tradeOffersGuAdvice(Trade t);    // 可请教手法
bool tradeOffersTrade(Trade t);       // 可做买卖

//  普通人的社交画像（与 NpcSocial 表分开：凡俗之人不必逐一手工设定）
NpcSocial socialForCommoner(const NpcAgent& a);

} // namespace gr
