// 顶层世界门面实现
#include "gr/sim/GameWorld.hpp"

#include <set>
#include "gr/gu/Inventory.hpp"
#include "gr/gu/GuFeeding.hpp"
#include "gr/ai/CommonerNpc.hpp"

#include <algorithm>

namespace gr {

GameWorld::GameWorld(std::uint64_t seed)
    : rng_(seed), registry_(0), online_(nullptr) {}

void GameWorld::buildVolume6Opening(int randomCaveCount) {
    // ---- 世界：两天五域 + 胎壁 + 洞天分布 ----
    world_.buildCanonWorld(rng_, randomCaveCount);

    // ---- 蛊虫模板 / 蛊方 / 杀招 ----
    auto templates = build_canon_gu_templates();
    for (const auto& t : templates) refinery_.addTemplate(t);

    //  资料库全表补充：手工精编条目只覆盖 35 种，
    //  距资料库明载的 602 种（450 凡蛊 + 362 仙蛊、按名去重）相差甚远。
    //  工程目的是还原蛊世界，故须全收 —— 此处并入生成条目，
    //  id 自 1000 起以避开手工精编条目已占用的 id。
    auto extra = buildCanonGuData(1000);
    for (const auto& t : extra) refinery_.addTemplate(t);
    for (const auto& t : extra) templates.push_back(t);

    auto canonRecipes = build_canon_recipes(templates);
    for (const auto& r : canonRecipes) refinery_.addRecipe(r);

    //  资料库全表蛊方补充：蛊方取自「炼制方法」列的合炼公式与「炼制材料」列。
    //  此前仅 8 条，覆盖率极低；还原蛊世界须照资料库补全。
    //  手工精编蛊方与生成蛊方重名者，以精编版为准（溯源更完整）。
    auto extraRecipes = buildCanonRecipeData(templates, 1000);
    std::set<std::string> haveRecipe;
    for (const auto& r : canonRecipes) haveRecipe.insert(r.name);
    for (const auto& r : extraRecipes) {
        if (haveRecipe.count(r.name)) continue;
        haveRecipe.insert(r.name);
        refinery_.addRecipe(r);
    }

    killerMoves_ = build_canon_killer_moves();

    // ---- 仙蛊唯一：开局在世的仙蛊入表 ----
    registry_.setNow(world_.now());
    for (const auto& t : templates) {
        if (!t.isImmortal()) continue;
        if (t.special != GuSpecial::None) continue;   // 仿伪蛊不入唯一表
        // 第五卷末：宿命蛊被毁 → 名额已释放；监天塔崩碎 → 名额已释放
        if (t.name == "宿命蛊" || t.name == "监天塔") {
            continue;
        }
        registry_.registerGu(t.name, t.rank, "原著持有者", t.special);
    }

    // ---- 势力格局：方源 + 巨阳 + 失控幽魂 围攻天庭 ----
    factions_.setupVolume6Opening();

    // ---- NPC 独立 AI ----
    npcs_ = build_canon_npcs(world_);

    //
    //  凡俗众生 —— 五域之内不只有尊者与蛊仙。
    //  山寨有猎户、草原有牧民、沙漠有商队、海上有渔夫、宗门有外门弟子。
    //  聚落登记在前，居民依之而生；未登记聚落则不臆造无处可居之人。
    //
    {
        auto commoners = build_commoner_npcs(settlements_);
        for (auto& c : commoners) npcs_.push_back(std::move(c));
    }
    for (auto& a : npcs_) {
        if (a.self.isImmortal()) {
            a.self.qi = ThreeQiPool{120.0, 110.0, 100.0};
        }
    }
    // 定仙游：仙蛊唯一，原著归方源所有。此处据此把「自有」授予方源一人，
    // 其余 NPC 与玩家一律为「未持有」。
    if (NpcAgent* fy = findNpc("fangyuan")) {
        fy->self.dingXianYouPossession = DingXianYouPossession::Owned;
        fy->self.holdsDingXianYou = true;
    }

    // ---- 终章：开局为第六卷，幽魂存活，游戏永不结束 ----
    endgame_ = EndgameState{};
    endgame_.activeBranches.push_back(WorldBranch::Volume6Opening);
}

void GameWorld::setPlayer(const Cultivator& c) {
    playerCult_ = c;
    playerCult_.isPlayer = true;
    hasPlayer_  = true;
}

void GameWorld::createDefaultPlayer(const std::string& name, Domain bornDomain,
                                    Rank startingRank) {
    Cultivator p;
    p.id          = "player";
    p.name        = name;
    p.isPlayer    = true;
    p.bornDomain  = bornDomain;
    p.rank        = startingRank;

    // 六转以上为蛊仙：仙元池远厚于凡俗真元
    const bool immortal = is_immortal_rank(startingRank);
    p.maxEssence  = immortal ? 1000.0 : 100.0;
    p.essence     = p.maxEssence;
    p.demonIdentity = DemonIdentity::NewTransmigrator;   // 新天外之魔

    // 开局随身：家族传承所得。
    //  必须给 —— 凡蛊方（如四味酒虫）除合炼用蛊外还需「酸甜苦辣四味美酒」，
    //  若无背包存货，炼制必被判「材料不齐」，凡人第一条方子就炼不成。
    for (const auto& st : startingInventory()) p.bag.add(st.name, st.amount, st.kind, st.source, st.desc);
    p.dao.set(Dao::Refine, DaoLevel::Entry);
    // 六转开局即予主修流派「小成」造诣。
    // 若一律停在入门，自创炼蛊成功率仅一成上下、演示中几乎炼不成，
    // 这条玩法等于没有 —— 故开局给一副可堪一试的底子（自创依旧很难）。
    if (immortal) p.dao.set(Dao::Refine, DaoLevel::Small);

    // 蛊仙开局：给予一处自家福地，并把玩家直接置于其中。
    // 关键点：「闭关须在自家仙窍/福地」判定的是**身处**该地，
    // 而非仅仅拥有 —— 只给所有权而位置仍在野外，闭关依旧会被拒绝。
    if (immortal) {
        //
        //  蛊仙开局须有一处「自家仙窍」方可闭关 —— 只给所有权
        //  而位置仍在野外的话，闭关依旧会被拒绝。
        //
        //  此处借用狐仙福地作初始所属之地，属工程便利：
        //  狐仙福地实为中洲天梯山所有（已被更正），与出身域不必一致。
        //  玩家出身可以在南疆、北原任何一域，只是「有一处可闭关的福地」。
        //
        const char* homeId = "nj_huxian_fudi";   // 狐仙福地（中洲天梯山）
        p.ownedSites.push_back(homeId);
        p.flowLevels[Dao::Refine] = FlowLevel::Master;
        if (const CaveParadise* home = world_.findCave(homeId)) {
            p.location = home->location();
            p.dingXianYou.unlock(p.location, SightSource::Arrived, 0, "自家福地");
            // 定仙游为仙蛊、世间唯一，原著归方源所有。玩家开局不得持有，
            // 须经由支线「方源将定仙游借给你」获得临时使用权。
            p.qi = ThreeQiPool{120.0, 110.0, 100.0};
            setPlayer(p);
            return;
        }
        // 福地缺失时退化为凡俗开局，不静默失败
    }

    Location loc;
    loc.layer  = RealmLayer::MortalSurface;
    loc.domain = bornDomain;
    loc.region = "出身地";
    p.location = loc;

    // 开局仅知晓出身地 —— 定仙游坐标库从零积累
    p.dingXianYou.unlock(loc, SightSource::Arrived, 0, "出身地");
    setPlayer(p);
}

NpcAgent* GameWorld::findNpc(const std::string& id) {
    for (auto& a : npcs_) if (a.self.id == id) return &a;
    return nullptr;
}

std::vector<Cultivator*> GameWorld::allCultivators() {
    std::vector<Cultivator*> v;
    for (auto& a : npcs_) v.push_back(&a.self);
    if (hasPlayer_) v.push_back(&playerCult_);
    return v;
}

const KillerMoveDef* GameWorld::killerMove(MoveId id) const {
    for (const auto& m : killerMoves_) if (m.id == id) return &m;
    return nullptr;
}

GameWorld::StepReport GameWorld::step(int ticks) {
    StepReport rep;
    for (int i = 0; i < ticks; ++i) {
        world_.advanceTime(1);
        registry_.setNow(world_.now());

        // NPC 自主行动：不随玩家转动
        for (auto& a : npcs_) {
            auto r = NpcAi::tick(a, world_, factions_, rng_);
            for (const auto& n : r.notes) rep.notes.push_back("[T" +
                std::to_string(world_.now()) + "] " + n);
        }

        // 玩家：只做环境与三气推进，行为由玩家输入驱动
        if (hasPlayer_) {
            auto pr = CultivatorSystem::tick(playerCult_, world_);
            for (const auto& n : pr.notes) rep.notes.push_back("[T" +
                std::to_string(world_.now()) + "] 玩家：" + n);

            //  蛊虫平时要吃饭：每推进一个世界刻度，囊中蛊扣一次饱食度。
            //  久不喂则功效衰减乃至饿死 —— 这是原著的核心设定，
            //  蛊虫不是拿到手就一劳永逸的死物。
            if (playerCult_.carriedGu.size() <= playerCult_.carriedGu.max_size()) {
                auto fr = tickFeeding(playerCult_.carriedGu,
                                      refinery_.templates(), 1, &feedSkip_);
                for (GuId id : fr.starved) {
                    const GuTemplate* t = refinery_.guTemplate(
                        [&]() -> GuId {
                            for (const auto& g : playerCult_.carriedGu)
                                if (g.instanceId == id) return g.templateId;
                            return 0;
                        }());
                    rep.notes.push_back("[T" + std::to_string(world_.now()) +
                        "] 玩家：蛊虫「" + (t ? t->name : "？") + "」久未喂食，饿死了");
                }
            }
        }
        rep.tick = world_.now();
    }
    return rep;
}

Result<void> GameWorld::triggerEndgame(EndgameEvent e) {
    switch (e) {
        case EndgameEvent::YouHunKilled:
            return WorldBranchSystem::onYouHunKilled(endgame_);
        case EndgameEvent::YouTianEraTriggered:
            return WorldBranchSystem::onYouTianEraTriggered(endgame_);
        case EndgameEvent::FangYuanKilled:
            return WorldBranchSystem::onFangYuanKilled(endgame_);
        case EndgameEvent::TianTingToppled:
            return WorldBranchSystem::onTianTingToppled(endgame_);
        case EndgameEvent::PlayerFoundNewOrder:
            return WorldBranchSystem::onPlayerFoundNewOrder(endgame_);
    }
    return Result<void>::fail(Err::RankTooLow, "未知终章事件");
}

DetectionResult GameWorld::detectDemon(const Cultivator& target,
                                       DetectorClass observer) const {
    return OtherworldlyDemonDetection::evaluate(target.exposure, observer,
                                                target.isOtherworldlyDemon());
}

GameWorld::Snapshot GameWorld::snapshot() const {
    Snapshot s;
    s.tick          = world_.now();
    s.caves         = world_.allCaves().size();
    s.guRegistered  = registry_.size();
    s.npcs          = npcs_.size();
    for (const auto& a : npcs_) if (a.self.alive) ++s.aliveNpcs;
    for (WorldBranch b : endgame_.activeBranches)
        s.branches.push_back(to_string(b));
    s.gameEnded = WorldBranchSystem::isGameOver(endgame_);   // 恒 false
    return s;
}

} // namespace gr
