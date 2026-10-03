// ============================================================================
//  《蛊真人》开放世界 —— 顶层世界门面
//  聚合全部子系统，提供统一的构建 / 推进 / 查询入口。
//  时间基线固定：第六卷开局（疯魔窟大战结束后）
// ============================================================================
#pragma once

#include "gr/core/Rng.hpp"
#include "gr/world/WorldMap.hpp"
#include "gr/world/Settlement.hpp"
#include "gr/gu/Refining.hpp"
#include "gr/gu/GuRegistry.hpp"
#include "gr/gu/KillerMove.hpp"
#include "gr/cultivator/Cultivator.hpp"
#include "gr/battle/Battle.hpp"
#include "gr/ai/Faction.hpp"
#include "gr/ai/NpcAgent.hpp"
#include "gr/ai/Detection.hpp"
#include "gr/net/MultiplayerReserve.hpp"

#include <string>
#include <vector>
#include <unordered_map>

namespace gr {

// 终章事件类型（八、终章机制彻底修改：无结局锁死）
enum class EndgameEvent : std::uint8_t {
    YouHunKilled = 0,
    YouTianEraTriggered,
    FangYuanKilled,
    TianTingToppled,
    PlayerFoundNewOrder
};

class GameWorld {
public:
    explicit GameWorld(std::uint64_t seed = 20240906);
    ~GameWorld() = default;

    // ---------------- 构建 ----------------
    // 固定第六卷开局：疯魔窟大战结束后
    void buildVolume6Opening(int randomCaveCount = 24);

    // 玩家：独立于 NPC 列表，避免 NPC AI 驱动玩家
    void setPlayer(const Cultivator& c);
    Cultivator* player() { return hasPlayer_ ? &playerCult_ : nullptr; }
    const Cultivator* player() const { return hasPlayer_ ? &playerCult_ : nullptr; }
    // 开局修为可选：
    //   · 一转（默认）—— 还原「新天外之魔」彭达的起点，从凡人成长
    //   · 六转       —— 直接以蛊仙开局。因三气平衡、闭关、洞天争夺、成尊
    //                   等核心机制均为六转以上内容，一转开局时这些系统
    //                   在界面上不可操作，故提供此选项便于体验与验证。
    void createDefaultPlayer(const std::string& name, Domain bornDomain,
                             Rank startingRank = Rank::R1);

    // ---------------- 推进 ----------------
    struct StepReport {
        Tick tick = 0;
        std::vector<std::string> notes;
    };
    StepReport step(int ticks = 1);

    //  喂养豁免：炼制中的材料蛊不扣饱食。
    //  它们正处于炼化过程，不该因为「没空喂」而在炉里饿死 ——
    //  否则长时炼制（推进数十刻度）必然失败，与炼蛊玩法冲突。
    void setFeedingSkip(std::vector<GuId> ids) { feedSkip_ = std::move(ids); }

    // ---------------- 查询 ----------------
    WorldMap&        world()       { return world_; }
    const WorldMap&  world() const { return world_; }
    Refinery&        refinery()    { return refinery_; }
    const Refinery&  refinery() const { return refinery_; }
    ImmortalGuRegistry& registry() { return registry_; }
    const ImmortalGuRegistry& registry() const { return registry_; }

    //  聚落与建筑：人类群居地里有什么
    SettlementRegistry& settlements() { return settlements_; }
    const SettlementRegistry& settlements() const { return settlements_; }
    FactionSystem&   factions()    { return factions_; }
    MultiplayerReserve& online()   { return online_; }
    const EndgameState& endgame() const { return endgame_; }

    std::vector<NpcAgent>& npcs() { return npcs_; }
    const std::vector<NpcAgent>& npcs() const { return npcs_; }
    NpcAgent* findNpc(const std::string& id);
    std::vector<Cultivator*> allCultivators();

    // 杀招表
    const std::vector<KillerMoveDef>& killerMoves() const { return killerMoves_; }
    const KillerMoveDef* killerMove(MoveId id) const;

    // ---------------- 终章分支（永不结束游戏） ----------------
    Result<void> triggerEndgame(EndgameEvent e);

    // ---------------- 天外之魔侦测 ----------------
    DetectionResult detectDemon(const Cultivator& target, DetectorClass observer) const;

    // ---------------- 存档快照（调试用） ----------------
    struct Snapshot {
        Tick tick = 0;
        std::size_t caves = 0;
        std::size_t guRegistered = 0;
        std::size_t npcs = 0;
        std::size_t aliveNpcs = 0;
        std::vector<std::string> branches;
        bool gameEnded = false;
    };
    Snapshot snapshot() const;

    Rng& rng() { return rng_; }

private:
    Rng                  rng_;
    WorldMap             world_;
    std::vector<GuId> feedSkip_;
    Refinery             refinery_;
    ImmortalGuRegistry   registry_;
    SettlementRegistry   settlements_;
    FactionSystem        factions_;
    MultiplayerReserve   online_;
    EndgameState         endgame_;
    std::vector<NpcAgent> npcs_;
    std::vector<KillerMoveDef> killerMoves_;
    Cultivator playerCult_;
    bool   hasPlayer_ = false;
};

} // namespace gr
