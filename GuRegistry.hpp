// ============================================================================
//  十一、仙蛊唯一体系 —— 全域注册表
//    · 六转至九转仙蛊全域唯一，同一时刻世间只能有一只同名仙蛊
//    · 封印 / 抢夺 / 封存 均「不清除注册表」，仍占据唯一名额
//    · 只有「彻底毁灭」后，该仙蛊才可在未来被重新炼出
//    · 仿伪蛊、尊者幻象为唯二特例，不占名额
//    · NPC 与玩家完全平等：同一张表，同一套规则
//    · 预留联机：注册表本身是世界级单例，可平移到多玩家共存世界
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"
#include "gr/gu/GuWorm.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace gr {

struct RegistryEntry {
    GuId        instanceId = 0;
    std::string guName;
    Rank        rank = Rank::R6;
    GuState     state = GuState::Active;
    std::string holder;                 // 持有者 id（玩家或 NPC，完全平等）
    GuSpecial   special = GuSpecial::None;
    bool        defective = false;      // 残次品（残缺蛊方产物）
    Tick        registeredAt = 0;
    Tick        destroyedAt  = -1;      // 彻底毁灭的世界时刻
};

class ImmortalGuRegistry {
public:
    // worldId：世界标识。联机多玩家世界共存时，各世界的仙蛊唯一名额相互独立，
    // 但同名仙蛊的跨世界冲突需要被检出（详见 MultiplayerReserve）。
    explicit ImmortalGuRegistry(Tick startTick = 0, std::uint32_t worldId = 1)
        : now_(startTick), worldId_(worldId) {}

    void setNow(Tick t) { now_ = t; }
    Tick now() const { return now_; }
    std::uint32_t worldId() const { return worldId_; }

    // ------------------------------------------------------------
    //  唯一性查询：同名仙蛊在世？
    //  Sealed / Stored 同样占位；Destroyed 不占位；特例不占位
    // ------------------------------------------------------------
    bool isNameOccupied(const std::string& guName) const;
    std::optional<RegistryEntry> lookupByName(const std::string& guName) const;
    std::optional<RegistryEntry> lookupById(GuId instanceId) const;

    // ------------------------------------------------------------
    //  注册：炼制出仙蛊时调用
    //  返回失败 Err::UniqueGuViolation 表示违反仙蛊唯一
    // ------------------------------------------------------------
    Result<GuId> registerGu(const std::string& guName, Rank rank,
                            const std::string& holder,
                            GuSpecial special = GuSpecial::None,
                            bool defective = false);

    // ------------------------------------------------------------
    //  状态流转：封印 / 解封 / 封存 / 抢夺（转移持有）
    //  这三者都不清除注册表
    // ------------------------------------------------------------
    Result<void> seal(GuId id);       // 封印
    Result<void> unseal(GuId id);     // 解封
    Result<void> storeAway(GuId id);  // 封存
    Result<void> transfer(GuId id, const std::string& newHolder);  // 抢夺/交易

    // ------------------------------------------------------------
    //  彻底毁灭：唯一可让同名仙蛊在未来重生的途径
    // ------------------------------------------------------------
    Result<void> destroy(GuId id);
    bool wasDestroyed(const std::string& guName) const;

    // ------------------------------------------------------------
    //  统计 / 调试
    // ------------------------------------------------------------
    std::vector<RegistryEntry> allEntries() const;
    std::size_t size() const { return byName_.size(); }

    // 预留联机：导出快照，用于多玩家世界共存时的权威性校验
    struct Snapshot {
        std::vector<RegistryEntry> entries;
        Tick         at      = 0;
        std::uint32_t worldId = 1;
    };
    Snapshot snapshot() const;

private:
    Tick          now_ = 0;
    std::uint32_t worldId_ = 1;
    GuId nextId_ = 1;
    std::unordered_map<std::string, GuId> byName_;              // 同名唯一索引
    std::unordered_map<GuId, RegistryEntry> entries_;
};

} // namespace gr
