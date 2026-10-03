// ============================================================================
//  NPC 互动
//
//  原著核心约束 —— **仙凡鸿沟**。
//
//  蛊师世界中，凡人与蛊仙之间是天堑：
//    · 六转蛊仙视凡人如蝼蚁，寻常不会理会搭话
//    · 九转尊者行踪莫测，其意念动念可决一域兴衰，凡人根本无从得见
//  因此本模块的第一条规则不是「能否互动」，而是「能否见到」——
//  见不到，则一切互动无从谈起。
//
//  另有两条约束：
//    · 势力敌对则闭门（天庭与方源势不两立）
//    · 情谊（affinity）可累积，但不会凭空而来
//
//  对话文本一律为工程撰写，符合世界观而【不冒充原著台词】。
//  原著具体对白须逐句可考，此处不伪造，故只给情境化通用回应。
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"
#include "gr/ai/NpcAgent.hpp"
#include "gr/ai/Faction.hpp"
#include "gr/core/Origin.hpp"

#include <string>
#include <vector>

namespace gr {

// ---------------------------------------------------------------- 态度
//
//  由「修为差 + 势力关系 + 情谊」三者共同决定，非单一数值。
//
enum class NpcAttitude : std::uint8_t {
    Invisible = 0,  // 无从得见（尊者对凡人）
    Scorn,          // 不屑 —— 视如蝼蚁，不予理会
    Hostile,        // 敌视 —— 势力对立
    Indifferent,    // 淡漠 —— 同阶陌路
    Neutral,        // 寻常 —— 可作言语
    Friendly,       // 友善 —— 愿作交易
    Cordial         // 亲近 —— 可传技艺
};

inline const char* to_string(NpcAttitude a) {
    switch (a) {
        case NpcAttitude::Invisible:   return "无从得见";
        case NpcAttitude::Scorn:       return "不屑一顾";
        case NpcAttitude::Hostile:     return "敌视";
        case NpcAttitude::Indifferent: return "淡漠";
        case NpcAttitude::Neutral:     return "寻常";
        case NpcAttitude::Friendly:    return "友善";
        case NpcAttitude::Cordial:     return "亲近";
    }
    return "？";
}

// ---------------------------------------------------------------- 互动种类
enum class NpcAction : std::uint8_t {
    Talk = 0,    // 交谈 —— 交换见闻，可得情报
    Trade,       // 交易 —— 买卖蛊虫与材料
    Learn,       // 请教 —— 学炼蛊手法
    Inquire,     // 打探 —— 求问传闻（如定仙游下落）
    Borrow,      // 求借 —— 借仙蛊（定仙游支线）
    Duel         // 挑战 —— 出手相斗
};

inline const char* to_string(NpcAction a) {
    switch (a) {
        case NpcAction::Talk:    return "交谈";
        case NpcAction::Trade:   return "交易";
        case NpcAction::Learn:   return "请教";
        case NpcAction::Inquire: return "打探";
        case NpcAction::Borrow:  return "求借";
        case NpcAction::Duel:    return "挑战";
    }
    return "？";
}

// ---------------------------------------------------------------- NPC 社交画像
//
//  与 NpcAgent 分离：Agent 是「他在做什么」，本结构是「他怎么待人」。
//
struct NpcSocial {
    std::string npcId;

    //  可提供的互动（空 = 什么都不给）
    std::vector<NpcAction> offers;

    //  何种修为才配与之言语。低于此者：
    //    · 凡人对蛊仙 → 不屑
    //    · 凡人对尊者 → 根本见不到
    Rank minRankToApproach = Rank::R1;

    //  尊者（九转）：行踪莫测，非同阶不得见
    bool inscrutable = false;

    //  可传授的手法 id（请教用）
    std::vector<int> teachTechniques;

    //  交易倾向：蛊虫、材料
    bool tradesGu = false;
    bool tradesItems = false;

    //  出身因缘：特定出身与之有旧，态度天然亲近
    //  例：南疆商家商人 —— 需求 11 明定「与商心慈有过一面之缘」。
    OriginId affinityOrigin = OriginId::NanJiang_ShangMerchant;
    bool     hasOriginTie   = false;
    std::string originTieNote;

    //  情境化回应（工程撰写，不冒充原著台词）
    std::string greet;      // 初见
    std::string brush;      // 被拒（不屑）
    std::string hostile;    // 被拒（敌对）
    std::string invisible;  // 无从得见
    std::string source;     // 溯源
    bool        canon = false;
};

const NpcSocial* findSocial(const std::string& npcId);
//  按 NPC 取画像：手工表内者取表，凡俗众生按职业推得
NpcSocial socialFor(const NpcAgent& a);
const std::vector<NpcSocial>& allSocials();

// ---------------------------------------------------------------- 关系状态
struct NpcRelation {
    int  affinity = 0;      // 情谊 -100..100
    bool met      = false;  // 是否见过面
    int  talkCount = 0;
};

// ---------------------------------------------------------------- 判定
struct NpcApproach {
    bool        visible   = false;   // 能否得见
    NpcAttitude attitude  = NpcAttitude::Invisible;
    bool        canTalk   = false;
    std::string line;                // 当面所说的话
    std::string reason;              // 拒绝缘由（给玩家看）
};

//
//  判定玩家能否接近某个 NPC、以及对方的态度。
//
//  规则顺序（越靠前越优先）：
//    1. 尊者行踪莫测 → 非同阶者无从得见
//    2. 势力敌对 → 敌视
//    3. 修为差过大 → 不屑（仙凡鸿沟）
//    4. 情谊 / 出身因缘 → 可观、友善、亲近
//
//
//  playerFaction：玩家所属势力。Cultivator 本身不带势力字段
//  （势力是「站位」，不是修士的固有属性），故由调用方传入。
//  未加入任何势力时传 Neutral —— 此时不论与谁都不构成敌对。
//
NpcApproach evaluateApproach(const NpcAgent& npc,
                             const Cultivator& player,
                             const NpcRelation& rel,
                             bool playerHasOrigin,
                             OriginId playerOrigin,
                             FactionId playerFaction = FactionId::Neutral);

//  某 NPC 在当前态度下允许哪些互动
std::vector<NpcAction> availableActions(const NpcSocial& soc,
                                        NpcAttitude att);

//  态度是否足以支持某项互动
bool attitudePermits(NpcAttitude att, NpcAction act);

//  一次交谈能带来的情报 id（交谈成功时由调用方写入 IntelSystem）
std::vector<std::string> intelFromTalk(const std::string& npcId);

} // namespace gr
