// NPC 自主 AI 实现：不随玩家转动，与玩家共用同一套世界规则
#include "gr/ai/NpcAgent.hpp"

#include <cstring>
#include "gr/core/CanonNumbers.hpp"
#include "gr/cultivator/ThreeQi.hpp"
#include "gr/cultivator/DingXianYou.hpp"

#include <algorithm>
#include <cmath>

namespace gr {

// ---------------------------------------------------------------------------
//  决策：优先级 —— 万劫 > 三气失衡 > 三气储备不足 > 争夺洞天 > 幕后布局
// ---------------------------------------------------------------------------
NpcIntent NpcAi::decide(const NpcAgent& agent, const WorldMap& world,
                        const FactionSystem& factions) {
    const Cultivator& c = agent.self;
    if (!c.alive) return NpcIntent::Idle;

    // 1) 万劫 / 灾劫迫近：优先级最高，必要时可中断闭关
    if (!agent.homeSite.empty()) {
        if (const CaveParadise* home = world.findCave(agent.homeSite)) {
            if (home->tribulationTimer > 0 && home->tribulationTimer <= 5)
                return NpcIntent::HandleTribulation;
        }
    }

    // 2) 闭关中：维持闭关，由 Seclude 分支负责「平衡后解除闭关」
    //    （切不可在此返回 Idle —— 那会让 NPC 永久卡死在闭关状态）
    if (c.threeQi.inSeclusion) return NpcIntent::Seclude;

    // 2) 三气失衡 —— 必须回自家洞天闭关
    if (ThreeQiSystem::requiresBalance(c.rank)) {
        const double dev = c.qi.deviation();
        if (dev > ThreeQiConfig{}.imbalanceLimit)
            return NpcIntent::ReturnHome;
        // 3) 储备见底 —— 外出搜集（按最缺的一气选择去处）
        if (c.qi.total() < 60.0) {
            if (c.qi.heaven <= c.qi.earth && c.qi.heaven <= c.qi.human)
                return NpcIntent::GatherHeavenQi;
            if (c.qi.earth <= c.qi.human)
                return NpcIntent::GatherEarthQi;
            return NpcIntent::GatherHumanQi;
        }
        // 周期性回洞天维持平衡（阈值需严于闭关结束阈值，避免反复进出闭关）
        if (c.qi.deviation() > ThreeQiConfig{}.tolerance * 0.5)
            return NpcIntent::ReturnHome;
    }

    // 4) 气功果炼化：持有气功果且未达自爆临界者优先转化道痕（研究报告 4.2：两天洞天气功果已膨胀至接近自爆边缘）
    if (agent.qiGongGuoStored >= 1.0 && agent.qiGuoExplosionRisk < 0.9)
        return NpcIntent::AbsorbQiGongGuo;

    // 5) 争夺异人洞天（方源、天庭在白天已占领部分，其余仍是争夺对象）
    (void)factions;
    if (!agent.targetSites.empty()) return NpcIntent::ContestCave;

    // 5) 幕后布局 / 狩猎
    if (agent.faction == FactionId::FangYuan) return NpcIntent::Scheme;
    if (agent.faction == FactionId::YouHun)   return NpcIntent::Hunt;

    return NpcIntent::Idle;
}

// ---------------------------------------------------------------------------
//  移动到目标：已探索坐标可定仙游瞬移，未探索只能常规跋涉
// ---------------------------------------------------------------------------
static bool travelTo(NpcAgent& agent, const Location& to, WorldMap& world,
                     NpcAi::TickReport& rep) {
    Cultivator& c = agent.self;
    if (c.location == to) return true;

    // NPC 同样遵守：定仙游只能跳转已探索点位
    if (c.canUseDingXianYou() && c.dingXianYou.knows(to) &&
        ThreeQiSystem::canLeave(c)) {
        JumpResult jr = DingXianYou::tryJump(c, to, world, true);
        if (jr.ok()) {
            rep.jumped = true;
            rep.jumpedTo = to;
            rep.notes.push_back(c.name + " 定仙游 → " + to.key());
            return true;
        }
    }
    // 常规跋涉：抵达后自动解锁坐标（探索地图 → 积累坐标库）
    auto mv = CultivatorSystem::moveTo(c, to, world);
    if (mv.ok()) {
        rep.notes.push_back(c.name + " 跋涉 → " + to.key());
        return true;
    }
    rep.notes.push_back(c.name + " 无法前往 " + to.key() + "：" + to_string(mv.err));
    return false;
}

NpcAi::TickReport NpcAi::tick(NpcAgent& agent, WorldMap& world,
                              const FactionSystem& factions, Rng& rng) {
    TickReport rep;
    Cultivator& c = agent.self;
    if (!c.alive) return rep;

    // 环境结算（NPC 同样吃环境 DEBUFF 与本土免疫）
    CultivatorSystem::applyEnvironment(c, world);
    if (!c.alive) { rep.notes.push_back(c.name + " 死于环境压制"); return rep; }

    agent.intent = decide(agent, world, factions);
    rep.intent   = agent.intent;

    bool advanced = false;   // 本刻度是否已推进过三气（避免闭关分支重复消耗）
    switch (agent.intent) {
        case NpcIntent::Idle:
            break;

        case NpcIntent::Seclude: {
            auto t = ThreeQiSystem::tick(c);
            advanced = true;
            // 闭关结束阈值严于触发阈值，确保出关后不会立刻又想回关
            if (c.qi.deviation() <= ThreeQiConfig{}.tolerance * 0.4) {
                ThreeQiSystem::endSeclusion(c);
                rep.notes.push_back(c.name + " 三气平衡完成，解除闭关");
            } else {
                rep.notes.push_back(c.name + " 闭关调和：差值 " +
                                    std::to_string(t.deviationAfter));
            }
            break;
        }

        case NpcIntent::ReturnHome: {
            const CaveParadise* home =
                agent.homeSite.empty() ? nullptr : world.findCave(agent.homeSite);
            if (!home) { rep.notes.push_back(c.name + " 无自家洞天可归"); break; }

            if (travelTo(agent, home->location(), world, rep)) {
                auto ent = ThreeQiSystem::beginSeclusion(c, world);
                if (ent.ok()) {
                    rep.notes.push_back(c.name + " 进入闭关调和三气");
                } else {
                    rep.notes.push_back(c.name + " 闭关失败：" + ent.detail);
                    // 无处闭关（自家居所尚不归自己掌控）→ 先外出搜集三气
                    auto got = ThreeQiSystem::gather(c, world, 1.0);
                    if (got.ok()) {
                        c.qi.heaven += got.value.heaven;
                        c.qi.earth  += got.value.earth;
                        c.qi.human  += got.value.human;
                    }
                }
            }
            break;
        }

        case NpcIntent::GatherHeavenQi:
        case NpcIntent::GatherEarthQi:
        case NpcIntent::GatherHumanQi: {
            // 选择已知的、对应气产出最高的所在
            const CaveParadise* best = nullptr;
            double bestScore = -1.0;
            for (const auto* site : world.allCaves()) {
                if (!c.dingXianYou.knows(site->location())) continue;  // 未探索不去
                double s = (agent.intent == NpcIntent::GatherHeavenQi) ? site->qiYieldHeaven
                         : (agent.intent == NpcIntent::GatherEarthQi)  ? site->qiYieldEarth
                         :                                              site->qiYieldHuman;
                if (s > bestScore) { bestScore = s; best = site; }
            }
            if (!best) {
                // 没有任何已知产气点：先就近探索（常规跋涉解锁坐标）
                auto caves = world.allCaves();
                if (!caves.empty()) {
                    const CaveParadise* t =
                        caves[static_cast<std::size_t>(rng.rangeInt(
                            0, static_cast<std::int64_t>(caves.size()) - 1))];
                    travelTo(agent, t->location(), world, rep);
                }
                break;
            }
            if (travelTo(agent, best->location(), world, rep)) {
                auto got = ThreeQiSystem::gather(c, world, 1.0);
                if (got.ok()) {
                    c.qi.heaven += got.value.heaven;
                    c.qi.earth  += got.value.earth;
                    c.qi.human  += got.value.human;
                    rep.notes.push_back(c.name + " 搜集三气：天+" +
                                        std::to_string(got.value.heaven) + " 地+" +
                                        std::to_string(got.value.earth) + " 人+" +
                                        std::to_string(got.value.human));
                } else {
                    rep.notes.push_back(c.name + " 搜集失败：" + got.detail);
                }
            }
            break;
        }

        case NpcIntent::HandleTribulation: {
            CaveParadise* home =
                agent.homeSite.empty() ? nullptr : world.findCaveMut(agent.homeSite);
            if (home && home->tribulationTimer <= 5) {
                // 万劫迫近：中断闭关奔赴灾劫现场
                if (c.threeQi.inSeclusion) {
                    ThreeQiSystem::endSeclusion(c);
                    rep.notes.push_back(c.name + " 万劫迫近，中断闭关");
                }

                // --------------------------------------------------------------
                //  灾劫是蛊仙道痕的「主要来源」，不是损耗项。
                //  数值口径库：地灾 250／天劫 750／浩劫 7250／万劫 86750（道痕/场）。
                //  正因为一场万劫近八万道痕，八转方能累至十万—三十万，
                //  进而满足成尊「主修流派 ≥ 30 万道痕」的门槛。
                //  此前误写为「渡劫损耗 500 道痕」，与设定完全相反，已修正。
                // --------------------------------------------------------------
                using namespace canon;
                const TribulationKind kind = home->pendingTribulation != TribulationKind::None
                    ? home->pendingTribulation
                    : TribulationKind::MyriadCalamity;
                const double gain = tribulation_mark_gain(kind);

                double cost = 80.0 + 30.0 * (rank_value(c.rank) - 5);
                if (c.essence >= cost) {
                    c.essence -= cost;
                    if (gain > 0.0) {
                        c.addDaoMarks(c.mainDao(), gain);
                        rep.notes.push_back(
                            c.name + " 渡过" + to_string(kind) + "，获道痕 " +
                            std::to_string((long long)gain) +
                            "（主修累计 " + std::to_string((long long)c.mainDaoMarks()) + "）");
                    } else {
                        // 混沌小难／大难：资料库未给收益，不作臆造
                        rep.notes.push_back(c.name + " 渡过" + to_string(kind) +
                                            "（资料库未给道痕收益，不臆造数值）");
                    }
                    home->pendingTribulation = TribulationKind::None;
                    home->tribulationTimer = 60 + static_cast<int>(rng.rangeInt(0, 60));
                    rep.notes.push_back(c.name + " 下次灾劫于 " +
                                        std::to_string(home->tribulationTimer) + " 刻后");
                } else {
                    rep.notes.push_back(c.name + " 仙元不足以渡劫，洞天受损");
                    c.health = std::max(0.0, c.health - 0.2);
                }
            }
            break;
        }

        case NpcIntent::ContestCave: {
            if (agent.targetSites.empty()) break;
            const std::string& target = agent.targetSites.front();
            const CaveParadise* site = world.findCave(target);
            if (!site) { agent.targetSites.erase(agent.targetSites.begin()); break; }
            // 已被至尊仙窍吞并的洞天不再是争夺目标
            if (!site->canBeContested) {
                agent.targetSites.erase(agent.targetSites.begin());
                rep.notes.push_back(c.name + " 放弃目标：" + site->name +
                                    "（已被至尊仙窍吞并）");
                break;
            }
            if (travelTo(agent, site->location(), world, rep)) {
                // 争夺：以战力与影响力掷骰
                double power = CultivatorSystem::combatPower(c, Dao::Refine);
                double roll  = rng.next();
                if (roll < std::min(0.85, power / (power + 3000.0))) {
                    CaveParadise* mut = world.findCaveMut(target);
                    if (mut) {
                        mut->owner = (agent.faction == FactionId::FangYuan)
                                         ? CaveOwnership::FangYuanSwallowed
                                         : (agent.faction == FactionId::TianTing)
                                               ? CaveOwnership::TianTingOccupied
                                               : CaveOwnership::NpcFaction;
                        mut->ownerTag = c.name;
                        c.ownedSites.push_back(target);
                        rep.notes.push_back(c.name + " 攻占洞天：" + mut->name);
                        agent.targetSites.erase(agent.targetSites.begin());

                        // ---- 至尊仙窍吞并（研究报告 7.2 扩张链条）----
                        // 拥有至尊仙窍者，攻占后可进一步将洞天吞并入仙窍，
                        // 转化为可携带的仙窍资源；吞并过快会带来内部平衡与灾劫压力。
                        if (agent.hasZhiZunXianQiao) {
                            if (BattleSystem::swallowCave(c, *mut, world).ok()) {
                                ++agent.swallowedCaves;
                                rep.notes.push_back(c.name + " 将 " + mut->name +
                                                    " 吞并入至尊仙窍（已吞并 " +
                                                    std::to_string(agent.swallowedCaves) + " 处）");
                            }
                        }
                    }
                } else {
                    rep.notes.push_back(c.name + " 争夺 " + site->name + " 未果");
                }
            }
            break;
        }

        case NpcIntent::AbsorbQiGongGuo: {
            // ---- 气功果炼化（研究报告 4.2）----
            // 气海分身以「气绝逢生」炼化气功果：一颗普通气功果约增六万气道道痕。
            // 但两天洞天气功果已膨胀至接近自爆边缘 —— 过度囤积即引爆风险。
            const double kMarksPerFruit = 60000.0;
            if (agent.qiGongGuoStored >= 1.0) {
                agent.qiGongGuoStored -= 1.0;
                c.daoMarks += kMarksPerFruit;
                c.essence   = std::min(c.maxEssence, c.essence + c.maxEssence * 0.15);
                agent.qiGuoExplosionRisk = std::max(0.0, agent.qiGuoExplosionRisk - 0.15);
                rep.notes.push_back(c.name + " 以「气绝逢生」炼化气功果，气道道痕 +" +
                                    std::to_string((long long)kMarksPerFruit) +
                                    "（累计 " + std::to_string((long long)c.daoMarks) + "）");
            }
            break;
        }

        case NpcIntent::Hunt:
        case NpcIntent::Scheme: {
            // 幕后布局 / 狩猎：积累道痕与情报，不随玩家转动
            c.daoMarks += 50.0;
            c.essence   = std::min(c.maxEssence, c.essence + c.maxEssence * 0.05);
            rep.notes.push_back(c.name + (agent.intent == NpcIntent::Scheme
                                              ? " 幕后布局（分身运作、情报网络）"
                                              : " 狩猎中"));
            break;
        }

        case NpcIntent::Battle:
            break;
    }

    // 三气自然推进（闭关分支已推进过，不重复消耗）
    if (!advanced) ThreeQiSystem::tick(c);
    return rep;
}

JumpResult NpcAi::jump(NpcAgent& agent, const Location& to, const WorldMap& world) {
    // 铁律：NPC 不会跳从未去过的地方
    return DingXianYou::tryJump(agent.self, to, world, agent.self.canUseDingXianYou());
}

// ---------------------------------------------------------------------------
//  第六卷卷初 / 断更时刻的原著人物开局数据
// ---------------------------------------------------------------------------
std::vector<NpcAgent> build_canon_npcs(const WorldMap& world) {
    std::vector<NpcAgent> v;

    auto mk = [&v, &world](const char* id, const char* name, Rank rank, Domain born,
                           FactionId fac, Dao mainDao, DaoLevel daoLv,
                           const char* homeSite, bool isFenShen = false,
                           const char* master = "") {
        NpcAgent a;
        a.self.id         = id;
        a.self.name       = name;
        a.self.rank       = rank;
        a.self.bornDomain = born;
        a.self.isPlayer   = false;
        a.self.maxEssence = is_immortal_rank(rank) ? 5000.0 : 300.0;
        a.self.essence    = a.self.maxEssence;

        // 道痕量级依数值口径库「流派境界与道痕」表，而非自定义公式。
        // 【已更正】此前用 100000×(转数−5) 的工程公式，与资料库严重不符：
        //   六转代码 100000 vs 资料库「数百至不足1000」（相差百倍以上）
        //   七转代码 200000 vs 资料库「数千至不足1万」
        // 现按世界观典型口径取中值；原著有名强者（方源、气海老祖等）另行单设。
        {
            using namespace canon;
            const DaoMarkScale sc = dao_mark_scale(rank);
            double marks = 0.0;
            if (sc.typicalLow != kUnknown && sc.typicalHigh != kUnknown) {
                marks = (sc.typicalLow + sc.typicalHigh) * 0.5;
            } else if (sc.typicalLow != kUnknown) {
                marks = sc.typicalLow;
            }
            a.self.daoMarks = marks;
            // 主修流派道痕分项：成尊只认这一项
            if (marks > 0.0) a.self.daoMarksByDao[mainDao] = marks;
            // 流派境界与道境对应（两套体系，见 CanonNumbers 说明）
            a.self.setFlow(mainDao, flow_level_of(daoLv));
        }
        a.self.dao.set(mainDao, daoLv);
        a.faction  = fac;
        a.homeSite = homeSite;
        a.isFenShen = isFenShen;
        a.masterId  = master;

        // 开局即知晓自家洞天所在
        // 分身（如气海老祖、吴帅）同样掌控其驻扎洞天，否则无处闭关调和三气
        if (const CaveParadise* home = world.findCave(homeSite)) {
            a.self.location = home->location();
            a.self.dingXianYou.unlock(home->location(), SightSource::Arrived, 0,
                                      "自家洞天");
            a.self.ownedSites.push_back(homeSite);
        }
        // 六转及以上：给予三气储备。
        //
        // 注意：此处【不再】给所有六转 NPC 派发定仙游。
        // 定仙游是仙蛊、世间唯一，原著归方源所有 —— 此前「六转以上人人持有」
        // 与项目自身的仙蛊唯一铁律直接冲突，属设计错误，现已修正：
        // 唯有方源为 Owned，其余一律 None。
        if (is_immortal_rank(rank)) {
            a.self.qi = ThreeQiPool{120.0, 110.0, 100.0};
        }
        // 定仙游：仙蛊唯一，原著归方源所有。
        // 此前「六转以上人人持有」与项目自身的仙蛊唯一铁律直接冲突，
        // 属设计错误 —— 现仅在定义方源这一条时授予 Owned，其余一律 None。
        if (std::strcmp(id, "fangyuan") == 0) {
            a.self.dingXianYouPossession = DingXianYouPossession::Owned;
            a.self.holdsDingXianYou = true;
        }
        v.push_back(std::move(a));
        return;
    };

    // 方源：九转炼道尊者，至尊仙窍，幕后布局
    // 研究报告 2.2／7.1：炼道尊者，名号「炼天魔尊」，依靠至尊仙窍、分身、影宗遗产
    // 成尊门槛：主修炼道 ≥ 30 万道痕（数值口径库）。
    mk("fangyuan", "方源（炼天魔尊）", Rank::R9, Domain::ZhongZhou,
       FactionId::FangYuan, Dao::Refine, DaoLevel::MarkFusion,
       "cn_zunzhe_xiantiao");
    // 数值口径库「方源气道道痕案例」：上百万道气道道痕，
    // 使七转杀招可达到八转层次，但仍消耗七转仙元。
    // 此为特例个案，不可当作八转通用量级。
    v.back().self.daoMarksByDao[Dao::Qi] = 1000000.0;
    v.back().self.daoMarks += 1000000.0;

    // 气海老祖：方源气道分身。研究报告 3.6：处理豪豨洞天气功果，
    // 诞生时已有八十多万气道道痕，可继续吸收两天洞天气功果。
    mk("qihailaozu", "气海老祖（方源气道分身）", Rank::R8, Domain::ZhongZhou,
       FactionId::FangYuan, Dao::Qi, DaoLevel::Foundation,
       "fy_hao_xi", true, "fangyuan");
    // 研究报告 3.6 明确「诞生时已有八十多万气道道痕」——
    // 远超八转典型量级（世界观概数约 3—4 万），因为是分身且专司炼化气功果。
    // 此为原著个案，不得反推为八转通用值。
    v.back().self.daoMarksByDao[Dao::Qi] = 800000.0;
    v.back().self.daoMarks = 800000.0;
    // 吴帅：方源用于南疆、异族联盟处的分身／身份。
    // 研究报告 Table4 明确「南疆：人道、异族联盟、吴帅活动区」——故驻南疆，非西漠。
    mk("wushuai", "吴帅（方源南疆身份）", Rank::R7, Domain::NanJiang,
       FactionId::FangYuan, Dao::Enslave, DaoLevel::Great,
       "yz_nanjiang_aliance", true, "fangyuan");

    // 星宿仙尊：借元境与天庭底蕴复活重登尊位
    mk("xingxiu", "星宿仙尊", Rank::R9, Domain::ZhongZhou,
       FactionId::TianTing, Dao::Wisdom, DaoLevel::MarkFusion,
       "tt_central");
    // 天庭方面：龙公已退出，此处以天庭蛊仙群体代表
    mk("tianting_zhenren", "天庭虚窍蛊仙", Rank::R7, Domain::ZhongZhou,
       FactionId::TianTing, Dao::Heaven, DaoLevel::Great, "cn_yitian");

    // 巨阳仙尊：运道 / 气道巨头
    mk("juyang", "巨阳仙尊", Rank::R9, Domain::BeiYuan,
       FactionId::JuYang, Dao::Luck, DaoLevel::MarkFusion,
       "cn_changshengtian");

    // 幽魂魔尊：失智、被困、被多方争夺
    mk("youhun", "幽魂魔尊", Rank::R9, Domain::ZhongZhou,
       FactionId::YouHun, Dao::Soul, DaoLevel::Foundation, "cn_fengmo_ku");

    // 人道线：商心慈
    mk("shangxinci", "商心慈", Rank::R6, Domain::NanJiang,
       FactionId::RenDaoAlliance, Dao::Human, DaoLevel::Small, "cn_baohuangtian");

    // 彭达：新天外之魔（普通穿越者样本）
    NpcAgent pengda;
    pengda.self.id       = "pengda";
    pengda.self.name     = "彭达（新天外之魔）";
    pengda.self.rank     = Rank::R1;
    pengda.self.bornDomain = Domain::None;   // 天外来客，无任何本域免疫
    pengda.self.demonIdentity = DemonIdentity::NewTransmigrator;
    pengda.faction = FactionId::Neutral;
    pengda.self.dao.set(Dao::Wisdom, DaoLevel::Entry);
    {
        Location loc; loc.layer = RealmLayer::MortalSurface;
        loc.domain = Domain::NanJiang; loc.region = "南疆村寨";
        pengda.self.location = loc;
        pengda.self.dingXianYou.unlock(loc, SightSource::Arrived, 0, "穿越落点");
    }
    v.push_back(std::move(pengda));

    // 开局给部分 NPC 注入「争夺异人洞天」目标，匹配方源/天庭已占领状态
    for (auto& a : v) {
        if (a.self.id == "fangyuan") {
            a.targetSites = {"yz_long_ren", "tt_mao_min"};
            // 方源：至尊仙胎体，可吞并他窍（研究报告 2.2）
            a.hasZhiZunXianQiao = true;
        } else if (a.self.id == "xingxiu") {
            a.targetSites = {"yz_long_ren"};
        } else if (a.self.id == "qihailaozu") {
            // 气海老祖：研究报告 3.6 —— 处理豪豨洞天气功果，
            // 两天洞天气功果已膨胀至接近自爆边缘
            a.qiGongGuoStored = 3.0;
            a.qiGuoExplosionRisk = 0.35;
        }
    }
    return v;
}

} // namespace gr
