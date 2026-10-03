// ============================================================================
//  势力与世界分支体系
//    十四、世界分支体系（保留 + 适配新终章无限玩法）
//      1. 击杀幽魂 = 无幽天、旧世界永久延续、无限旧时代修行
//      2. 幽魂存活 = 幽天开启、新时代无限修行
//      3. 击杀方源、天庭高层均会永久改写世界格局
//      4. 所有分支无结局锁死，均为长期开放世界
//    八、终章机制彻底修改（取消结局锁死）
//      幽魂变天、四大结局 ≠ 游戏结尾；游戏为无限开放成长模式，无结局卡死
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"

#include <string>
#include <vector>

namespace gr {

enum class FactionId : std::uint8_t {
    Neutral    = 0,
    TianTing,      // 天庭
    FangYuan,      // 方源势力（本体 + 分身 + 影宗遗产）
    JuYang,        // 巨阳 / 长生天
    YouHun,        // 幽魂魔尊残党
    YingZong,      // 影宗
    RenDaoAlliance,// 人道联盟
    Merchant,      // 商家等散修势力
    PlayerFaction  // 玩家自建势力（含自建天庭 / 自建魔道秩序）
};

inline const char* to_string(FactionId f) {
    switch (f) {
        case FactionId::Neutral:        return "中立";
        case FactionId::TianTing:       return "天庭";
        case FactionId::FangYuan:       return "方源势力";
        case FactionId::JuYang:         return "巨阳/长生天";
        case FactionId::YouHun:         return "幽魂残党";
        case FactionId::YingZong:       return "影宗";
        case FactionId::RenDaoAlliance: return "人道联盟";
        case FactionId::Merchant:       return "散修商盟";
        case FactionId::PlayerFaction:  return "玩家自建势力";
    }
    return "？";
}

struct Faction {
    FactionId   id = FactionId::Neutral;
    std::string name;
    double      influence = 0.0;                 // 影响力
    std::vector<std::string> controlledSites;    // 已掌控洞天
    bool        leaderAlive = true;
};

// ---------------------------------------------------------------------------
//  世界分支状态：不锁死任何结局，只改写世界格局
// ---------------------------------------------------------------------------
enum class WorldBranch : std::uint8_t {
    Volume6Opening = 0,   // 第六卷开局：疯魔窟大战结束后
    YouHunSlain    = 1,   // 击杀幽魂：无幽天，旧世界永久延续
    YouTianEra     = 2,   // 幽魂存活：幽天开启，新时代
    FangYuanSlain  = 3,   // 击杀方源：世界格局永久改写
    TianTingToppled= 4,   // 天庭高层尽殁：秩序崩塌
    PlayerNewOrder = 5    // 玩家自建天庭 / 自建魔道秩序
};

inline const char* to_string(WorldBranch b) {
    switch (b) {
        case WorldBranch::Volume6Opening:  return "第六卷开局（疯魔窟大战后）";
        case WorldBranch::YouHunSlain:     return "击杀幽魂：旧世界永久延续";
        case WorldBranch::YouTianEra:      return "幽魂存活：幽天时代开启";
        case WorldBranch::FangYuanSlain:   return "击杀方源：格局永久改写";
        case WorldBranch::TianTingToppled: return "天庭倾覆：秩序崩塌";
        case WorldBranch::PlayerNewOrder:  return "玩家自建新秩序";
    }
    return "？";
}

// 终章标记：永远不是游戏结束
struct EndgameState {
    bool youHunAlive   = true;
    bool fangYuanAlive = true;
    bool tianTingIntact= true;
    bool youTianEraOpened = false;
    bool gameEnded     = false;   // 恒为 false：无结局锁死

    std::vector<WorldBranch> activeBranches;
};

class WorldBranchSystem {
public:
    // 8.1 幽魂变天、四大结局 ≠ 游戏结尾；这里只改写世界格局，绝不结束游戏
    static Result<void> onYouHunKilled(EndgameState& s);
    static Result<void> onYouTianEraTriggered(EndgameState& s);
    static Result<void> onFangYuanKilled(EndgameState& s);
    static Result<void> onTianTingToppled(EndgameState& s);
    static Result<void> onPlayerFoundNewOrder(EndgameState& s);

    // 无论进入哪个分支，游戏都不结束
    static bool isGameOver(const EndgameState& s) { (void)s; return false; }
};

// 势力关系
class FactionSystem {
public:
    void addFaction(const Faction& f);
    Faction* faction(FactionId id);
    const Faction* faction(FactionId id) const;

    // 关系值：-1.0 死敌 ~ +1.0 同盟
    void   setRelation(FactionId a, FactionId b, double v);
    double relation(FactionId a, FactionId b) const;
    bool   hostile(FactionId a, FactionId b) const { return relation(a, b) < -0.2; }

    // 第六卷开局势力格局
    void setupVolume6Opening();

    std::vector<Faction> all() const;

private:
    static std::uint64_t key(FactionId a, FactionId b) {
        std::uint64_t x = static_cast<std::uint64_t>(a);
        std::uint64_t y = static_cast<std::uint64_t>(b);
        return (x << 8) | y;
    }
    std::vector<Faction> factions_;
    std::vector<std::pair<std::uint64_t, double>> relations_;
};

} // namespace gr
