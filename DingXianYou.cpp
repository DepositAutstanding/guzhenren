// 定仙游机制实现：坐标必须先被亲眼见过 / 抵达过 / 感知过
#include "gr/cultivator/DingXianYou.hpp"
#include "gr/cultivator/Cultivator.hpp"

#include <algorithm>

namespace gr {

// ---------------------------------------------------------------- 坐标库
void DingXianYouCoordinates::unlock(const Location& loc, SightSource src,
                                    Tick at, std::string note) {
    const std::string k = loc.key();
    auto it = sites_.find(k);
    if (it == sites_.end()) {
        KnownSite s;
        s.loc        = loc;
        s.source     = src;
        s.acquiredAt = at;
        s.note       = std::move(note);
        sites_.emplace(k, std::move(s));
    } else {
        // 提升获知途径的可靠度：抵达 > 亲眼见 > 感知
        if (static_cast<int>(src) < static_cast<int>(it->second.source)) {
            it->second.source = src;
            it->second.note   = std::move(note);
        }
    }
}

const KnownSite* DingXianYouCoordinates::site(const Location& loc) const {
    auto it = sites_.find(loc.key());
    return it == sites_.end() ? nullptr : &it->second;
}

std::vector<KnownSite> DingXianYouCoordinates::all() const {
    std::vector<KnownSite> v;
    v.reserve(sites_.size());
    for (const auto& kv : sites_) v.push_back(kv.second);
    std::sort(v.begin(), v.end(), [](const KnownSite& a, const KnownSite& b) {
        return a.acquiredAt < b.acquiredAt;
    });
    return v;
}

std::vector<KnownSite> DingXianYouCoordinates::inLayer(RealmLayer l) const {
    std::vector<KnownSite> v;
    for (const auto& kv : sites_)
        if (kv.second.loc.layer == l) v.push_back(kv.second);
    return v;
}

// ---------------------------------------------------------------- 跳转
JumpResult DingXianYou::tryJump(Cultivator& c, const Location& target,
                                const WorldMap& world, bool holdsDingXianYouGu) {
    JumpResult r;

    if (!holdsDingXianYouGu) {
        r.err    = Err::DingXianYouNotHeld;
        r.detail = "未持有定仙游，无法进行空间跳转";
        return r;
    }

    // 铁律：定仙游绝对只能前往自己曾经亲眼见过、抵达过、感知过的坐标
    if (!c.dingXianYou.knows(target)) {
        r.err    = Err::CoordinateUnknown;
        r.detail = "未探索 / 未抵达 / 未亲眼看见的坐标，绝对无法定仙游跳转"
                   "（禁止无依据空跳、禁止地图全开跳转）";
        return r;
    }

    // 闭关期间不可外出
    if (!ThreeQiSystem::canLeave(c)) {
        r.err    = Err::LockedBySeclusion;
        r.detail = "定点闭关调和三气中，无法外出";
        return r;
    }

    // 世界通行合法性（胎壁 / 罡风 / 位面压制）
    auto trav = world.evaluateTraversal(c.location, target, c.rank);
    if (!trav.ok()) {
        r.err    = trav.err;
        r.detail = trav.detail;
        return r;
    }

    // 消耗：跳跃距离越远、层级跨度越大，仙元消耗越高
    double cost = 12.0;
    if (isHeavenLayer(c.location.layer) != isHeavenLayer(target.layer)) cost += 25.0;
    if (c.location.domain != target.domain) cost += 8.0;
    cost *= (1.0 + 0.5 * static_cast<double>(rank_value(c.rank) - 6 > 0
                                             ? rank_value(c.rank) - 6 : 0));

    if (c.essence < cost) {
        r.err    = Err::InsufficientEssence;
        r.detail = "仙元不足，无法催动定仙游";
        return r;
    }

    c.essence   -= cost;
    c.location   = target;
    r.essenceSpent = cost;
    r.landedAt   = target;

    const KnownSite* s = c.dingXianYou.site(target);
    r.detail = "定仙游成功 → " + target.key() +
               "（依据：" + to_string(s ? s->source : SightSource::Arrived) + "）";
    return r;
}

void DingXianYou::observe(Cultivator& c, const Location& loc, SightSource src, Tick at) {
    c.dingXianYou.unlock(loc, src, at);
}

} // namespace gr
