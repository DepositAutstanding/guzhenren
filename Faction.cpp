// 势力与世界分支实现
#include "gr/ai/Faction.hpp"

#include <algorithm>

namespace gr {

// ---------------------------------------------------------------- 世界分支
Result<void> WorldBranchSystem::onYouHunKilled(EndgameState& s) {
    s.youHunAlive    = false;
    s.youTianEraOpened = false;
    if (std::find(s.activeBranches.begin(), s.activeBranches.end(),
                  WorldBranch::YouHunSlain) == s.activeBranches.end())
        s.activeBranches.push_back(WorldBranch::YouHunSlain);
    return Result<void>::success("击杀幽魂：无幽天，旧世界永久延续，无限旧时代修行（游戏不结束）");
}

Result<void> WorldBranchSystem::onYouTianEraTriggered(EndgameState& s) {
    s.youTianEraOpened = true;
    if (std::find(s.activeBranches.begin(), s.activeBranches.end(),
                  WorldBranch::YouTianEra) == s.activeBranches.end())
        s.activeBranches.push_back(WorldBranch::YouTianEra);
    return Result<void>::success("幽魂存活：幽天开启，新时代无限修行（游戏不结束）");
}

Result<void> WorldBranchSystem::onFangYuanKilled(EndgameState& s) {
    s.fangYuanAlive = false;
    if (std::find(s.activeBranches.begin(), s.activeBranches.end(),
                  WorldBranch::FangYuanSlain) == s.activeBranches.end())
        s.activeBranches.push_back(WorldBranch::FangYuanSlain);
    return Result<void>::success("击杀方源：世界格局永久改写（游戏不结束）");
}

Result<void> WorldBranchSystem::onTianTingToppled(EndgameState& s) {
    s.tianTingIntact = false;
    if (std::find(s.activeBranches.begin(), s.activeBranches.end(),
                  WorldBranch::TianTingToppled) == s.activeBranches.end())
        s.activeBranches.push_back(WorldBranch::TianTingToppled);
    return Result<void>::success("天庭高层尽殁：秩序崩塌（游戏不结束）");
}

Result<void> WorldBranchSystem::onPlayerFoundNewOrder(EndgameState& s) {
    if (std::find(s.activeBranches.begin(), s.activeBranches.end(),
                  WorldBranch::PlayerNewOrder) == s.activeBranches.end())
        s.activeBranches.push_back(WorldBranch::PlayerNewOrder);
    return Result<void>::success("玩家自建天庭 / 自建魔道秩序（游戏不结束）");
}

// ---------------------------------------------------------------- 势力
void FactionSystem::addFaction(const Faction& f) {
    for (auto& x : factions_)
        if (x.id == f.id) { x = f; return; }
    factions_.push_back(f);
}

Faction* FactionSystem::faction(FactionId id) {
    for (auto& f : factions_) if (f.id == id) return &f;
    return nullptr;
}

const Faction* FactionSystem::faction(FactionId id) const {
    for (const auto& f : factions_) if (f.id == id) return &f;
    return nullptr;
}

void FactionSystem::setRelation(FactionId a, FactionId b, double v) {
    const std::uint64_t k1 = key(a, b), k2 = key(b, a);
    for (auto& kv : relations_) {
        if (kv.first == k1 || kv.first == k2) { kv.second = v; return; }
    }
    relations_.emplace_back(k1, v);
}

double FactionSystem::relation(FactionId a, FactionId b) const {
    if (a == b) return 1.0;
    const std::uint64_t k1 = key(a, b), k2 = key(b, a);
    for (const auto& kv : relations_)
        if (kv.first == k1 || kv.first == k2) return kv.second;
    return 0.0;  // 默认互不相干
}

void FactionSystem::setupVolume6Opening() {
    // 断更时刻（第三百六十八节「方源、巨阳战星宿」）格局：
    // 方源 + 巨阳 + 失控幽魂 三面施压天庭
    auto mk = [this](FactionId id, const char* n, double inf, bool leader) {
        Faction f; f.id = id; f.name = n; f.influence = inf; f.leaderAlive = leader;
        addFaction(f);
    };
    mk(FactionId::TianTing,       "天庭",        90.0, true);
    mk(FactionId::FangYuan,       "方源势力",    95.0, true);
    mk(FactionId::JuYang,         "巨阳/长生天", 80.0, true);
    mk(FactionId::YouHun,         "幽魂残党",    45.0, true);
    mk(FactionId::YingZong,       "影宗",        30.0, true);
    mk(FactionId::RenDaoAlliance, "人道联盟",    35.0, true);
    mk(FactionId::Merchant,       "散修商盟",    25.0, true);
    mk(FactionId::PlayerFaction,  "玩家自建势力", 5.0, true);

    // 三方围攻天庭
    setRelation(FactionId::TianTing, FactionId::FangYuan, -0.95);
    setRelation(FactionId::TianTing, FactionId::JuYang,   -0.70);
    setRelation(FactionId::TianTing, FactionId::YouHun,   -0.85);
    // 方源与巨阳：临时合作，缺乏信任
    setRelation(FactionId::FangYuan, FactionId::JuYang,    0.30);
    // 影宗已并入方源资源
    setRelation(FactionId::FangYuan, FactionId::YingZong,  0.60);
    // 人道联盟与方源：介入但不完全可控
    setRelation(FactionId::FangYuan, FactionId::RenDaoAlliance, 0.25);
    setRelation(FactionId::TianTing, FactionId::RenDaoAlliance, 0.15);
}

std::vector<Faction> FactionSystem::all() const { return factions_; }

} // namespace gr
