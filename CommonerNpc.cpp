#include "gr/ai/CommonerNpc.hpp"
#include "gr/core/Origin.hpp"

#include <cstring>
#include <set>

namespace gr {
namespace {

// ---------------------------------------------------------------- 姓氏池
//
//  依地点总表所载诸族。五域各用其姓，不可混同 ——
//  北原草原上不会跑出西漠的房家人。
//
const char* kSurnames[] = {
    //
    //  南疆 —— 商家、铁家、乔家等。
    //
    //  【已移除】古月、白、熊三姓。
    //
    //  原著：白凝冰十绝体自爆，「直接将三寨中人全部杀死，
    //  将整个青茅山都化为绝死冰域」。本项目设定的第六卷时间线，
    //  青茅山已成冰域绝地 —— 三寨（古月、白、熊）尽没。
    //
    //  此前姓氏池仍含这三姓，会生成「古月雨」这样的活人在百家寨起居作息，
    //  与「三寨尽没」直接冲突：这跟在废墟里摆一排活人是同一类错误，
    //  只是换了形式 —— 不在废墟摆，而让已灭之族在别处悄然复活。
    //
    //  需说明的是，古月一族并非「一个都没剩」：
    //  方源当时随商队外出躲过，古月方正被天鹤上人救走。
    //  但二人已不属族中（方源是炼天魔尊，方正在天庭），
    //  他们是【具名人物】，单独登记，不受此随机姓氏池影响。
    //  同理熊家寨有主动撤离的幸存者、白家有白凝冰 ——
    //  皆已脱离青茅山，不会出现在南疆寻常聚落里。
    //
    "商", "铁", "乔", "张", "陈",
    // 北原（黄金家族）
    "黑", "东方", "刘", "关", "慕容", "耶律", "楚", "宫",
    // 西漠（十四家族）
    "房", "万", "左丘", "萧", "田", "董", "唐", "秦", "孙", "莫", "石", "龚", "林", "拓跋", "习",
    // 东海（八大家族）
    "青岳", "宋", "华", "夏", "蔡", "苏", "南宫", "解", "吴",
    // 中洲（十大古派门下及寻常百姓）
    "风", "云", "柳", "秦", "沈", "韩", "杨", "赵",
};

//  各域姓氏在 kSurnames 中的区间 [begin, end)
struct SurnameRange { int begin, end; };
SurnameRange surnameRange(Domain d) {
    switch (d) {
        case Domain::NanJiang: return {0, 5};    // 商 铁 乔 张 陈（古月/白/熊已除）
        case Domain::BeiYuan:  return {5, 13};
        case Domain::XiMo:     return {13, 28};
        case Domain::DongHai:  return {28, 37};
        case Domain::ZhongZhou:return {37, 45};
        default:               return {0, 45};
    }
}

// ---------------------------------------------------------------- 名
//
//  蛊世界的名字多用自然物与德行，不用现代名。
//
const char* kGivenNames[] = {
    "山", "石", "铁", "岩", "松", "竹", "梅", "兰",
    "虎", "狼", "鹰", "燕", "龙", "豹", "麟", "鹤",
    "勇", "刚", "义", "德", "福", "贵", "寿", "安",
    "平", "宁", "清", "静", "秀", "芳", "明", "亮",
    "河", "海", "江", "湖", "风", "雷", "云", "雨",
};

// ---------------------------------------------------------------- 职业池（按域）
struct JobDef { Trade t; const char* name; };

const JobDef kJobsNanJiang[] = {
    {Trade::Hunter,   "猎户"},   {Trade::GuMaster, "蛊师"},
    {Trade::Elder,    "寨老"},   {Trade::Merchant, "商贩"},
    {Trade::Teacher,  "教习"},   {Trade::Guard,    "寨卫"},
    {Trade::Craftsman,"铁匠"},   {Trade::Farmer,   "药农"},
};
const JobDef kJobsBeiYuan[] = {
    {Trade::Herder,   "牧民"},   {Trade::Guard,    "骑手"},
    {Trade::Elder,    "族老"},   {Trade::GuMaster, "巫祝"},
    {Trade::Merchant, "马贩"},   {Trade::Craftsman,"皮匠"},
    {Trade::Hunter,   "猎户"},   {Trade::Rogue,    "游骑"},
};
const JobDef kJobsXiMo[] = {
    {Trade::Merchant, "商队向导"}, {Trade::Miner,    "矿工"},
    {Trade::Farmer,   "绿洲农户"}, {Trade::Rogue,    "沙行者"},
    {Trade::Elder,    "族老"},     {Trade::GuMaster, "蛊师"},
    {Trade::Guard,    "护队"},     {Trade::Craftsman,"驼夫"},
};
const JobDef kJobsDongHai[] = {
    {Trade::Hunter,   "渔夫"},     {Trade::Miner,    "采珠人"},
    {Trade::Merchant, "海商"},     {Trade::Craftsman,"船匠"},
    {Trade::Elder,    "族老"},     {Trade::GuMaster, "蛊师"},
    {Trade::Guard,    "水卫"},     {Trade::Rogue,    "散修"},
};
const JobDef kJobsZhongZhou[] = {
    {Trade::Teacher,  "外门执事"}, {Trade::GuMaster, "外门弟子"},
    {Trade::Farmer,   "农户"},     {Trade::Craftsman,"工匠"},
    {Trade::Merchant, "书贾"},     {Trade::Elder,    "长老"},
    {Trade::Guard,    "山门护卫"}, {Trade::Rogue,    "散修"},
};

const JobDef* jobPool(Domain d, int& n) {
    switch (d) {
        case Domain::NanJiang:  n = 8; return kJobsNanJiang;
        case Domain::BeiYuan:   n = 8; return kJobsBeiYuan;
        case Domain::XiMo:      n = 8; return kJobsXiMo;
        case Domain::DongHai:   n = 8; return kJobsDongHai;
        case Domain::ZhongZhou: n = 8; return kJobsZhongZhou;
        default:                n = 8; return kJobsZhongZhou;
    }
}

//  简易确定性伪随机：以聚落 id 与序号为种子，
//  保证同一局生成的居民稳定（不因重绘界面而变脸改名）。
struct MiniRng {
    std::uint64_t s;
    explicit MiniRng(std::uint64_t seed) : s(seed ? seed : 1) {}
    std::uint64_t next() {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        return s;
    }
    int pick(int n) { return n <= 0 ? 0 : static_cast<int>(next() % static_cast<std::uint64_t>(n)); }
};

std::uint64_t hashStr(const std::string& t) {
    std::uint64_t h = 1469598103934665603ull;
    for (char c : t) { h ^= static_cast<std::uint8_t>(c); h *= 1099511628211ull; }
    return h;
}

//  按聚落规模定人数：村寨少、城池多
int headcountFor(Settlement::Scale sc) {
    switch (sc) {
        case Settlement::Scale::Village: return 5;
        case Settlement::Scale::Town:    return 6;
        case Settlement::Scale::City:    return 8;
        case Settlement::Scale::Sect:    return 7;
        case Settlement::Scale::Capital: return 9;
    }
    return 5;
}

//  首领修为：大聚落的首领修为更高
Rank leaderRank(Settlement::Scale sc) {
    switch (sc) {
        case Settlement::Scale::Village: return Rank::R3;
        case Settlement::Scale::Town:    return Rank::R4;
        case Settlement::Scale::City:    return Rank::R5;
        case Settlement::Scale::Sect:    return Rank::R5;
        case Settlement::Scale::Capital: return Rank::R6;
    }
    return Rank::R3;
}

} // namespace

// ===========================================================================
//  生成
// ===========================================================================
std::vector<NpcAgent> build_commoner_npcs(const SettlementRegistry& reg) {
    std::vector<NpcAgent> out;
    //  全表共用一张重名表：不同聚落之间也不该满地同名
    std::set<std::string> usedNames;

    for (const Settlement& st : reg.all()) {
        //
        //  已毁之地不生人 ——
        //  古月山寨已随青茅山化为冰域绝地，三寨尽没，
        //  在此处摆一排活人，等于否认第一卷的结局。
        //
        if (st.ruined) continue;

        const int n = headcountFor(st.scale);
        MiniRng rng(hashStr(st.id) ^ 0x9e3779b97f4a7c15ull);
        const SurnameRange sr = surnameRange(st.domain);
        int jobN = 0;
        const JobDef* jobs = jobPool(st.domain, jobN);

        for (int i = 0; i < n; ++i) {
            const JobDef job = jobs[rng.pick(jobN)];

            NpcAgent a;
            a.isCommoner      = true;
            a.occupation      = job.name;
            a.homeSettlement  = st.id;
            a.whereId         = st.landmarkId;

            //
            //  姓名 —— 须避重名。
            //  同一聚落内撞名（三个「陈平」并立）既不合情理，
            //  也让玩家分不清在跟谁说话。最多重试若干次，
            //  仍撞则退而加序数字样，保证可用而不死循环。
            //
            std::string nm;
            for (int tryN = 0; tryN < 24; ++tryN) {
                const int si = sr.begin + rng.pick(sr.end - sr.begin);
                const int gi = rng.pick(static_cast<int>(
                    sizeof(kGivenNames) / sizeof(kGivenNames[0])));
                nm = std::string(kSurnames[si]) + kGivenNames[gi];
                if (usedNames.find(nm) == usedNames.end()) break;
                nm.clear();
            }
            if (nm.empty()) nm = std::string(kSurnames[sr.begin]) + "氏";
            usedNames.insert(nm);
            a.self.name = nm;
            a.self.id   = "cm_" + st.id + "_" + std::to_string(i);

            //
            //  修为 —— 不是人人都有。
            //
            //  原著里开窍需开窍大典与开窍蛊，资质分甲乙丙丁，
            //  无资质者终身为凡人。此前一律赋一~三转，
            //  等于说「世间无人不是蛊师」，与设定相悖。
            //
            //  故按职业分两类：
            //    · 修行相关（蛊师、教习、族老、护卫）→ 必有修为
            //    · 生业相关（农户、工匠、商贾、猎户、牧民、矿工、散修）
            //      → 多为凡夫（R0），仅少数开窍者
            //
            //  比例取六成凡夫：村镇之中寻常百姓终究是多数，
            //  但也不至于一个修行者都没有（否则蛊师成了稀罕物）。
            //
            const bool isCultivatingJob =
                (job.t == Trade::GuMaster || job.t == Trade::Teacher ||
                 job.t == Trade::Elder    || job.t == Trade::Guard);

            if (i == 0) {
                a.self.rank = leaderRank(st.scale);          // 首领必有修为
            } else if (isCultivatingJob) {
                const int r = 1 + rng.pick(3);               // 一 ~ 三转
                a.self.rank = static_cast<Rank>(r);
            } else if (rng.pick(10) < 6) {
                a.self.rank = Rank::R0;                      // 凡夫：未开窍
            } else {
                const int r = 1 + rng.pick(2);               // 一 ~ 二转
                a.self.rank = static_cast<Rank>(r);
            }
            //  蛊师与教习修为略高，合乎情理
            if (job.t == Trade::GuMaster || job.t == Trade::Teacher) {
                int r = static_cast<int>(a.self.rank);
                if (r < 3) r = 3;
                a.self.rank = static_cast<Rank>(r);
            }

            a.self.bornDomain = st.domain;
            a.self.isPlayer   = false;
            a.self.alive      = true;

            //
            //  凡夫无空窍，故无真元、无蛊虫 ——
            //  不是「真元为零」，是根本没有这一项。
            //  这与「蛊师真元耗尽」是两回事，界面上也要分开说。
            //
            if (a.self.rank == Rank::R0) {
                a.self.maxEssence = 0.0;
                a.self.essence    = 0.0;
                a.self.carriedGu.clear();
            } else {
                a.self.maxEssence = 60.0 + 40.0 * static_cast<double>(a.self.rank);
                a.self.essence    = a.self.maxEssence;
            }
            a.self.location.domain = st.domain;
            a.self.location.region = st.name;
            a.faction = (job.t == Trade::Rogue) ? FactionId::Neutral
                                                : FactionId::Merchant;
            a.intent  = NpcIntent::Idle;

            out.push_back(std::move(a));
        }
    }
    return out;
}

// ===========================================================================
//  职业 → 互动
// ===========================================================================
bool tradeOffersGuAdvice(Trade t) {
    return t == Trade::GuMaster || t == Trade::Teacher || t == Trade::Elder;
}

bool tradeOffersTrade(Trade t) {
    switch (t) {
        case Trade::Merchant:
        case Trade::Hunter:
        case Trade::Miner:
        case Trade::Craftsman:
        case Trade::Herder:
        case Trade::Farmer:
            return true;
        default:
            return false;
    }
}

// ===========================================================================
//  普通人的社交画像
//
//  凡俗之人不必逐一手工设定，按职业推出即可：
//  门槛一律一转 —— 同是世间人，没有鸿沟。
// ===========================================================================
NpcSocial socialForCommoner(const NpcAgent& a) {
    NpcSocial s;
    s.npcId = a.self.id;
    s.minRankToApproach = Rank::R1;     // 同是凡俗，无鸿沟
    s.inscrutable = false;
    s.offers.push_back(NpcAction::Talk);

    //  职业决定其余互动
    //  occupation 是中文职业名，反查 Trade 以便判定
    Trade t = Trade::Rogue;
    int jobN = 0;
    const JobDef* jobs = jobPool(a.self.bornDomain, jobN);
    for (int i = 0; i < jobN; ++i)
        if (a.occupation == jobs[i].name) { t = jobs[i].t; break; }

    //
    //  凡夫（未开窍）所能做的有限 ——
    //  他连空窍都没有，谈何传授手法；与人相斗更是送死。
    //
    //  这不是「难度高」，是根本不成立：
    //  无真元则催不动蛊，无蛊则无以为战。
    //  故凡夫只可交谈、交易、打探。
    //
    const bool mortal = (a.self.rank == Rank::R0);

    if (tradeOffersTrade(t)) s.offers.push_back(NpcAction::Trade);
    if (tradeOffersGuAdvice(t) && !mortal) s.offers.push_back(NpcAction::Learn);
    s.offers.push_back(NpcAction::Inquire);
    if (!mortal) s.offers.push_back(NpcAction::Duel);

    s.tradesGu    = tradeOffersTrade(t) && !mortal;   // 凡夫无蛊可易
    s.tradesItems = tradeOffersTrade(t);              // 土产仍可买卖

    //  蛊师与教习可传手法（但只是最粗浅的一两种）
    if (tradeOffersGuAdvice(t) && !mortal) s.teachTechniques.push_back(1);

    s.canon  = false;
    s.source = "工程生成的凡俗众生（非原著具名角色）";

    //  情境化回应：按职业分
    switch (t) {
        case Trade::Merchant:
            s.greet = "「客官要点什么？」";
            s.brush = "「小本生意，恕不赊账。」";
            break;
        case Trade::Hunter:
            s.greet = "「山里野兽多，独自一人可要当心。」";
            s.brush = "「我还要赶着下网，改日再聊。」";
            break;
        case Trade::Herder:
            s.greet = "「草原上的风，今日有些紧。」";
            s.brush = "「牛羊还没归栏呢。」";
            break;
        case Trade::Miner:
            s.greet = "「矿里的活计苦，却也安稳。」";
            s.brush = "「歇工了，不想说话。」";
            break;
        case Trade::GuMaster:
        case Trade::Teacher:
            s.greet = "「炼蛊之事，急不得。」";
            s.brush = "「修行要紧，恕不奉陪。」";
            break;
        case Trade::Elder:
            s.greet = "「坐下罢，老朽给你说段旧事。」";
            s.brush = "「老了，记不清了。」";
            break;
        case Trade::Guard:
            s.greet = "「站住。此地不是外人随意走动的。」";
            s.brush = "「当值之时，不便闲谈。」";
            break;
        case Trade::Craftsman:
            s.greet = "「手艺活，一分价钱一分货。」";
            s.brush = "「手上忙着呢。」";
            break;
        case Trade::Farmer:
            s.greet = "「今年的收成，还算过得去。」";
            s.brush = "「地里还有活。」";
            break;
        default:
            s.greet = "「你也是赶路的？」";
            s.brush = "「各自赶路罢。」";
            break;
    }
    s.hostile   = "「你我并无过节，别自找麻烦。」";
    s.invisible = "此人不在左近。";
    return s;
}

} // namespace gr
