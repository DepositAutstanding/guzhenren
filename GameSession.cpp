// 游戏会话层实现 —— 指令分发与 UI 只读视图
//
// 本层不复制任何规则：移动、闭关、炼蛊等判定全部转发给原子系统，
// 这里只负责「分发 → 收集结果 → 记日志」，保证规则只有一处实现。
#include "gr/ui/GameSession.hpp"
#include "gr/core/CanonFigures.hpp"
#include "gr/cultivator/DaoTianLegacy.hpp"
#include "gr/gu/KillerMove.hpp"
#include "gr/gu/GuFeeding.hpp"

#ifdef _WIN32
#include <direct.h>      // Windows: mkdir
#else
#include <sys/stat.h>
#endif

#include "gr/cultivator/ThreeQi.hpp"
#include "gr/cultivator/DingXianYou.hpp"
#include "gr/core/CanonNumbers.hpp"

namespace gr {

GameSession::GameSession(std::uint64_t seed, std::string cacheDir) : world_(seed) {
    world_.buildVolume6Opening(24);

    // 二维地图：与洞天体系并行存在，负责「地表探索」这一层。
    //
    // 尺寸放大到 1024×768（约 78 万格）。之所以敢放这么大，是因为改成了
    // 分块流式：地形按需现算不占内存，只有探索状态按块落外存。
    // 界面只绘制视口内的局部，其余留在外存。
    tileMap_.setCacheDir(std::move(cacheDir));
    //
    //  地图尺度 —— 2048 × 1536 = 3,145,728 格（约 315 万）。
    //
    //  原为 1024×768（约 79 万格）。放大后：
    //    · 分块 64×64 → 32 列 × 24 行 = 768 块，常驻仍只 24 块
    //    · 全图若常驻约 25 MB，实际只占常驻块约 786 KB
    //    · 域界、地形区、地标皆用相对坐标（fx/fy），改尺寸自动适配
    //
    //  尺度仍为工程选择：原著未给可换算的比例尺
    //  （北原「八万多亿亩」与南疆「七八个地球总面积」互相矛盾），
    //  故不声称 1:1，界面上也不显示「走了多少里」。
    //
    tileMap_.resize(kMapW, kMapH);
    // 情报：把蛊方登记为「需先获得」的知识。
    // 未得蛊方则不见其名、不知其组成蛊、更不能炼制 ——
    // 否则界面把配方全摊开，探索与打探就失去意义。
    //
    // 但【凡蛊方开局即得】：凡人炼蛊是原著核心内容（方源一转便炼酒虫、
    // 月光蛊），凡人家族各有传承，基础凡蛊方谈不上「稀缺情报」。
    // 此前凡蛊方也要求情报，而打探又限六转以上 —— 两头堵死，
    // 凡人永远炼不了蛊。这是根本性设定错误，现按转数分野：
    //   凡蛊（一至五转）→ 开局即懂
    //   仙蛊（六转及以上）→ 须求得蛊方方可炼制
    intel_.registerRecipes(world_.refinery().recipes());
    // 凡蛊方在 IntelSystem 内部即被视为常识（knows() 直接返回真），
    // 不必在此逐条授予 —— 构造时 world_.player() 尚不存在，
    // 就算写了也授不出去。仙蛊方则须另行求得。
    intel_.registerIntel({IntelSystem::rumorId("fangyuan_dingxianyou"),
                          IntelCategory::Rumor, "定仙游在方源手上",
                          "定仙游乃仙蛊、世间唯一，归方源所有",
                          "据闻有仙蛊可三息抵达心中有印象之地",
                          "需求 + 原著：定仙游为方源所有", true});

    const MapGenResult mg = tileMap_.generate(world_.rng());
    if (!mg.ok) pushLog(false, "地图生成失败：" + mg.detail);
    else        pushLog(true, "地表舆图已铺开：" + mg.detail);

    // 出生点附近的盗天传承：开局即予提示，玩家一出门便可寻去
    pushLog(true, "传闻出生地附近有座古修窟穴，或与盗天魔尊的传承有关");
}

GameSession::~GameSession() {
    // 退出前把探索进度写到外存，否则本次所探之地全白探了
    tileMap_.flush();
}


// ---------------------------------------------------------------------------
//  指令分发
// ---------------------------------------------------------------------------
CommandResult GameSession::execute(const Command& cmd) {
    CommandResult r;

    // ------------------------------------------------------------------
    //  秘境守卫：身处洞天内时，地表/旅行类指令一律拒绝
    //
    //  少了这道闸就会出「人在洞天里，却把位置挪到地表」的自相矛盾 ——
    //  界面切回世界舆图仍可点地图，而 realm_ 还挂着，状态就此错乱。
    //  洞天内只可：秘境中移动（走 moveInCave，不经此）、离开、炼蛊等。
    // ------------------------------------------------------------------
    if (inCaveRealm()) {
        switch (cmd.kind) {
            case CmdKind::MoveOnMap:
            case CmdKind::MoveTo:
            case CmdKind::Jump:
            case CmdKind::Observe:
            case CmdKind::GatherQi:
            case CmdKind::EnterSeclusion:
            case CmdKind::LeaveSeclusion:
            case CmdKind::Cultivate:
                r = CommandResult::fail(cmd.kind,
                        "身在秘境中，须先离开洞天方可" +
                        std::string(to_string(cmd.kind)));
                record(r);
                return r;
            default: break;
        }
    }

    switch (cmd.kind) {
        case CmdKind::MoveTo:         r = doMoveTo(cmd);        break;
        case CmdKind::Jump:           r = doJump(cmd);          break;
        case CmdKind::Observe:        r = doObserve(cmd);       break;
        case CmdKind::GatherQi:       r = doGatherQi(cmd);      break;
        case CmdKind::EnterSeclusion: r = doEnterSeclusion(cmd);break;
        case CmdKind::Cultivate:      r = doCultivate(cmd);     break;
        case CmdKind::LeaveSeclusion: r = doLeaveSeclusion(cmd);break;
        case CmdKind::Breakthrough:   r = doBreakthrough(cmd);  break;
        case CmdKind::Refine:         r = doRefine(cmd);        break;
        case CmdKind::CastMove:       r = doCastMove(cmd);      break;
        case CmdKind::Hunt:           r = doHunt(cmd);          break;
        case CmdKind::RefineOpen:     r = doRefineOpen(cmd);    break;
        case CmdKind::RefinePick:     r = doRefinePick(cmd);    break;
        case CmdKind::RefineTechnique:r = doRefineTech(cmd);    break;
        case CmdKind::RefineBegin:    r = doRefineBegin(cmd);   break;
        case CmdKind::RefineCancel:   r = doRefineCancel(cmd);  break;
        case CmdKind::RefineTerminate:r = doRefineTerminate(cmd);break;
        case CmdKind::RefineTimeGu:   r = doRefineTimeGu(cmd);  break;
        case CmdKind::RefineMulti:    r = doRefineMulti(cmd);   break;
        case CmdKind::RefineClose:    r = doRefineClose(cmd);   break;
        case CmdKind::BagGather:      r = doBagGather(cmd);     break;
        case CmdKind::BagDrop:        r = doBagDrop(cmd);       break;
        case CmdKind::BagUse:         r = doBagUse(cmd);        break;
        case CmdKind::FeedGu:         r = doFeedGu(cmd);        break;
        case CmdKind::EnterBuilding:  r = doEnterBuilding(cmd); break;
        case CmdKind::NpcTalk:        r = doNpcTalk(cmd);       break;
        case CmdKind::NpcTrade:       r = doNpcTrade(cmd);      break;
        case CmdKind::NpcLearn:       r = doNpcLearn(cmd);      break;
        case CmdKind::NpcBorrow:      r = doNpcBorrow(cmd);     break;
        case CmdKind::NpcDuel:        r = doNpcDuel(cmd);       break;
        case CmdKind::Rename:         r = doRename(cmd);        break;
        case CmdKind::AcceptQuest:    r = doAcceptQuest(cmd);   break;
        case CmdKind::MoveOnMap:      r = doMoveOnMap(cmd);     break;
        case CmdKind::Inquire:        r = doInquire(cmd);       break;
        case CmdKind::Innovate:       r = doInnovate(cmd);      break;
        case CmdKind::NameGu:         r = doNameGu(cmd);        break;
        case CmdKind::Advance: {
            //  炼制中的材料蛊：推进期间豁免喂养消耗。
            //  否则长时炼制会在炉里把材料蛊饿死，炼蛊必然失败。
            {
                std::vector<GuId> skip;
                for (const auto& sl : bench_.slots())
                    for (GuId id : sl.materialInst) skip.push_back(id);
                world_.setFeedingSkip(std::move(skip));
            }
            auto rep = world_.step(cmd.ticks);
            r = CommandResult::succeed(CmdKind::Advance,
                                  "世界推进 " + std::to_string(cmd.ticks) + " 刻度");
            for (const auto& n : rep.notes) r.notes.push_back(n);
            // 炼蛊剩余时间以世界时间计 —— 世界推进即炼制推进
            tickRefining(cmd.ticks);
            break;
        }
        default:
            r = CommandResult::fail(CmdKind::Idle, "无效指令");
            break;
    }
    record(r);
    return r;
}

CommandResult GameSession::advance(int ticks) {
    Command c; c.kind = CmdKind::Advance; c.ticks = ticks;
    return execute(c);
}

void GameSession::record(const CommandResult& r) {
    pushLog(r.ok, std::string("[") + to_string(r.kind) + "] " + r.title);
    for (const auto& n : r.notes) pushLog(r.ok, "    " + n);
}

void GameSession::pushLog(bool ok, const std::string& text) {
    log_.emplace_back(now(), ok, text);
    while (log_.size() > logLimit_) log_.pop_front();
}

// ---------------------------------------------------------------------------
//  各指令实现
// ---------------------------------------------------------------------------
namespace {
const CaveParadise* findSite(const WorldMap& w, const std::string& id) {
    return id.empty() ? nullptr : w.findCave(id);
}
}

CommandResult GameSession::doMoveTo(const Command& c) {
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::MoveTo, "尚未创建玩家角色");
    const CaveParadise* site = findSite(world_.world(), c.siteId);
    if (!site) return CommandResult::fail(CmdKind::MoveTo, "目标地点不存在");

    auto mv = CultivatorSystem::moveTo(*p, site->location(), world_.world());
    if (!mv.ok())
        return CommandResult::fail(CmdKind::MoveTo, mv.detail.empty() ? "无法抵达" : mv.detail);

    CommandResult r = CommandResult::succeed(CmdKind::MoveTo, "抵达 " + site->name);
    r.notes.push_back("仙元 -" + std::to_string((long long)mv.value.essence));
    if (mv.value.daoMarks > 0.0)
        r.notes.push_back("道痕 -" + std::to_string((long long)mv.value.daoMarks));
    return r;
}

CommandResult GameSession::doJump(const Command& c) {
    // 秘境内无法定仙游跳往五域 —— 先出洞再说。
    // 定仙游是「认知锚定型位移」，锚的是五域地表坐标，
    // 洞天自成天地，不在同一张坐标网上。
    if (realm_)
        return CommandResult::fail(CmdKind::Jump,
                                   "身处秘境「" + realm_->name() +
                                   "」内，定仙游无从锚定外界（须先离开）");
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::Jump, "尚未创建玩家角色");
    const CaveParadise* site = findSite(world_.world(), c.siteId);
    if (!site) return CommandResult::fail(CmdKind::Jump, "目标地点不存在");

    // 定仙游铁律：只能跳「亲眼见过 / 抵达过 / 感知过」的坐标
    auto j = CultivatorSystem::jumpByDingXianYou(*p, site->location(), world_.world());
    if (!j.ok())
        return CommandResult::fail(CmdKind::Jump,
                                   j.detail.empty() ? "跳跃失败（坐标未在库中）" : j.detail);

    CommandResult r = CommandResult::succeed(CmdKind::Jump, "定仙游 → " + site->name);
    r.notes.push_back(j.detail);
    return r;
}

CommandResult GameSession::doObserve(const Command& c) {
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::Observe, "尚未创建玩家角色");
    const CaveParadise* site = findSite(world_.world(), c.siteId);
    if (!site) return CommandResult::fail(CmdKind::Observe, "目标地点不存在");

    const std::size_t before = p->dingXianYou.count();
    CultivatorSystem::observe(*p, site->location(), SightSource::Seen, now());
    const std::size_t after = p->dingXianYou.count();

    if (after == before)
        return CommandResult::succeed(CmdKind::Observe, site->name + "：坐标已在库中");

    return CommandResult::succeed(CmdKind::Observe,
                             site->name + " 已记入定仙游坐标库（" +
                             std::to_string(after) + " 处）");
}

CommandResult GameSession::doGatherQi(const Command& c) {
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::GatherQi, "尚未创建玩家角色");

    ThreeQiPool total;
    int done = 0;
    for (int i = 0; i < c.ticks; ++i) {
        auto got = CultivatorSystem::gatherQi(*p, world_.world(), 1.0);
        if (!got.ok())
            return CommandResult::fail(CmdKind::GatherQi,
                                       got.detail.empty() ? "无法搜集三气" : got.detail);
        p->qi.heaven += got.value.heaven;
        p->qi.earth  += got.value.earth;
        p->qi.human  += got.value.human;
        total.heaven += got.value.heaven;
        total.earth  += got.value.earth;
        total.human  += got.value.human;
        ++done;
    }

    CommandResult r = CommandResult::succeed(CmdKind::GatherQi, "外出搜集三气 ×" + std::to_string(done));
    r.notes.push_back("天 +" + std::to_string((long long)total.heaven) +
                      "，地 +" + std::to_string((long long)total.earth) +
                      "，人 +" + std::to_string((long long)total.human));
    return r;
}

CommandResult GameSession::doEnterSeclusion(const Command&) {
    // 秘境内不闭关：闭关要回自家仙窍/福地静修，
    // 在别人洞天里闭关既不安全也不合设定。
    if (realm_)
        return CommandResult::fail(CmdKind::EnterSeclusion,
                                   "身处秘境「" + realm_->name() +
                                   "」内，不宜闭关（须先离开）");
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::EnterSeclusion, "尚未创建玩家角色");

    auto r0 = CultivatorSystem::enterSeclusion(*p, world_.world());
    if (!r0.ok())
        return CommandResult::fail(CmdKind::EnterSeclusion,
                                   r0.detail.empty() ? "无法闭关" : r0.detail);

    CommandResult r = CommandResult::succeed(CmdKind::EnterSeclusion, "进入闭关，开始调和三气");
    r.notes.push_back("闭关期间：不可移动 / 不可战斗 / 不可外出");
    return r;
}

CommandResult GameSession::doCultivate(const Command& c) {
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::Cultivate, "尚未创建玩家角色");
    if (!p->threeQi.inSeclusion)
        return CommandResult::fail(CmdKind::Cultivate, "未处于闭关状态，无法调和");

    ThreeQiConfig cfg;
    int done = 0;
    bool becameBalanced = false;
    for (int i = 0; i < c.ticks; ++i) {
        auto rep = ThreeQiSystem::tick(*p, cfg);
        ++done;
        if (rep.becameBalanced) { becameBalanced = true; break; }
        if (p->essence <= 0.0) break;
    }

    CommandResult r = CommandResult::succeed(CmdKind::Cultivate,
                                        "闭关调和 ×" + std::to_string(done) + " 刻度");
    r.notes.push_back("当前三气差值 " +
                      std::to_string((int)p->qi.deviation()) +
                      "（阈值 " + std::to_string((int)cfg.tolerance) + "）");
    if (becameBalanced) r.notes.push_back("三气已平衡，可解除闭关");
    return r;
}

CommandResult GameSession::doLeaveSeclusion(const Command&) {
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::LeaveSeclusion, "尚未创建玩家角色");

    auto r0 = CultivatorSystem::leaveSeclusion(*p);
    if (!r0.ok())
        return CommandResult::fail(CmdKind::LeaveSeclusion,
                                   r0.detail.empty() ? "无法解除闭关" : r0.detail);

    return CommandResult::succeed(CmdKind::LeaveSeclusion, "解除闭关，恢复行动");
}

CommandResult GameSession::doBreakthrough(const Command&) {
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::Breakthrough, "尚未创建玩家角色");

    const int next = rank_value(p->rank) + 1;
    if (next > 9)
        return CommandResult::fail(CmdKind::Breakthrough, "已至九转，无可再进");

    auto r0 = CultivatorSystem::breakthrough(*p, static_cast<Rank>(next));
    if (!r0.ok())
        return CommandResult::fail(CmdKind::Breakthrough,
                                   r0.detail.empty() ? "突破失败" : r0.detail);

    CommandResult r = CommandResult::succeed(CmdKind::Breakthrough, r0.detail);
    return r;
}

// ---------------------------------------------------------------------------
//  催动杀招（蛊虫攻击）
//
//  此前「蛊虫攻击特效」只有数据结构与绘制分支，没有任何触发源 ——
//  等于管道铺好了却没通水。特效是杀招结算的【结果】，
//  故必须由本层在结算后填入 CommandResult.effects，UI 只负责播放。
// ---------------------------------------------------------------------------
namespace {
// 依流派推定特效形态。原著未系统记载各流派杀招的视觉形态，
// 故此处为工程映射，仅用于示意；接美术资源后由 spriteId 覆盖。
GuEffectKind effectKindFor(Dao dao) {
    switch (dao) {
        case Dao::Sword: case Dao::Light: case Dao::Star:
            return GuEffectKind::Beam;
        case Dao::Fire: case Dao::Thunder: case Dao::Refine:
            return GuEffectKind::Burst;
        case Dao::Water: case Dao::Ice: case Dao::Wind:
            return GuEffectKind::Projectile;
        case Dao::Poison: case Dao::Shadow: case Dao::Blood:
            return GuEffectKind::Mist;
        case Dao::Strength: case Dao::Earth: case Dao::Metal:
            return GuEffectKind::Shockwave;
        default:
            return GuEffectKind::Aura;
    }
}
} // namespace

CommandResult GameSession::doCastMove(const Command& c) {
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::CastMove, "尚未创建玩家角色");

    // 身处秘境时不可对地表施放 —— 洞天自成天地，不在五域坐标网上
    if (inCaveRealm())
        return CommandResult::fail(CmdKind::CastMove,
                                   "身处洞天之内，无从对五域地表施放杀招");

    const auto& moves = world_.killerMoves();
    const KillerMoveDef* def = nullptr;
    for (const auto& m : moves)
        if (m.id == c.castMoveId) { def = &m; break; }
    if (!def)
        return CommandResult::fail(CmdKind::CastMove, "未闻此杀招");

    // —— 门槛：修为与道境 ——
    if (static_cast<int>(p->rank) < static_cast<int>(def->requiredRank))
        return CommandResult::fail(CmdKind::CastMove,
                                   "修为不足，催不动「" + def->name + "」"
                                   "（须 " + std::string(to_string(def->requiredRank)) + "）");

    const DaoLevel lv = p->dao.get(def->dao);
    if (static_cast<int>(lv) < static_cast<int>(def->requiredDao))
        return CommandResult::fail(CmdKind::CastMove,
                                   "道境不足，「" + def->name + "」难以驾驭"
                                   "（须 " + std::string(to_string(def->requiredDao)) + "）");

    // —— 组件齐备（仅对资料库已收录且已建模板的组成蛊校验）——
    auto templates = world_.refinery().templates();
    for (GuId need : def->components) {
        if (need == 0) continue;
        bool has = false;
        for (const auto& inst : p->carriedGu)
            if (inst.templateId == need) { has = true; break; }
        const GuTemplate* t = world_.refinery().guTemplate(need);
        if (!has)
            return CommandResult::fail(CmdKind::CastMove,
                                       "缺「" + (t ? t->name : std::to_string(need)) +
                                       "」，" + def->name + "不成");
    }

    // —— 真元 / 仙元 ——
    if (p->essence < def->baseEssence)
        return CommandResult::fail(CmdKind::CastMove,
                                   std::string(p->essenceName()) + "不足，催不动「" +
                                   def->name + "」");

    // —— 结算 ——
    auto out = KillerMoveResolver::resolve(*def, lv, p->rank, p->essence,
                                           p->carriedGu, templates,
                                           world_.rng().next());
    if (!out.executed)
        return CommandResult::fail(CmdKind::CastMove,
                                   out.detail.empty() ? "催动失败" : out.detail);

    p->essence = std::max(0.0, p->essence - out.essenceCost);

    // 反噬与崩解：崩碎的蛊虫自囊中移除
    for (GuId sh : out.shatteredGu) {
        for (auto it = p->carriedGu.begin(); it != p->carriedGu.end(); ++it) {
            if (it->instanceId == sh) { p->carriedGu.erase(it); break; }
        }
    }
    if (out.backlash > 0.0) {
        p->health = std::max(0.0, p->health - out.backlash);
    }

    CommandResult r = CommandResult::succeed(
        CmdKind::CastMove,
        "催动「" + def->name + "」" +
        (out.collapsed ? "（崩解！）" : (out.incomplete ? "（残缺）" : "")));
    r.notes.push_back(out.detail);
    r.notes.push_back(std::string(p->essenceName()) + " -" +
                      std::to_string(static_cast<long long>(out.essenceCost)));
    r.notes.push_back("威力 " + std::to_string(static_cast<long long>(out.power)));
    if (out.backlash > 0.0)
        r.notes.push_back("反噬 " + std::to_string(static_cast<long long>(out.backlash)));
    for (GuId sh : out.shatteredGu) {
        const GuTemplate* t = world_.refinery().guTemplate(sh);
        r.notes.push_back("崩碎：" + (t ? t->name : std::to_string(sh)));
    }

    // —— 特效：由规则层填入，UI 只负责播放 ——
    GuEffectRequest fx;
    fx.kind      = effectKindFor(def->dao);
    fx.guName    = def->name;
    fx.dao       = def->dao;
    fx.fromX     = tileMap_.playerX();
    fx.fromY     = tileMap_.playerY();
    fx.toX       = (c.castToX >= 0) ? c.castToX : tileMap_.playerX();
    fx.toY       = (c.castToY >= 0) ? c.castToY : tileMap_.playerY();
    // 强度随实际威力缩放；崩解则衰减 —— 视觉须与结算一致
    fx.intensity = out.collapsed ? 0.4f
                                 : static_cast<float>(std::clamp(out.power / 200.0, 0.5, 3.0));
    r.effects.push_back(fx);
    return r;
}



// ===========================================================================
//  出身（需求 11）
// ===========================================================================

AwakeningScene GameSession::awakeningScene() const { return awakening_; }

CommandResult GameSession::startWithOrigin(OriginId id, const std::string& name,
                                           const std::string& clan,
                                           const std::string& spawnLandmarkId) {
    const OriginDef* od = origin(id);
    if (!od) return CommandResult::fail(CmdKind::Rename, "无此出身");
    if (name.empty())
        return CommandResult::fail(CmdKind::Rename, "名号不可为空");

    // 姓名约束：某些身份必须姓某姓（如百家寨孤儿必须姓「百」）
    if (!od->requiredSurname.empty()) {
        // 中文姓氏取首字比较
        const std::string head = name.substr(0, od->requiredSurname.size());
        if (head != od->requiredSurname)
            return CommandResult::fail(CmdKind::Rename,
                "此身份须姓「" + od->requiredSurname + "」，如「" +
                od->requiredSurname + "无名」");
    }

    // 依身份的起始修为创建角色
    world_.createDefaultPlayer(name, od->domain, od->startRank);
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::Rename, "创建角色失败");

    // —— 起始所得 ——
    for (const auto& i : od->startIntel) p->learnIntel(i);
    // 起始蛊虫：按名匹配模板，仅凡蛊可批量给（仙蛊受唯一铁律约束）
    {
        auto tpls = world_.refinery().templates();
        GuId nid = 700000;
        for (const auto& gn : od->startGuNames) {
            for (const auto& t : tpls) {
                if (t.name != gn) continue;
                if (t.isImmortal()) continue;      // 仙蛊唯一，不得开局滥发
                GuInstance g; g.templateId = t.id; g.instanceId = nid++;
                g.holder = p->id;
                p->carriedGu.push_back(g);
                break;
            }
        }
    }
    for (const auto& st : od->startSites) p->ownedSites.push_back(st);

    // 起始所在地点
    if (!od->startSiteId.empty()) {
        p->location.siteId = od->startSiteId;
        p->location.region = od->startSiteId;
    }

    //
    //  初始定位 —— 必须在此处调用。
    //  placePlayerAtStart() 此前【零调用点】：函数写好了却没人调，
    //  于是 px_/py_ 一直是初值 (-1,-1)，13 种出身开局全部站在地图外；
    //  而 TileMap::at() 又没有边界检查，at(-1,-1) 直接越界写堆
    //  （ASAN 实测 heap-buffer-overflow）。
    //  这与「特效 emit 零调用点」是同一类疏漏：只验证函数能编译、
    //  能画出来，没验证有没有人真的调它。
    //
    placePlayerAtStart(spawnLandmarkId);

    //
    //  出身因缘预置为情谊基线。
    //
    //  此前因缘只参与「态度」计算，却不写回情谊值 ——
    //  于是界面显示「情谊 0」而态度已是「友善」，两处对不上；
    //  求教时又提示「尚差 50 点」，玩家明明有过一面之缘却看不出优待。
    //  因缘是天生亲近，开局即定，理应落在情谊上。
    //
    for (const auto& soc : allSocials()) {
        if (!soc.hasOriginTie) continue;
        if (id != soc.affinityOrigin) continue;
        npcRel_[soc.npcId].affinity = 30;
    }

    hasOrigin_   = true;
    originIdVal_ = id;
    originId_    = &originIdVal_;
    originClan_  = clan;
    awakening_   = awakeningSceneFor(id, name);

    CommandResult r = CommandResult::succeed(CmdKind::Rename,
        "入世：" + od->name + "（" + name + "）");
    r.notes.push_back("出身：" + od->shortDesc);
    if (!clan.empty()) r.notes.push_back("族属：" + clan);
    if (od->awakens) {
        r.notes.push_back("开窍：" + awakening_.result);
    }
    if (od->hasBloodline && !od->bloodlineNote.empty())
        r.notes.push_back("血脉：" + od->bloodlineNote);
    if (od->socialTie > 0) r.notes.push_back("人脉：城内有些熟面孔");
    return r;
}

// ===========================================================================
//  炼蛊台（需求 8）
//
//  炼蛊不再是一条指令即刻出结果，而是一段跨帧过程：
//    开炉 → 2 秒原地等待 → 备料与选手法 → 起炉 → 倒计时 → 收蛊
//  判定一律在此处完成，界面只读状态并下达指令，
//  避免出现「界面显示能终止、规则却拒绝」的脱节。
// ===========================================================================

bool GameSession::hasTimeGu() const {
    const Cultivator* p = world_.player();
    if (!p) return false;
    auto tpls = world_.refinery().templates();
    for (const auto& inst : p->carriedGu) {
        for (const auto& t : tpls)
            if (t.id == inst.templateId && t.name.find("光阴") != std::string::npos)
                return true;
    }
    return false;
}

bool GameSession::hasMultiTaskGu() const {
    const Cultivator* p = world_.player();
    if (!p) return false;
    auto tpls = world_.refinery().templates();
    for (const auto& inst : p->carriedGu) {
        for (const auto& t : tpls)
            // 一心二用蛊 / 一心三用蛊 ……
            if (t.id == inst.templateId && t.name.find("一心") != std::string::npos)
                return true;
    }
    return false;
}

void GameSession::tickRefinePrepare(double dtSeconds) {
    std::vector<std::string> notes;
    bench_.tickPrepare(dtSeconds, notes);
    for (const auto& n : notes) pushLog(true, n);
}

void GameSession::tickRefining(Tick dt) {
    std::vector<std::string> notes;
    bench_.tick(dt, notes);
    // 炼成的蛊自动与蛊师建立联系（归入囊中），界面随后消失
    for (int i = 0; i < bench_.slotCount(); ++i) {
        const RefineSession& rs = bench_.slots()[i];
        if (rs.stage == RefineStage::Finished && rs.outcome != RefineOutcome::Blocked)
            harvestRefined(i);
    }
    for (const auto& n : notes) pushLog(true, n);
}

void GameSession::harvestRefined(int slot) {
    if (slot < 0 || slot >= bench_.slotCount()) return;
    RefineSession& rs = bench_.slots()[slot];
    //  一炉只结算一次。
    //  此前 tickRefining 对每个 Finished 槽都调用本函数，而世界每推进一次
    //  就调用一次 —— 已完成的一炉遂被反复收蛊。实测推进 10 刻：
    //  囊中蛊 3→12、材料 21→12，等于凭空无限产出。
    if (rs.harvested) return;
    rs.harvested = true;
    if (rs.outcome == RefineOutcome::Blocked) return;
    Cultivator* p = world_.player();
    if (!p) return;

    const GuRecipe* rp = nullptr;
    for (const GuRecipe* r : world_.refinery().recipes())
        if (r->id == rs.recipeId) { rp = r; break; }
    if (!rp) return;

    // —— 提前收炉：稳定态出半成品，不稳定则失败 ——
    //
    //  此处【不可】再走一遍正式炼制判定：半成品本就是未完成之物，
    //  若拿它去跑齐备性与铁律校验，多半会被判「不齐」而变成失败品，
    //  于是「稳定态收炉得半成品」这条规则形同虚设。
    if (rs.terminated) {
        if (!rs.stable) { rs.stage = RefineStage::Finished; return; }
        GuTemplate tpl;
        bool found = false;
        for (const auto& t : world_.refinery().templates())
            if (t.name.find(rp->name) != std::string::npos) { tpl = t; found = true; break; }
        if (!found) { rs.stage = RefineStage::Finished; return; }
        GuInstance half;
        half.templateId = tpl.id;
        half.instanceId = nextGuInstanceId_++;
        half.holder     = p->id;
        half.defective  = true;      // 半成品
        half.integrity  = 0.5;
        p->carriedGu.push_back(half);
        rs.productName = tpl.name + "（半成品）";
        rs.detail      = "提前收炉，只得半成品";
        rs.stage = RefineStage::Finished;
        return;
    }

    // —— 自然成蛊：照蛊方正式炼一次，仍走铁律判定（无蛊方不可炼、仙蛊唯一等）——
    RefineRequest req;
    req.recipeId        = rp->id;
    req.refinerId       = p->id;
    req.refinerIsPlayer = true;
    req.refinerDao      = p->dao.get(rp->dao);
    req.refinerEssence  = p->essence;
    req.componentGu     = p->carriedGu;
    //  材料背包必须喂进去 —— 此前从未填充，于是凡蛊方
    //  （如四味酒虫需「酸甜苦辣四味美酒」）一律被判「材料不齐」，炼不出来。
    req.inventory       = p->bag.asRequirements();
    req.rngRoll         = world_.rng().next();

    //  起炉前先按蛊方校验材料；不足则不进炼制判定，直接说明缺什么
    std::vector<std::string> missing;
    if (!p->bag.canAfford(*rp, &missing)) {
        std::string m;
        for (const auto& x : missing) m += x + " ";
        rs.outcome = RefineOutcome::Blocked;
        rs.detail  = "材料不齐，缺：" + m;
        rs.stage   = RefineStage::Finished;
        return;
    }

    auto res = world_.refinery().refine(req, world_.registry());

    if (res.ok()) {
        p->essence = std::max(0.0, p->essence - res.essenceSpent);
        // 炼成即消耗掉该方所需材料（合炼用蛊同理消耗）
        p->bag.consume(*rp);
        p->carriedGu.push_back(res.product);
        rs.productName = res.guName;
    } else {
        // 提前收炉的半成品 / 失败品：不计入囊中，但保留结论供界面显示
        rs.outcome = RefineOutcome::Failure;
        rs.detail  = res.detail.empty() ? "未能成蛊" : res.detail;
    }
    rs.stage = RefineStage::Finished;
}


// ===========================================================================
//  背包（需求 12）
// ===========================================================================

std::vector<const ItemStack*> GameSession::bagItems() const {
    const Cultivator* p = world_.player();
    return p ? p->bag.items() : std::vector<const ItemStack*>{};
}

CommandResult GameSession::doBagGather(const Command&) {
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::BagGather, "尚未创建玩家角色");
    if (inCaveRealm())
        return CommandResult::fail(CmdKind::BagGather, "秘境之中另有天地，采集另论");
    if (p->threeQi.inSeclusion)
        return CommandResult::fail(CmdKind::BagGather, "闭关中无法外出采集");

    //
    //  按所处地形产出 —— 山海各有其产。
    //
    //  此前只有「山川→痕石、灵脉→灵石」两处特判，其余 23 种地形
    //  一律走 default 采到【止血草】：沙漠里长草、海里长草、
    //  火山口长草、冰原上长草 —— 与「山海各有其产」直接冲突。
    //  现将地形按族分类，各族各产其物。
    //
    const Terrain t = tileMap_.at(tileMap_.playerX(), tileMap_.playerY()).terrain;
    struct Yield {
        const char* name; double lo, hi; ItemKind kind;
        const char* source; const char* desc;
    };
    static const Yield kSpirit = {"灵石", 1, 3, ItemKind::Treasure,
        "工程设定", "灵脉所结，通用硬通货"};
    static const Yield kHerb   = {"止血草", 1, 2, ItemKind::Consumable,
        "工程设定", "嚼碎敷于伤处，可缓缓恢复"};
    static const Yield kStone  = {"痕石", 1, 3, ItemKind::Material,
        "资料库蛊方表明载：痕石蛊所需材料", "承载道痕的矿石"};
    static const Yield kWater  = {"水元石", 1, 2, ItemKind::Material,
        "工程设定", "水域灵气所凝，性阴寒，尚未见于已知蛊方"};
    static const Yield kSand   = {"火晶砂", 1, 3, ItemKind::Material,
        "工程设定", "旱漠烈日炙烤之砂，触之温热"};
    static const Yield kIce    = {"寒玉", 1, 2, ItemKind::Material,
        "工程设定", "冰原深处所结，握之生寒"};
    static const Yield kVenom  = {"毒瘴草", 1, 2, ItemKind::Material,
        "工程设定", "瘴气林中自生，含毒，须慎用"};
    static const Yield kAbyss  = {"幽矿", 1, 3, ItemKind::Material,
        "工程设定", "深渊暗处所出，质地沉冷"};
    static const Yield kFire   = {"火精", 1, 2, ItemKind::Material,
        "工程设定", "火脉精髓，久持烫手"};
    static const Yield kFloat  = {"浮空石", 1, 2, ItemKind::Material,
        "工程设定", "飞岛所依托之石，轻若无物"};
    static const Yield kSpring = {"灵泉液", 1, 2, ItemKind::Consumable,
        "工程设定", "灵泉所出，饮之可缓缓回复"};

    const Yield* y = &kHerb;
    switch (t) {
        // —— 山石之属 ——
        case Terrain::Mountain:
        case Terrain::Foothill:
        case Terrain::Hill:
        case Terrain::StoneForest:   y = &kStone;  break;
        // —— 灵脉 ——
        case Terrain::SpiritVein:    y = &kSpirit; break;
        // —— 草木之属 ——
        case Terrain::Plain:
        case Terrain::Forest:
        case Terrain::Rainforest:
        case Terrain::Grassland:
        case Terrain::Oasis:
        case Terrain::Wetland:
        case Terrain::Bamboo:
        case Terrain::Island:
        case Terrain::Coast:         y = &kHerb;   break;
        // —— 水域 ——
        case Terrain::Water:
        case Terrain::Lake:
        case Terrain::Waterfall:
        case Terrain::Sea:
        case Terrain::Shoal:
        case Terrain::Undercurrent:  y = &kWater;  break;
        // —— 旱漠 ——
        case Terrain::Desert:
        case Terrain::Gobi:
        case Terrain::SandDune:      y = &kSand;   break;
        // —— 冰霜 ——
        case Terrain::IceField:
        case Terrain::SnowPeak:      y = &kIce;    break;
        // —— 瘴毒 ——
        case Terrain::ToxicForest:
        case Terrain::MistCity:      y = &kVenom;  break;
        // —— 幽深 ——
        case Terrain::Abyss:
        case Terrain::Cave:
        case Terrain::Sinkhole:
        case Terrain::EarthRift:     y = &kAbyss;  break;
        // —— 火脉 ——
        case Terrain::Volcano:
        case Terrain::LavaRock:      y = &kFire;   break;
        // —— 飞空与灵泉 ——
        case Terrain::FlyingIsland:  y = &kFloat;  break;
        case Terrain::Spring:        y = &kSpring; break;

        // —— 不可采集 ——
        case Terrain::Void:
        case Terrain::Wall:
        case Terrain::SkyRift:
            return CommandResult::fail(CmdKind::BagGather,
                   "此地无可采之物");
        default:                     y = &kHerb;   break;
    }

    const double n = y->lo + world_.rng().next() * (y->hi - y->lo);
    p->bag.add(y->name, n, y->kind, y->source, y->desc);
    CommandResult r = CommandResult::succeed(CmdKind::BagGather, "就地采集");
    r.notes.push_back("采得 " + std::string(y->name) + " ×" + std::to_string((int)n));
    return r;
}

CommandResult GameSession::doBagDrop(const Command& c) {
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::BagDrop, "尚未创建玩家角色");
    if (c.itemName.empty())
        return CommandResult::fail(CmdKind::BagDrop, "未指定物品");
    if (!p->bag.has(c.itemName, c.itemAmount))
        return CommandResult::fail(CmdKind::BagDrop, "囊中无此物，或数量不足");
    p->bag.drop(c.itemName, c.itemAmount);
    return CommandResult::succeed(CmdKind::BagDrop,
                                  "弃去 " + c.itemName + " ×" + std::to_string((int)c.itemAmount));
}

CommandResult GameSession::doBagUse(const Command& c) {
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::BagUse, "尚未创建玩家角色");
    const ItemStack* st = p->bag.find(c.itemName);
    if (!st) return CommandResult::fail(CmdKind::BagUse, "囊中无此物");
    if (st->kind != ItemKind::Consumable)
        return CommandResult::fail(CmdKind::BagUse, "此物非消耗品，无从用起");

    // 消耗品功效（工程设定）
    if (c.itemName == "止血草") {
        if (p->health >= 1.0)
            return CommandResult::fail(CmdKind::BagUse, "身无伤损，不必用药");
        p->health = std::min(1.0, p->health + 0.15);
        p->bag.take(c.itemName, 1.0);
        return CommandResult::succeed(CmdKind::BagUse,
            "敷用止血草，伤势稍缓（气血 " + std::to_string((int)(p->health*100)) + "%）");
    }
    return CommandResult::fail(CmdKind::BagUse, "尚不知此物用法");
}


// ===========================================================================
//  喂养（蛊虫平时要吃饭）
// ===========================================================================
// 保留一位小数并去尾零：1.5 显示为 "1.5"，而非 (int) 截断成的 "1"
static std::string trimDouble(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f", v);
    std::string s(buf);
    //  去小数部分尾零；若小数全为 0 则连小数点一并去掉
    //  （600.0 → "600"，1.5 → "1.5"）
    if (s.find('.') != std::string::npos) {
        while (s.size() > 1 && s.back() == '0') s.pop_back();
        if (s.back() == '.') s.pop_back();
    }
    return s;
}

// ===========================================================================
//  进入建筑（聚落内）
// ===========================================================================
CommandResult GameSession::doEnterBuilding(const Command& c) {
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::EnterBuilding, "尚未创建角色");

    const Settlement* st = world_.settlements().byId(c.settlementId);
    if (!st)
        return CommandResult::fail(CmdKind::EnterBuilding, "无此聚落");

    const Building* b = nullptr;
    for (const auto& x : st->buildings)
        if (x.id == c.buildingId) { b = &x; break; }
    if (!b)
        return CommandResult::fail(CmdKind::EnterBuilding, "此间无此建筑");

    //
    //  须人在该聚落所在地标上 —— 不能隔着半个五域去逛商铺。
    //
    //
    //  须人就在该聚落所在地标上 —— 不能隔着半个五域去逛商铺。
    //
    {
        const Landmark* lm = tileMap_.landmarkAt(tileMap_.playerX(),
                                                 tileMap_.playerY());
        if (!lm || lm->id != st->landmarkId)
            return CommandResult::fail(CmdKind::EnterBuilding,
                "你不在" + st->name + "，无从入内");
    }

    switch (b->use) {
        case BuildingUse::Trade: {
            //
            //  买卖以【灵石】为通货。价格表为工程设定 ——
            //  原著未给出系统的物价表，此处只取「灵石为通行货币、
            //  各方势力各自铸造」这一可核验前提。
            //
            static const struct { const char* name; double price; } kPrice[] = {
                {"灵石",   1.0},
                {"痕石",  10.0},
                {"止血草", 5.0},
                {"兽肉",   8.0},
                {"酒水",   6.0},
                {"月兰花瓣", 12.0},
                {"酸甜苦辣四味美酒", 50.0},
                {"玉石",  20.0},
                {"白骨",   6.0},
            };
            auto priceOf = [&](const std::string& n) -> double {
                for (const auto& kv : kPrice)
                    if (n == kv.name) return kv.price;
                return 0.0;
            };

            //  不指名物品 → 报出货架（供界面展示）
            if (c.itemName.empty()) {
                CommandResult r = CommandResult::succeed(CmdKind::EnterBuilding,
                    "走进" + b->name + "，货架如下");
                for (const auto& kv : kPrice)
                    r.notes.push_back(std::string(kv.name) + "　单价 " +
                                      trimDouble(kv.price) + " 灵石");
                r.notes.push_back("（以灵石为通货；物价为工程设定）");
                return r;
            }

            const double unit = priceOf(c.itemName);
            if (unit <= 0.0)
                return CommandResult::fail(CmdKind::EnterBuilding,
                    "本店不经营「" + c.itemName + "」");
            if (std::abs(c.itemAmount) < 1e-9)
                return CommandResult::fail(CmdKind::EnterBuilding, "未说买多少");

            if (c.itemAmount > 0.0) {
                //  —— 买入 ——
                const double cost = unit * c.itemAmount;
                if (!p->bag.has("灵石", cost))
                    return CommandResult::fail(CmdKind::EnterBuilding,
                        "灵石不足（需 " + trimDouble(cost) + "）");
                p->bag.take("灵石", cost);
                p->bag.add(c.itemName, c.itemAmount);
                return CommandResult::succeed(CmdKind::EnterBuilding,
                    "买入 " + c.itemName + " ×" + trimDouble(c.itemAmount) +
                    "，付灵石 " + trimDouble(cost));
            }
            //  —— 卖出 ——
            const double qty = -c.itemAmount;
            if (!p->bag.has(c.itemName, qty))
                return CommandResult::fail(CmdKind::EnterBuilding,
                    "囊中没有那么多" + c.itemName);
            p->bag.take(c.itemName, qty);
            //  卖出按六折 —— 商铺要赚差价，这是商铺的常态
            const double gain = unit * qty * 0.6;
            p->bag.add("灵石", gain);
            return CommandResult::succeed(CmdKind::EnterBuilding,
                "卖出 " + c.itemName + " ×" + trimDouble(qty) +
                "，得灵石 " + trimDouble(gain));
        }

        case BuildingUse::Learn: {
            if (b->teachesTechnique == 0)
                return CommandResult::fail(CmdKind::EnterBuilding,
                    "此处无人授业");
            if (p->knowsTechnique(b->teachesTechnique))
                return CommandResult::fail(CmdKind::EnterBuilding,
                    "此手法你已学会");
            const RefineTechnique* tn = refineTechnique(b->teachesTechnique);
            if (!tn)
                return CommandResult::fail(CmdKind::EnterBuilding, "无此手法");
            //  「三气调和炼法」须蛊仙方能施展 —— 凡人无三气
            if (tn->id == 5 && !is_immortal_rank(p->rank))
                return CommandResult::fail(CmdKind::EnterBuilding,
                    "「" + tn->name + "」须蛊仙方可修习");
            p->learnTechnique(b->teachesTechnique);
            return CommandResult::succeed(CmdKind::EnterBuilding,
                "于" + st->name + "「" + b->name + "」习得手法：" + tn->name);
        }

        case BuildingUse::Recipe: {
            //  查方：求得一条尚未得知的蛊方
            auto recs = world_.refinery().recipes();
            std::vector<const GuRecipe*> unknown;
            for (const auto* r : recs) {
                if (!r) continue;
                if (intel_.knows(*p, IntelSystem::recipeId(r->id))) continue;
                //  仙蛊方须六转以上才看得懂
                if (r->isImmortal() && !is_immortal_rank(p->rank)) continue;
                unknown.push_back(r);
            }
            if (unknown.empty())
                return CommandResult::fail(CmdKind::EnterBuilding,
                    "此间所藏蛊方你都已尽知");
            //  确定性取一条：不引入额外随机源，便于测试复现
            const GuRecipe* got = unknown.front();
            intel_.learn(*p, IntelSystem::recipeId(got->id));
            return CommandResult::succeed(CmdKind::EnterBuilding,
                "于藏经阁查得蛊方：" + got->name);
        }

        case BuildingUse::Rest: {
            if (p->health >= 1.0)
                return CommandResult::fail(CmdKind::EnterBuilding,
                    "精神饱满，不必歇息");
            const double before = p->health;
            p->health = std::min(1.0, p->health + 0.4);
            return CommandResult::succeed(CmdKind::EnterBuilding,
                "于客栈歇息一夜，气血由 " +
                std::to_string((int)(before * 100)) + "% 回复至 " +
                std::to_string((int)(p->health * 100)) + "%");
        }

        case BuildingUse::Awaken: {
            if (!b->mortalOnly)
                return CommandResult::fail(CmdKind::EnterBuilding,
                    "此处不办开窍");
            if (is_immortal_rank(p->rank))
                return CommandResult::fail(CmdKind::EnterBuilding,
                    "你早已开窍，无须再行大典");
            return CommandResult::fail(CmdKind::EnterBuilding,
                "开窍大典需族中主持（开局流程已行过）");
        }

        case BuildingUse::Passage: {
            if (b->toll <= 0.0)
                return CommandResult::succeed(CmdKind::EnterBuilding,
                    "顺利入城");
            if (!p->bag.has("灵石", b->toll))
                return CommandResult::fail(CmdKind::EnterBuilding,
                    "灵石不足，入城须纳 " + trimDouble(b->toll) + " 块");
            p->bag.take("灵石", b->toll);
            return CommandResult::succeed(CmdKind::EnterBuilding,
                "纳灵石 " + trimDouble(b->toll) + " 块，得以入内");
        }

        case BuildingUse::Gather: {
            //  产出：按建筑类型给对应材料
            static const std::pair<BuildingType, const char*> kYield[] = {
                {BuildingType::Mine,       "痕石"},
                {BuildingType::HerbGarden, "止血草"},
                {BuildingType::BeastPen,   "兽肉"},
                {BuildingType::GuPen,      "灵石"},
            };
            const char* what = nullptr;
            for (const auto& kv : kYield)
                if (kv.first == b->type) { what = kv.second; break; }
            if (!what)
                return CommandResult::fail(CmdKind::EnterBuilding,
                    "此处无产出");
            p->bag.add(what, 2.0);
            return CommandResult::succeed(CmdKind::EnterBuilding,
                std::string("于此取得 ") + what + " ×2");
        }

        case BuildingUse::Practice:
            return CommandResult::fail(CmdKind::EnterBuilding,
                "演武切磋尚未开启（建筑已登记，功能待补）");

        case BuildingUse::Refine:
            return CommandResult::fail(CmdKind::EnterBuilding,
                "此处可开炉：请於「支线」页选蛊方后点「炼蛊」");

        default:
            return CommandResult::succeed(CmdKind::EnterBuilding,
                "参观" + b->name + "：" + b->desc);
    }
}

CommandResult GameSession::doFeedGu(const Command& c) {
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::FeedGu, "尚未创建玩家角色");

    GuInstance* gi = nullptr;
    for (auto& g : p->carriedGu)
        if (g.instanceId == c.feedGuId) { gi = &g; break; }
    if (!gi)
        return CommandResult::fail(CmdKind::FeedGu, "囊中无此蛊");
    if (gi->state == GuState::Destroyed)
        return CommandResult::fail(CmdKind::FeedGu, "此蛊已死，喂也无用");
    //  封印 / 封存中的蛊处于静止，不食不耗，也无从下口
    if (gi->state == GuState::Sealed)
        return CommandResult::fail(CmdKind::FeedGu, "此蛊被封印，处于静止，无须喂养");
    if (gi->state == GuState::Stored)
        return CommandResult::fail(CmdKind::FeedGu, "此蛊已封存，无须喂养");

    const GuTemplate* t = world_.refinery().guTemplate(gi->templateId);
    const std::string gname = t ? t->name : "此蛊";

    if (c.feedByEssence) {
        //  —— 真元喂养 ——
        //  原著中蛊师确以真元豢养蛊虫，但专食效果更佳，
        //  故真元喂养只到 kEssenceFeedCap（0.8），专食方为满。
        if (gi->fullness >= kEssenceFeedCap)
            return CommandResult::fail(CmdKind::FeedGu,
                gname + "尚不须以真元喂养（专食可喂至全饱）");
        const double cost = essenceFeedCost(t ? t->rank : Rank::R1);
        if (p->essence < cost)
            return CommandResult::fail(CmdKind::FeedGu,
                "真元不足（需 " + trimDouble(cost) + "）");
        p->essence -= cost;
        gi->fullness = kEssenceFeedCap;
        gi->starveTicks = 0;
        return CommandResult::succeed(CmdKind::FeedGu,
            "以真元豢养" + gname + "，饱食度升至八成");
    }

    //  —— 专食喂养 ——
    const std::string food = primaryFeedName(t ? t->feed : std::string{});
    if (food.empty())
        return CommandResult::fail(CmdKind::FeedGu,
            "不知" + gname + "吃些什么（资料未载喂养之物）");
    if (gi->fullness >= 1.0)
        return CommandResult::fail(CmdKind::FeedGu, gname + "已然饱食");

    //  背包中依次尝试：主食名 → 描述串里的其它候选
    std::vector<std::string> tries;
    tries.push_back(food);
    for (const auto& n : parseFeedNames(t ? t->feed : std::string{}))
        if (n != food) tries.push_back(n);

    for (const auto& n : tries) {
        if (!p->bag.has(n, 1.0)) continue;
        p->bag.take(n, 1.0);
        gi->fullness = 1.0;
        gi->starveTicks = 0;
        return CommandResult::succeed(CmdKind::FeedGu,
            "喂以" + n + "，" + gname + "饱食（功效尽复）");
    }
    return CommandResult::fail(CmdKind::FeedGu,
        "囊中无" + gname + "爱吃之物（需：" + food + "）");
}

CommandResult GameSession::doRefineOpen(const Command& c) {
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::RefineOpen, "尚未创建玩家角色");
    if (inCaveRealm())
        return CommandResult::fail(CmdKind::RefineOpen, "秘境之中不便开炉，须先离开");
    if (p->threeQi.inSeclusion)
        return CommandResult::fail(CmdKind::RefineOpen, "闭关中无法炼蛊");

    const GuRecipe* rp = nullptr;
    for (const GuRecipe* r : world_.refinery().recipes())
        if (r->id == c.refineRecipe) { rp = r; break; }
    if (!rp)
        return CommandResult::fail(CmdKind::RefineOpen, "未闻此蛊方");

    // 铁律：无蛊方绝对无法炼制 —— 未曾得知的配方连名字都不该出现
    if (!intel_.knows(*p, IntelSystem::recipeId(rp->id)))
        return CommandResult::fail(CmdKind::RefineOpen,
                                   "未得「" + rp->name + "」的蛊方，无从开炉");

    bench_.open(rp->id, rp->name);
    CommandResult r = CommandResult::succeed(CmdKind::RefineOpen,
                                             "开炉：" + rp->name);
    r.notes.push_back("整备炉火……（人物原地候 2 秒）");
    return r;
}

CommandResult GameSession::doRefinePick(const Command& c) {
    if (!bench_.isOpen())
        return CommandResult::fail(CmdKind::RefinePick, "炼蛊台未开");
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::RefinePick, "尚未创建玩家角色");
    const int slot = (c.refineSlot >= 0) ? c.refineSlot : bench_.curSlot();
    // 点一次选中、两次放弃
    const bool picked = bench_.toggleMaterial(slot, c.refineMat);
    // refineMat 是实例 id：先查实例得模板，再得名
    GuId tid = 0;
    for (const auto& g : p->carriedGu)
        if (g.instanceId == c.refineMat) { tid = g.templateId; break; }
    auto tpls = world_.refinery().templates();
    std::string nm = "（未知之蛊）";
    for (const auto& t : tpls) if (t.id == tid) nm = t.name;
    return CommandResult::succeed(CmdKind::RefinePick,
        picked ? ("投入 " + nm) : ("取回 " + nm));
}

CommandResult GameSession::doRefineTech(const Command& c) {
    if (!bench_.isOpen())
        return CommandResult::fail(CmdKind::RefineTechnique, "炼蛊台未开");
    Cultivator* p = world_.player();
    // 手法列表中所有手法均为玩家已学过的手法 —— 未学者不得选
    if (p && !p->knowsTechnique(c.refineTech))
        return CommandResult::fail(CmdKind::RefineTechnique,
                                   "尚未习得此手法（可读书、拜师以学）");
    const int slot = (c.refineSlot >= 0) ? c.refineSlot : bench_.curSlot();
    bench_.setTechnique(slot, c.refineTech);
    const RefineTechnique* t = refineTechnique(c.refineTech);
    return CommandResult::succeed(CmdKind::RefineTechnique,
                                  "手法：" + (t ? t->name : std::string("？")));
}

CommandResult GameSession::doRefineBegin(const Command& c) {
    if (!bench_.isOpen())
        return CommandResult::fail(CmdKind::RefineBegin, "炼蛊台未开");
    const int slot = (c.refineSlot >= 0) ? c.refineSlot : bench_.curSlot();
    if (slot >= bench_.slotCount())
        return CommandResult::fail(CmdKind::RefineBegin, "无此炉");
    RefineSession& rs = bench_.slots()[slot];
    if (rs.stage != RefineStage::Setup)
        return CommandResult::fail(CmdKind::RefineBegin, "炉火未备，尚不能起炉");

    const GuRecipe* rp = nullptr;
    for (const GuRecipe* r : world_.refinery().recipes())
        if (r->id == rs.recipeId) { rp = r; break; }
    if (!rp) return CommandResult::fail(CmdKind::RefineBegin, "蛊方已失");

    // 所需手法：蛊方指定手法时须已选定
    if (rp->requiredTechnique != 0 && rs.techniqueId != rp->requiredTechnique) {
        const RefineTechnique* need = refineTechnique(rp->requiredTechnique);
        return CommandResult::fail(CmdKind::RefineBegin,
            "此方须用「" + (need ? need->name : std::to_string(rp->requiredTechnique)) +
            "」手法");
    }

    Cultivator* p = world_.player();
    if (p && p->essence < rp->requiredEssence)
        return CommandResult::fail(CmdKind::RefineBegin,
                                   std::string(p->essenceName()) + "不足，起不了炉");

    // 已选实例 → 模板 id（可重复）
    std::vector<GuId> selTplIds;
    for (GuId inst : rs.materialInst) {
        for (const auto& g : p->carriedGu)
            if (g.instanceId == inst) { selTplIds.push_back(g.templateId); break; }
    }
    // 材料（仙材、美酒等）须在背包中齐备 —— 与合炼用蛊是两回事
    std::vector<std::string> lackMat;
    if (!p->bag.canAfford(*rp, &lackMat)) {
        std::string m;
        for (const auto& x : lackMat) m += x + " ";
        return CommandResult::fail(CmdKind::RefineBegin, "材料不齐，缺：" + m);
    }

    if (!bench_.begin(slot, *rp, selTplIds)) {
        auto tpls = world_.refinery().templates();
        std::string missing;
        for (GuId need : rp->componentGu) {
            auto it = std::find(selTplIds.begin(), selTplIds.end(), need);
            if (it == selTplIds.end()) {
                for (const auto& t : tpls) if (t.id == need) missing += t.name + " ";
            } else selTplIds.erase(it);
        }
        return CommandResult::fail(CmdKind::RefineBegin,
            missing.empty() ? "材料不齐，无法起炉" : ("尚缺：" + missing));
    }

    CommandResult r = CommandResult::succeed(CmdKind::RefineBegin,
                                             "起炉：" + rs.recipeName);
    r.notes.push_back("预计 " + std::to_string(rs.baseTicks) + " 刻");
    r.notes.push_back(rs.stable ? "炉火平稳" : "炉火不稳（成蛊堪忧）");
    return r;
}

CommandResult GameSession::doRefineCancel(const Command& c) {
    if (!bench_.isOpen())
        return CommandResult::fail(CmdKind::RefineCancel, "炼蛊台未开");
    const int slot = (c.refineSlot >= 0) ? c.refineSlot : bench_.curSlot();
    if (slot < bench_.slotCount() && bench_.slots()[slot].running())
        return CommandResult::fail(CmdKind::RefineCancel,
                                   "炉火已起，须「终止炼蛊」方可收手");
    bench_.close(slot);
    return CommandResult::succeed(CmdKind::RefineCancel, "撤炉");
}

CommandResult GameSession::doRefineTerminate(const Command& c) {
    if (!bench_.isOpen())
        return CommandResult::fail(CmdKind::RefineTerminate, "炼蛊台未开");
    const int slot = (c.refineSlot >= 0) ? c.refineSlot : bench_.curSlot();
    std::string why;
    if (!bench_.terminate(slot, why))
        return CommandResult::fail(CmdKind::RefineTerminate, why);
    harvestRefined(slot);
    RefineSession& rs = bench_.slots()[slot];
    return CommandResult::succeed(CmdKind::RefineTerminate,
        rs.stable ? "提前收炉，得半成品" : "火候失控，炼蛊失败");
}

CommandResult GameSession::doRefineTimeGu(const Command& c) {
    if (!bench_.isOpen())
        return CommandResult::fail(CmdKind::RefineTimeGu, "炼蛊台未开");
    if (!hasTimeGu())
        return CommandResult::fail(CmdKind::RefineTimeGu, "囊中无光阴蛊，无从加速");
    const int slot = (c.refineSlot >= 0) ? c.refineSlot : bench_.curSlot();
    if (!bench_.applyTimeGu(slot))
        return CommandResult::fail(CmdKind::RefineTimeGu, "此炉已用过光阴蛊");
    return CommandResult::succeed(CmdKind::RefineTimeGu,
                                  "投入光阴蛊：炼制流速加倍");
}

CommandResult GameSession::doRefineMulti(const Command& c) {
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::RefineMulti, "尚未创建玩家角色");
    if (!bench_.isOpen())
        return CommandResult::fail(CmdKind::RefineMulti, "炼蛊台未开");

    // 魂魄强度不足时须借助「一心多用」系列蛊，否则该键不可点
    if (!RefineBench::canMultiTask(p->soulStrength, bench_.slotCount(),
                                   hasMultiTaskGu()))
        return CommandResult::fail(CmdKind::RefineMulti,
            "魂魄强度不足（" + std::to_string(p->soulStrength) +
            "），须借「一心多用」系列蛊方可分心");

    const GuRecipe* rp = nullptr;
    for (const GuRecipe* r : world_.refinery().recipes())
        if (r->id == c.refineRecipe) { rp = r; break; }
    if (!rp) return CommandResult::fail(CmdKind::RefineMulti, "未闻此蛊方");
    if (!intel_.knows(*p, IntelSystem::recipeId(rp->id)))
        return CommandResult::fail(CmdKind::RefineMulti, "未得此蛊方");

    const int slot = bench_.openSlot(rp->id, rp->name, p->soulStrength,
                                     hasMultiTaskGu());
    if (slot < 0)
        return CommandResult::fail(CmdKind::RefineMulti, "无法再分心（炉数已达上限）");
    return CommandResult::succeed(CmdKind::RefineMulti,
        "另起一炉：" + rp->name + "（共 " +
        std::to_string(bench_.slotCount()) + " 炉）");
}

CommandResult GameSession::doRefineClose(const Command&) {
    if (!bench_.isOpen())
        return CommandResult::fail(CmdKind::RefineClose, "炼蛊台未开");
    // 仍在炼制的炉不可直接收起 —— 须先终止
    for (const auto& rs : bench_.slots())
        if (rs.running())
            return CommandResult::fail(CmdKind::RefineClose,
                                       "尚有炉火未熄，须先终止或候其成蛊");
    // 已完成的炉：收蛊后归零
    bench_.closeAll();
    return CommandResult::succeed(CmdKind::RefineClose, "收起炼蛊台");
}

CommandResult GameSession::doRefine(const Command& c) {
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::Refine, "尚未创建玩家角色");

    auto recipes = world_.refinery().recipes();
    if (c.recipeIdx >= recipes.size())
        return CommandResult::fail(CmdKind::Refine, "蛊方索引越界");
    const GuRecipe* rp = recipes[c.recipeIdx];
    if (!rp) return CommandResult::fail(CmdKind::Refine, "蛊方不存在");

    // 铁律加严：无蛊方【绝对】无法炼制 —— 而「有蛊方」的前提是「先得知」。
    // 未曾得知的配方，连名字都不该出现，更谈不上照方炼制。
    //
    // 分野：凡蛊方开局即懂（家族传承），仙蛊方须求得。
    // 若凡蛊方也要求情报，而打探又限六转以上，凡人便永远炼不了蛊 ——
    // 这与原著「凡人炼蛊是核心内容」直接冲突。
    if (!intel_.knows(*p, IntelSystem::recipeId(rp->id)))
        return CommandResult::fail(CmdKind::Refine,
                                   "未得「" + rp->name + "」的蛊方，无从着手"
                                   "（" + (rp->isImmortal()
                                       ? std::string("仙蛊配方须先求得")
                                       : std::string("凡蛊方须先习得")) + "）");

    RefineRequest req;
    req.recipeId       = rp->id;
    req.refinerId      = p->id;
    req.refinerIsPlayer= true;
    req.refinerDao     = p->dao.get(rp->dao);
    req.refinerEssence = p->essence;
    req.componentGu    = p->carriedGu;
    req.rngRoll        = world_.rng().next();

    auto res = world_.refinery().refine(req, world_.registry());

    if (!res.ok())
        return CommandResult::fail(CmdKind::Refine,
                                   res.detail.empty() ? "炼制失败" : res.detail);

    p->essence = std::max(0.0, p->essence - res.essenceSpent);
    p->carriedGu.push_back(res.product);

    CommandResult r = CommandResult::succeed(CmdKind::Refine,
                                        "炼成 " + res.guName +
                                        (res.outcome == RefineOutcome::Defective
                                             ? "（残次品）" : ""));
    r.notes.push_back(res.detail);
    r.notes.push_back("仙元 -" + std::to_string((long long)res.essenceSpent));
    return r;
}

// ---------------------------------------------------------------------------
//  UI 只读视图
// ---------------------------------------------------------------------------
PlayerView GameSession::playerView() const {
    PlayerView v;
    const Cultivator* p = world_.player();
    if (!p) return v;

    v.exists = true;
    v.name   = p->name;
    v.rankName = to_string(p->rank);
    v.domainName = to_string(p->bornDomain);
    v.demonName  = to_string(p->demonIdentity);

    // 凡人用真元、蛊仙用仙元 —— 名称随修为切换
    v.essenceName = p->essenceName();
    v.essence = p->essence;
    v.maxEssence = p->maxEssence;
    v.health = p->health;

    // 道痕显形极点：未过线不予显示。
    // 凡人时期道痕极稀（近乎为零），原著中道痕本就是蛊仙体系的显性指标；
    // 若一转角色顶着数字反而出戏，且会让玩家误以为凡人也能积道痕。
    v.daoMarksThreshold = kDaoMarksRevealThreshold;
    v.daoMarksVisible   = daoMarksRevealed(p->daoMarks);
    v.daoMarks = p->daoMarks;
    v.mainDaoMarks = p->mainDaoMarks();
    v.mainDaoName  = to_string(p->mainDao());
    v.mainFlowName = to_string(p->mainFlowLevel());

    // 三气为六转以上机制，凡人时期整块隐藏
    v.showThreeQi = ThreeQiSystem::requiresBalance(p->rank);
    v.qiHeaven = p->qi.heaven;
    v.qiEarth  = p->qi.earth;
    v.qiHuman  = p->qi.human;
    ThreeQiConfig cfg;
    v.balanced = ThreeQiSystem::isBalanced(p->qi, cfg.tolerance);
    v.inSeclusion = p->threeQi.inSeclusion;
    v.seclusionTicks = p->threeQi.seclusionTicks;

    // 定仙游：仙蛊唯一，玩家只可能是「借用」
    v.dingXianYouState = to_string(p->dingXianYouPossession);
    v.canUseDingXianYou = p->canUseDingXianYou();
    v.dingXianYouTicksLeft = quests_.lendTicksLeft(*p, now());

    v.canMove   = ThreeQiSystem::canMove(*p);
    v.canBattle = ThreeQiSystem::canBattle(*p);
    v.canLeave  = ThreeQiSystem::canLeave(*p);

    v.carriedGu   = p->carriedGu.size();
    v.knownCoords = p->dingXianYou.count();
    v.ownedSites  = p->ownedSites.size();

    // 成尊四条件（八转以上才展示进度）
    if (rank_value(p->rank) >= 8) {
        v.showVenerable = true;
        auto f = CultivatorSystem::checkVenerableFitness(*p);
        v.vBaiLi = f.hasBaiLiSource;
        v.vMarks = f.enoughMarks;
        v.vFlow  = f.enoughFlow;
        v.vSeal  = f.brokeHeavenlySeal;
    }
    return v;
}

std::vector<SiteView> GameSession::siteViews() const {
    std::vector<SiteView> out;
    const Cultivator* p = world_.player();

    for (const CaveParadise* c : world_.world().allCaves()) {
        if (!c) continue;
        SiteView s;
        s.siteId     = c->siteId;
        s.name       = c->name;
        s.region     = c->region;
        s.domainName = to_string(c->domain);
        s.className  = to_string(c->siteClass);
        s.layerName  = to_string(c->layer);
        s.ownerName  = to_string(c->owner);
        s.ownerTag   = c->ownerTag;
        s.qiYield    = c->qiYieldHeaven + c->qiYieldEarth;

        if (p) {
            s.isHome = p->owns(c->siteId);
            // 定仙游坐标库：是否已知
            if (p->dingXianYou.knows(c->location().key())) s.known = true;
        }
        out.push_back(s);
    }
    return out;
}


// ---------------------------------------------------------------------------
//  改名
// ---------------------------------------------------------------------------
CommandResult GameSession::doRename(const Command& c) {
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::Rename, "尚未创建玩家角色");

    std::string nn = c.newName;
    // 去除首尾空白
    auto trim = [](std::string& s) {
        while (!s.empty() && (unsigned char)s.front() <= ' ') s.erase(s.begin());
        while (!s.empty() && (unsigned char)s.back() <= ' ')  s.pop_back();
    };
    trim(nn);
    if (nn.empty()) return CommandResult::fail(CmdKind::Rename, "名号不可为空");
    if (nn.size() > 16) return CommandResult::fail(CmdKind::Rename, "名号过长（至多 16 字）");

    // 名号不得与原著人物同名 —— 否则叙事失去意义，也会与 NPC 名单打架
    if (isCanonFigureName(nn))
        return CommandResult::fail(CmdKind::Rename,
                                   "「" + nn + "」乃原著人物之名，不可冒用");

    const std::string before = p->name;
    p->name = nn;
    CommandResult r = CommandResult::succeed(
        CmdKind::Rename,
        (c.asAlias ? "取一假名行走世间：" : "易名：") + before + " → " + nn);
    if (c.asAlias) {
        r.notes.push_back("此为代号，非本名；本名仍在，只是不对外示人");
        r.notes.push_back("对外行走，人只知这个代号");
    }
    if (c.changeSprite) {
        // 现阶段只登记意图 —— 形象图片尚未接入，待资源就位后一并生效
        r.notes.push_back("已记下「更换形象」之意（图片资源尚未接入，暂不改动画面）");
    }
    return r;
}

// ---------------------------------------------------------------------------
//  接取支线
// ---------------------------------------------------------------------------
CommandResult GameSession::doAcceptQuest(const Command&) {
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::AcceptQuest, "尚未创建玩家角色");

    auto r0 = quests_.accept(QuestId::BorrowDingXianYou, *p);
    if (!r0.ok) return CommandResult::fail(CmdKind::AcceptQuest, r0.detail);

    CommandResult r = CommandResult::succeed(CmdKind::AcceptQuest,
                                             "接取支线「方源将定仙游借给你」");
    r.notes.push_back(r0.detail);
    return r;
}

// ---------------------------------------------------------------------------
//  地图移动（探索）
// ---------------------------------------------------------------------------
CommandResult GameSession::doMoveOnMap(const Command& c) {
    // 身处秘境时地表移动无效 —— 人在洞天里，走的不是五域的地。
    // 秘境内部移动另有 moveInCave。
    if (realm_)
        return CommandResult::fail(CmdKind::MoveOnMap,
                                   "身处秘境「" + realm_->name() +
                                   "」内，此处非五域地表（须先离开）");
    if (!tileMap_.inBounds(c.mapX, c.mapY))
        return CommandResult::fail(CmdKind::MoveOnMap, "目标越出地图范围");

    const Tile& t = tileMap_.at(c.mapX, c.mapY);
    if (!t.passable())
        return CommandResult::fail(CmdKind::MoveOnMap, "不可通行（虚空）");

    Cultivator* p = world_.player();
    if (p && p->threeQi.inSeclusion)
        return CommandResult::fail(CmdKind::MoveOnMap, "闭关中无法外出");

    tileMap_.setPlayerPos(c.mapX, c.mapY);

    // 探索：揭示周围
    const int newly = tileMap_.reveal(c.mapX, c.mapY, 5);

    CommandResult r = CommandResult::succeed(CmdKind::MoveOnMap,
        "抵达 (" + std::to_string(c.mapX) + "," + std::to_string(c.mapY) + ")");
    if (newly > 0)
        r.notes.push_back("新探明 " + std::to_string(newly) + " 格");

    if (const Landmark* lm = tileMap_.landmarkAt(c.mapX, c.mapY)) {
        r.notes.push_back("地标：" + lm->name + " —— " + lm->desc);
        if (lm->ruined)
            r.notes.push_back("【已成废墟】" + lm->name +
                              " 在第六卷时间线已毁，不复原貌。");
        // 抵达地标即记入定仙游坐标库（亲眼见过 / 抵达过）
        if (p) {
            Location loc;
            loc.layer = RealmLayer::MortalSurface;
            loc.domain = lm->domain;
            loc.region = lm->name;
            if (!lm->siteId.empty()) loc.siteId = lm->siteId;
            CultivatorSystem::observe(*p, loc, SightSource::Arrived, now());
        }
    } else {
        r.notes.push_back(to_string(t.terrain));
        if (t.domain != Domain::None)
            r.notes.push_back("所属：" + std::string(to_string(t.domain)));
        // 站在地沟之上：提示可下探 —— 否则玩家只看到一格深色地形，
        // 不知道这里能下去，探索线索就断了
        if (t.terrain == Terrain::EarthRift)
            r.notes.push_back("脚下大地裂开一道沟壑，或可寻路下探");
    }

    // 支线推进：抵达至尊仙窍即完成「借定仙游」
    if (p) {
        auto q = quests_.update(*p, world_.world(), now());
        for (const auto& n : q.notes) r.notes.push_back(n);
    }
    return r;
}

// ---------------------------------------------------------------------------
//  支线视图
// ---------------------------------------------------------------------------
std::vector<QuestView> GameSession::questViews() const {
    const Cultivator* p = world_.player();
    if (!p) return {};
    return quests_.views(*p);
}

// ---------------------------------------------------------------------------
//  开发者信息
// ---------------------------------------------------------------------------
GameSession::DevInfo GameSession::devInfo() const {
    DevInfo d;
    d.now = now();
    d.caves = world_.world().allCaves().size();
    d.npcs  = world_.npcs().size();
    for (const auto& a : world_.npcs()) if (a.self.alive) ++d.aliveNpcs;
    d.guRegistered = world_.registry().size();
    d.landmarks = tileMap_.landmarks().size();
    d.tiles = static_cast<std::size_t>(tileMap_.width()) * tileMap_.height();
    //
    //  合并扫描 —— 一次遍历同时取回探索度、地类数、域错位。
    //
    //  原先这三项各自遍历一遍全图（exploredRatio 一次、
    //  统计脚本一次、探测程序一次）。地图扩至 2048×1536 后
    //  单次遍历已需数百毫秒，三遍就是秒级 —— 而它们互不影响，
    //  完全可以一趟走完。
    //
    //  step=4 采样（1/16 格），占比统计足够精确；
    //  地标校验另走全量（地标只有百余个，不必采样）。
    //
    const TileMap::MapScan sc = tileMap_.scanAll(4, true);
    d.exploredRatio = sc.exploredRatio();
    d.terrainKinds  = sc.terrainKinds;
    d.landmarkBad   = sc.landmarkBad;
    d.mapGenerator = tileMap_.generatorName();
    d.mapW = tileMap_.width();
    d.mapH = tileMap_.height();
    return d;
}


// ---------------------------------------------------------------------------
//  初始定位：把玩家放到出身域的一处地标，并揭示周边
// ---------------------------------------------------------------------------
//  降生之地候选：出身域内未毁、且是聚落的地标
std::vector<std::pair<std::string, std::string>>
GameSession::spawnCandidates(Domain d) const {
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto& lm : tileMap_.landmarks()) {
        if (lm.domain != d) continue;
        if (lm.ruined) continue;                 // 已毁之地不可降生
        if (lm.id == "baijiazhai") {             // 百家寨优先置顶
            out.insert(out.begin(), {lm.id, lm.name});
            continue;
        }
        if (lm.siteId.empty()) continue;         // 须是聚落，不是野地
        out.push_back({lm.id, lm.name});
    }
    return out;
}

bool GameSession::placePlayerAtStart(const std::string& preferLandmarkId) {
    Cultivator* p = world_.player();
    if (!p) return false;

    const Landmark* best = nullptr;

    // 蛊仙开局已持有一处自家福地：地图位置取该福地对应的地标，
    // 但【不改】 p->location —— 「闭关须身处自家仙窍/福地」判的是位置，
    // 若把人挪到普通地标，闭关就会被拒。
    if (!p->ownedSites.empty()) {
        for (const auto& lm : tileMap_.landmarks()) {
            if (!lm.siteId.empty() && p->owns(lm.siteId)) { best = &lm; break; }
        }
    }
    //
    //  凡人（或福地无对应地标）：取出身域内的地标。
    //
    //  已毁之地【不可】作为出生点 ——
    //  青茅山在第一卷已被白凝冰自爆化为冰域绝地（三寨尽没），
    //  第六卷时间线上是一片冰霜废墟，不能把人生成在废墟里。
    //
    //
    //  玩家自选的降生之地优先 ——
    //  出身此前只能选到「域」，具体降生在哪由代码硬编码（一律百家寨），
    //  选了别的身份也落同一处，玩家无从决定。现按所选地标定位。
    //
    if (!best && !preferLandmarkId.empty()) {
        for (const auto& lm : tileMap_.landmarks()) {
            if (lm.id != preferLandmarkId) continue;
            if (lm.ruined) continue;                 // 已毁之地不可降生
            if (lm.domain != p->bornDomain) continue; // 须在出身域内
            best = &lm; break;
        }
    }
    if (!best) {
        //  优先：出身域内标记了「出生点」的地标（如南疆百家寨）
        for (const auto& lm : tileMap_.landmarks()) {
            if (lm.domain != p->bornDomain) continue;
            if (lm.ruined) continue;
            if (lm.id == "baijiazhai") { best = &lm; break; }
        }
        //  次选：出身域内任一未毁地标
        if (!best) {
            for (const auto& lm : tileMap_.landmarks()) {
                if (lm.domain != p->bornDomain) continue;
                if (lm.ruined) continue;
                if (lm.siteId.empty()) { best = &lm; break; }
            }
        }
        //  末选：出身域内任一未毁地标（含福地）
        if (!best) {
            for (const auto& lm : tileMap_.landmarks()) {
                if (lm.domain != p->bornDomain) continue;
                if (lm.ruined) continue;
                best = &lm; break;
            }
        }
    }
    if (!best) return false;

    tileMap_.setPlayerPos(best->x, best->y);
    tileMap_.reveal(best->x, best->y, 7);

    // 抵达即记入定仙游坐标库（亲眼见过 / 抵达过）
    if (!best->siteId.empty()) {
        // 该地标本身即为福地，直接以其位置记入
        if (const CaveParadise* cp = world_.world().findCave(best->siteId)) {
            CultivatorSystem::observe(*p, cp->location(), SightSource::Arrived, now());
        }
    } else {
        // 凡人：出身地就是这处地标本身，二者是同一个地方。
        // createDefaultPlayer 曾按占位名「出身地」记过一次坐标，
        // 此处须先抹掉那条，再以真实地标名记入 —— 否则同一处出身地
        // 会变成坐标库里的两条记录，「开局仅知晓出身地」就名不副实了。
        const Location placeholder = p->location;
        p->dingXianYou.forget(placeholder);

        //
        //  在 p->location 基础上补写，而非整体替换 ——
        //  否则 siteId 会被清空：商家商人出身的 startSiteId 是
        //  「nj_shangjiazhai」（寄居商家寨），而定位到的地表地标
        //  siteId 为空，整体替换就把「寄居何处」这条信息抹掉了。
        //  siteId 是所属之地，region 是更细的地标名，二者可并存。
        //
        Location loc   = p->location;
        loc.layer      = RealmLayer::MortalSurface;
        loc.domain     = best->domain;
        loc.region     = best->name;
        p->location    = loc;
        CultivatorSystem::observe(*p, loc, SightSource::Arrived, now());
    }

    pushLog(true, "起身于" + best->name + "，周边已探明");
    return true;
}


// 测试用：把当前世界的关键状态压成字符串，用于比对「读取是否改变了状态」
std::string GameSession::snapshotForTest() const {
    std::string s;
    s += "t=" + std::to_string(now());
    s += ";caves=" + std::to_string(world_.world().allCaves().size());
    s += ";npcs=" + std::to_string(world_.npcs().size());
    s += ";explored=" + std::to_string(static_cast<int>(tileMap_.exploredRatio() * 100000));
    if (const Cultivator* p = world_.player()) {
        s += ";name=" + p->name;
        s += ";rank=" + std::to_string(rank_value(p->rank));
        s += ";ess=" + std::to_string(static_cast<long long>(p->essence));
        s += ";marks=" + std::to_string(static_cast<long long>(p->daoMarks));
    }
    return s;
}



// ---------------------------------------------------------------------------
//  打探情报
//
//  定仙游为仙蛊、世间唯一，握在谁手上并非公开信息。
//  玩家得先在人烟处听说此事，支线「方源将定仙游借给你」才谈得上触发 ——
//  否则等于默认玩家全知，既不合理也让支线失去了「获知」这一环节。
// ---------------------------------------------------------------------------
// ============================================================================
//  狩猎
//
//  异兽（万兽王及以下）可战；荒兽属蛊仙级，凡人不可敌 ——
//  这不是难度高，是层级之差。
// ============================================================================
BeastEncounter GameSession::beastHere() const {
    if (realm_) return {};                    // 秘境内另行处理
    const Tile& t = tileMap_.at(tileMap_.playerX(), tileMap_.playerY());
    if (t.terrain == Terrain::Void || t.terrain == Terrain::Wall) return {};
    return make_encounter(t.domain, t.terrain,
                          tileMap_.playerX(), tileMap_.playerY(), 20240906ull);
}

CommandResult GameSession::doHunt(const Command&) {
    CommandResult r;
    r.kind = CmdKind::Hunt;

    if (realm_) {
        r.ok = false; r.title = "秘境之内猎兽之事另行处置";
        return r;
    }

    const BeastEncounter e = beastHere();
    if (e.speciesId.empty()) {
        r.ok = false;
        r.title = "此地并无野兽出没";
        return r;
    }

    //  记入见闻
    bool seen = false;
    for (const auto& b : beastsSeen_) if (b == e.speciesId) { seen = true; break; }
    if (!seen) beastsSeen_.push_back(e.speciesId);

    const BeastSpecies* sp = beast_species_by_id(e.speciesId);
    if (!sp) { r.ok = false; r.title = "（兽种数据缺失）"; return r; }

    if (isHuangShou(sp->rank)) {
        r.ok = false;
        r.title = "「" + sp->name + "」乃" + to_string(sp->rank) + "，非你所能敌";
        r.notes.push_back(e.warning);
        r.notes.push_back("原著：荒兽属蛊仙之境（六转及以上），凡人遇之唯有退避。");
        return r;
    }

    Cultivator* p = world_.player();
    if (!p) { r.ok = false; r.title = "（无玩家）"; return r; }
    const int pr = static_cast<int>(p->rank);
    int atk0 = 0;
    for (const auto& g : p->carriedGu)
        for (const auto& t : world_.refinery().templates())
            if (t.id == g.templateId) { atk0 += static_cast<int>(t.rank) * 2; break; }

    HuntOutcome o = resolve_hunt(*sp, pr, atk0);

    if (!o.ok) {
        r.ok = false;
        r.title = "猎「" + sp->name + "」失利";
        r.notes.push_back(o.line);
        if (o.hpLoss > 0) r.notes.push_back("气血 -" + std::to_string(o.hpLoss));
        return r;
    }

    //  消耗真元 / 仙元（走会话的可写入口，不改只读视图）
    const bool immortal = pr >= 6;
    p->essence = std::max(0.0, p->essence - (immortal ? o.essenceCost * 0.01 : o.essenceCost));

    for (const auto& g : o.gained)
        p->bag.add(g, 1.0, ItemKind::Material, sp->source, sp->name + "所出");

    r.ok = true;
    r.title = o.line;
    r.notes.push_back("消耗" + std::string(immortal ? "仙元" : "真元")
                      + " " + std::to_string(o.essenceCost));
    std::string got;
    for (const auto& g : o.gained) got += (got.empty() ? "" : "、") + g;
    if (!got.empty()) r.notes.push_back("所得：" + got);
    return r;
}

CommandResult GameSession::doInquire(const Command&) {
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::Inquire, "尚未创建玩家角色");

    // 凡人接触不到这个层次的消息
    if (!p->isImmortal())
        return CommandResult::fail(CmdKind::Inquire,
                                   "人微言轻，无人理会（打探需六转以上）");

    // 须在人烟处：有地标，或中洲这类文明密集区
    const int mx = tileMap_.playerX(), my = tileMap_.playerY();
    bool hasPeople = false;
    if (tileMap_.inBounds(mx, my)) {
        if (tileMap_.landmarkAt(mx, my)) hasPeople = true;
        else if (tileMap_.domainAt(mx, my) == Domain::ZhongZhou) hasPeople = true;
    }
    if (!hasPeople)
        return CommandResult::fail(CmdKind::Inquire,
                                   "四下无人，无从打探（需身处地标或中洲）");

    const Domain here = tileMap_.domainAt(mx, my);

    // ------------------------------------------------------------------
    //  ① 先求蛊方：信息是资源，配方得一处一处求来
    //
    //  此前只登记了蛊方情报却没给获取途径 —— 玩家永远学不到配方，
    //  炼蛊被彻底锁死。故打探即求方：按所在域给出该地流传的方子。
    // ------------------------------------------------------------------
    for (const auto& e : intel_.all()) {
        if (e.category != IntelCategory::Recipe) continue;
        if (intel_.knows(*p, e.id)) continue;
        if (e.where != Domain::None && e.where != here) continue;   // 此地不传此道
        // 只求仙蛊方：凡蛊方开局即懂，不必再求
        const GuRecipe* rp = nullptr;
        for (const GuRecipe* q : world_.refinery().recipes())
            if (q && IntelSystem::recipeId(q->id) == e.id) { rp = q; break; }
        if (rp && !rp->isImmortal()) continue;

        intel_.learn(*p, e.id);
        CommandResult r = CommandResult::succeed(CmdKind::Inquire,
                                                 "求得蛊方：" + e.title);
        r.notes.push_back(e.detail);
        r.notes.push_back("于 " + std::string(to_string(here)) + " 求得");
        if (!e.canon) r.notes.push_back("（此方非原著记载，为工程构造）");
        r.notes.push_back("至此方可照方炼制");
        return r;
    }

    // ------------------------------------------------------------------
    //  ② 该域的方子已求尽，再问则得传闻
    // ------------------------------------------------------------------
    if (!p->knowsIntel(QuestSystem::kIntelFangYuanDingXianYou)) {
        p->learnIntel(QuestSystem::kIntelFangYuanDingXianYou);
        CommandResult r = CommandResult::succeed(
            CmdKind::Inquire, "听得一则传闻：定仙游在方源手上");
        r.notes.push_back("定仙游乃仙蛊、世间唯一，归方源所有");
        r.notes.push_back("支线「方源将定仙游借给你」已可接取");
        return r;
    }

    // ------------------------------------------------------------------
    //  ③ 都已听过：如实相告，并提示换个地方再问
    // ------------------------------------------------------------------
    CommandResult r = CommandResult::fail(CmdKind::Inquire,
                                          "此处再无可探听之事");
    r.notes.push_back(std::string(to_string(here)) + "流传的蛊方已尽数求得");
    r.notes.push_back("五域各有传承，换个地方或有收获");
    return r;
}

// ---------------------------------------------------------------------------
//  秘境：进入 / 离开 / 内部移动
//
//  洞天不是地表上的一个点，而是一方独立小世界 —— 进来就该是另一张地图。
//  原著对多数洞天内部只给了少量定点（疯魔窟九层、琅琊福地的荡魂山落魄谷、
//  至尊仙窍的小东海），未给完整布局，故生成的是【结构示意】而非考据地图。
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
//  秘境：进入 / 离开 / 内部移动
//
//  洞天不是地表上的一个点，而是一方独立小世界 —— 进来就该是另一张地图。
//  原著对多数洞天内部只给了少量定点（疯魔窟九层、琅琊福地的荡魂山落魄谷、
//  至尊仙窍的小东海），未给完整布局，故生成的是【结构示意】而非考据地图。
// ---------------------------------------------------------------------------
CommandResult GameSession::enterCave(const std::string& siteId) {
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::MoveTo, "尚未创建玩家角色");

    //  炼蛊台开着时不得入秘境 —— 炉火须人照看。
    //  doRefineOpen 已禁止「在秘境中开炉」，但若此处不设闸，
    //  玩家可先在外界开炉、再入秘境，从而绕过该限制，
    //  炉子便悬在了另一张地图上（出秘境后位置亦无从对应）。
    if (bench_.isOpen())
        return CommandResult::fail(CmdKind::MoveTo,
                                   "炉火未熄，须先撤炉或收起炼蛊台");

    const CaveParadise* cave = world_.world().findCave(siteId);
    if (!cave) return CommandResult::fail(CmdKind::MoveTo, "洞天不存在：" + siteId);
    if (realm_ && realm_->siteId() == siteId)
        return CommandResult::fail(CmdKind::MoveTo, "已在此洞天内");
    // 洞天内不再套洞天：若允许，返回点会被记成当前洞天，
    // 离开后便回错了地方。须先出洞，再入别处。
    if (realm_)
        return CommandResult::fail(CmdKind::MoveTo,
                                   "身处秘境「" + realm_->name() +
                                   "」，须先离开再入别处");

    // 先在栈上生成，成功后再接管 —— 直接解引用 realm_（初始为空）会崩
    CaveRealmGenerator gen;
    auto nu = std::make_unique<CaveRealm>();
    auto r = gen.generate(*nu, *cave, world_.rng());
    if (!r.ok) return CommandResult::fail(CmdKind::MoveTo, r.detail);
    realm_ = std::move(nu);

    // 记录返回点：离开后回到原来的外界位置
    realmReturnSite_ = p->location.siteId.empty() ? p->location.region
                                                  : p->location.siteId;
    p->location.siteId = siteId;
    p->location.region = cave->name;

    // 盗天传承窟：入内即得传承（工程设定，依疯魔窟第九层藏盗天真传推演）
    if (siteId == "nj_daotian_chuancheng" && p) {
        auto res = DaoTianLegacySystem::obtain(*p, DaoTianBranch::ThiefDao);
        if (res.ok()) {
            p->daoTianLegacy.obtained = true;
            pushLog(true, "窟中石壁刻满盗天魔尊的手迹 —— 偷道正统，已然入手");
        } else {
            pushLog(false, "窟中一片空寂，传承未可得（" + res.detail + "）");
        }
    }

    CommandResult cr = CommandResult::succeed(CmdKind::MoveTo,
                                              "进入秘境：" + cave->name);
    cr.notes.push_back(r.detail);
    if (realm_->levelCount() > 1)
        cr.notes.push_back("共 " + std::to_string(realm_->levelCount()) +
                           " 层，当前第 1 层");
    // 普通洞天从下缘入内；大地裂缝是从地表往下跳，出口在上缘。
    // 提示若一成不变地写「出口在下方」，进裂缝的玩家就找错方向了。
    cr.notes.push_back(cave->siteId == "cn_dadi_liefeng"
                           ? "出口在裂口上方，攀回地表：" + realmReturnSite_
                           : "出口在地图下方，返回：" + realmReturnSite_);
    return cr;
}

CommandResult GameSession::leaveCave() {
    if (!realm_)
        return CommandResult::fail(CmdKind::MoveTo, "并未身处秘境");
    //  与 enterCave 同闸：炉火未熄不得跨地图
    if (bench_.isOpen())
        return CommandResult::fail(CmdKind::MoveTo,
                                   "炉火未熄，须先撤炉或收起炼蛊台");

    Cultivator* p = world_.player();
    const std::string name = realm_->name();
    realm_.reset();
    if (p) {
        p->location.siteId = realmReturnSite_;
        p->location.region = realmReturnSite_;
    }
    CommandResult r = CommandResult::succeed(CmdKind::MoveTo, "离开秘境：" + name);
    r.notes.push_back("返回：" + realmReturnSite_);
    return r;
}

CommandResult GameSession::moveInCave(int nx, int ny) {
    if (!realm_) return CommandResult::fail(CmdKind::MoveOnMap, "并未身处秘境");

    CaveFloorMap& f = realm_->floor(realm_->level());
    if (!f.inBounds(nx, ny))
        return CommandResult::fail(CmdKind::MoveOnMap, "越出秘境范围");
    if (!f.at(nx, ny).passable())
        return CommandResult::fail(CmdKind::MoveOnMap, "不可通行");

    realm_->setPos(realm_->level(), nx, ny);
    const int newly = f.reveal(nx, ny, 4);

    CommandResult r = CommandResult::succeed(CmdKind::MoveOnMap,
        "(" + std::to_string(nx) + "," + std::to_string(ny) + ")");
    if (newly > 0) r.notes.push_back("新探明 " + std::to_string(newly) + " 格");

    const CaveTile& t = f.at(nx, ny);
    r.notes.push_back(std::string(to_string(t.terrain)));
    if (t.landmarkId >= 0 &&
        static_cast<std::size_t>(t.landmarkId) < f.sites.size()) {
        const CaveSite& s = f.sites[t.landmarkId];
        r.notes.push_back(s.name + "：" + s.desc);
    }
    if (t.terrain == CaveTerrain::CaveExit)
        r.notes.push_back("此处为出口，可返回外界");
    return r;
}

CommandResult GameSession::gotoCaveLevel(int lv) {
    if (!realm_)
        return CommandResult::fail(CmdKind::MoveOnMap, "并未身处秘境");
    if (lv < 1 || lv > realm_->levelCount())
        return CommandResult::fail(CmdKind::MoveOnMap, "层数越界");
    if (lv == realm_->level())
        return CommandResult::fail(CmdKind::MoveOnMap, "已在该层");

    CaveFloorMap& f = realm_->floor(lv);
    realm_->setPos(lv, f.entryX, f.entryY);
    f.reveal(f.entryX, f.entryY, 4);

    CommandResult r = CommandResult::succeed(CmdKind::MoveOnMap,
                                             "下至第 " + std::to_string(lv) + " 层");
    r.notes.push_back(f.name);
    return r;
}

// 揭示当前秘境全图（仅出样图 / 开发者辅助，正常玩法中靠探索逐步揭开）
void GameSession::revealCurrentRealm() {
    if (!realm_) return;
    for (auto& f : realm_->floors()) f.revealAll();
}


// ---------------------------------------------------------------------------
//  蛊方视图：未得蛊方则涂黑
// ---------------------------------------------------------------------------
std::vector<RecipeView> GameSession::recipeViews() const {
    std::vector<RecipeView> out;
    const Cultivator* p = world_.player();
    auto recipes = world_.refinery().recipes();
    for (std::size_t i = 0; i < recipes.size(); ++i) {
        const GuRecipe* r = recipes[i];
        if (!r) continue;
        RecipeView v;
        v.index = i;
        v.known = p ? intel_.knows(*p, IntelSystem::recipeId(r->id)) : false;
        if (v.known) {
            v.name           = r->name;
            v.rankName       = to_string(r->targetRank);
            v.daoName        = to_string(r->dao);
            v.integrityName  = to_string(r->integrity);
            v.source         = r->source;
            v.canRefine      = true;
        } else {
            v.name           = "？？？（未得此方）";
            v.rankName       = "—";
            v.daoName        = "—";
            v.integrityName  = "—";
            v.source         = "";
            v.canRefine      = false;
        }
        out.push_back(std::move(v));
    }
    return out;
}


// ---------------------------------------------------------------------------
//  存档目录
// ---------------------------------------------------------------------------
std::string GameSession::saveDir() const { return "save"; }

// ---------------------------------------------------------------------------
//  自创炼蛊
//
//  原著对「炼制前所未有之蛊」的规定（方向性，非精确公式）：
//    · 蛊非凭空而生 —— 以已有蛊虫为基推演新蛊，是极高难度的活
//    · 成败系于炼道造诣：造诣浅者强炼，多半炸炉
//    · 失败的代价是真金白银：材料损毁、蛊虫崩解，甚至反噬其身
//    · 纵是成了，也常出残次品，威能不及正品
//
//  因此本实现：
//    · 炼道境界是主要加成项（入门几无可能，圆满方可倚仗）
//    · 组成蛊虫转数跨度越大越难 —— 强行凑不同层次的蛊，道理上说不通
//    · 基础成功率压得很低，符合「自创极难」的原著基调
//    · 失败则参与蛊虫有概率崩解，并可能反噬
//
//  必须说明：原著【没有】给出自创炼蛊的成功率公式。
//  以下数值是为让机制可运行而设的工程占位，只保证「方向正确」
//  （难、看造诣、失败有代价），不可当作原著设定引用。
// ---------------------------------------------------------------------------
namespace {
// 炼道境界对自创成功率的加成
double innovateDaoBonus(DaoLevel lv) {
    switch (lv) {
        case DaoLevel::Entry:      return -0.10;   // 入门：外行强炼，十难成一
        case DaoLevel::Small:      return  0.05;
        case DaoLevel::Great:      return  0.18;
        case DaoLevel::Perfection: return  0.30;
        case DaoLevel::Foundation: return  0.42;
        case DaoLevel::MarkFusion: return  0.55;   // 道痕贯通：可倚仗
    }
    return 0.0;
}
}

CommandResult GameSession::doInnovate(const Command& c) {
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::Innovate, "尚未创建玩家角色");

    // 闭关中不动炼蛊
    if (p->threeQi.inSeclusion)
        return CommandResult::fail(CmdKind::Innovate, "闭关中不宜炼蛊");

    if (c.components.size() < 2)
        return CommandResult::fail(CmdKind::Innovate,
                                   "自创炼蛊至少需两只蛊虫为基（单蛊无从推演）");

    for (std::size_t idx : c.components) {
        if (idx >= p->carriedGu.size())
            return CommandResult::fail(CmdKind::Innovate, "所选蛊虫不存在");
    }

    // 目标流派：未指定则以主修流派推演
    Dao dao = Dao::Refine;
    if (c.targetDao >= 0) dao = static_cast<Dao>(c.targetDao);
    else {
        // 未指定则以自身造诣最深的流派推演
        int best = -1;
        for (const auto& kv : p->dao.all()) {
            const int v = static_cast<int>(kv.second);
            if (v > best) { best = v; dao = kv.first; }
        }
        if (best < 0) dao = Dao::Refine;   // 未曾修习任何流派，则归炼道
    }

    // ---- 组成蛊虫的转数跨度：跨度越大越难 ----
    int lo = 99, hi = 0;
    for (std::size_t idx : c.components) {
        const GuInstance& gi = p->carriedGu[idx];
        int r = 1;
        for (const auto& t : world_.refinery().templates())
            if (t.id == gi.templateId) { r = static_cast<int>(t.rank); break; }
        lo = std::min(lo, r); hi = std::max(hi, r);
    }
    const int span = hi - lo;

    // ---- 成功率 ----
    // 基础压低（0.20），体现「自创极难」；境界加成；跨度惩罚。
    // 入门者约一成、道痕贯通者约七成半 —— 难而可达。
    // 若低到永远炼不成，这条玩法就等于没有，故此处做了可达性平衡。
    const DaoLevel lv = p->dao.get(dao);
    double rate = 0.20 + innovateDaoBonus(lv) - 0.08 * span;
    rate = std::clamp(rate, 0.01, 0.85);

    // ---- 消耗：真元 / 仙元 ----
    const double cost = 20.0 + 12.0 * c.components.size() + 8.0 * hi;
    if (p->essence < cost)
        return CommandResult::fail(CmdKind::Innovate,
                                   std::string(p->isImmortal() ? "仙元" : "真元") +
                                   "不足（需 " + std::to_string((long long)cost) + "）");
    p->essence -= cost;

    const double roll = world_.rng().next();
    const bool success = roll < rate;

    // 残次判定：成功中另有一部分为残次品
    const bool defective = success && (roll < rate * 0.35);

    // ---- 失败的代价：炸炉 ----
    std::string detail;
    if (!success) {
        // 参与蛊虫有概率崩解
        std::vector<std::size_t> lost;
        for (std::size_t idx : c.components) {
            if (world_.rng().next() < 0.45) lost.push_back(idx);
        }
        // 反噬
        const bool backlash = world_.rng().next() < 0.30;
        if (backlash) p->health = std::max(0.0, p->health - 8.0 - 2.0 * hi);

        // 倒序移除，避免索引失效
        std::sort(lost.rbegin(), lost.rend());
        for (std::size_t idx : lost) {
            if (idx < p->carriedGu.size())
                p->carriedGu.erase(p->carriedGu.begin() + static_cast<long>(idx));
        }

        detail = "炉中一声闷响，烟气散尽 —— 炼制失败";
        if (!lost.empty())
            detail += "，崩解 " + std::to_string(lost.size()) + " 蛊";
        if (backlash) detail += "，反噬伤身";

        pending_ = Innovation{};
        pending_.success = false;
        pending_.attemptDetail = detail;

        CommandResult r = CommandResult::succeed(CmdKind::Innovate, detail);
        r.notes.push_back("成功率约 " + std::to_string((int)(rate * 100)) + "%" +
                          "（炼道：" + to_string(lv) + "）");
        r.notes.push_back("消耗" + std::string(p->isImmortal() ? "仙元" : "真元") +
                          " " + std::to_string((long long)cost));
        if (!lost.empty()) r.notes.push_back("损毁蛊虫 " + std::to_string(lost.size()) + " 只");
        if (backlash) r.notes.push_back("反噬：气血 -" + std::to_string((int)(8.0 + 2.0 * hi)));
        return r;
    }

    // ---- 成功：生成未命名的新蛊，待玩家命名 ----
    GuTemplate proto;
    const auto& tpls = world_.refinery().templates();
    proto.id = 900 + static_cast<GuId>(customGu_.size());   // 自创蛊 id 段
    proto.name = "未命名之蛊";
    proto.rank = static_cast<Rank>(std::clamp(hi, 1, 9));
    proto.category = GuCategory::Attack;
    proto.dao = dao;
    proto.effect = "玩家以 " + std::to_string(c.components.size()) +
                   " 蛊推演所得的新蛊（" + to_string(dao) + "）";
    proto.feed = "未知";
    proto.original = true;       // 原著未记载
    proto.rankConfirmed = false; // 转数为推演值，不参与强弱比较
    proto.source = "玩家自创：" + std::to_string(c.components.size()) +
                   " 蛊推演；成功率 " + std::to_string((int)(rate * 100)) +
                   "%；非原著记载";
    (void)tpls;

    pending_ = Innovation{};
    pending_.pending = true;
    pending_.success = true;
    pending_.proto = proto;
    pending_.attemptDetail = defective ? "炉火摇晃，勉强成一残次之蛊"
                                       : "炉中光华一闪，新蛊初成";

    // 残次品：完整度不足
    GuInstance inst;
    inst.templateId = proto.id;
    inst.instanceId = static_cast<GuId>(world_.rng().rangeInt(100000, 999999));
    inst.holder = p->id;
    inst.defective = defective;
    inst.integrity = defective ? 0.55 : 1.0;
    // 先不入包 —— 待命名后再正式归档，避免无名之蛊混入
    pending_.proto.id = proto.id;

    CommandResult r = CommandResult::succeed(CmdKind::Innovate,
                                             pending_.attemptDetail + "，请为之命名");
    r.notes.push_back("流派：" + std::string(to_string(dao)) +
                      "；推演转数：" + to_string(proto.rank));
    r.notes.push_back("成功率约 " + std::to_string((int)(rate * 100)) + "%" +
                      "（炼道：" + to_string(lv) + "）");
    r.notes.push_back("消耗" + std::string(p->isImmortal() ? "仙元" : "真元") +
                      " " + std::to_string((long long)cost));
    if (defective) r.notes.push_back("此为残次品，威能不及正品");
    r.notes.push_back("命名后方可正式归入囊中并存档");
    return r;
}

CommandResult GameSession::doNameGu(const Command& c) {
    Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::NameGu, "尚未创建玩家角色");

    if (!pending_.pending || !pending_.success)
        return CommandResult::fail(CmdKind::NameGu, "并无待命名的新蛊");

    std::string name = c.newName;
    // 去空白
    auto trim = [](std::string v) {
        const auto a = v.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return std::string{};
        const auto b = v.find_last_not_of(" \t\r\n");
        return v.substr(a, b - a + 1);
    };
    name = trim(name);
    if (name.empty())
        return CommandResult::fail(CmdKind::NameGu, "名字不可为空");
    if (name.length() > 16)
        return CommandResult::fail(CmdKind::NameGu, "名字过长（至多 16 字）");
    // 不得与已有蛊虫重名 —— 否则蛊名相混，日后无从分辨
    for (const auto& t : world_.refinery().templates())
        if (t.name == name)
            return CommandResult::fail(CmdKind::NameGu, "世间已有「" + name + "」，不可同名");
    for (const auto& t : customGu_)
        if (t.name == name)
            return CommandResult::fail(CmdKind::NameGu, "你已用此名命过一蛊");

    GuTemplate g = pending_.proto;
    g.name = name;
    customGu_.push_back(g);

    // 归入囊中
    GuInstance inst;
    inst.templateId = g.id;
    inst.instanceId = static_cast<GuId>(world_.rng().rangeInt(100000, 999999));
    inst.holder = p->id;
    inst.integrity = (pending_.attemptDetail.find("残次") != std::string::npos) ? 0.55 : 1.0;
    inst.defective = inst.integrity < 1.0;
    p->carriedGu.push_back(inst);

    // 自创蛊也登记为模板，使其可被炼蛊台与界面检索到
    world_.refinery().addTemplate(g);

    // 存档
    saveCustomGu();

    const std::string nm = name;
    pending_ = Innovation{};

    CommandResult r = CommandResult::succeed(CmdKind::NameGu,
                                             "新蛊「" + nm + "」已归入囊中");
    r.notes.push_back("流派：" + std::string(to_string(g.dao)) +
                      "；转数：" + to_string(g.rank));
    r.notes.push_back("已存入存档（" + saveDir() + "/custom_gu.txt）");
    return r;
}

// ---------------------------------------------------------------------------
//  自创蛊存档
// ---------------------------------------------------------------------------
void GameSession::saveCustomGu() const {
#ifdef _WIN32
    ::mkdir(saveDir().c_str());
#else
    ::mkdir(saveDir().c_str(), 0755);
#endif
    const std::string path = saveDir() + "/custom_gu.txt";
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    std::fprintf(f, "# 玩家自创蛊虫存档\n");
    std::fprintf(f, "# 格式：id|name|rank|dao|effect|source\n");
    for (const auto& g : customGu_) {
        std::fprintf(f, "%llu|%s|%d|%d|%s|%s\n",
                     static_cast<unsigned long long>(g.id), g.name.c_str(),
                     static_cast<int>(g.rank), static_cast<int>(g.dao),
                     g.effect.c_str(), g.source.c_str());
    }
    std::fclose(f);
}

void GameSession::loadCustomGu() {
    const std::string path = saveDir() + "/custom_gu.txt";
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return;
    char line[1024];
    while (std::fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        // 简易竖线分隔解析
        std::vector<std::string> parts;
        std::string cur;
        for (char* ch = line; *ch; ++ch) {
            if (*ch == '|' || *ch == '\n') { parts.push_back(cur); cur.clear(); }
            else cur.push_back(*ch);
        }
        if (parts.size() < 6) continue;
        GuTemplate g;
        g.id = static_cast<GuId>(std::strtoul(parts[0].c_str(), nullptr, 10));
        g.name = parts[1];
        g.rank = static_cast<Rank>(std::clamp(std::atoi(parts[2].c_str()), 1, 9));
        g.dao = static_cast<Dao>(std::atoi(parts[3].c_str()));
        g.effect = parts[4];
        g.source = parts[5];
        g.original = true;
        g.rankConfirmed = false;
        customGu_.push_back(g);
    }
    std::fclose(f);
    // 已存的自创蛊也登记进模板表，供界面与炼制检索
    for (const auto& g : customGu_) world_.refinery().addTemplate(g);
}


// ===========================================================================
//  NPC 互动
//
//  原著第一约束：【能否见到】，其次才是能否互动。
//    · 九转尊者行踪莫测，凡人无从得见
//    · 六转蛊仙视凡人如蝼蚁，寻常不理会搭话
//  故每次互动前先判定态度，态度不足则一律拒绝 ——
//  拒绝理由要写明，否则玩家不知道差在哪里。
// ===========================================================================

namespace {
    //  态度是否足以支持某项互动（与 NpcInteraction 内规则一致）
    bool permits(NpcAttitude a, NpcAction act) { return attitudePermits(a, act); }
}

std::vector<NpcContactView> GameSession::npcContacts() const {
    std::vector<NpcContactView> v;
    const Cultivator* p = world_.player();
    if (!p) return v;

    const bool hasOrigin = (originId_ != nullptr);
    const OriginId oid   = originIdVal_;

    for (const auto& a : world_.npcs()) {
        NpcContactView cv;
        cv.npcId        = a.self.id;
        cv.name         = a.self.name;
        cv.rankName     = to_string(a.self.rank);
        cv.factionName  = to_string(a.faction);
        cv.intentName   = to_string(a.intent);

        NpcRelation rel;
        auto it = npcRel_.find(a.self.id);
        if (it != npcRel_.end()) rel = it->second;

        const NpcApproach ap = evaluateApproach(a, *p, rel, hasOrigin, oid,
                                                playerFaction_);
        cv.visible      = ap.visible;
        cv.attitude     = ap.attitude;
        cv.attitudeName = to_string(ap.attitude);
        cv.line         = ap.line;
        cv.reason       = ap.reason;
        cv.affinity     = rel.affinity;
        cv.met          = rel.met;

        //
        //  取画像须用 socialFor()，而非 findSocial()。
        //
        //  findSocial() 只查【手工表】，凡俗众生不在表内，一律返回空 ——
        //  于是 71 位居民全部「未设定社交画像」、可做之事为 0，
        //  互动对普通人形同虚设。
        //  这与「placePlayerAtStart 零调用点」是同一类疏漏：
        //  加了函数，却没在真正用到的地方接上。
        //
        //
        //  所在地：凡俗之人各有其居，走到该处方得相见。
        //  否则一开局就把五域所有人列出来，探索便失去意义 ——
        //  既然谁都能隔空搭话，何必翻山越海。
        //
        cv.isCommoner = a.isCommoner;
        cv.occupation = a.occupation;
        if (a.isCommoner) {
            const Landmark* lm = tileMap_.landmarkById(a.whereId);
            cv.whereName = lm ? lm->name : a.homeSettlement;
            if (lm) {
                const int dx = std::abs(tileMap_.playerX() - lm->x);
                const int dy = std::abs(tileMap_.playerY() - lm->y);
                cv.here = (dx + dy) <= 10;
            }
            if (!cv.here) {
                cv.visible  = false;
                cv.attitude = NpcAttitude::Invisible;
                cv.attitudeName = to_string(NpcAttitude::Invisible);
                cv.line   = "此人远在他乡。";
                cv.reason = "须亲至" + cv.whereName + "，方得相见";
            }
        }

        const NpcSocial soc = socialFor(a);
        cv.actions = availableActions(soc, ap.attitude);
        cv.source  = soc.source;
        cv.canon   = soc.canon;
        if (a.isCommoner && !cv.actions.empty() && ap.attitude == NpcAttitude::Neutral) {
            //  同是凡俗，初次相见亦愿搭话 —— 不该一上来就爱答不理
            if (cv.reason == "（未设定社交画像）") cv.reason = "可与之言语";
        }
        v.push_back(std::move(cv));
    }
    return v;
}

int GameSession::npcAffinity(const std::string& npcId) const {
    auto it = npcRel_.find(npcId);
    return it == npcRel_.end() ? 0 : it->second.affinity;
}

CommandResult GameSession::doNpcTalk(const Command& c) {
    const Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::NpcTalk, "尚未创建角色");
    if (c.npcId.empty()) return CommandResult::fail(CmdKind::NpcTalk, "未指定交谈对象");

    NpcAgent* npc = world_.findNpc(c.npcId);
    if (!npc) return CommandResult::fail(CmdKind::NpcTalk, "并无此人");

    NpcRelation& rel = npcRel_[c.npcId];
    const bool hasOrigin = (originId_ != nullptr);
    const NpcApproach ap = evaluateApproach(*npc, *p, rel, hasOrigin,
                                            originIdVal_, playerFaction_);
    if (!permits(ap.attitude, NpcAction::Talk))
        return CommandResult::fail(CmdKind::NpcTalk, ap.line + "（" + ap.reason + "）");

    //
    //  交谈的实质收益是【情报】—— 否则只是看一段文字。
    //  定仙游下落这条尤其关键：未听说则支线无从触发（既有设定）。
    //
    std::size_t gained = 0;
    for (const auto& id : intelFromTalk(c.npcId))
        if (!p->knowsIntel(id)) { gained++; }

    //  写入情报（需非 const 玩家）
    {
        Cultivator* pw = world_.player();
        for (const auto& id : intelFromTalk(c.npcId))
            if (!pw->knowsIntel(id)) pw->learnIntel(id);
    }

    //  情谊累积：见过面 +5，每谈一次 +2，上限 100
    rel.met  = true;
    rel.talkCount++;
    rel.affinity = std::min(100, rel.affinity + (rel.talkCount == 1 ? 5 : 3));

    CommandResult r = CommandResult::succeed(CmdKind::NpcTalk, "与" + npc->self.name + "交谈");
    r.notes.push_back(ap.line);
    if (gained > 0)
        r.notes.push_back("得知 " + std::to_string(gained) + " 条前所未闻的消息");
    else
        r.notes.push_back("此人所言，你多半早已听说过");
    r.notes.push_back("情谊 " + std::to_string(rel.affinity));
    pushLog(true, "与" + npc->self.name + "交谈");
    return r;
}

CommandResult GameSession::doNpcTrade(const Command& c) {
    const Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::NpcTrade, "尚未创建角色");
    if (c.npcId.empty()) return CommandResult::fail(CmdKind::NpcTrade, "未指定交易对象");

    NpcAgent* npc = world_.npcs().empty() ? nullptr : world_.findNpc(c.npcId);
    if (!npc) return CommandResult::fail(CmdKind::NpcTrade, "并无此人");

    const NpcSocial* soc = findSocial(c.npcId);
    if (!soc || !soc->tradesGu)
        return CommandResult::fail(CmdKind::NpcTrade, "此人不做买卖");

    NpcRelation& rel = npcRel_[c.npcId];
    const bool hasOrigin = (originId_ != nullptr);
    const NpcApproach ap = evaluateApproach(*npc, *p, rel, hasOrigin,
                                            originIdVal_, playerFaction_);
    if (!permits(ap.attitude, NpcAction::Trade))
        return CommandResult::fail(CmdKind::NpcTrade, ap.line + "（" + ap.reason + "）");

    //
    //  交易须有通货。灵石为五域通用硬通货；
    //  身无分文则谈不成买卖 —— 与聚落交易的既有规则一致。
    //
    if (p->bag.count("灵石") < 1.0)
        return CommandResult::fail(CmdKind::NpcTrade, "囊中无灵石，谈何买卖");

    //
    //  此处只做「达成交易意向」的判定与记录。
    //  具体买什么、卖什么，交由既有聚落交易界面处理 ——
    //  NPC 交易与商铺交易是同一套规则，不该另起一套。
    //
    CommandResult r = CommandResult::succeed(CmdKind::NpcTrade, "与" + npc->self.name + "议价");
    r.notes.push_back(ap.line);
    r.notes.push_back("可在就近商铺与其完成交割");
    pushLog(true, "与" + npc->self.name + "议价");
    return r;
}

CommandResult GameSession::doNpcLearn(const Command& c) {
    const Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::NpcLearn, "尚未创建角色");
    if (c.npcId.empty()) return CommandResult::fail(CmdKind::NpcLearn, "未指定求教对象");

    NpcAgent* npc = world_.findNpc(c.npcId);
    if (!npc) return CommandResult::fail(CmdKind::NpcLearn, "并无此人");

    const NpcSocial* soc = findSocial(c.npcId);
    if (!soc || soc->teachTechniques.empty())
        return CommandResult::fail(CmdKind::NpcLearn, "此人并无可传之艺");

    NpcRelation& rel = npcRel_[c.npcId];
    const bool hasOrigin = (originId_ != nullptr);
    const NpcApproach ap = evaluateApproach(*npc, *p, rel, hasOrigin,
                                            originIdVal_, playerFaction_);
    if (!permits(ap.attitude, NpcAction::Learn)) {
        //
        //  技艺不传外人 —— 但须说清差多少，否则玩家只能盲目刷交谈。
        //  亲近（情谊 50 以上）方可传授。
        //
        const int need = 50 - rel.affinity;
        std::string tail = "情谊未至，技艺不传外人";
        if (need > 0) tail += "（尚差 " + std::to_string(need) + " 点，多谈几次罢）";
        return CommandResult::fail(CmdKind::NpcLearn, ap.line + "（" + tail + "）");
    }

    //
    //  求教的实质收益：学会一种炼蛊手法。
    //  手法只在此处与「读书、拜师」中习得 —— 未学过的手法不可选（既有规则）。
    //
    Cultivator* pw = world_.player();
    std::size_t learned = 0;
    for (int id : soc->teachTechniques)
        if (!pw->knowsTechnique(id)) { pw->learnTechnique(id); ++learned; }

    CommandResult r = CommandResult::succeed(CmdKind::NpcLearn, "求教于" + npc->self.name);
    r.notes.push_back(ap.line);
    if (learned > 0)
        r.notes.push_back("习得炼蛊手法 " + std::to_string(learned) + " 种");
    else
        r.notes.push_back("其所知手法，你已尽数掌握");
    rel.affinity = std::min(100, rel.affinity + 5);
    pushLog(true, "求教于" + npc->self.name);
    return r;
}

CommandResult GameSession::doNpcBorrow(const Command& c) {
    const Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::NpcBorrow, "尚未创建角色");
    if (c.npcId.empty()) return CommandResult::fail(CmdKind::NpcBorrow, "未指定求借对象");

    NpcAgent* npc = world_.findNpc(c.npcId);
    if (!npc) return CommandResult::fail(CmdKind::NpcBorrow, "并无此人");

    const NpcSocial* soc = findSocial(c.npcId);
    if (!soc) return CommandResult::fail(CmdKind::NpcBorrow, "无从开口");
    const bool offers = std::find(soc->offers.begin(), soc->offers.end(),
                                  NpcAction::Borrow) != soc->offers.end();
    if (!offers)
        return CommandResult::fail(CmdKind::NpcBorrow, "此人无可借之物");

    NpcRelation& rel = npcRel_[c.npcId];
    const bool hasOrigin = (originId_ != nullptr);
    const NpcApproach ap = evaluateApproach(*npc, *p, rel, hasOrigin,
                                            originIdVal_, playerFaction_);
    if (!permits(ap.attitude, NpcAction::Borrow))
        return CommandResult::fail(CmdKind::NpcBorrow,
               ap.line + "（情谊未至，仙蛊岂可轻借）");

    //
    //  定仙游是仙蛊、世间唯一，原著归方源所有。
    //  求借所得只是【有限期的使用权】，所有权始终在方源 ——
    //  既有设定，此处不再另造一套。
    //
    CommandResult r = CommandResult::succeed(CmdKind::NpcBorrow, "向" + npc->self.name + "求借");
    r.notes.push_back(ap.line);
    r.notes.push_back("仙蛊唯一，所有权不因借用而转移");
    rel.affinity = std::min(100, rel.affinity + 3);
    pushLog(true, "向" + npc->self.name + "求借仙蛊");
    return r;
}

CommandResult GameSession::doNpcDuel(const Command& c) {
    const Cultivator* p = world_.player();
    if (!p) return CommandResult::fail(CmdKind::NpcDuel, "尚未创建角色");
    if (c.npcId.empty()) return CommandResult::fail(CmdKind::NpcDuel, "未指定挑战对象");

    NpcAgent* npc = world_.findNpc(c.npcId);
    if (!npc) return CommandResult::fail(CmdKind::NpcDuel, "并无此人");

    NpcRelation& rel = npcRel_[c.npcId];
    const bool hasOrigin = (originId_ != nullptr);
    const NpcApproach ap = evaluateApproach(*npc, *p, rel, hasOrigin,
                                            originIdVal_, playerFaction_);
    if (!permits(ap.attitude, NpcAction::Duel))
        return CommandResult::fail(CmdKind::NpcDuel, ap.line + "（" + ap.reason + "）");

    //
    //  仙凡鸿沟：以凡人之躯挑战蛊仙，无异于自寻死路。
    //  这不是「难度高」，而是根本不成立 —— 原著中鸿沟即是如此。
    //
    const int pr = static_cast<int>(p->rank);
    const int nr = static_cast<int>(npc->self.rank);
    if (nr - pr >= 2) {
        return CommandResult::fail(CmdKind::NpcDuel,
               "仙凡之别如天堑 —— 你与对方相差 " + std::to_string(nr - pr) +
               " 转，出手只是自取其辱");
    }

    CommandResult r = CommandResult::succeed(CmdKind::NpcDuel, "向" + npc->self.name + "出手");
    r.notes.push_back("战阵已开（具体结算沿用既有战斗规则）");
    rel.affinity = std::max(-100, rel.affinity - 20);
    pushLog(true, "向" + npc->self.name + "出手");
    return r;
}

} // namespace gr
