// 出身表实现（需求 11）
#include "gr/core/Origin.hpp"

#include <algorithm>

namespace gr {

const std::vector<OriginDef>& allOrigins() {
    static const std::vector<OriginDef> k = [] {
        std::vector<OriginDef> v;
        auto mk = [](OriginId id, Domain d, const char* name, const char* shortDesc,
                     const char* desc, Rank rk, const char* src, bool canon) {
            OriginDef o;
            o.id = id; o.domain = d; o.name = name; o.shortDesc = shortDesc;
            o.desc = desc; o.startRank = rk; o.source = src; o.canon = canon;
            return o;
        };

        // ================= 南疆 =================
        {
            // 商家：南疆贸易中心，商家城几覆整个商量山（第一卷可核验）
            auto o = mk(OriginId::NanJiang_ShangMerchant, Domain::NanJiang,
                "寄居商家寨的商人", "客居商家寨，与商心慈有一面之缘",
                "你非商家子弟，只因行商寄居于商家寨。寨中商贾辐辏，"
                "认得些人，走动也方便些。曾于一次交易中远远见过商家家主商心慈"
                "——那是一位极擅笼络人心的女子，你未必入得了她的眼，"
                "但这点香火情，日后或有用处。",
                Rank::R1, "第一卷：商家城几覆商量山，为南疆贸易中心；商心慈为商家家主", true);
            o.socialTie   = 2;                 // 城内有点人脉
            o.startIntel  = {"intel_shangxin_ci"};
            o.startSiteId = "nj_shangjiazhai";
            v.push_back(o);
        }
        {
            // 百家寨孤儿：当日开窍，姓名必须为「百」
            auto o = mk(OriginId::NanJiang_BaiOrphan, Domain::NanJiang,
                "百家寨孤儿", "族中孤儿，今日开窍",
                "你是百家寨中的孤儿，自小吃百家饭长大，连父母的面也没见过。"
                "今日寨中开窍大典，你也排在队列里。成败在此一举。",
                Rank::R1, "需求设定：百家寨孤儿；开窍场景依第一卷", false);
            o.requiredSurname = "百";
            o.awakens  = true;
            o.aptitude = canon::Aptitude::Bing;       // 丙等：元海占空窍四五成
            o.startSiteId = "nj_baijiazhai";
            v.push_back(o);
        }
        {
            auto o = mk(OriginId::NanJiang_Rogue, Domain::NanJiang,
                "南疆野生散修", "无门无派，凭几手传承混日子",
                "你没有师门，也无家族可倚。只是在山野间跌跌撞撞地活下来，"
                "机缘凑巧得了几个小传承的消息，怀里也揣着几只勉强能用的蛊。",
                Rank::R1, "需求设定：有几个小传承的信息和蛊虫", false);
            o.startIntel = {"legacy_x1", "legacy_x2", "legacy_x3"};
            o.startGuNames = {"酒虫", "月光蛊"};
            v.push_back(o);
        }

        // ================= 东海 =================
        {
            auto o = mk(OriginId::DongHai_RogueNoSea, Domain::DongHai,
                "无海域的东海散修", "漂在海上，一寸海域也不属于你",
                "东海以海域划分势力，而你哪里都不算数。"
                "没有自己的海域，便只能替人下海、替人卖命。",
                Rank::R1, "地理研究：东海以海域而非行政区划分", true);
            v.push_back(o);
        }
        {
            auto o = mk(OriginId::DongHai_RogueWithSea, Domain::DongHai,
                "有小片海域的东海散修", "巴掌大一块海，也是你的立足处",
                "你有一小片自己的海域。虽小，却产得出些东西，"
                "在东海这片讲究海域归属的地方，总算有了说话的余地。",
                Rank::R1, "地理研究：东海以海域划分", true);
            o.startSites = {"dh_xiao_haiyu"};
            v.push_back(o);
        }

        // ================= 北原 =================
        {
            auto o = mk(OriginId::BeiYuan_ChuDisciple, Domain::BeiYuan,
                "楚门弟子", "楚家（楚门）门下，草原上的正统出身",
                "你入楚家门下。北原部族林立，楚门虽非黄金血脉，"
                "却也是草原上叫得出名号的势力。",
                Rank::R1, "需求设定：楚门（楚家）弟子", false);
            o.startIntel = {"intel_chumen"};
            v.push_back(o);
        }
        {
            auto o = mk(OriginId::BeiYuan_GoldenBlood, Domain::BeiYuan,
                "黄金家族弟子", "体内流着巨阳仙尊的血，今日开窍",
                "你体内流着巨阳仙尊的血脉 —— 北原把持大局的黄金部族，"
                "便是这些血脉的后裔。今日族中开窍大典，"
                "比寻常部族要隆重得多。",
                Rank::R1, "百度百科/原著：体内流着巨阳仙尊血脉的蛊师部族统称黄金家族；"
                          "北原黄金部族把持大局", true);
            o.awakens  = true;
            o.aptitude = canon::Aptitude::Yi;         // 血脉加持，乙等
            // 北原黄金家族择一：黑家（黑楼兰所在）等
            o.optionalClans = {"黑家", "蛮家", "蒙家"};
            o.hasBloodline  = true;
            o.bloodlineNote = "巨阳仙尊血脉（黄金家族）";
            v.push_back(o);
        }
        {
            auto o = mk(OriginId::BeiYuan_Beastman, Domain::BeiYuan,
                "兽人族人", "人身兽首，北原异族之一",
                "你是北原的兽人。人身而兽首，在黄金部族眼里，"
                "异族终究矮上一头。",
                Rank::R1, "需求设定：兽人族人", false);
            v.push_back(o);
        }

        // ================= 西漠 =================
        {
            auto o = mk(OriginId::XiMo_ShangClan, Domain::XiMo,
                "商家族人", "西漠商家，商队纵横大漠",
                "西漠商家，以商队往来于绿洲之间。虽与南疆商家同姓，"
                "却是另起的一支。",
                Rank::R1, "需求设定：西漠商家族人", false);
            o.socialTie = 1;
            o.startIntel = {"intel_shangjia"};
            v.push_back(o);
        }
        {
            auto o = mk(OriginId::XiMo_TangClan, Domain::XiMo,
                "唐家人", "西漠唐家，家族之中学蛊",
                "你是唐家子弟。西漠十四家族之一，族中自有传蛊的法子。",
                Rank::R1, "地理研究：西漠·十四家族（唐家）", true);
            o.startIntel = {"intel_tangjia"};
            v.push_back(o);
        }
        {
            auto o = mk(OriginId::XiMo_OtherClan, Domain::XiMo,
                "西漠其它家族人", "西漠十四家族中的某一支",
                "西漠家族众多，你出身其中不起眼的一支。"
                "家族给你一个起点，路却要自己走。",
                Rank::R1, "地理研究：西漠·十四家族", true);
            v.push_back(o);
        }

        // ================= 中洲 =================
        {
            auto o = mk(OriginId::ZhongZhou_TenSects, Domain::ZhongZhou,
                "十大宗门弟子", "中洲十大古派门下",
                "中洲元气最盛，十大古派立于此地。你入得其中一门，"
                "自此有师承、有资源，也有门规。",
                Rank::R1, "需求设定：十大宗门弟子", false);
            o.socialTie = 1;
            o.startIntel = {"intel_zongmen"};
            v.push_back(o);
        }
        {
            auto o = mk(OriginId::ZhongZhou_Rogue, Domain::ZhongZhou,
                "中洲野生散修", "宗门之外，自生自灭",
                "中洲是正道的地盘，十大宗门之外，散修的活路很窄。",
                Rank::R1, "需求设定：中洲野生散修", false);
            v.push_back(o);
        }
        return v;
    }();
    return k;
}

const OriginDef* origin(OriginId id) {
    for (const auto& o : allOrigins())
        if (o.id == id) return &o;
    return nullptr;
}

std::vector<const OriginDef*> originsIn(Domain d) {
    std::vector<const OriginDef*> v;
    for (const auto& o : allOrigins())
        if (o.domain == d) v.push_back(&o);
    return v;
}

// ---------------------------------------------------------------- 开窍场景
AwakeningScene awakeningSceneFor(OriginId id, const std::string& playerName) {
    AwakeningScene s;
    const OriginDef* od = origin(id);
    if (!od || !od->awakens) return s;
    s.enabled = true;

    const auto spec = canon::aptitude_spec(od->aptitude);
    const double ratio = (spec.yuanHaiLow + spec.yuanHaiHigh) * 0.5;
    const char* apt    = canon::to_string(od->aptitude);

    if (id == OriginId::NanJiang_BaiOrphan) {
        //
        //  南疆百家寨：依第一卷所述开窍大典。
        //    族中少年于开窍大典上开辟空窍、凝聚真元海，自此方为蛊师；
        //    刚开窍者皆为一转初阶；资质以元海占空窍的比例分甲乙丙丁。
        //
        s.where  = "百家寨 · 祠堂前";
        s.rite   = "开窍大典";
        s.detail = "祠堂前黑压压跪了一片少年。族老捧着开窍蛊，一个个点过去。"
                   "轮到你时，那蛊虫化作一线微光钻入体内 —— "
                   "空窍被生生撑开，一点真元自窍底渗出，渐渐汇成一片小小的海。"
                   "你是孤儿，族中没有人为你高兴，也没有人为你难过。";
        s.result = playerName + " 开窍成功，开辟空窍、凝聚真元海（约 " +
                   std::to_string(static_cast<int>(ratio * 100)) + " 成），"
                   "自此为一转初阶蛊师。资质：" + apt + "。";
        s.source = "第一卷：开窍大典开辟空窍、凝聚真元海；"
                   "九转境界，每转分初阶/中阶/高阶/巅峰；资质甲乙丙丁";
    } else if (id == OriginId::BeiYuan_GoldenBlood) {
        //
        //  北原黄金家族：同为开窍大典，但更北原化 ——
        //    草原、部族、血脉。黄金家族把持北原大局，
        //    开窍是部族大事，观礼者众，且血脉本身即是一种加持。
        //
        s.where  = "北原 · 黄金部族营地（苍莽草地）";
        s.rite   = "开窍大典（北原部族式）";
        s.detail = "苍莽草地上搭起祭台，各部族的旗帜在风里猎猎作响。"
                   "你是黄金血脉，观礼的人比寻常部族多出数倍 —— "
                   "他们要看的不是一个孩子开窍，而是巨阳先祖的血还在不在你身上。"
                   "开窍蛊入体的那一刻，你体内的血像是被唤醒了，"
                   "真元海比同辈涨得更快、更满。";
        s.result = playerName + " 开窍成功，真元海约 " +
                   std::to_string(static_cast<int>(ratio * 100)) + " 成，"
                   "一转初阶。资质：" + apt + "（血脉加持）。";
        s.source = "开窍依第一卷；北原化依据：黄金部族把持北原大局、"
                   "体内流巨阳仙尊血脉者统称黄金家族";
    }
    return s;
}

} // namespace gr
