// ============================================================================
//  九、预留联机功能（正式写入底层框架）
//    9.1 联机预留接口（完全预留，不改动单机本体）
//      1. 底层框架完全支持后续联机拓展
//      2. 预留：
//         · 多玩家世界共存
//         · 玩家洞天互相入侵
//         · 玩家宝黄天跨玩家交易
//         · 玩家势力对战、结盟
//         · 多玩家共同触发大世界事件
//      3. 当前版本为纯单机，联机模块只预留架构，不启用
//      4. 仙蛊唯一、世界唯一实例机制可兼容联机多人世界
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"
#include "gr/core/Status.hpp"
#include "gr/gu/GuWorm.hpp"
#include "gr/gu/GuRegistry.hpp"

#include <functional>
#include <string>
#include <vector>

namespace gr {

// 联机特性开关：当前版本全部关闭，仅保留架构
enum class OnlineFeature : std::uint8_t {
    MultiplayerWorldCoexist = 0,  // 多玩家世界共存
    ParadiseInvasion,             // 玩家洞天互相入侵
    BaoHuangTianTrade,            // 玩家宝黄天跨玩家交易
    FactionWarAndAlliance,        // 玩家势力对战、结盟
    SharedWorldEvent              // 多玩家共同触发大世界事件
};

inline const char* to_string(OnlineFeature f) {
    switch (f) {
        case OnlineFeature::MultiplayerWorldCoexist: return "多玩家世界共存";
        case OnlineFeature::ParadiseInvasion:       return "玩家洞天互相入侵";
        case OnlineFeature::BaoHuangTianTrade:      return "宝黄天跨玩家交易";
        case OnlineFeature::FactionWarAndAlliance:  return "势力对战/结盟";
        case OnlineFeature::SharedWorldEvent:       return "大世界事件共触发";
    }
    return "？";
}

// ---------------------------------------------------------------------------
//  联机事件：单机下永不产生；联机启用后由网络层投递
// ---------------------------------------------------------------------------
struct OnlineEvent {
    OnlineFeature feature = OnlineFeature::MultiplayerWorldCoexist;
    std::string   peerId;
    std::string   payload;   // 序列化后的事件体
};

// ---------------------------------------------------------------------------
//  联机预留接口：纯虚接口 + 空实现，保证单机本体零改动
// ---------------------------------------------------------------------------
class IOnlineBackend {
public:
    virtual ~IOnlineBackend() = default;
    virtual bool enabled() const = 0;
    virtual Result<void>  publish(const OnlineEvent& e) = 0;
    virtual std::vector<OnlineEvent> drain() = 0;
};

// 单机默认后端：什么都不做
class NullOnlineBackend final : public IOnlineBackend {
public:
    bool enabled() const override { return false; }
    Result<void> publish(const OnlineEvent&) override {
        return Result<void>::fail(Err::RankTooLow, "联机模块未启用（当前为纯单机）");
    }
    std::vector<OnlineEvent> drain() override { return {}; }
};

// ---------------------------------------------------------------------------
//  联机预留门面：所有预留能力在此登记，单机下调用即被安全拒绝
// ---------------------------------------------------------------------------
class MultiplayerReserve {
public:
    explicit MultiplayerReserve(IOnlineBackend* backend = nullptr);

    bool enabled() const { return backend_ && backend_->enabled(); }

    // 预留能力入口（未启用时统一返回失败，不改变单机本体行为）
    Result<void> invadeParadise(const std::string& peerId, const std::string& siteId);
    Result<void> tradeViaBaoHuangTian(const std::string& peerId, const std::string& guName);
    Result<void> factionWar(const std::string& peerId, bool declare);
    Result<void> triggerSharedWorldEvent(const std::string& eventId);
    Result<void> joinSharedWorld(const std::string& peerId);

    // 仙蛊唯一在联机多人世界下的兼容校验：
    // 同一世界内同名仙蛊仍只能有一只；跨玩家世界由注册表快照兜底
    struct CoexistCheck {
        bool compatible = true;
        std::vector<std::string> conflicts;
        std::string detail;
    };
    static CoexistCheck checkGuUniquenessAcrossWorlds(
        const ImmortalGuRegistry& local,
        const std::vector<ImmortalGuRegistry::Snapshot>& peerWorlds);

    std::vector<OnlineEvent> pollEvents();
    IOnlineBackend* backend() { return backend_; }

private:
    Result<void> require(const OnlineEvent& e);
    IOnlineBackend* backend_;
    NullOnlineBackend nullBackend_;
};

} // namespace gr
