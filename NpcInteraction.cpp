#include "gr/ai/NpcInteraction.hpp"
#include "gr/core/Origin.hpp"
#include "gr/ai/CommonerNpc.hpp"

#include <algorithm>

namespace gr {

// ===========================================================================
//  NPC 社交画像表
//
//  八位原著人物，逐一设定「他怎么待人」。
//  溯源以项目内既有资料（研究报告、地点总表）为据；
//  对话文本为工程撰写的情境化通用回应，不冒充原著台词。
// ===========================================================================
static const std::vector<NpcSocial> kSocials = {
    //
    //  方源 —— 九转炼道尊者，至尊仙窍，幕后布局。
    //  唯一定仙游所有者（仙蛊唯一）。
    //  尊者行踪莫测，凡人无从得见；六转以上方可试探接近。
    //
    {
        "fangyuan",
        {NpcAction::Talk, NpcAction::Inquire, NpcAction::Borrow, NpcAction::Trade},
        Rank::R6,   // 至少蛊仙，否则尊者根本不现身
        true,       // 尊者，行踪莫测
        {},         // 不传手法（方源不授艺于外人）
        true, false,
        OriginId::NanJiang_ShangMerchant, false, {},
        "「你想要什么？」他甚至没有抬头。",
        "他连看你一眼都嫌费事。",
        "「挡路了。」",
        "尊者行踪莫测，岂是你能寻见的。",
        "研究报告 2.2／7.1：炼道尊者，名号「炼天魔尊」",
        true
    },
    //
    //  气海老祖 —— 方源气道分身，八转。
    //  专司炼化气功果，驻豪豨洞天。
    //
    {
        "qihailaozu",
        {NpcAction::Talk, NpcAction::Inquire},
        Rank::R6,
        false,
        {},
        false, false,
        OriginId::NanJiang_ShangMerchant, false, {},
        "老者周身气息如海，只淡淡扫你一眼。",
        "他正忙着炼化气功果，无暇理会。",
        "「滚。」",
        "洞天深处，非请莫入。",
        "研究报告 3.6：处理豪豨洞天气功果，诞生时八十多万气道道痕",
        true
    },
    //
    //  吴帅 —— 方源南疆身份，七转奴道。
    //  驻南疆异族联盟处。
    //
    {
        "wushuai",
        {NpcAction::Talk, NpcAction::Inquire, NpcAction::Trade},
        Rank::R5,
        false,
        {},
        true, false,
        OriginId::NanJiang_ShangMerchant, false, {},
        "「南疆之地，近来不太平。」",
        "他摆摆手，示意你退下。",
        "「你是谁的人？」",
        "此人踪迹不定。",
        "研究报告 Table4：南疆人道、异族联盟、吴帅活动区",
        true
    },
    //
    //  星宿仙尊 —— 九转，借元境与天庭底蕴复活重登尊位。
    //
    {
        "xingxiu",
        {NpcAction::Talk, NpcAction::Inquire},
        Rank::R6,
        true,
        {},
        false, false,
        OriginId::NanJiang_ShangMerchant, false, {},
        "星光照彻，她似在推演什么，并未看你。",
        "「凡俗之事，不必来禀。」",
        "「天庭之地，岂容你放肆。」",
        "天庭洞天高悬九霄，非常人能至。",
        "地点总表：天庭洞天，中洲正中高空，历代洞天融合归并",
        true
    },
    //
    //  天庭虚窍蛊仙 —— 七转天道。
    //  可交易、可请教（天庭藏有诸多传承）。
    //
    {
        "tianting_zhenren",
        {NpcAction::Talk, NpcAction::Trade, NpcAction::Learn, NpcAction::Inquire},
        Rank::R4,
        false,
        {1, 2},     // 可传两种炼蛊手法
        true, true,
        OriginId::NanJiang_ShangMerchant, false, {},
        "「可是要求购些什么？」",
        "他瞥你一眼，转身去了。",
        "「正道子弟，何故与魔道往来？」",
        "此人不在左近。",
        "地点总表：中洲十大古派，第六卷前已空置",
        true
    },
    //
    //  巨阳仙尊 —— 九转运道／气道巨头，长生天之主。
    //
    {
        "juyang",
        {NpcAction::Talk, NpcAction::Inquire},
        Rank::R6,
        true,
        {},
        false, false,
        OriginId::NanJiang_ShangMerchant, false, {},
        "「运道流转，自有定数。」",
        "他掐指一算，便不再看你。",
        "「长生天之下，皆为子民。你也不例外。」",
        "长生天在北原之上，凡人如何得见。",
        "地点总表：长生天，北原上空白天天域，巨阳仙尊九转洞天",
        true
    },
    //
    //  幽魂魔尊 —— 九转，魂道。曾毁灭太阳、破坏天堑天柱。
    //
    {
        "youhun",
        {NpcAction::Talk, NpcAction::Inquire},
        Rank::R6,
        true,
        {},
        false, false,
        OriginId::NanJiang_ShangMerchant, false, {},
        "阴影中传来低语，分不清是第几道魂魄在说话。",
        "「你的魂魄——太弱了。」",
        "「正好拿你炼魂。」",
        "魔尊残党藏于暗处，寻之无益。",
        "地点总表：太古白天，天堑／天柱被幽魂魔尊破坏；太阳亦被其毁灭",
        true
    },
    //
    //  商心慈 —— 南疆商家，六转。
    //
    //  需求 11 明定：「寄居于商家寨的商人，与商家家主商心慈有过一面之缘，
    //  城内有点人脉」。故南疆商人出身与之有天然因缘。
    //
    //  她是本项目中最早、也最可能真正与之交谈的蛊仙 ——
    //  若非如此，凡人开局将无人可说话，互动系统形同虚设。
    //
    //
    //  彭达 —— 新天外之魔，一转凡人（工程自设样本，非原著人物）。
    //  同为凡人，可与之言语 —— 这是凡人开局唯一「平辈」的交流对象。
    //
    {
        "pengda",
        {NpcAction::Talk, NpcAction::Inquire, NpcAction::Trade},
        Rank::R1,
        false,
        {},
        false, true,
        OriginId::NanJiang_ShangMerchant, false, {},
        "「兄弟也是从那边来的？」他压低了声音。",
        "他瞥你一眼，没搭腔。",
        "「你想干什么？」",
        "此人不在左近。",
        "工程自设：新天外之魔样本（非原著人物）",
        false
    },
    {
        "shangxinci",
        {NpcAction::Talk, NpcAction::Trade, NpcAction::Learn, NpcAction::Inquire},
        //
        //  门槛定为 R1 —— 凡人亦可搭话。
        //
        //  这是本表中【唯一】对凡人开放的蛊仙，且是有据的例外：
        //  需求 11 明定南疆商人出身「与商家家主商心慈有过一面之缘，
        //  城内有点人脉」。若无此例，凡人开局将无人可说话，
        //  互动系统对凡人玩家形同虚设。
        //
        //  但仙凡鸿沟并未因此破除：她仍是六转蛊仙，
        //  寻常凡人搭话只得到「淡漠」，交易与请教仍需情谊累积。
        //
        Rank::R1,
        false,
        {1, 2, 3},             // 可传三种炼蛊手法：商家富甲一方，藏有传承
        true, true,
        OriginId::NanJiang_ShangMerchant, true,
        "曾在商家城中有一面之缘",
        "「公子也是来做买卖的么？」",
        "她微微颔首，便去招呼别的客人了。",
        "「商家不欢迎与魔道往来之人。」",
        "她不在城中。",
        "需求 11：南疆商家商人与商心慈有一面之缘；地点总表：商家城为南疆第一贸易中心",
        false
    },
};

const std::vector<NpcSocial>& allSocials() { return kSocials; }

const NpcSocial* findSocial(const std::string& npcId) {
    for (const auto& s : kSocials)
        if (s.npcId == npcId) return &s;
    return nullptr;
}

//
//  凡俗众生不在手工表内，按职业即时推画像 ——
//  不可能为上百位居民逐一手工设定，也没有必要。
//
NpcSocial socialFor(const NpcAgent& a) {
    if (!a.isCommoner) {
        if (const NpcSocial* s = findSocial(a.self.id)) return *s;
        NpcSocial fallback;
        fallback.npcId = a.self.id;
        fallback.offers = {NpcAction::Talk};
        fallback.greet = "此人沉默不语。";
        fallback.source = "（未设定社交画像）";
        return fallback;
    }
    return socialForCommoner(a);
}

// ===========================================================================
//  态度判定
// ===========================================================================
NpcApproach evaluateApproach(const NpcAgent& npc,
                             const Cultivator& player,
                             const NpcRelation& rel,
                             bool playerHasOrigin,
                             OriginId playerOrigin,
                             FactionId playerFaction) {
    NpcApproach out;

    //
    //  取画像须用 socialFor()，而非 findSocial()。
    //
    //  findSocial() 只查【手工表】。凡俗众生不在表内，
    //  evaluateApproach 因此走进「无社交画像」的兜底分支：
    //  态度一律淡漠、reason 写着「未设定社交画像」——
    //  71 位居民全部无法互动，而我上一轮只改了视图层的
    //  npcContacts()，没改这里的规则层，于是视图拿到互动列表、
    //  态度却是错的，两处对不上。
    //
    const NpcSocial socVal = socialFor(npc);
    const NpcSocial* soc = &socVal;

    const int pr = static_cast<int>(player.rank);
    const int mr = static_cast<int>(soc->minRankToApproach);

    //
    //  一、尊者行踪莫测 —— 非同阶者无从得见
    //
    //  九转尊者动念可决一域兴衰，凡人、寻常蛊仙根本寻不见。
    //  这是原著的根本设定，不可为「让玩家有得玩」而破坏。
    //
    if (soc->inscrutable && pr < mr) {
        out.visible  = false;
        out.attitude = NpcAttitude::Invisible;
        out.line     = soc->invisible;
        out.reason   = "修为差距过大，无从得见其踪";
        return out;
    }

    //
    //  二、势力敌对 —— 闭门
    //
    //  天庭与方源势不两立。玩家若已入敌对势力，对方自然不予理会。
    //
    if (npc.faction != FactionId::Neutral && playerFaction != FactionId::Neutral) {
        const bool hostile =
            (npc.faction == FactionId::TianTing && playerFaction == FactionId::FangYuan) ||
            (npc.faction == FactionId::FangYuan && playerFaction == FactionId::TianTing) ||
            (npc.faction == FactionId::JuYang   && playerFaction == FactionId::TianTing) ||
            (npc.faction == FactionId::TianTing && playerFaction == FactionId::JuYang);
        if (hostile && rel.affinity < 60) {
            out.visible  = true;
            out.attitude = NpcAttitude::Hostile;
            out.line     = soc->hostile;
            out.reason   = "所属势力与之对立";
            return out;
        }
    }

    //
    //  三、仙凡鸿沟 —— 修为不足则不屑
    //
    if (pr < mr) {
        out.visible  = true;
        out.attitude = NpcAttitude::Scorn;
        out.line     = soc->brush;
        out.reason   = "对方修为远高于你，视你如蝼蚁";
        return out;
    }

    //
    //  四、情谊与因缘
    //
    int score = rel.affinity;

    //  出身因缘：南疆商家商人与商心慈有一面之缘
    if (soc->hasOriginTie && playerHasOrigin && playerOrigin == soc->affinityOrigin) {
        score += 30;
        out.reason = soc->originTieNote;
    }
    //  见过面且多次交谈，情谊渐生
    if (rel.met) score += 10;
    if (rel.talkCount >= 3) score += 10;

    out.visible = true;
    if      (score >= 50) out.attitude = NpcAttitude::Cordial;
    else if (score >= 20) out.attitude = NpcAttitude::Friendly;
    else if (score >  -20) out.attitude = NpcAttitude::Neutral;
    else                   out.attitude = NpcAttitude::Indifferent;

    out.canTalk = true;
    out.line = rel.met ? soc->greet : soc->greet;
    if (out.reason.empty()) out.reason = "可与之言语";
    return out;
}

// ===========================================================================
//  可做的互动
// ===========================================================================
bool attitudePermits(NpcAttitude att, NpcAction act) {
    switch (att) {
        case NpcAttitude::Invisible:
            return false;                      // 见不到，一切都谈不上
        case NpcAttitude::Hostile:
            return act == NpcAction::Duel;     // 敌对时只剩动手
        case NpcAttitude::Scorn:
            return false;                      // 不屑一顾，不予理会
        case NpcAttitude::Indifferent:
            return act == NpcAction::Talk || act == NpcAction::Duel;
        case NpcAttitude::Neutral:
            return act == NpcAction::Talk || act == NpcAction::Inquire ||
                   act == NpcAction::Duel;
        case NpcAttitude::Friendly:
            return act == NpcAction::Talk || act == NpcAction::Inquire ||
                   act == NpcAction::Trade || act == NpcAction::Duel;
        case NpcAttitude::Cordial:
            return true;                       // 亲近则无所不可（含请教、求借）
    }
    return false;
}

std::vector<NpcAction> availableActions(const NpcSocial& soc, NpcAttitude att) {
    std::vector<NpcAction> v;
    for (NpcAction a : soc.offers)
        if (attitudePermits(att, a)) v.push_back(a);
    return v;
}

// ===========================================================================
//  交谈可得的情报
// ===========================================================================
std::vector<std::string> intelFromTalk(const std::string& npcId) {
    //
    //  情报是「交谈」的实质收益 —— 否则交谈只是看一段文字。
    //  定仙游下落这条尤其关键：未听说则支线无从触发（既有设定）。
    //
    if (npcId == "fangyuan" || npcId == "wushuai" || npcId == "qihailaozu")
        return {"intel:dingxianyou_in_fangyuan"};
    if (npcId == "shangxinci")
        return {"intel:shangjiacheng_trade", "intel:dingxianyou_in_fangyuan"};
    if (npcId == "tianting_zhenren")
        return {"intel:tianting_court"};
    if (npcId == "xingxiu")
        return {"intel:tianting_court", "intel:yuanjing"};
    if (npcId == "juyang")
        return {"intel:changshengtian"};
    if (npcId == "youhun")
        return {"intel:youhun_sun"};
    return {};
}

} // namespace gr
