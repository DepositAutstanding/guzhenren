// ============================================================================
//  四、定仙游机制完全重写
//    4.1 全新唯一规则
//      定仙游绝对只能前往：自己曾经亲眼见过、抵达过、感知过的坐标
//        1. 未探索、未抵达、未亲眼看见的地点绝对无法定仙游跳转
//        2. 禁止无依据空跳、禁止地图全开跳转
//        3. 蛊仙探索地图、解锁视野、抵达点位，才能积累定仙游坐标库
//        4. NPC 同样遵守该规则：NPC 不会跳从未去过的地方
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"
#include "gr/world/WorldMap.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace gr {

struct Cultivator;  // 前向声明

// 坐标的获知途径
enum class SightSource : std::uint8_t {
    Arrived   = 0,  // 亲自抵达过
    Seen      = 1,  // 亲眼看见过
    Perceived = 2   // 感知过（神念/侦查蛊/道痕共鸣等）
};

inline const char* to_string(SightSource s) {
    switch (s) {
        case SightSource::Arrived:   return "抵达过";
        case SightSource::Seen:      return "亲眼见过";
        case SightSource::Perceived: return "感知过";
    }
    return "？";
}

struct KnownSite {
    Location    loc;
    SightSource source = SightSource::Arrived;
    Tick        acquiredAt = 0;
    std::string note;
};

// ---------------------------------------------------------------------------
//  定仙游坐标库 —— 玩家与 NPC 共用同一实现，规则完全平等
// ---------------------------------------------------------------------------
class DingXianYouCoordinates {
public:
    // 记录一个坐标（探索、抵达、感知均调用）
    void unlock(const Location& loc, SightSource src, Tick at, std::string note = {});

    // 是否已解锁
    bool knows(const Location& loc) const { return sites_.count(loc.key()) > 0; }
    bool knows(const std::string& key) const { return sites_.count(key) > 0; }

    const KnownSite* site(const Location& loc) const;

    std::size_t count() const { return sites_.size(); }
    std::vector<KnownSite> all() const;

    // 按层级筛选，便于调试与 UI
    std::vector<KnownSite> inLayer(RealmLayer l) const;

    // 遗忘（如记忆被夺、道痕清洗）
    void forget(const Location& loc) { sites_.erase(loc.key()); }

private:
    std::unordered_map<std::string, KnownSite> sites_;
};

// ---------------------------------------------------------------------------
//  跳转判定
// ---------------------------------------------------------------------------
struct JumpRequest {
    Location target;
    bool     usesDingXianYou = false;  // 是否经由定仙游蛊（而非其他宇道手段）
};

struct JumpResult {
    Err         err = Err::Ok;
    Location    landedAt;
    double      essenceSpent = 0;
    std::string detail;
    bool ok() const { return err == Err::Ok; }
};

class DingXianYou {
public:
    // 核心判定：目标坐标必须已被亲眼见过 / 抵达过 / 感知过
    static JumpResult tryJump(Cultivator& c, const Location& target,
                              const WorldMap& world, bool holdsDingXianYouGu);

    // 探索行为：抵达 / 看见 / 感知 都会扩充坐标库
    static void observe(Cultivator& c, const Location& loc, SightSource src, Tick at);
};

} // namespace gr
