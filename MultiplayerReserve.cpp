// 联机预留实现：不改动单机本体
#include "gr/net/MultiplayerReserve.hpp"

namespace gr {

MultiplayerReserve::MultiplayerReserve(IOnlineBackend* backend)
    : backend_(backend ? backend : &nullBackend_) {}

Result<void> MultiplayerReserve::require(const OnlineEvent& e) {
    if (!enabled()) {
        return Result<void>::fail(Err::RankTooLow,
            std::string("联机模块仅预留架构，未启用：") + to_string(e.feature));
    }
    return backend_->publish(e);
}

Result<void> MultiplayerReserve::invadeParadise(const std::string& peerId,
                                                const std::string& siteId) {
    OnlineEvent e;
    e.feature = OnlineFeature::ParadiseInvasion;
    e.peerId  = peerId;
    e.payload = siteId;
    return require(e);
}

Result<void> MultiplayerReserve::tradeViaBaoHuangTian(const std::string& peerId,
                                                      const std::string& guName) {
    OnlineEvent e;
    e.feature = OnlineFeature::BaoHuangTianTrade;
    e.peerId  = peerId;
    e.payload = guName;
    return require(e);
}

Result<void> MultiplayerReserve::factionWar(const std::string& peerId, bool declare) {
    OnlineEvent e;
    e.feature = OnlineFeature::FactionWarAndAlliance;
    e.peerId  = peerId;
    e.payload = declare ? "declare" : "ally";
    return require(e);
}

Result<void> MultiplayerReserve::triggerSharedWorldEvent(const std::string& eventId) {
    OnlineEvent e;
    e.feature = OnlineFeature::SharedWorldEvent;
    e.payload = eventId;
    return require(e);
}

Result<void> MultiplayerReserve::joinSharedWorld(const std::string& peerId) {
    OnlineEvent e;
    e.feature = OnlineFeature::MultiplayerWorldCoexist;
    e.peerId  = peerId;
    return require(e);
}

MultiplayerReserve::CoexistCheck MultiplayerReserve::checkGuUniquenessAcrossWorlds(
    const ImmortalGuRegistry& local,
    const std::vector<ImmortalGuRegistry::Snapshot>& peerWorlds) {
    CoexistCheck ck;
    for (const auto& e : local.allEntries()) {
        if (e.state == GuState::Destroyed) continue;      // 已彻底毁灭，名额已释放
        if (e.special != GuSpecial::None) continue;       // 仿伪蛊 / 尊者幻象：唯二特例
        for (const auto& w : peerWorlds) {
            if (w.worldId == local.worldId()) continue;   // 同一世界的快照不参与跨世界比对
            for (const auto& pe : w.entries) {
                if (pe.state == GuState::Destroyed) continue;
                if (pe.special != GuSpecial::None) continue;
                // 跨世界：instanceId 编号空间相互独立，不可用于排除「同一只蛊」；
                // 同名仙蛊若在两个世界同时存在，即构成仙蛊唯一冲突。
                if (pe.guName == e.guName) {
                    ck.compatible = false;
                    ck.conflicts.push_back(e.guName + "（本地#" +
                                           std::to_string(e.instanceId) + " vs 远端#" +
                                           std::to_string(pe.instanceId) + "）");
                }
            }
        }
    }
    ck.detail = ck.compatible
        ? "仙蛊唯一在多人世界下兼容：无同名冲突"
        : "检测到 " + std::to_string(ck.conflicts.size()) + " 项同名仙蛊冲突";
    return ck;
}

std::vector<OnlineEvent> MultiplayerReserve::pollEvents() {
    if (!enabled()) return {};
    return backend_->drain();
}

} // namespace gr
