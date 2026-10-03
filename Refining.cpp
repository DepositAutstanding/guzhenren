// 炼蛊系统实现：蛊方前置铁律 + 仙蛊唯一 + 道境门槛
#include "gr/gu/Refining.hpp"

#include <algorithm>
#include <cmath>

namespace gr {

void Refinery::addRecipe(const GuRecipe& r) { recipes_.push_back(r); }
void Refinery::addTemplate(const GuTemplate& t) { templates_.push_back(t); }

const GuRecipe* Refinery::recipe(RecipeId id) const {
    for (const auto& r : recipes_) if (r.id == id) return &r;
    return nullptr;
}

const GuTemplate* Refinery::guTemplate(GuId id) const {
    for (const auto& t : templates_) if (t.id == id) return &t;
    return nullptr;
}

const GuTemplate* Refinery::templateByName(const std::string& n) const {
    for (const auto& t : templates_) if (t.name == n) return &t;
    return nullptr;
}

std::vector<const GuRecipe*> Refinery::recipesFor(const std::string& guName) const {
    std::vector<const GuRecipe*> v;
    for (const auto& r : recipes_) if (r.name == guName) v.push_back(&r);
    return v;
}

RefineResult Refinery::refine(const RefineRequest& req, ImmortalGuRegistry& registry) {
    RefineResult res;

    // ---------------------------------------------------------------
    // 铁律 1：无蛊方 → 绝对无法炼制、无法推演、无法瞎炼
    // ---------------------------------------------------------------
    const GuRecipe* rec = recipe(req.recipeId);
    if (!rec) {
        res.outcome = RefineOutcome::Blocked;
        res.err     = Err::RecipeMissing;
        res.detail  = "无对应蛊方，绝对无法炼制、无法推演、无法瞎炼";
        return res;
    }
    res.guName = rec->name;

    // 无据推演（Speculative）等同无效蛊方
    if (rec->integrity == RecipeIntegrity::Speculative) {
        res.outcome = RefineOutcome::Blocked;
        res.err     = Err::RecipeMissing;
        res.detail  = "该配方属无据推演，不视为蛊方，绝对无法炼制";
        return res;
    }

    // ---------------------------------------------------------------
    // 材料校验
    // ---------------------------------------------------------------
    for (const auto& need : rec->materials) {
        double have = 0.0;
        for (const auto& inv : req.inventory)
            if (inv.name == need.name) have += inv.amount;
        if (have + 1e-9 < need.amount) {
            res.outcome = RefineOutcome::Blocked;
            res.err     = Err::MaterialMissing;
            res.detail  = "材料不齐：缺 " + need.name + "（需 " +
                          std::to_string(need.amount) + "，有 " + std::to_string(have) + "）";
            return res;
        }
    }
    // 合炼公式所需的既有蛊
    for (GuId needGu : rec->componentGu) {
        bool have = false;
        for (const auto& g : req.componentGu)
            if (g.templateId == needGu && g.usable()) { have = true; break; }
        if (!have) {
            res.outcome = RefineOutcome::Blocked;
            res.err     = Err::MaterialMissing;
            res.detail  = "合炼组件不齐：缺蛊 id=" + std::to_string(needGu);
            return res;
        }
    }

    // ---------------------------------------------------------------
    // 仙元校验
    // ---------------------------------------------------------------
    if (req.refinerEssence < rec->requiredEssence) {
        res.outcome = RefineOutcome::Blocked;
        res.err     = Err::InsufficientEssence;
        res.detail  = "仙元不足：需 " + std::to_string(rec->requiredEssence);
        return res;
    }

    // ---------------------------------------------------------------
    // 铁律 2：残缺蛊方 → 只能炼残缺品、残次蛊、失败品
    // ---------------------------------------------------------------
    const bool incomplete = (rec->integrity == RecipeIntegrity::Incomplete);
    const bool daoShort   = !dao_meets(req.refinerDao, rec->requiredDao);

    // 完整蛊方 + 齐全材料 + 炼道境界 → 可正常炼制；任一缺失则降级
    double successRate;
    if (incomplete)      successRate = 0.15;                    // 残缺方：多为残次/失败
    else if (daoShort)   successRate = 0.40;                    // 道境不足：勉强
    else                 successRate = 0.92;                    // 三者齐备

    const GuTemplate* tpl = templateByName(rec->name);
    const Rank target = tpl ? tpl->rank : rec->targetRank;

    // ---------------------------------------------------------------
    // 铁律 4：仙蛊唯一 —— 六转及以上需在注册表占位
    // ---------------------------------------------------------------
    const bool isImmortal = is_immortal_rank(target);
    GuSpecial special = tpl ? tpl->special : GuSpecial::None;

    if (isImmortal && special == GuSpecial::None && registry.isNameOccupied(rec->name)) {
        res.outcome = RefineOutcome::Blocked;
        res.err     = Err::UniqueGuViolation;
        res.detail  = "仙蛊唯一：「" + rec->name + "」世间已存在，不可再炼第二只";
        if (req.refinerEssence >= rec->requiredEssence)
            res.essenceSpent = rec->requiredEssence * 0.1;  // 尝试本身仍有微量损耗
        return res;
    }

    // ---------------------------------------------------------------
    // 结果判定
    // ---------------------------------------------------------------
    res.essenceSpent = rec->requiredEssence;

    if (req.rngRoll >= successRate) {
        res.outcome = RefineOutcome::Failure;
        res.err     = Err::Ok;
        res.detail  = "炼制失败（成功率 " + std::to_string(successRate * 100) + "%）";
        return res;
    }

    GuInstance inst;
    inst.templateId = tpl ? tpl->id : 0;
    inst.instanceId = allocInstanceId();
    inst.state      = GuState::Active;
    inst.holder     = req.refinerId;
    inst.refinementLevel = 0;

    if (incomplete || daoShort) {
        inst.defective = true;
        inst.integrity = incomplete ? 0.45 : 0.75;
        res.outcome    = RefineOutcome::Defective;
        res.detail     = incomplete
            ? "残缺蛊方 → 只炼出残次蛊/残缺品（完整度 " + std::to_string(inst.integrity) + "）"
            : "炼道境界不足 → 残次品（完整度 " + std::to_string(inst.integrity) + "）";
    } else {
        inst.integrity = 1.0;
        res.outcome    = RefineOutcome::Success;
        res.detail     = "完整蛊方 + 齐全材料 + 炼道境界 → 正常炼制成功";
    }

    // 仙蛊入注册表（NPC 与玩家完全平等）
    if (isImmortal) {
        auto reg = registry.registerGu(rec->name, target, req.refinerId,
                                       special, inst.defective);
        if (!reg.ok()) {
            res.outcome = RefineOutcome::Blocked;
            res.err     = reg.err;
            res.detail  = reg.detail;
            return res;
        }
        res.registryId = reg.value;
    }

    res.product = inst;
    return res;
}

// ---------------------------------------------------------------------------
//  原著 / 资料库可核验蛊虫模板骨架样本
//  （资料库收录：凡蛊 450、仙蛊 362、仙蛊屋 44；此处按机制需要落地代表条目）
// ---------------------------------------------------------------------------
std::vector<GuTemplate> build_canon_gu_templates() {
    std::vector<GuTemplate> v;
    auto add = [&v](GuTemplate g) { v.push_back(std::move(g)); };

    // ---------------- 凡蛊（一至五转，同种可大量存在） ----------------
    { GuTemplate g; g.id=1;  g.name="酒虫";   g.rank=Rank::R1; g.category=GuCategory::Support;
      g.dao=Dao::Food; g.effect="凌空飞行；精炼一转真元，提升一个小境界；温养空窍无后遗症";
      g.feed="酒水（一坛青竹酒约维持4日；浊酒、米酒亦可）";
      g.source="凡蛊总表：一转 辅助·消耗；食道为工程推定（依酒虫系列四味酒虫/七香酒虫归属，资料库类别字段未标流派）"; add(g); }
    { GuTemplate g; g.id=2;  g.name="月光蛊"; g.rank=Rank::R1; g.category=GuCategory::Attack;
      g.dao=Dao::Light; g.effect="凝成巴掌大幽蓝月刃，射程十米";
      g.feed="月兰花瓣，每日早晚各2片；知心草可减少消耗";
      g.source="凡蛊总表：一转 攻击·光道；古月一族秘法培育"; add(g); }
    { GuTemplate g; g.id=3;  g.name="月芒蛊"; g.rank=Rank::R2; g.category=GuCategory::Attack;
      g.dao=Dao::Light; g.effect="月光水刃，攻击力约月光蛊三倍"; g.feed="月兰花瓣";
      g.source="凡蛊总表：二转 攻击·光道；月光蛊＋小光蛊×2→月芒蛊"; add(g); }
    { GuTemplate g; g.id=4;  g.name="天元宝莲蛊"; g.rank=Rank::R3; g.category=GuCategory::Support;
      g.dao=Dao::Wood; g.effect="号称移动元泉，每日约产五十枚元石及大量真元；有潜力合炼至六转";
      g.source="凡蛊总表：三转 辅助·元泉（升炼路径 3→4→5转，可合炼至六转仙蛊）；木道为工程推定（因九转天元宝皇莲属木道，资料库类别字段未标流派）"; add(g); }
    { GuTemplate g; g.id=5;  g.name="白象元力蛊"; g.rank=Rank::R2; g.category=GuCategory::Support;
      g.dao=Dao::Strength; g.effect="形成白象虚影，增幅力气；可晋升三转";
      g.source="凡蛊总表：二转 辅助·力气（升炼路径 2→3→4→5转，此处取基础形态）"; add(g); }
    { GuTemplate g; g.id=6;  g.name="胆识蛊";   g.rank=Rank::R3; g.category=GuCategory::Consumable;
      g.dao=Dao::Soul; g.effect="壮人魂魄，藏于胆石，敲碎后飞出，只存在一瞬";
      g.source="凡蛊总表：三转 消耗·魂道"; add(g); }
    { GuTemplate g; g.id=7;  g.name="骨竹蛊";   g.rank=Rank::R1; g.category=GuCategory::Logistics;
      g.dao=Dao::Bone; g.effect="配合鬼火修复战骨车轮"; g.feed="白骨";
      g.source="凡蛊总表：一转 后勤·骨道（升炼路径 1→5转）"; add(g); }
    { GuTemplate g; g.id=8;  g.name="地藏花蛊"; g.rank=Rank::R2; g.category=GuCategory::Storage;
      g.dao=Dao::Wood; g.effect="将蛊虫包养于花心，模拟封印使其沉眠；可晋升地藏花王蛊";
      g.source="凡蛊总表：二转 后勤·存储（可晋升地藏花王蛊；升炼路径 2→3转）；木道为工程推定（资料库类别字段仅标「后勤·存储」，未标流派）"; add(g); }
    { GuTemplate g; g.id=9;  g.name="小光蛊";   g.rank=Rank::R1; g.category=GuCategory::Support;
      g.dao=Dao::Light; g.effect="增幅月光蛊攻击，单体增幅不叠加";
      g.source="凡蛊总表：一转 辅助·光道"; add(g); }
    { GuTemplate g; g.id=10; g.name="百战不殆蛊"; g.rank=Rank::R5; g.category=GuCategory::Consumable;
      g.dao=Dao::Refine; g.effect="信王传承获得；定仙游合炼组件之一";
      g.source="凡蛊总表：五转 消耗·炼道"; add(g); }
    { GuTemplate g; g.id=11; g.name="三更蛊";   g.rank=Rank::R5; g.category=GuCategory::Consumable;
      g.dao=Dao::Time; g.effect="定仙游合炼组件之一（需两只）";
      g.source="凡蛊总表：五转 消耗·宙道"; add(g); }

    // ---------------- 杀招组成蛊虫（资料库凡蛊总表已收录者） ----------------
    // 三心合魂为三转杀招，其组成蛊取基础形态三转 —— 取四转会与杀招转数自相矛盾
    { GuTemplate g; g.id=12; g.name="飞魂蛊"; g.rank=Rank::R3; g.category=GuCategory::Support;
      g.dao=Dao::Soul; g.effect="魂魄离体；三心合魂组成蛊之一";
      g.source="凡蛊总表：三转 魂道·辅助（升炼路径 3→4转，此处取基础形态）"; add(g); }
    { GuTemplate g; g.id=13; g.name="魂链蛊"; g.rank=Rank::R3; g.category=GuCategory::Support;
      g.dao=Dao::Soul; g.effect="锁链勾连魂魄；三心合魂组成蛊之一";
      g.source="凡蛊总表：三转 魂道·辅助（升炼路径 3→4转，此处取基础形态）"; add(g); }
    { GuTemplate g; g.id=14; g.name="剑影蛊"; g.rank=Rank::R2; g.category=GuCategory::Attack;
      g.dao=Dao::Sword; g.effect="二转剑道攻击蛊";
      g.source="凡蛊总表：二转 攻击·剑道（升炼路径 2→3→4→5转，方源由赌石获得）"; add(g); }
    { GuTemplate g; g.id=15; g.name="水箭蛊"; g.rank=Rank::R1; g.category=GuCategory::Attack;
      g.dao=Dao::Water; g.effect="发射水箭";
      g.source="凡蛊总表：一转 攻击·水道"; add(g); }
    { GuTemplate g; g.id=16; g.name="水罩蛊"; g.rank=Rank::R2; g.category=GuCategory::Defense;
      g.dao=Dao::Water; g.effect="球形水流卸力，防御优于白玉";
      g.source="凡蛊总表：二转 防御·水道（升炼路径 2→4→5转）"; add(g); }
    { GuTemplate g; g.id=17; g.name="全力以赴蛊"; g.rank=Rank::R3; g.category=GuCategory::Support;
      g.dao=Dao::Strength; g.effect="上古绝迹蛊，百分百催发兽力虚影；万我仙蛊（杀招固化）组成蛊之一";
      g.source="凡蛊总表：三转 辅助·兽力（升炼路径 3→4→5转，此处取基础形态；"
               "流派归力道，兽力虚影流为其分支）"; add(g); }

    // ---------------- 仙蛊（六转及以上，同名世间唯一） ----------------
    { GuTemplate g; g.id=101; g.name="定仙游"; g.rank=Rank::R6; g.category=GuCategory::Movement;
      g.dao=Dao::Space; g.effect="空间挪移，仅可前往亲眼见过/抵达过/感知过的坐标；后升炼至七转";
      g.source="仙蛊总表：六转（后升七转，当前按六转主行） 宇道"; add(g); }
    // 春秋蝉：资料库主行「七转（初载六转）」。
    // 同表另有「六转春秋蝉（方源本体）」「春秋蝉六转重复抑制行」两行，
    // 均标注「与七转同一本体，不计入总数」—— 即原著初登场为六转，最终设定为七转。
    // 资料库口径规则：「仙蛊按最终设定状态记录」，故此处取七转。
    // 游戏开局（第六卷）方源持有的是该蛊本体，转数随其升炼而变，非二蛊并存。
    { GuTemplate g; g.id=102; g.name="春秋蝉"; g.rank=Rank::R7; g.category=GuCategory::Support;
      g.dao=Dao::Time; g.effect="献祭整个身躯与全部修为，借光阴长河逆流回到过去；"
                               "令宿主气运持续衰落，有失败风险；奇蛊榜第3（原第7，义天山大战后升）";
      g.source="仙蛊总表：七转（初载六转） 宙道；【红莲魔尊】【古月方源】；"
               "奇蛊榜第3（炼制者红莲魔尊）"; add(g); }
    { GuTemplate g; g.id=103; g.name="智慧蛊"; g.rank=Rank::R9; g.category=GuCategory::Support;
      g.dao=Dao::Wisdom; g.effect="辅助推演，增幅智道杀招";
      g.source="仙蛊总表：九转 智道"; add(g); }
    { GuTemplate g; g.id=104; g.name="至尊仙胎蛊"; g.rank=Rank::R9; g.category=GuCategory::Support;
      g.dao=Dao::Refine; g.effect="铸至尊仙胎体，可无流派壁垒地吞并他窍（方源第五卷所得）";
      g.source="第六卷卷初研究报告（资料库未收录，转数依研究报告「九转至尊仙胎蛊」）"; add(g); }
    { GuTemplate g; g.id=105; g.name="宿命蛊"; g.rank=Rank::R9; g.category=GuCategory::Support;
      g.dao=Dao::Heaven; g.effect="锚定命轨；第五卷末已被摧毁，名额待重生";
      g.source="仙蛊总表：九转 天道"; add(g); }
    { GuTemplate g; g.id=106; g.name="偷生蛊"; g.rank=Rank::R8; g.category=GuCategory::Support;
      g.dao=Dao::Thief; g.effect="偷取生机，盗天传承相关";
      g.source="仙蛊总表：八转 偷道/魂道（待考）；与「神不知」配对的盗天真传蛊"; add(g); }
    { GuTemplate g; g.id=107; g.name="天元宝皇莲"; g.rank=Rank::R9; g.category=GuCategory::Support;
      g.dao=Dao::Wood; g.effect="移动元泉的仙蛊形态，由天元宝莲逐级合炼升炼而成";
      g.source="仙蛊总表：九转 木道"; add(g); }
    { GuTemplate g; g.id=108; g.name="神游蛊"; g.rank=Rank::R6; g.category=GuCategory::Movement;
      g.dao=Dao::Space; g.effect="人饮四种极品美酒后自然凝结；定仙游合炼核心组件";
      g.source="仙蛊总表：六转 宇道"; add(g); }
    // 仿伪蛊：仙蛊唯一体系的唯二特例之一。
    // 将世间任一其他仙蛊与此蛊接触一段时间后，催动便能令此蛊转变成相应仙蛊 ——
    // 「稍稍突破仙蛊唯一的常规」，但仿制出的蛊威能不及正品。
    // 巨阳仙尊据盗天真传炼制，现由巨阳仙尊掌握。
    //
    // 【已更正】此前误注「奇蛊榜第八」。资料库奇蛊榜明确记载：
    //   第 1、2、8、9 名「原著未获可核验的明确对应名称与功能」，
    //   第 7 名为通心蛊（星宿仙尊）、第 3 名为春秋蝉（红莲魔尊）。
    // 仿伪蛊并不在资料库奇蛊榜的可核验条目中，故删除排名表述，不作补全。
    { GuTemplate g; g.id=109; g.name="仿伪蛊"; g.rank=Rank::R8;
      g.category=GuCategory::Support; g.dao=Dao::Thief;
      g.effect="接触任一仙蛊后可仿制转变之，稍稍突破「仙蛊唯一」常规；仿制品威能不及正品";
      g.special=GuSpecial::FakeGu;
      g.source="仙蛊总表：八转 偷道·仿制；【盗天魔尊原创】【巨阳仙尊】现掌握；"
               "（奇蛊榜排名：资料库第8名无可核验名称，未反向补名）"; add(g); }

    // ---------------- 盗天真传体系（盗天魔尊传承） ----------------
    // 研究报告 4.3／资料库：盗天魔尊主修偷道、护道宇道；
    // 疯魔窟为其真传所在（赌蛊条：疯魔窟盗天真传核心蛊之一）。
    { GuTemplate g; g.id=110; g.name="大盗蛊"; g.rank=Rank::R7; g.category=GuCategory::Support;
      g.dao=Dao::Thief; g.effect="房家偷道真传核心蛊，宽头独角甲虫；真正运用盗天真传的关键，方源据其构建杀招「大盗鬼手」";
      g.source="仙蛊总表：七转 偷道·核心；【盗天魔尊】原属西漠房家，后方源所得"; add(g); }
    { GuTemplate g; g.id=111; g.name="态度蛊"; g.rank=Rank::R8; g.category=GuCategory::Support;
      g.dao=Dao::Transform; g.effect="改变他人「心中所见」，非单纯幻象；「见面曾相识」核心蛊；消耗心力而非必然消耗仙元";
      g.source="仙蛊总表：八转 变化道；【盗天魔尊真传】【古月方源】"; add(g); }
    // 转数待考：资料库明示「转数待考」，此前硬编八转属臆造，已改为占位并标记。
    // rankConfirmed=false 时，转数不得参与强弱比较（资料库「不编造原则」）。
    { GuTemplate g; g.id=112; g.name="赌蛊"; g.rank=Rank::R8; g.category=GuCategory::Support;
      g.dao=Dao::Thief; g.effect="疯魔窟盗天真传核心蛊之一；削取天外偷道道痕炼制，结果完全随机；"
                                 "关联仙道杀招「赌运」，赢家掠夺输家运道气场（转数原著待考）";
      g.rankConfirmed=false;
      g.source="仙蛊总表：转数待考 偷道·运道风格；【盗天魔尊】疯魔窟真传；"
               "rank 为占位值，非原著确认转数"; add(g); }
    // 电脑蛊：钢铁侠真传核心。资料库明确「彭达原持有」——
    // 这是「新天外之魔」彭达线与盗天传承的直接关联。
    // 转数待考：资料库明示「转数待考」，此前硬编八转属臆造，已改为占位并标记。
    { GuTemplate g; g.id=113; g.name="电脑蛊"; g.rank=Rank::R8; g.category=GuCategory::Support;
      g.dao=Dao::Wisdom; g.effect="盗天依天外机甲世界记忆所创；操控机甲、承载复杂杀招运算，"
                                  "参与「金红战甲」后可将心力念头消耗减少大半，维持时间延长至少一倍（转数原著待考）";
      g.rankConfirmed=false;
      g.source="仙蛊总表：转数待考 智道·科技风格；【盗天魔尊】钢铁侠真传；彭达原持有；"
               "rank 为占位值，非原著确认转数"; add(g); }

    // ---------------- 仙蛊屋（资料库「仙蛊屋」表） ----------------
    { GuTemplate g; g.id=201; g.name="监天塔"; g.rank=Rank::R9; g.category=GuCategory::House;
      g.dao=Dao::Heaven; g.effect="天庭九转仙蛊屋（第五卷末崩碎，名额待重生）";
      g.source="仙蛊屋：天道 九转"; add(g); }
    { GuTemplate g; g.id=202; g.name="近水楼台"; g.rank=Rank::R7; g.category=GuCategory::House;
      g.dao=Dao::Water; g.effect="七转水道仙蛊屋";
      g.source="仙蛊屋：水道 七转"; add(g); }
    { GuTemplate g; g.id=203; g.name="龙宫"; g.rank=Rank::R8; g.category=GuCategory::House;
      g.dao=Dao::Enslave; g.effect="八转奴道仙蛊屋";
      g.source="仙蛊屋：奴道 八转"; add(g); }
    { GuTemplate g; g.id=204; g.name="镇运天宫"; g.rank=Rank::R8; g.category=GuCategory::House;
      g.dao=Dao::Luck; g.effect="八转运道仙蛊屋（巨阳一系）";
      g.source="仙蛊屋：运道 八转"; add(g); }

    // ------------------------------------------------------------------
    //  工程原创蛊虫（原著未记载，依原著设定推导）
    //
    //  为什么要加这些：原著对「探索、赶路、环境适应」一类日常需求着墨极少，
    //  资料库也相应留白；但本项目有二维地图与跨域玩法，缺了这些环节就不闭环。
    //  故依原著已确立的设定（五域环境、界壁、海底潜流、两天、定仙游的认知锚定等）
    //  推导出以下蛊虫，填补玩法空白。
    //
    //  纪律：original=true，source 写明推导依据；转数多为占位
    //  （rankConfirmed=false），不得参与强弱比较。完整设计说明见
    //  docs/原创蛊虫设计.md —— 与 canon 条目严格区分，不可混同。
    // ------------------------------------------------------------------
    { GuTemplate g; g.id=301; g.name="望气蛊"; g.rank=Rank::R1; g.category=GuCategory::Support;
      g.dao=Dao::Qi; g.effect="察元气浓淡，指出附近灵脉与三气失衡之兆";
      g.feed="晨露三滴";
      g.original=true; g.rankConfirmed=false;
      g.source="工程原创：依「五域元气分布不均、中洲元气最充裕」推导；"
               "转数占位，不参与强弱比较"; add(g); }
    { GuTemplate g; g.id=302; g.name="探脉蛊"; g.rank=Rank::R2; g.category=GuCategory::Support;
      g.dao=Dao::Earth; g.effect="探地脉灵脉走向，于图上标出灵脉节点";
      g.feed="灵土一撮";
      g.original=true; g.rankConfirmed=false;
      g.source="工程原创：依「地脉/灵脉扰动、灵缘斋等地以灵脉立派」推导"; add(g); }
    { GuTemplate g; g.id=303; g.name="记游蛊"; g.rank=Rank::R3; g.category=GuCategory::Support;
      g.dao=Dao::Space; g.effect="记下所经路线与沿途印象，供日后按印象重返";
      g.feed="路旁尘土";
      g.original=true; g.rankConfirmed=false;
      g.source="工程原创：依定仙游「须对地点有具体印象」的认知锚定机制推导 —— "
               "印象需先记录，故有此蛊"; add(g); }
    { GuTemplate g; g.id=304; g.name="渡壁蛊"; g.rank=Rank::R5; g.category=GuCategory::Movement;
      g.dao=Dao::Space; g.effect="短暂削弱界壁的斥力，减轻跨域阻力（不消除，仅降低）";
      g.feed="域界交界处的壁土";
      g.original=true; g.rankConfirmed=false;
      g.source="工程原创：依「界壁对本域引力、外域斥力，层次越高阻碍越强」推导；"
               "只减不消，以免架空跨域成本"; add(g); }
    { GuTemplate g; g.id=305; g.name="潜流蛊"; g.rank=Rank::R4; g.category=GuCategory::Movement;
      g.dao=Dao::Water; g.effect="辨海底潜流走向，借潜流作天然航路快速跨海";
      g.feed="深海水草";
      g.original=true; g.rankConfirmed=false;
      g.source="工程原创：依「东海海底潜流被商人用作天然航路，可跨数万至数十万里」推导"; add(g); }
    { GuTemplate g; g.id=306; g.name="避瘴蛊"; g.rank=Rank::R3; g.category=GuCategory::Defense;
      g.dao=Dao::Poison; g.effect="屏息避瘴，短时免于瘴气侵蚀";
      g.feed="瘴气林腐叶";
      g.original=true; g.rankConfirmed=false;
      g.source="工程原创：依「南疆瘴气、凡人家族依山建寨以避瘴兽潮」推导"; add(g); }
    { GuTemplate g; g.id=307; g.name="御寒蛊"; g.rank=Rank::R2; g.category=GuCategory::Defense;
      g.dao=Dao::Ice; g.effect="抵御风雪严寒，于冰原维持体温";
      g.feed="冰原苔藓";
      g.original=true; g.rankConfirmed=false;
      g.source="工程原创：依「北原苍莽草地、大风、冰原及周期性超级大风雪」推导"; add(g); }
    { GuTemplate g; g.id=308; g.name="沙行蛊"; g.rank=Rank::R3; g.category=GuCategory::Movement;
      g.dao=Dao::Earth; g.effect="于流沙上行走不陷，辨识沙丘移动轨迹";
      g.feed="细沙一捧";
      g.original=true; g.rankConfirmed=false;
      g.source="工程原创：依「西漠数十万里规模迁移的沙丘『怎渡丘』」推导"; add(g); }
    { GuTemplate g; g.id=309; g.name="观天蛊"; g.rank=Rank::R4; g.category=GuCategory::Support;
      g.dao=Dao::Star; g.effect="观测两天轮替与天罡气墙盛衰，预告气墙裂缝开启之机";
      g.feed="高空流风";
      g.original=true; g.rankConfirmed=false;
      g.source="工程原创：依「黑白两天轮替形成五域昼夜、天罡气墙为入天通路」推导"; add(g); }
    // 定星蛊 / 碧空蛊：定仙游合炼链的另两个组件。
    // 【转数须为五转凡蛊，不可设六转】—— 资料库「仙蛊总表·定仙游」的炼制方法
    // 明确标注「碧空蛊(五转)、定星蛊(五转)」。若设成六转，就成了仙蛊，
    // 既与资料库冲突，也会被仙蛊唯一体系收编（仙蛊不该当合炼耗材）。
    { GuTemplate g; g.id=310; g.name="定星蛊"; g.rank=Rank::R5; g.category=GuCategory::Support;
      g.dao=Dao::Star; g.effect="锁定方位，为定仙游合炼提供坐标基准";
      //  【非工程原创】资料库「仙蛊总表」明载此蛊（五转，星道待考），
      //  故 original=false、rankConfirmed=true —— 此前误标为原创兼待考。
      g.original=false; g.rankConfirmed=true;
      g.source="仙蛊总表：五转（用于定仙游合炼），星道（待考）；"
               "为定仙游合炼组件"; add(g); }
    { GuTemplate g; g.id=311; g.name="碧空蛊"; g.rank=Rank::R5; g.category=GuCategory::Support;
      g.dao=Dao::Space; g.effect="借太古荣耀之光定位空域，为定仙游合炼组件";
      //  【非工程原创】资料库「仙蛊总表」明载此蛊（五转，宇道），
      //  另有凡蛊总表四转/五转「消耗·毒道」条目 —— 此处取仙蛊总表的合炼组件口径。
      g.original=false; g.rankConfirmed=true;
      g.source="仙蛊总表：五转（用于定仙游合炼），宇道；"
               "凡蛊总表另有四转/五转「消耗·毒道」条目"; add(g); }

    return v;
}

// ---------------------------------------------------------------------------
//  原著 / 资料库可核验蛊方样例
//  合炼公式取自资料库「炼制方法（合炼公式）」列
// ---------------------------------------------------------------------------
std::vector<GuRecipe> build_canon_recipes(const std::vector<GuTemplate>& tpls) {
    std::vector<GuRecipe> v;
    auto add = [&v](GuRecipe r) { v.push_back(std::move(r)); };
    auto idOf = [&tpls](const std::string& n) -> GuId {
        for (const auto& t : tpls) if (t.name == n) return t.id;
        return 0;
    };

    // ---- 凡蛊：合炼公式照抄资料库「炼制方法（合炼公式）」列 ----

    // 酒虫链：2酒虫＋酸甜苦辣四味美酒→四味酒虫（资料库：一转酒虫 合炼公式）
    { GuRecipe r; r.id=1; r.name="四味酒虫"; r.targetRank=Rank::R2; r.dao=Dao::Food;
      r.integrity=RecipeIntegrity::Complete;
      r.componentGu={idOf("酒虫"), idOf("酒虫")};
      r.materials={{"酸甜苦辣四味美酒", 1}};
      r.requiredDao=DaoLevel::Entry; r.requiredEssence=5;
      r.source="凡蛊总表·酒虫：2酒虫＋酸甜苦辣四味美酒→四味酒虫"; add(r); }

    // 月光链：月光蛊＋小光蛊×2→月芒蛊（资料库：二转月芒蛊 合炼公式）
    { GuRecipe r; r.id=2; r.name="月芒蛊"; r.targetRank=Rank::R2; r.dao=Dao::Light;
      r.integrity=RecipeIntegrity::Complete;
      r.componentGu={idOf("月光蛊"), idOf("小光蛊"), idOf("小光蛊")};
      r.requiredDao=DaoLevel::Entry; r.requiredEssence=8;
      r.source="凡蛊总表·月芒蛊：月光蛊＋小光蛊×2→月芒蛊"; add(r); }

    // 月痕蛊：核心合炼树「月光蛊→月芒/月痕/月旋」，材料栏为「痕石蛊」。
    // 资料库只给出「痕石蛊」这一组件名而未列完整公式 —— 判为残缺蛊方，只出残次品。
    { GuRecipe r; r.id=3; r.name="月痕蛊"; r.targetRank=Rank::R2; r.dao=Dao::Light;
      r.integrity=RecipeIntegrity::Incomplete;
      r.componentGu={idOf("月光蛊")};
      r.materials={{"痕石蛊", 1}};
      r.requiredDao=DaoLevel::Entry; r.requiredEssence=8;
      r.source="核心合炼树·月光链（仅列「痕石蛊」，完整公式未披露 → 残缺蛊方）"; add(r); }

    // ---- 仙蛊：炼制方法照抄资料库「仙蛊总表·炼制方法」列 ----

    // 定仙游（六转宇道）：神游蛊、碧空蛊、定星蛊 借太古荣耀之光合炼；
    // 另用两只三更蛊、一只百战不殆蛊。
    // 其中 神游蛊(六转宇道)、三更蛊(五转消耗·宙道)、百战不殆蛊(五转消耗·炼道) 已建模板；
    // 碧空蛊(五转)、定星蛊(五转) 亦已建模板（id 311/310，工程原创，转数依本条标注）；
    // 太古荣耀之光 非蛊，仍记于材料。
    { GuRecipe r; r.id=101; r.name="定仙游"; r.targetRank=Rank::R6; r.dao=Dao::Space;
      r.integrity=RecipeIntegrity::Complete;
      r.componentGu={idOf("神游蛊"), idOf("三更蛊"), idOf("三更蛊"),
                     idOf("百战不殆蛊")};
      r.materials={{"碧空蛊", 1}, {"定星蛊", 1}, {"太古荣耀之光", 1}};
      r.requiredDao=DaoLevel::Great; r.requiredEssence=500;
      r.source="仙蛊总表·定仙游：神游蛊、碧空蛊、定星蛊借太古荣耀之光合炼；另用两只三更蛊、一只百战不殆蛊";
      add(r); }

    // 春秋蝉（六转宙道）：资料库炼制方法二栏均为「不详」；
    // 但「蛊材通用规则」表载明其炼制涉及荒兽血、百花凝液、仙僵肉身（A级），
    // 且「每一种花基本照应一种处理方法」—— 工序繁复，判为残缺蛊方。
    { GuRecipe r; r.id=102; r.name="春秋蝉"; r.targetRank=Rank::R6; r.dao=Dao::Time;
      r.integrity=RecipeIntegrity::Incomplete;
      r.materials={{"荒兽血", 1}, {"百花凝液", 1}, {"仙僵肉身", 1}};
      r.requiredDao=DaoLevel::Perfection; r.requiredEssence=800;
      r.source="仙蛊总表·六转春秋蝉：炼制方法/材料均「不详」；"
               "蛊材通用规则表载其炼制涉及荒兽血、百花凝液、仙僵肉身【A】→ 判残缺";
      add(r); }

    // 天元宝皇莲（九转木道）：天元宝莲逐级合炼、升炼；材料为废天然元泉等大量仙材
    { GuRecipe r; r.id=103; r.name="天元宝皇莲"; r.targetRank=Rank::R9; r.dao=Dao::Wood;
      r.integrity=RecipeIntegrity::Complete;
      r.componentGu={idOf("天元宝莲蛊")};
      r.materials={{"废天然元泉", 3}, {"木道仙材", 2}};
      r.requiredDao=DaoLevel::Foundation; r.requiredEssence=3000;
      r.source="仙蛊总表·天元宝皇莲（九转 木道）：天元宝莲逐级合炼、升炼；废天然元泉等大量仙材";
      add(r); }

    // 仿伪蛊（八转 偷道·仿制）：资料库载「巨阳仙尊据盗天真传炼制；具体合炼式不详」
    // —— 合炼式未披露，故判为残缺蛊方。
    { GuRecipe r; r.id=104; r.name="仿伪蛊"; r.targetRank=Rank::R8; r.dao=Dao::Thief;
      r.integrity=RecipeIntegrity::Incomplete;
      r.materials={{"偷道仙材", 3}};
      r.requiredDao=DaoLevel::Great; r.requiredEssence=2000;
      r.source="仙蛊总表·仿伪蛊：巨阳仙尊据盗天真传炼制；具体合炼式不详 → 判残缺";
      add(r); }

    // 无据推演：等同无蛊方，绝对无法炼制。
    // 第六卷卷初研究报告明确指出：「十转」「方源最终永生」属 C 类读者推测，非正文事实。
    { GuRecipe r; r.id=999; r.name="十转永生蛊"; r.targetRank=Rank::R9; r.dao=Dao::Heaven;
      r.integrity=RecipeIntegrity::Speculative; r.canon=false;
      r.materials={{"未知材料", 99}};
      r.requiredDao=DaoLevel::MarkFusion; r.requiredEssence=99999;
      r.source="C类推测（非正文事实）：研究报告明示「十转」「最终永生」为读者推演";
      add(r); }

    return v;
}

} // namespace gr
