// ============================================================================
//  《蛊真人》开放世界单机（预留联机）—— 机制演示程序
//  时间基线：第六卷开局（疯魔窟大战结束后）
// ----------------------------------------------------------------------------
//  演示顺序严格对应需求说明书章节：
//    一   全局基础设定（世界架构 / 天罡罡风）
//    二   五域环境 DEBUFF 与本土免疫
//    三   洞天福地分布
//    四   定仙游机制
//    五   三气平衡闭环
//    六   炼蛊铁律
//    七   道境体系
//    八   终章无结局锁死
//    九   联机预留
//    十   天外之魔侦测
//    十一 仙蛊唯一
//    十二 NPC 自主 AI
// ============================================================================
#include "gr/sim/GameWorld.hpp"
#include "gr/ui/GameSession.hpp"
#include "gr/cultivator/DaoTianLegacy.hpp"

#include <iomanip>
#include <iostream>
#include <string>

using namespace gr;

namespace {

//  章节标题：单行即可。原先每节用三行「===」框住再加前后空行，
//  一次演示光是分隔线就占掉近 80 行，反而把实质数据埋没了。
void hr(const std::string& title) {
    std::cout << "\n## " << title << "\n";
}

void sub(const std::string& t) { std::cout << "- " << t << "\n"; }

void show(const Result<void>& r, const std::string& action) {
    std::cout << "  " << (r.ok() ? "[成功] " : "[被拒] ") << action;
    if (!r.detail.empty()) std::cout << "  → " << r.detail;
    std::cout << "\n";
}

} // namespace

int main() {
    std::cout << std::fixed << std::setprecision(2);

    hr("《蛊真人》开放世界 —— 第六卷开局（疯魔窟大战结束后）");

    // =====================================================================
    //  一 / 三：世界空间架构与洞天分布
    // =====================================================================
    hr("一、三、世界空间全新架构 + 洞天福地分布");

    GameWorld gw(20240906);
    gw.buildVolume6Opening(24);

    auto audit = gw.world().audit();
    std::cout << "  世界层级：五域凡界(地表/地下) + 黑天 + 白天 + 世界胎壁\n";
    std::cout << "  界外空域：已彻底删除（全地图收纳于两天五域之内）\n";
    std::cout << "  洞天总数               : " << gw.world().allCaves().size() << "\n";
    std::cout << "  白天异人洞天 / 异族洞天 : " << audit.whiteHeavenYiren << " / "
              << audit.whiteHeavenYizu << "\n";
    std::cout << "  黑天天道秘境 / 洞天     : " << audit.blackHeavenTiandao << "\n";
    std::cout << "  方源已吞并 / 天庭已占领 : " << audit.fangYuanOwned << " / "
              << audit.tianTingOwned << "\n";
    std::cout << "  原著固定实例 / 随机生成 : " << audit.fixedInstances << " / "
              << audit.randomized << "\n";
    std::cout << "  违规落点 / 虚空洞天     : " << audit.illegalPlacement << " / "
              << audit.voidSpaceCaves << "  （均应为 0）\n";

    sub("1.2 世界胎壁：世界绝对屏障");
    show(WorldMap::tryCrossWombWall(WombWallPrivilege::None, true),
         "玩家尝试穿越世界胎壁");
    show(WorldMap::tryCrossWombWall(WombWallPrivilege::StoryOnlyZunZhe, false),
         "特殊尊者级 NPC（剧情限定）触碰胎壁");

    sub("1.3 天罡罡风：动态天气，非永久狂暴");
    TiangangGale gale;
    gale.reset(GalePhase::Calm);
    std::cout << "  轮换序列：";
    for (int i = 0; i < 6; ++i) {
        int guard = 0;
        GalePhase prev = gale.phase();
        while (gale.phase() == prev && guard++ < 200) gale.advance();
        std::cout << to_string(gale.phase()) << " ";
    }
    std::cout << "\n  平静期消耗倍率=" << [] { TiangangGale g; g.reset(GalePhase::Calm);
                                            return g.traverseCostMultiplier(); }()
              << "，暴风灾期消耗倍率=" << [] { TiangangGale g; g.reset(GalePhase::Disaster);
                                            return g.traverseCostMultiplier(); }()
              << "（八转以下必被撕裂重创）\n";

    // =====================================================================
    //  二：五域环境 DEBUFF 与本土免疫
    // =====================================================================
    hr("二、五域环境 DEBUFF —— 特定时段 / 特定区域触发，非全域常驻");

    WorldMap probe;
    Location beiyuan;
    beiyuan.layer = RealmLayer::MortalSurface;
    beiyuan.domain = Domain::BeiYuan;
    beiyuan.region = "暴雪岭";

    WorldClimate warmDay; warmDay.warmSeason = true; warmDay.phase = DayPhase::Day;
    probe.setClimate(warmDay);
    std::cout << "  [暖季·白昼·北原暴雪岭] 环境状态数 = "
              << probe.evaluateEnvironment(beiyuan, Domain::NanJiang).size()
              << "  （白天、晴天、暖季无任何严寒惩罚）\n";

    WorldClimate polarNight; polarNight.polarNight = true; polarNight.phase = DayPhase::Day;
    probe.setClimate(polarNight);
    auto nativeCold = probe.evaluateEnvironment(beiyuan, Domain::BeiYuan);
    auto outCold    = probe.evaluateEnvironment(beiyuan, Domain::NanJiang);
    std::cout << "  [极夜·北原暴雪岭] 跨域者严寒强度 = "
              << (outCold.empty() ? 0.0 : outCold[0].magnitude)
              << "，北原本土 = "
              << (nativeCold.empty() ? 0.0 : nativeCold[0].magnitude)
              << "  （本土高额免疫，几乎无感）\n";

    Location nanjiangPlain; nanjiangPlain.layer = RealmLayer::MortalSurface;
    nanjiangPlain.domain = Domain::NanJiang; nanjiangPlain.region = "南疆平原";
    Location nanjiangValley = nanjiangPlain; nanjiangValley.region = "瘴气谷";
    std::cout << "  [南疆平原] 环境状态数 = "
              << probe.evaluateEnvironment(nanjiangPlain, Domain::BeiYuan).size()
              << "；[瘴气谷] 环境状态数 = "
              << probe.evaluateEnvironment(nanjiangValley, Domain::BeiYuan).size()
              << "  （瘴气仅固定刷新于瘴气谷/毒泽/原始深林禁地）\n";

    Location black; black.layer = RealmLayer::BlackHeaven;
    auto supp = probe.evaluateEnvironment(black, Domain::BeiYuan);
    std::cout << "  [黑天] " << (supp.empty() ? "" : supp[0].name)
              << " 强度=" << (supp.empty() ? 0.0 : supp[0].magnitude)
              << "  （两天位面压制属天地规则，无本土免疫，凡俗平等承受）\n";

    // =====================================================================
    //  玩家：新天外之魔
    // =====================================================================
    hr("玩家登场：新天外之魔");
    gw.createDefaultPlayer("彭达", Domain::NanJiang);
    Cultivator* p = gw.player();
    std::cout << "  玩家：" << p->name << "（" << to_string(p->demonIdentity) << "，"
              << to_string(p->rank) << "，出身" << to_string(p->bornDomain) << "）\n";
    std::cout << "  定仙游坐标库：" << p->dingXianYou.count() << " 处（仅出身地）\n";

    // =====================================================================
    //  四：定仙游
    // =====================================================================
    hr("四、定仙游 —— 仅限去过、见过、感知过的坐标");

    const CaveParadise* baohuang = gw.world().findCave("cn_baohuangtian");
    Location bh = baohuang->location();
    std::cout << "  目标：宝黄天（玩家从未抵达 / 未见 / 未感知）\n";
    auto j0 = CultivatorSystem::jumpByDingXianYou(*p, bh, gw.world());
    std::cout << "  " << (j0.ok() ? "[成功] " : "[被拒] ")
              << "尚未持有定仙游：" << j0.detail << "\n";

    // 定仙游为仙蛊、世间唯一，原著归方源所有。
    // 演示走的是支线路径：向方源求借，取得【使用权】而非所有权 ——
    // 注册表中的持有者始终是方源，玩家只拿到有期限的催动资格。
    sub("支线：方源将定仙游借给你（使用权，非所有权）");
    if (auto dxEntry = gw.registry().lookupByName("定仙游")) {
        std::cout << "  注册表持有者（借出前）：" << dxEntry->holder << "\n";
        p->dingXianYouPossession = DingXianYouPossession::Lent;
        p->holdsDingXianYou = true;
        std::cout << "  玩家取得：借用权（" << to_string(p->dingXianYouPossession) << "）\n";
        std::cout << "  注册表持有者（借出后，仍为方源）："
                  << gw.registry().lookupByName("定仙游")->holder << "\n";
        std::cout << "  铁律不变：仅可跳往亲眼见过 / 抵达过 / 感知过的坐标\n";
    }

    auto j1 = CultivatorSystem::jumpByDingXianYou(*p, bh, gw.world());
    std::cout << "  " << (j1.ok() ? "[成功] " : "[被拒] ")
              << "坐标未解锁：" << j1.detail << "\n";

    sub("探索地图 → 解锁视野 → 积累坐标库");
    CultivatorSystem::observe(*p, bh, SightSource::Seen, gw.world().now());
    std::cout << "  已亲眼见过宝黄天，坐标库 = " << p->dingXianYou.count() << " 处\n";
    auto j2 = CultivatorSystem::jumpByDingXianYou(*p, bh, gw.world());
    std::cout << "  " << (j2.ok() ? "[成功] " : "[被拒] ") << j2.detail
              << "，仙元 -" << j2.essenceSpent << "\n";

    sub("禁止无依据空跳：随机挑一个从未去过的洞天仍然被拒");
    for (const auto* c : gw.world().allCaves()) {
        Location far = c->location();
        if (p->dingXianYou.knows(far)) continue;
        auto j3 = CultivatorSystem::jumpByDingXianYou(*p, far, gw.world());
        std::cout << "  " << (j3.ok() ? "[成功] " : "[被拒] ") << c->name << " → "
                  << j3.detail << "\n";
        break;
    }

    // =====================================================================
    //  五：三气平衡闭环
    // =====================================================================
    hr("五、三气平衡 —— 完整闭环");
    std::cout << "  a. 突破六转 → 强制解锁三气静修平衡模式\n";
    show(CultivatorSystem::breakthrough(*p, Rank::R6), "玩家突破六转");
    p->qi = ThreeQiPool{260.0, 20.0, 30.0};
    std::cout << "  当前三气：天=" << p->qi.heaven << "，地=" << p->qi.earth
              << "，人=" << p->qi.human << "，差值=" << p->qi.deviation() << "\n";

    sub("b. 必须居于自身仙窍 / 自己掌控的福地洞天内部");
    show(CultivatorSystem::enterSeclusion(*p, gw.world()),
         "于宝黄天（非自家洞天）尝试闭关");

    // 给玩家一处自家福地
    // 玩家出身南疆，以「南疆异族联盟洞天」为自家福地
    // （研究报告 Table 4：南疆为「人道、异族联盟、吴帅活动区」）
    const CaveParadise* home = gw.world().findCave("yz_nanjiang_aliance");
    if (!home) { std::cout << "  [错误] 洞天实例缺失\n"; return 1; }
    p->ownedSites.push_back(home->siteId);
    p->location = home->location();
    CultivatorSystem::observe(*p, home->location(), SightSource::Arrived, gw.world().now());
    show(CultivatorSystem::enterSeclusion(*p, gw.world()),
         "返回自家福地·万兽山后闭关");

    sub("c. 定点不动调和三气差值（期间无法移动 / 战斗 / 外出）");
    std::cout << "  可移动=" << ThreeQiSystem::canMove(*p)
              << "，可战斗=" << ThreeQiSystem::canBattle(*p)
              << "，可外出=" << ThreeQiSystem::canLeave(*p) << "\n";
    ThreeQiConfig cfg;
    int t = 0;
    while (!ThreeQiSystem::isBalanced(p->qi, cfg.tolerance) && t < 300) {
        ThreeQiSystem::tick(*p, cfg);
        ++t;
        if (t % 12 == 0)
            std::cout << "    第 " << t << " 刻：差值 = " << p->qi.deviation() << "\n";
    }
    std::cout << "  调和完成，共 " << t << " 刻，最终差值 = " << p->qi.deviation() << "\n";

    sub("d. 完成平衡，解除闭关，恢复行动");
    show(CultivatorSystem::leaveSeclusion(*p), "解除闭关");
    std::cout << "  可移动=" << ThreeQiSystem::canMove(*p)
              << "，可战斗=" << ThreeQiSystem::canBattle(*p) << "\n";

    sub("三气获取闭环：天气→黑天/天道秘境；地气→五域地脉/福地；人气→白天/人道势力");
    Location blackSpot; blackSpot.layer = RealmLayer::BlackHeaven;
    blackSpot.region = "黑天·天道秘境带";
    p->essence = p->maxEssence;   // 演示用：补满仙元以支撑跨界穿行
    auto mv = CultivatorSystem::moveTo(*p, blackSpot, gw.world());
    std::cout << "  前往黑天：" << (mv.ok() ? "[成功] " : "[被拒] ") << mv.detail
              << "，仙元 -" << (mv.ok() ? mv.value.essence : 0.0) << "\n";
    if (mv.ok()) {
        auto got = CultivatorSystem::gatherQi(*p, gw.world(), 1.0);
        if (got.ok())
            std::cout << "  于黑天搜集：天+" << got.value.heaven << "，地+"
                      << got.value.earth << "，人+" << got.value.human
                      << "  （天气为主，符合 5.2 设定）\n";
    }

    // =====================================================================
    //  六：炼蛊铁律
    // =====================================================================
    hr("六、炼蛊前置铁律");
    Refinery& rf = gw.refinery();

    RefineRequest blind;
    blind.recipeId = 88888;  // 无蛊方
    blind.refinerId = "player";
    blind.refinerDao = DaoLevel::MarkFusion;
    blind.refinerEssence = 999999;
    blind.inventory = {{"任意仙材", 9999}};
    auto r1 = rf.refine(blind, gw.registry());
    std::cout << "  [无蛊方] 结果=" << to_string(r1.outcome) << "，" << r1.detail << "\n";

    RefineRequest partial;
    partial.recipeId = 3;  // 残缺蛊方·月痕蛊
    partial.refinerId = "player";
    partial.refinerDao = DaoLevel::MarkFusion;
    partial.refinerEssence = 1000;
    partial.componentGu = {GuInstance{2, 9001, GuState::Active, "player"}};
    partial.inventory = {{"痕石", 1}};
    partial.rngRoll = 0.01;
    auto r2 = rf.refine(partial, gw.registry());
    std::cout << "  [残缺蛊方] 结果=" << to_string(r2.outcome) << "，" << r2.detail << "\n";

    RefineRequest full;
    full.recipeId = 1;  // 完整蛊方·四味酒虫
    full.refinerId = "player";
    full.refinerDao = DaoLevel::Entry;
    full.refinerEssence = 100;
    full.inventory = {{"酸甜苦辣四味美酒", 1}};
    full.componentGu = {GuInstance{1, 9002, GuState::Active, "player"},
                        GuInstance{1, 9003, GuState::Active, "player"}};
    full.rngRoll = 0.01;
    auto r3 = rf.refine(full, gw.registry());
    std::cout << "  [完整蛊方] 结果=" << to_string(r3.outcome) << "，" << r3.detail << "\n";

    // =====================================================================
    //  十一：仙蛊唯一
    // =====================================================================
    hr("十一、仙蛊唯一体系");
    ImmortalGuRegistry& reg = gw.registry();
    std::cout << "  开局在册仙蛊：" << reg.size() << " 只\n";
    std::cout << "  宿命蛊在世？ " << (reg.isNameOccupied("宿命蛊") ? "是" : "否")
              << "  （第五卷末已被摧毁，名额已释放，未来可重炼）\n";

    auto dup = reg.registerGu("定仙游", Rank::R6, "某NPC");
    std::cout << "  [重炼同名仙蛊] " << (dup.ok() ? "成功" : "被拒") << "："
              << dup.detail << "\n";

    auto zh = reg.lookupByName("智慧蛊");
    if (zh) {
        show(reg.seal(zh->instanceId), "封印智慧蛊");
        std::cout << "  封印后仍占唯一名额？ " << (reg.isNameOccupied("智慧蛊") ? "是" : "否")
                  << "  （封印 / 抢夺 / 封存均不清除注册表）\n";
        show(reg.transfer(zh->instanceId, "player"), "玩家抢夺智慧蛊");
        show(reg.destroy(zh->instanceId), "彻底毁灭智慧蛊");
        auto reborn = reg.registerGu("智慧蛊", Rank::R7, "player");
        std::cout << "  彻底毁灭后可重生？ " << (reborn.ok() ? "是" : "否") << "\n";
    }

    // =====================================================================
    //  七：道境体系
    // =====================================================================
    hr("七、道境体系 —— 道境高低直接压制杀招强度");

    auto moves = gw.killerMoves();
    auto tpls = rf.templates();
    auto moveByName = [&moves](const char* n) -> const KillerMoveDef* {
        for (const auto& m : moves) if (m.name == n) return &m;
        return nullptr;
    };
    const KillerMoveDef* sanxin = moveByName("三心合魂");  // 资料库：三转 魂道、音道复合
    const KillerMoveDef* wanwo  = moveByName("万我");      // 资料库：七转 人道（奴力合流）
    // 三心合魂组成中「飞魂蛊(12)、魂链蛊(13)」已建模板，参与齐备性校验
    std::vector<GuInstance> gu = {GuInstance{12, 8101, GuState::Active, "player"},
                                  GuInstance{13, 8102, GuState::Active, "player"}};

    MoveOutcome hi = KillerMoveResolver::resolve(
        *sanxin, DaoLevel::MarkFusion, Rank::R8, 99999, gu, tpls, 0.99);
    MoveOutcome wanwoOk = KillerMoveResolver::resolve(
        *wanwo, DaoLevel::Great, Rank::R7, 99999, gu, tpls, 0.99);
    MoveOutcome lo = KillerMoveResolver::resolve(
        *wanwo, DaoLevel::Entry, Rank::R7, 99999, gu, tpls, 0.0);

    std::cout << "  杀招一：三心合魂（资料库：三转 魂道、音道复合）\n";
    std::cout << "    组成：" << sanxin->componentNames[0];
    for (std::size_t i = 1; i < sanxin->componentNames.size(); ++i)
        std::cout << "＋" << sanxin->componentNames[i];
    std::cout << "\n    已建模板参与校验者 " << sanxin->components.size()
              << " 只；其余资料库未单列，不臆造其转数\n";
    std::cout << "    [道痕贯通] 威力=" << hi.power << "，消耗=" << hi.essenceCost
              << "，反噬=" << hi.backlash << "\n";
    std::cout << "  杀招二：万我（资料库：七转 人道·奴力合流，需人道大成）\n";
    std::cout << "    [人道大成] 威力=" << wanwoOk.power << "，反噬=" << wanwoOk.backlash
              << "，崩解=" << (wanwoOk.collapsed ? "是" : "否") << "\n";
    std::cout << "    [入门强行] 威力=" << lo.power << "，反噬=" << lo.backlash
              << "，崩解=" << (lo.collapsed ? "是" : "否")
              << "，残缺=" << (lo.incomplete ? "是" : "否") << "\n";
    std::cout << "  → " << lo.detail << "\n";

    std::cout << "\n  道境阶梯（入门→小成→大成→圆满→道基→道痕贯通）：\n";
    for (int lv = 0; lv <= 5; ++lv) {
        DaoCoefficients c = dao_coefficients(static_cast<DaoLevel>(lv));
        std::cout << "    " << to_string(static_cast<DaoLevel>(lv))
                  << "  威力×" << c.power << "  消耗×" << c.cost
                  << "  反噬×" << c.backlash << "  抗性" << c.resistance
                  << "  稳定" << c.stability << "\n";
    }

    // =====================================================================
    //  十：天外之魔侦测
    // =====================================================================
    hr("盗天传承 —— 疯魔窟一脉（资料库：盗天魔尊真传）");
    std::cout << "  资料库可核验：盗天魔尊主修偷道、护道宇道；\n"
              << "  疯魔窟为其真传所在（赌蛊条：疯魔窟盗天真传核心蛊之一）。\n"
              << "  电脑蛊（钢铁侠真传）资料库明确「彭达原持有」—— 新天外之魔线与盗天传承的联结。\n";

    Cultivator& pc = *gw.player();
    show(DaoTianLegacySystem::obtain(pc, DaoTianBranch::ThiefDao), "于疯魔窟得「偷道正统」传承");
    show(DaoTianLegacySystem::obtain(pc, DaoTianBranch::IronMan), "得「钢铁侠真传（机甲）」");
    show(DaoTianLegacySystem::cultivate(pc, 10.0), "精进修习");
    std::cout << "  偷道杀招增幅 ×"
              << DaoTianLegacySystem::bonusFor(pc.daoTianLegacy, Dao::Thief)
              << "；火道（不相关）×"
              << DaoTianLegacySystem::bonusFor(pc.daoTianLegacy, Dao::Fire) << "\n";
    std::cout << "  研究报告 4.3 限制：完整天外之魔 / 盗天传承"
              << (DaoTianLegacySystem::grantsImmunity() ? "＝免疫一切" : "≠ 免疫一切")
              << "（仍受战斗伤害、仙蛊反噬、他人算计）\n";

    hr("两天五域地理 —— 界壁、疯魔窟、两天洞天新状态");
    std::cout << "  " << gw.world().twoHeavens().describe() << "\n";
    std::cout << "  太阳是「全天下最大的天脉节点、最大的光炎脉节点」，"
                 "不是可简单消灭的普通天体。\n";
    std::cout << "  幽魂虽吞噬黑天天灵，然其神志异常，方源、巨阳、星宿三方仍在博弈"
                 "—— 不可写成「黑天赢下白天后五域立即黑暗」。\n\n";

    std::cout << "  疯魔窟：北原十大凶地，共九层（非普通山谷）\n";
    if (const CaveParadise* fmk = gw.world().findCave("cn_fengmo_ku")) {
        std::cout << "    位置：" << fmk->region << "，层数 "
                  << fmk->floors << "；第九层含元境、书山、九转衍化蛊、疯魔九虚阵\n";
    }
    std::cout << "  地点分类（地理研究：A 级区分，仙窍内坐标 ≠ 五域地表）——\n";
    auto showClass = [&gw](const char* id, const char* label) {
        if (const CaveParadise* c = gw.world().findCave(id))
            std::cout << "    " << label << "：" << to_string(c->siteClass) << "\n";
    };
    showClass("cn_yitian",           "义天山（南疆）");
    showClass("dh_danghun_shan",     "荡魂山");
    showClass("ns_niliu_he",         "逆流河");
    showClass("gy_guangyin_changhe", "光阴长河");
    showClass("tt_central",          "天庭洞天");

    std::cout << "\n  五域界壁（对本域引力、外域斥力，层次越高阻碍越强）——\n";
    for (Domain d : {Domain::ZhongZhou, Domain::NanJiang, Domain::BeiYuan,
                     Domain::XiMo, Domain::DongHai}) {
        std::cout << "    " << to_string(d) << "：" << WorldMap::domainWallName(d) << "\n";
    }
    {
        auto low  = WorldMap::evaluateDomainWall(Domain::ZhongZhou, Domain::BeiYuan, Rank::R3);
        auto high = WorldMap::evaluateDomainWall(Domain::ZhongZhou, Domain::BeiYuan, Rank::R8);
        auto sea  = WorldMap::evaluateDomainWall(Domain::ZhongZhou, Domain::DongHai, Rank::R8);
        std::cout << "    中洲→北原：三转 ×" << low.essenceMul
                  << "，八转 ×" << high.essenceMul
                  << "；中洲→东海（海潮薄弱）×" << sea.essenceMul << "\n";
    }

    hr("成尊四条件 —— 数值口径库：非修为够即可登临九转");
    {
        Cultivator v; v.id = "v"; v.name = "求尊者"; v.rank = Rank::R8;
        std::cout << "  成尊四条件：白荔本源 ＋ 主修道痕≥30万 ＋ 无上大宗师 ＋ 突破天道封锁\n";
        auto f0 = CultivatorSystem::checkVenerableFitness(v);
        std::cout << "  [初查] 满足？ " << (f0.satisfied ? "是" : "否") << "\n";
        std::cout << "    " << f0.detail << "\n";

        v.setFlow(Dao::Refine, FlowLevel::Supreme);
        v.addDaoMarks(Dao::Refine, 400000.0);
        auto f1 = CultivatorSystem::checkVenerableFitness(v);
        std::cout << "  [补主修] 满足？ " << (f1.satisfied ? "是" : "否")
                  << "（仍缺「突破天道封锁」）\n";

        std::cout << "  [偏科验证] 他派道痕不得计入主修 ——\n";
        Cultivator m; m.id = "m"; m.name = "偏科者"; m.rank = Rank::R8;
        m.setFlow(Dao::Refine, FlowLevel::Supreme);
        m.addDaoMarks(Dao::Refine, 100000.0);
        m.addDaoMarks(Dao::Sword,  500000.0);
        m.brokeHeavenlyDaoSeal = true;
        auto fm = CultivatorSystem::checkVenerableFitness(m);
        std::cout << "    合计道痕 " << (long long)m.daoMarks
                  << "（已超 30 万），但主修仅 "
                  << (long long)fm.mainDaoMarks << " → 满足？ "
                  << (fm.satisfied ? "是" : "否") << "\n";

        v.brokeHeavenlyDaoSeal = true;
        show(CultivatorSystem::breakthrough(v, Rank::R9), "四条件齐备，登临九转");
    }

    hr("十、天外之魔侦测 —— 无魔气值，纯行为判定");
    std::cout << "  [无任何暴露行为 · 高阶天道存在] "
              << to_string(gw.detectDemon(*p, DetectorClass::HighRankHeaven).level) << "\n";
    std::cout << "  [无任何暴露行为 · 普通蛊仙]     "
              << to_string(gw.detectDemon(*p, DetectorClass::None).level) << "\n";

    OtherworldlyDemonDetection::recordBattleLeak(p->exposure, "星宿仙尊", Dao::Refine);
    std::cout << "  [战斗泄露道痕后 · 天庭侦查蛊]   "
              << to_string(gw.detectDemon(*p, DetectorClass::TianTingScoutGu).level) << "\n";
    OtherworldlyDemonDetection::recordIntelBetrayal(p->exposure, "内鬼");
    std::cout << "  [再遭情报出卖 · 高阶天道存在]   "
              << to_string(gw.detectDemon(*p, DetectorClass::HighRankHeaven).level) << "\n";
    std::cout << "  → 暴露仅来自战斗泄露道痕 / 专属侦查杀招 / 情报出卖，无任何数值概率\n";

    // =====================================================================
    //  十二：NPC 自主 AI
    // =====================================================================
    hr("十二、NPC 完整 AI —— 自主行动，不随玩家转动");
    std::cout << "  开局 NPC：\n";
    for (const auto& a : gw.npcs()) {
        std::cout << "    " << a.self.name << "（" << to_string(a.self.rank) << "，"
                  << to_string(a.faction) << "，道痕" << (long long)a.self.daoMarks
                  << "）\n";
    }

    std::cout << "\n  开局关键机制状态（研究报告 3.6 / 7.2）：\n";
    if (const NpcAgent* qihai = gw.findNpc("qihailaozu")) {
        std::cout << "    气海老祖：储备气功果 " << qihai->qiGongGuoStored
                  << " 颗，自爆风险 " << qihai->qiGuoExplosionRisk
                  << "（一颗完全吸收约增六万气道道痕；"
                     "两天洞天气功果已膨胀至接近自爆边缘）\n";
    }
    if (const NpcAgent* fy = gw.findNpc("fangyuan")) {
        std::cout << "    方源：持至尊仙窍，可吞并他窍（吞并过快带来内部平衡与灾劫压力）；"
                     "已吞并 " << fy->swallowedCaves << " 处\n";
    }

    std::cout << "\n  推进 240 个世界刻度（玩家不介入）：\n";
    int shown = 0;
    for (int i = 0; i < 240; ++i) {
        auto rep = gw.step(1);
        for (const auto& n : rep.notes) {
            if (shown < 22) { std::cout << "    " << n << "\n"; ++shown; }
        }
    }
    std::cout << "    ……（其余省略）\n";

    auto audit2 = gw.world().audit();
    std::cout << "\n  NPC 自主行动后：方源已吞并 " << audit2.fangYuanOwned
              << " 处，天庭已占领 " << audit2.tianTingOwned << " 处\n";

    sub("气功果体系（研究报告 4.2）与至尊仙窍吞并（研究报告 7.2）");
    if (NpcAgent* qihai = gw.findNpc("qihailaozu")) {
        std::cout << "  气海老祖：气功果已炼化完毕，气道道痕累计 "
                  << (long long)qihai->self.daoMarks << "\n";
    }
    if (NpcAgent* fy = gw.findNpc("fangyuan")) {
        std::cout << "  方源：至尊仙窍已吞并 " << fy->swallowedCaves
                  << " 处洞天（扩张链条：联盟—资源—洞天—至尊仙窍）\n";
    }

    sub("NPC 定仙游同样只能跳转已探索点位");
    NpcAgent* fy = gw.findNpc("fangyuan");
    if (fy) {
        const CaveParadise* unknownSite = gw.world().findCave("ht_tiandao_2");
        bool knows = fy->self.dingXianYou.knows(unknownSite->location());
        std::cout << "  方源是否知晓「星宿遗境」？ " << (knows ? "是" : "否") << "\n";
        if (!knows) {
            auto jr = NpcAi::jump(*fy, unknownSite->location(), gw.world());
            std::cout << "  NPC 跳转未探索坐标 → " << (jr.ok() ? "成功" : "被拒")
                      << "：" << jr.detail << "\n";
        }
    }

    // =====================================================================
    //  八 / 十四：终章与世界分支 —— 无结局锁死
    // =====================================================================
    hr("八、十四、终章机制 —— 无结局锁死，无限开放成长");
    sub("分支 A：击杀幽魂");
    GameWorld gwA(111);
    gwA.buildVolume6Opening(8);
    show(gwA.triggerEndgame(EndgameEvent::YouHunKilled), "触发「击杀幽魂」");
    gwA.step(10);
    std::cout << "  幽天是否开启：" << (gwA.endgame().youTianEraOpened ? "是" : "否")
              << "；游戏是否结束：" << (WorldBranchSystem::isGameOver(gwA.endgame()) ? "是" : "否")
              << "\n";

    sub("分支 B：幽魂存活 → 幽天开启");
    GameWorld gwB(222);
    gwB.buildVolume6Opening(8);
    show(gwB.triggerEndgame(EndgameEvent::YouTianEraTriggered), "触发「幽天开启」");
    show(gwB.triggerEndgame(EndgameEvent::FangYuanKilled), "触发「击杀方源」");
    show(gwB.triggerEndgame(EndgameEvent::TianTingToppled), "触发「天庭倾覆」");
    show(gwB.triggerEndgame(EndgameEvent::PlayerFoundNewOrder), "玩家自建天庭 / 魔道秩序");
    gwB.step(10);
    std::cout << "  当前激活分支：";
    for (WorldBranch b : gwB.endgame().activeBranches)
        std::cout << to_string(b) << " / ";
    std::cout << "\n  游戏是否结束：" << (WorldBranchSystem::isGameOver(gwB.endgame()) ? "是" : "否")
              << "  （可继续无限修行、堆道痕、突破尊者、开创大道、改造世界）\n";

    // =====================================================================
    //  九：联机预留
    // =====================================================================
    hr("九、联机预留 —— 架构已预留，当前版本为纯单机");
    MultiplayerReserve& mp = gw.online();
    std::cout << "  联机是否启用：" << (mp.enabled() ? "是" : "否") << "\n";
    show(mp.joinSharedWorld("peer_001"), "预留：多玩家世界共存");
    show(mp.invadeParadise("peer_001", "yz_nanjiang_aliance"), "预留：玩家洞天互相入侵");
    show(mp.tradeViaBaoHuangTian("peer_001", "春秋蝉"), "预留：宝黄天跨玩家交易");
    show(mp.factionWar("peer_001", true), "预留：玩家势力对战 / 结盟");
    show(mp.triggerSharedWorldEvent("幽天开启"), "预留：多玩家共同触发大世界事件");

    sub("仙蛊唯一在联机多人世界下的兼容校验");
    ImmortalGuRegistry peerWorld(0, /*worldId=*/2);
    peerWorld.registerGu("定仙游", Rank::R6, "peer_player");   // 与玩家世界同名
    auto coexist = MultiplayerReserve::checkGuUniquenessAcrossWorlds(
        gw.registry(), {peerWorld.snapshot()});
    std::cout << "  场景一·远端世界也有「定仙游」："
              << (coexist.compatible ? "[兼容] " : "[冲突] ") << coexist.detail << "\n";
    for (const auto& c : coexist.conflicts) std::cout << "    冲突项：" << c << "\n";

    // 对照：本世界宿命蛊已被摧毁（名额释放），故跨世界无同名冲突
    ImmortalGuRegistry peerWorld2(0, /*worldId=*/2);
    peerWorld2.registerGu("宿命蛊", Rank::R9, "peer_player");
    auto coexist2 = MultiplayerReserve::checkGuUniquenessAcrossWorlds(
        gw.registry(), {peerWorld2.snapshot()});
    std::cout << "  场景二·远端世界有「宿命蛊」："
              << (coexist2.compatible ? "[兼容] " : "[冲突] ") << coexist2.detail
              << "  （本世界宿命蛊已毁，名额释放，故不冲突）\n";

    // =====================================================================
    //  自创炼蛊：以手中之蛊推演前所未有之蛊，成则命名存档
    // =====================================================================
    hr("自创炼蛊：推演前所未有之蛊");
    {
        // 自创炼蛊由会话层（GameSession）提供 —— 本演示主体的 gw 不走会话层，
        // 故此处另起一个会话展示这套机制，其状态与主演示相互独立。
        GameSession sess(20240906, "");
        sess.world().createDefaultPlayer("演示者", Domain::NanJiang, Rank::R6);
        sess.placePlayerAtStart();
        Cultivator* pl = sess.world().player();
        std::cout << "  自创炼蛊至少需两蛊为基；成败系于炼道造诣，"
                     "失败则炉毁蛊崩、反噬伤身。\n";
        if (pl) {
            while (pl->carriedGu.size() < 2) {
                GuInstance g; g.templateId = 1; g.holder = pl->id;
                pl->carriedGu.push_back(g);
            }
            // 炼道造诣有限，成功率本就不高，故多试几次
            bool made = false;
            for (int attempt = 0; attempt < 80 && !made; ++attempt) {
                while (pl->carriedGu.size() < 2) {
                    GuInstance g; g.templateId = 1; g.holder = pl->id;
                    pl->carriedGu.push_back(g);
                }
                if (pl->essence < 300.0) pl->essence = 2000.0;
                // 此处直接用会话层不便（本演示程序不走 GameSession），
                // 故以同样规则调用一次尝试，展示机制本身
                Command c; c.kind = CmdKind::Innovate; c.components = {0, 1};
                auto r = sess.execute(c);
                made = sess.pendingInnovation().pending;
                if (made) {
                    std::cout << "  [成功] " << r.title << "\n";
                    for (const auto& n : r.notes) std::cout << "    · " << n << "\n";
                } else if (attempt == 0) {
                    std::cout << "  " << r.title << "\n";
                    for (const auto& n : r.notes) std::cout << "    · " << n << "\n";
                }
            }
            if (made) {
                Command n; n.kind = CmdKind::NameGu; n.newName = "元初一气蛊";
                auto r = sess.execute(n);
                std::cout << "  [命名] " << r.title << "\n";
                for (const auto& x : r.notes) std::cout << "    · " << x << "\n";
            }
        }
    }

    // =====================================================================
    //  最终快照
    // =====================================================================
    hr("世界快照");
    auto snap = gw.snapshot();
    std::cout << "  世界时刻        : T" << snap.tick << "\n";
    std::cout << "  洞天总数        : " << snap.caves << "\n";
    std::cout << "  在册仙蛊        : " << snap.guRegistered << "\n";
    std::cout << "  NPC 存活 / 总数 : " << snap.aliveNpcs << " / " << snap.npcs << "\n";
    std::cout << "  激活世界分支    : ";
    for (const auto& b : snap.branches) std::cout << b << " / ";
    std::cout << "\n  游戏是否结束    : " << (snap.gameEnded ? "是" : "否（无结局锁死）") << "\n";

    std::cout << "\n演示结束。\n";
    return 0;
}
