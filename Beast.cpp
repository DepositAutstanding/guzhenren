#include "gr/world/Beast.hpp"

#include <algorithm>
#include <cstdint>

namespace gr {
namespace {

//  确定性伪随机
inline std::uint64_t h64(std::uint64_t x) {
    x ^= x >> 33; x *= 0xff51afd7ed558ccdull;
    x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ull;
    x ^= x >> 33;
    return x;
}

// ============================================================================
//  兽种表
//
//  分两类：
//    A. 原著具名（canon = true）—— 可核验，写出处
//    B. 按地形推定的寻常野兽（canon = false）—— 工程设定
//
//  判定原则：凡兽多不具名，原著只写「狼潮」「猛兽」「毒虫」之类，
//  具体物种是工程填充；不可冒充原著载明。
// ============================================================================
const std::vector<BeastSpecies> kSpecies = {

// ---------------------------------------------------------------- 原著具名：太古传奇
{ "disheng",   "孽龙·帝藏生",   BeastRank::LegendaryHuang, Domain::None,
  "龙宫之主，太古传奇荒兽。方源重生后龙宫为其龙人分身吴帅所掌控。",
  "原著：孽龙帝藏生，太古传奇荒兽，龙宫之主", true, false,
  {"龙鳞", "龙血", "龙筋"}, {} },
{ "shabi",     "煞狴九十五",     BeastRank::LegendaryHuang, Domain::None,
  "太古传奇荒兽，排行九十五。",
  "原著：煞狴九十五，太古传奇荒兽", true, false,
  {"狴骨", "煞气结晶"}, {} },
{ "ruandan",   "青玉鹤·阮丹",    BeastRank::LegendaryHuang, Domain::None,
  "太古传奇荒兽，青玉鹤。",
  "原著：青玉鹤阮丹，太古传奇荒兽", true, false,
  {"青玉鹤羽"}, {} },
{ "cangxuan",  "苍天藤·苍玄子",  BeastRank::LegendaryHuang, Domain::None,
  "太古传奇荒植（荒兽之属）。",
  "原著：苍天藤苍玄子，传奇太古荒植", true, false,
  {"苍藤", "木道道痕结晶"}, {} },
{ "cangjing",  "苍蓝龙鲸",       BeastRank::LegendaryHuang, Domain::DongHai,
  "东海太古传奇荒兽。",
  "原著：苍蓝龙鲸，太古传奇荒兽（东海）", true, false,
  {"龙鲸脂", "鲸骨"}, {Terrain::Sea} },
{ "moliqiu",   "狗尾续命貂·毛里球", BeastRank::LegendaryHuang, Domain::None,
  "太古传奇荒兽，寿元极长。",
  "原著：狗尾续命貂毛里球，太古传奇荒兽", true, false,
  {"续命貂毛"}, {} },
{ "tamowa",    "贪食太魔蛙",     BeastRank::LegendaryHuang, Domain::None,
  "太古传奇荒兽。",
  "原著：贪食太魔蛙，太古传奇荒兽", true, false,
  {"魔蛙毒囊"}, {} },
{ "hunyuan",   "混元一气子",     BeastRank::LegendaryHuang, Domain::None,
  "太古传奇荒兽。",
  "原著：混元一气子，太古传奇荒兽", true, false, {"混元之气"}, {} },
{ "dingzhen",  "定真树",         BeastRank::LegendaryHuang, Domain::None,
  "太古传奇荒植。",
  "原著：定真树，传奇太古荒植", true, false, {"定真木"}, {} },

// ---------------------------------------------------------------- 原著具名：太古
{ "shangji",   "上极天鹰",       BeastRank::PrimordialHuang, Domain::None,
  "太古荒兽，方源曾以上极天鹰为坐骑。",
  "原著：太古荒兽上极天鹰，方源坐骑", true, false,
  {"天鹰羽", "鹰骨"}, {Terrain::Mountain, Terrain::SnowPeak} },
{ "xufu",      "墟蝠",           BeastRank::PrimordialHuang, Domain::None,
  "太古荒兽，身负宇道道痕；尸骸可致空间扭曲，生出虚幻宫殿之景。",
  "原著/野史：墟蝠，太古荒兽，宇道道痕", true, false,
  {"宇道道痕碎片"}, {Terrain::Cave, Terrain::Abyss} },
{ "liusha",    "一指流鲨",       BeastRank::PrimordialHuang, Domain::DongHai,
  "东海太古荒兽，八转垫底层次。",
  "原著：一指流鲨，太古荒兽（东海）", true, false,
  {"鲨牙", "鲨皮"}, {Terrain::Sea} },
{ "jianlong",  "太古剑龙",       BeastRank::PrimordialHuang, Domain::None,
  "剑道太古荒兽。",
  "原著：太古剑龙（剑道荒兽）", true, false,
  {"剑龙骨", "剑龙鳞"}, {Terrain::Mountain} },
{ "cishenwei", "刺神猬",         BeastRank::PrimordialHuang, Domain::None,
  "太古荒兽，其刺可炼正反狙神针，为剑道仙材。",
  "原著：刺神猬的刺（正反狙神针），剑道仙材", true, false,
  {"刺神猬的刺"}, {Terrain::Mountain, Terrain::StoneForest} },

// ---------------------------------------------------------------- 原著具名：上古
{ "jianjiao",  "上古剑蛟",       BeastRank::AncientHuang, Domain::None,
  "上古荒兽。天赋本能「剑光龙息」，连续喷吐不过二三十次。",
  "原著：上古剑蛟，剑光龙息为其最强手段", true, false,
  {"蛟鳞", "蛟血"}, {Terrain::Water, Terrain::Lake} },
{ "shilong",   "尸龙",           BeastRank::AncientHuang, Domain::None,
  "繁星洞天中的尸龙，七转巅峰层次。",
  "原著：繁星洞天内七转巅峰尸龙", true, false,
  {"尸龙鳞", "尸气"}, {Terrain::Cave, Terrain::Abyss} },

// ---------------------------------------------------------------- 原著具名：荒兽
{ "swordjiao", "剑蛟",           BeastRank::Huang, Domain::None,
  "剑道荒兽。",
  "原著：剑蛟（剑道荒兽）", true, false,
  {"蛟皮", "蛟筋"}, {Terrain::Water, Terrain::Lake} },

// ---------------------------------------------------------------- 原著具名：异兽及特异
{ "baijiaoniu","白角牛",         BeastRank::BeastKing, Domain::NanJiang,
  "南疆白角山所产。身漆黑如墨，角雪白，能吸引律道蛊虫共生，"
  "故白角山为南疆律道蛊虫重要产地。",
  "地点总表：白角山产白角牛，律道蛊虫重要产地", true, false,
  {"白牛角", "牛皮"}, {Terrain::Grassland, Terrain::Foothill} },
{ "yecha",     "夜叉章鱼",       BeastRank::MyriadKing, Domain::None,
  "地沟峭壁上凿洞而居。",
  "原著：地沟峭壁有夜叉章鱼凿洞而居", true, false,
  {"章鱼腕", "墨囊"}, {Terrain::EarthRift, Terrain::Abyss} },
{ "diqiao",    "地壳蜗牛",       BeastRank::MyriadKing, Domain::None,
  "栖于地沟极深处，其粘涎可加工为星夜黏涎。",
  "原著：地壳蜗牛栖于地沟极深处，粘涎可加工为星夜黏涎", true, false,
  {"星夜黏涎", "蜗壳"}, {Terrain::EarthRift, Terrain::Abyss} },
{ "zhima",     "万里芝马",       BeastRank::BeastKing, Domain::None,
  "碧潭福地中跑出的灵兽。",
  "原著：碧潭福地万里芝马", true, false,
  {"芝马茸"}, {Terrain::Grassland} },

// ---------------------------------------------------------------- 道兽（道痕凝聚，无要害）
{ "hunshou",   "魂兽",           BeastRank::MyriadKing, Domain::None,
  "魂道道兽。红莲魔尊破坏宿命蛊后，生死门威能下降，"
  "无数魂魄遗留世间，逐渐形成魂兽。",
  "原著：宿命蛊被破坏后遗留魂魄渐成魂兽（道兽）", true, true,
  {"魂道道痕结晶"}, {Terrain::Abyss, Terrain::Cave} },
{ "yingshou",  "影兽",           BeastRank::MyriadKing, Domain::None,
  "影道道兽，无器官要害。",
  "原著：影兽为道兽之一", true, true,
  {"影道道痕结晶"}, {Terrain::Cave} },
{ "niguai",    "泥怪",           BeastRank::MyriadKing, Domain::None,
  "土道道兽，无器官要害。",
  "原著：泥怪为道兽之一", true, true,
  {"土道道痕结晶"}, {Terrain::Wetland, Terrain::Gobi} },

// ---------------------------------------------------------------- 寻常野兽（工程设定）
//  山川
{ "lang",  "狼",   BeastRank::Wild, Domain::None,
  "成群出没。南疆青茅山曾遭狼潮。", "原著：狼潮（第一卷青茅山）", false, false,
  {"兽皮", "兽骨", "兽筋"}, {Terrain::Mountain, Terrain::Forest, Terrain::Grassland} },
{ "hu",    "虎",   BeastRank::Wild, Domain::None,
  "山林之王。南疆明知山为虎类生态区。", "地点总表：明知山虎兽", false, false,
  {"虎骨", "虎皮", "虎鞭"}, {Terrain::Mountain, Terrain::Forest, Terrain::Rainforest} },
{ "bao",   "豹",   BeastRank::Wild, Domain::None,
  "迅捷的猛兽。", "工程设定", false, false,
  {"豹皮", "兽骨"}, {Terrain::Mountain, Terrain::Forest, Terrain::Hill} },
{ "xiong", "熊",   BeastRank::Wild, Domain::None,
  "力大无穷。", "工程设定", false, false,
  {"熊胆", "熊皮", "兽肉"}, {Terrain::Mountain, Terrain::Forest, Terrain::IceField} },
{ "yuan",  "猿猴", BeastRank::Wild, Domain::None,
  "灵巧的山猿。", "工程设定", false, false,
  {"猿骨", "兽皮"}, {Terrain::Mountain, Terrain::Forest, Terrain::Rainforest} },
{ "ying",  "鹰",   BeastRank::Wild, Domain::None,
  "高空盘旋的猛禽。", "工程设定", false, false,
  {"鹰羽", "鹰爪"}, {Terrain::Mountain, Terrain::SnowPeak, Terrain::StoneForest, Terrain::Grassland} },
//  森林 / 雨林
{ "yezhu", "野猪", BeastRank::Wild, Domain::None,
  "白豕蛊常寄于野猪体内。", "原著：白豕蛊与野猪", false, false,
  {"兽肉", "獠牙", "兽皮"}, {Terrain::Forest, Terrain::Rainforest, Terrain::Foothill} },
{ "lu",    "鹿",   BeastRank::Wild, Domain::None,
  "温顺的草食兽。", "工程设定", false, false,
  {"鹿茸", "鹿皮"}, {Terrain::Forest, Terrain::Grassland, Terrain::Spring} },
{ "hu2",   "狐",   BeastRank::Wild, Domain::None,
  "机敏狡黠。", "工程设定", false, false,
  {"狐皮"}, {Terrain::Forest, Terrain::Hill, Terrain::Oasis} },
//  瘴气林 / 毒泽
{ "dushe", "毒蛇", BeastRank::Wild, Domain::None,
  "瘴气林中毒蛇横行。", "工程设定", false, false,
  {"蛇信子", "蛇胆", "毒囊"}, {Terrain::ToxicForest, Terrain::Rainforest, Terrain::Wetland} },
{ "zhizhu","毒蜘蛛", BeastRank::Wild, Domain::None,
  "结网于幽暗处。", "工程设定", false, false,
  {"蛛丝", "毒囊"}, {Terrain::ToxicForest, Terrain::Cave, Terrain::Rainforest} },
{ "wugong","蜈蚣", BeastRank::Wild, Domain::None,
  "百足之虫，性毒。", "工程设定", false, false,
  {"蜈蚣干", "毒囊"}, {Terrain::ToxicForest, Terrain::Cave} },
{ "xie",   "蝎",   BeastRank::Wild, Domain::None,
  "尾钩带毒。", "工程设定", false, false,
  {"蝎尾", "毒囊"}, {Terrain::Desert, Terrain::Gobi, Terrain::Cave} },
{ "mang",  "蟒",   BeastRank::Wild, Domain::None,
  "巨蛇，可绞杀猎物。", "工程设定", false, false,
  {"蟒皮", "蛇胆"}, {Terrain::ToxicForest, Terrain::Wetland, Terrain::Water, Terrain::Rainforest} },
//  草原
{ "ma",    "马",   BeastRank::Wild, Domain::None,
  "草原上的马群。", "工程设定", false, false,
  {"马皮", "兽肉"}, {Terrain::Grassland} },
{ "yang",  "羊",   BeastRank::Wild, Domain::None,
  "成群的食草兽。", "工程设定", false, false,
  {"羊毛", "兽肉"}, {Terrain::Grassland, Terrain::Hill} },
{ "niu",   "牛",   BeastRank::Wild, Domain::None,
  "力大，可驯为役畜。", "工程设定", false, false,
  {"牛皮", "牛角", "兽肉"}, {Terrain::Grassland} },
//  沙漠 / 戈壁
{ "shaxie","沙蝎", BeastRank::Wild, Domain::XiMo,
  "潜于沙下。", "工程设定", false, false,
  {"蝎尾", "毒囊"}, {Terrain::Desert, Terrain::Gobi, Terrain::SandDune} },
{ "shashe","沙蛇", BeastRank::Wild, Domain::XiMo,
  "游走于沙丘之间。", "工程设定", false, false,
  {"蛇信子", "蛇皮"}, {Terrain::Desert, Terrain::SandDune, Terrain::Gobi} },
{ "shahu", "沙狐", BeastRank::Wild, Domain::XiMo,
  "耐旱的小兽。", "工程设定", false, false,
  {"狐皮"}, {Terrain::Desert, Terrain::Oasis, Terrain::Gobi} },
{ "luotuo","骆驼", BeastRank::Wild, Domain::XiMo,
  "商队必备的役畜。", "工程设定", false, false,
  {"驼皮", "驼毛"}, {Terrain::Desert, Terrain::Oasis} },
//  冰原
{ "xuelang","雪狼", BeastRank::Wild, Domain::BeiYuan,
  "北原雪原上的狼群。", "工程设定", false, false,
  {"兽皮", "兽骨"}, {Terrain::IceField, Terrain::SnowPeak} },
{ "baixiong","白熊", BeastRank::Wild, Domain::BeiYuan,
  "冰原巨兽。", "工程设定", false, false,
  {"熊胆", "熊皮"}, {Terrain::IceField} },
{ "xuebao","雪豹", BeastRank::Wild, Domain::BeiYuan,
  "雪线上的猎手。", "工程设定", false, false,
  {"豹皮"}, {Terrain::SnowPeak, Terrain::IceField} },
//  水域
{ "yu",    "鱼",   BeastRank::Wild, Domain::None,
  "江河湖海皆有。", "工程设定", false, false,
  {"鱼肉", "鱼鳔"}, {Terrain::Water, Terrain::Lake, Terrain::Sea, Terrain::Wetland} },
{ "gui",   "龟",   BeastRank::Wild, Domain::None,
  "长寿的水族。", "工程设定", false, false,
  {"龟甲"}, {Terrain::Water, Terrain::Lake, Terrain::Sea, Terrain::Coast} },
{ "e",     "鳄",   BeastRank::Wild, Domain::None,
  "潜伏水畔的猛兽。", "工程设定", false, false,
  {"鳄皮", "鳄牙"}, {Terrain::Wetland, Terrain::Water, Terrain::Lake} },
{ "sha",   "鲨",   BeastRank::Wild, Domain::DongHai,
  "海中的猎手。", "工程设定", false, false,
  {"鲨牙", "鲨皮"}, {Terrain::Sea} },
{ "xie2",  "蟹",   BeastRank::Wild, Domain::None,
  "水畔甲壳类。", "工程设定", false, false,
  {"蟹壳"}, {Terrain::Coast, Terrain::Shoal, Terrain::Sea} },
//  火山
{ "huoxi", "火蜥", BeastRank::Wild, Domain::XiMo,
  "栖于灼热的岩隙。", "工程设定", false, false,
  {"火晶砂", "蜥皮"}, {Terrain::Volcano, Terrain::LavaRock} },
{ "yanshe","炎蛇", BeastRank::Wild, Domain::XiMo,
  "体表泛红的毒蛇。", "工程设定", false, false,
  {"炎蛇胆", "蛇信子"}, {Terrain::Volcano, Terrain::LavaRock} },
//  云竹 / 雾都
{ "zhuye", "竹叶蛇", BeastRank::Wild, Domain::NanJiang,
  "栖于云竹林。", "工程设定", false, false,
  {"蛇信子", "蛇皮"}, {Terrain::Bamboo, Terrain::MistCity} },
{ "yunbao","云豹", BeastRank::Wild, Domain::NanJiang,
  "雾都中的猛兽。", "原著：雾都云竹、云雾、猛兽并存", false, false,
  {"豹皮"}, {Terrain::MistCity, Terrain::Bamboo} },
//  洞窟
{ "bianfu","蝙蝠", BeastRank::Wild, Domain::None,
  "洞中群栖。", "工程设定", false, false,
  {"蝠翼", "蝠粪"}, {Terrain::Cave, Terrain::Sinkhole} },
//  海岛
{ "hainiao","海鸟", BeastRank::Wild, Domain::DongHai,
  "海岛上的群鸟。", "工程设定", false, false,
  {"鸟羽"}, {Terrain::Island, Terrain::Coast} },
//  石林 / 泉
{ "he",    "鹤",   BeastRank::Wild, Domain::None,
  "灵秀的水鸟。", "工程设定", false, false,
  {"鹤羽"}, {Terrain::Spring, Terrain::Wetland, Terrain::Lake} },
};

} // namespace

// ============================================================================
const std::vector<BeastSpecies>& all_beast_species() { return kSpecies; }

const BeastSpecies* beast_species_by_id(const std::string& id) {
    for (const auto& s : kSpecies)
        if (s.id == id) return &s;
    return nullptr;
}

std::vector<const BeastSpecies*> beasts_at(Domain d, Terrain t) {
    std::vector<const BeastSpecies*> out;
    for (const auto& s : kSpecies) {
        //  域不符者跳过（None = 五域皆可）
        if (s.domain != Domain::None && s.domain != d) continue;
        //  未指定栖息地者不参与按地形检索（多为原著具名的定点荒兽）
        if (s.habitats.empty()) continue;
        for (Terrain h : s.habitats)
            if (h == t) { out.push_back(&s); break; }
    }
    return out;
}

// ============================================================================
//  遭遇
// ============================================================================
BeastEncounter make_encounter(Domain d, Terrain t, int x, int y, std::uint64_t seed) {
    BeastEncounter e;
    const auto pool = beasts_at(d, t);
    if (pool.empty()) return e;

    const std::uint64_t h = h64(seed ^ (static_cast<std::uint64_t>(x) * 73856093ull)
                                     ^ (static_cast<std::uint64_t>(y) * 19349663ull));

    //
    //  荒兽极稀 —— 不可满地都是太古荒兽。
    //
    //  原著里太古荒兽是動輒震动一域的存在（上极天鹰、刺神猬之属），
    //  寻常山野不可能随便撞见。故分池：荒兽池仅 4% 概率，
    //  凡兽池 96%。否则玩家在山川走两步就撞上太古剑龙，
    //  既不合理，也让「荒兽」二字失去分量。
    //
    std::vector<const BeastSpecies*> common, huang;
    for (const auto* sp : pool)
        (isHuangShou(sp->rank) ? huang : common).push_back(sp);

    const bool drawHuang = !huang.empty() && !common.empty()
                           && ((h >> 3) % 100) < 4;
    const auto& use = drawHuang ? huang : (common.empty() ? huang : common);
    const std::size_t i = static_cast<std::size_t>((h >> 7) % use.size());
    const BeastSpecies& sp = *use[i];

    e.speciesId = sp.id;
    e.name      = sp.name;
    e.rank      = sp.rank;
    e.canon     = sp.canon;
    e.source    = sp.source;

    //  群居还是独行：狼、羊、鱼、蝙蝠成群，虎、熊、豹独行
    const bool gregarious =
        (sp.id == "lang" || sp.id == "yang" || sp.id == "yu" ||
         sp.id == "bianfu" || sp.id == "ma" || sp.id == "niu" ||
         sp.id == "xuelang" || sp.id == "hainiao" || sp.id == "xie2");
    e.count = gregarious ? static_cast<int>(1 + (h >> 8) % 5) : 1;

    //
    //  兽群之王 —— 同种之兽可为百兽王、千兽王。
    //
    //  原著分级里百兽王、千兽王是「一群兽的首领」，并非独立物种，
    //  故不作单独条目，而在遭遇时按概率擢升。
    //
    if (sp.rank == BeastRank::Wild) {
        const int roll = static_cast<int>((h >> 11) % 100);
        if (roll < 5)       e.rank = BeastRank::ThousandKing;
        else if (roll < 20) e.rank = BeastRank::HundredKing;
    }

    //  荒兽级：凡人不可敌（六转及以上另论，见 resolve_hunt）
    if (isHuangShou(e.rank)) {
        e.fightable = false;
        e.warning = "此兽已入蛊仙之境，非你所能敌 —— 走罢。";
    }
    return e;
}

// ============================================================================
//  狩猎结算
//
//  不掷骰：按品阶对比，避免随机性失控。
//  玩家的攻击力由杀招威力与修为折算而来。
// ============================================================================
HuntOutcome resolve_hunt(const BeastSpecies& sp, int playerRank, int playerAttack) {
    HuntOutcome o;
    o.beastName = sp.name;
    o.rank      = sp.rank;

    //
    //  荒兽属蛊仙之境 —— 凡人不可敌，但蛊仙可以一战。
    //
    //  原先把「荒兽级一概拒绝」写死了，结果六转蛊仙猎剑蛟也被挡下，
    //  这不对：凡人是层级之差，蛊仙是同级之争。
    //
    if (isHuangShou(sp.rank) && playerRank < 6) {
        o.ok   = false;
        o.fled = true;
        o.line = "你连与之正面相对的资格都没有 —— 转身便走。";
        return o;
    }

    const int beastLevel = rankToRankNumber(sp.rank);
    const int power = playerRank * 10 + playerAttack;
    //  野兽亦有起码之凶；荒兽同级更强（余量 20），
    //  故六转空手不敌荒兽，须有蛊虫在身。
    int need = beastLevel * 10;
    if (need < 10) need = 10;
    if (isHuangShou(sp.rank)) need += 20;

    if (power < need) {
        o.ok   = false;
        o.line = "你不是它的对手，负伤而退。";
        o.hpLoss = static_cast<int>((need - power) * 0.6) + 5;
        return o;
    }

    o.ok = true;
    o.essenceCost = 5 + beastLevel * 3;
    o.hpLoss      = static_cast<int>((need * 0.3));
    for (const auto& d : sp.drops) o.gained.push_back(d);
    o.line = "斩获" + sp.name + "。";
    return o;
}

} // namespace gr
