// 仙蛊唯一注册表实现
#include "gr/gu/GuRegistry.hpp"

#include <algorithm>

namespace gr {

bool ImmortalGuRegistry::isNameOccupied(const std::string& guName) const {
    auto it = byName_.find(guName);
    if (it == byName_.end()) return false;
    const RegistryEntry& e = entries_.at(it->second);
    if (e.special != GuSpecial::None) return false;   // 仿伪蛊 / 尊者幻象：唯二特例
    return e.state != GuState::Destroyed;             // 封印、封存仍占位
}

std::optional<RegistryEntry> ImmortalGuRegistry::lookupByName(const std::string& guName) const {
    auto it = byName_.find(guName);
    if (it == byName_.end()) return std::nullopt;
    return entries_.at(it->second);
}

std::optional<RegistryEntry> ImmortalGuRegistry::lookupById(GuId instanceId) const {
    auto it = entries_.find(instanceId);
    if (it == entries_.end()) return std::nullopt;
    return it->second;
}

Result<GuId> ImmortalGuRegistry::registerGu(const std::string& guName, Rank rank,
                                            const std::string& holder,
                                            GuSpecial special, bool defective) {
    // 特例（仿伪蛊 / 尊者幻象）不占唯一名额，允许重复注册
    if (special == GuSpecial::None && isNameOccupied(guName)) {
        auto cur = lookupByName(guName);
        std::string d = "「" + guName + "」世间已存在（状态：" +
                        to_string(cur ? cur->state : GuState::Active) +
                        "，持有者：" + (cur ? cur->holder : std::string("?")) +
                        "）。封印/封存不清除注册表，仅彻底毁灭后方可重生。";
        return Result<GuId>::fail(Err::UniqueGuViolation, d);
    }

    GuId id = nextId_++;
    RegistryEntry e;
    e.instanceId  = id;
    e.guName      = guName;
    e.rank        = rank;
    e.state       = GuState::Active;
    e.holder      = holder;
    e.special     = special;
    e.defective   = defective;
    e.registeredAt = now_;
    entries_.emplace(id, e);
    if (special == GuSpecial::None) byName_[guName] = id;
    return Result<GuId>::success(id, "注册仙蛊：" + guName);
}

Result<void> ImmortalGuRegistry::seal(GuId id) {
    auto it = entries_.find(id);
    if (it == entries_.end()) return Result<void>::fail(Err::RankTooLow, "仙蛊不存在");
    if (it->second.state == GuState::Destroyed)
        return Result<void>::fail(Err::UniqueGuViolation, "已彻底毁灭，无法封印");
    it->second.state = GuState::Sealed;
    return Result<void>::success("封印（不清除注册表，仍占唯一名额）");
}

Result<void> ImmortalGuRegistry::unseal(GuId id) {
    auto it = entries_.find(id);
    if (it == entries_.end()) return Result<void>::fail(Err::RankTooLow, "仙蛊不存在");
    if (it->second.state != GuState::Sealed)
        return Result<void>::fail(Err::UniqueGuViolation, "该仙蛊未被封印");
    it->second.state = GuState::Active;
    return Result<void>::success("解封");
}

Result<void> ImmortalGuRegistry::storeAway(GuId id) {
    auto it = entries_.find(id);
    if (it == entries_.end()) return Result<void>::fail(Err::RankTooLow, "仙蛊不存在");
    if (it->second.state == GuState::Destroyed)
        return Result<void>::fail(Err::UniqueGuViolation, "已彻底毁灭");
    it->second.state = GuState::Stored;
    return Result<void>::success("封存（不清除注册表，仍占唯一名额）");
}

Result<void> ImmortalGuRegistry::transfer(GuId id, const std::string& newHolder) {
    auto it = entries_.find(id);
    if (it == entries_.end()) return Result<void>::fail(Err::RankTooLow, "仙蛊不存在");
    if (it->second.state == GuState::Destroyed)
        return Result<void>::fail(Err::UniqueGuViolation, "已彻底毁灭，无法转移");
    std::string old = it->second.holder;
    it->second.holder = newHolder;
    if (it->second.state == GuState::Sealed) it->second.state = GuState::Active;
    return Result<void>::success("持有权转移：" + old + " → " + newHolder +
                                 "（抢夺不改变注册表唯一性）");
}

Result<void> ImmortalGuRegistry::destroy(GuId id) {
    auto it = entries_.find(id);
    if (it == entries_.end()) return Result<void>::fail(Err::RankTooLow, "仙蛊不存在");
    it->second.state       = GuState::Destroyed;
    it->second.destroyedAt = now_;
    it->second.holder.clear();
    return Result<void>::success("彻底毁灭：「" + it->second.guName +
                                 "」名额释放，未来可被重新炼出");
}

bool ImmortalGuRegistry::wasDestroyed(const std::string& guName) const {
    auto e = lookupByName(guName);
    return e.has_value() && e->state == GuState::Destroyed;
}

std::vector<RegistryEntry> ImmortalGuRegistry::allEntries() const {
    std::vector<RegistryEntry> v;
    v.reserve(entries_.size());
    for (const auto& kv : entries_) v.push_back(kv.second);
    std::sort(v.begin(), v.end(),
              [](const RegistryEntry& a, const RegistryEntry& b) {
                  return a.instanceId < b.instanceId;
              });
    return v;
}

ImmortalGuRegistry::Snapshot ImmortalGuRegistry::snapshot() const {
    Snapshot s;
    s.entries = allEntries();
    s.at      = now_;
    s.worldId = worldId_;
    return s;
}

} // namespace gr
