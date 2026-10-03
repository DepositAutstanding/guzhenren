// ============================================================================
//  1.2 世界空间全新架构（彻底重写，删除界外空域）
//    1. 五域凡界（地表 + 地下）
//    2. 黑天、白天两大位面（悬浮覆盖五域上空）
//    3. 世界胎壁（世界绝对边界，不可突破）
//  核心规则：
//    · 不存在界外空域，全地图所有空间全部收纳在两天五域之内
//    · 世界胎壁为世界绝对屏障：99.99% 蛊师/蛊仙永远无法穿透；
//      仅极少数特殊尊者级、特殊宿命存在可短暂触碰/突破（NPC 剧情限定，玩家不可突破）
//  ============================================================================
#pragma once

#include "gr/core/Types.hpp"
#include "gr/core/Status.hpp"
#include "gr/core/Rng.hpp"
#include "gr/world/Climate.hpp"
#include "gr/world/CaveParadise.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace gr {

// 南疆瘴气固定刷新区域（2.2：仅固定刷新在特定瘴气谷、毒泽、原始深林禁地）
inline const std::unordered_set<std::string>& miasma_regions() {
    static const std::unordered_set<std::string> s = {
        "瘴气谷", "毒泽", "原始深林禁地", "万毒谷", "腐臭烂泥山"
    };
    return s;
}

// 北原极寒相关（2.1：仅极夜/暴风雪/深夜极寒时段触发）
inline const std::unordered_set<std::string>& beiyuan_cold_regions() {
    static const std::unordered_set<std::string> s = {
        "北原腹地", "冰原", "极夜荒原", "暴雪岭", "长生天"
    };
    return s;
}

// 允许突破世界胎壁的特殊存在（NPC 剧情限定，玩家永远不在此列）
enum class WombWallPrivilege : std::uint8_t {
    None = 0,
    StoryOnlyZunZhe = 1  // 极少数特殊尊者级/特殊宿命存在，剧情限定
};

struct TraverseCost {
    double essence    = 0.0;  // 仙元/真元消耗
    double daoMarks   = 0.0;  // 道痕损耗
    bool   lethal     = false;
    std::string note;
};

// ---------------------------------------------------------------------------
//  世界地图：层级、气候、洞天分布、通行与环境结算的唯一入口
// ---------------------------------------------------------------------------
class WorldMap {
public:
    WorldMap();

    // ---------------- 地图构建 ----------------
    void buildCanonWorld(Rng& rng, int randomCaveCount = 24);

    void addCave(const CaveParadise& c);
    const CaveParadise* findCave(const std::string& siteId) const;
    CaveParadise*       findCaveMut(const std::string& siteId);
    std::vector<const CaveParadise*> cavesIn(RealmLayer l) const;
    std::vector<const CaveParadise*> allCaves() const;

    // ---------------- 世界时间推进 ----------------
    // 罡风、气候、万劫倒计时均随世界时间自主演化
    void advanceTime(Tick dt = 1);
    Tick now() const { return now_; }

    const TiangangGale& gale() const { return gale_; }
    const WorldClimate& climate() const { return climate_; }
    void setClimate(const WorldClimate& c) { climate_ = c; }

    // ---------------- 层级合法性 ----------------
    static bool isValidLayer(RealmLayer l) { return l != RealmLayer::WorldWombWall; }

    // ------------------------------------------------------------------
    //  五域界壁（地理研究：中洲圣贤界壁、南疆瘴气界壁、北原甘草界壁、
    //  西漠狂炎界壁、东海苍水界壁）
    //
    //  界壁是「空间摩擦力」而非国境线篱笆：
    //    · 对本域引力、外域斥力
    //    · 对层次越高者阻碍越强 —— 这不是 bug，是设定
    //    · 蛊仙穿越通常要付出可观代价
    //  东海因海潮使部分界壁较薄弱，故外域人、散修与商贸更活跃（B 级）。
    // ------------------------------------------------------------------
    static const char* domainWallName(Domain d);

    // 两天洞天天灵状态（第六卷开局：黑天天灵被吞噬、白天天灵受损、太阳爆裂）
    const TwoHeavensState& twoHeavens() const { return twoHeavens_; }

    // 跨域消耗：修为越高，界壁压制越强（与直觉相反，但符合设定）
    struct DomainWallCost {
        double essenceMul  = 1.0;   // 仙元消耗倍率
        double daoMarksMul = 1.0;   // 道痕损耗倍率
        bool   penetrable  = true;  // 是否可通过
        const char* note   = "";
    };
    static DomainWallCost evaluateDomainWall(Domain from, Domain to, Rank r);

    // 胎壁穿越判定：玩家永远被拒绝；仅 storyOnly 的 NPC 可短暂触碰
    static Result<void> tryCrossWombWall(WombWallPrivilege priv, bool isPlayer);

    // ---------------- 通行结算 ----------------
    // 综合：层级合法性 + 胎壁 + 白天天罡罡风 + 位面压制
    Result<TraverseCost> evaluateTraversal(const Location& from,
                                           const Location& to,
                                           Rank rank) const;

    // ---------------- 环境 DEBUFF 结算 ----------------
    // 返回该单位身处此地应当承受的状态列表（已包含本土免疫后的最终强度）
    std::vector<StatusEffect> evaluateEnvironment(const Location& loc,
                                                  Domain bornDomain) const;

    // 2.3 全域通用本土免疫规则
    //   本域出生单位：高额免疫（默认 85% 减免，几乎无感）
    //   跨域玩家/外来蛊仙/异族：全额吃环境 DEBUFF
    //   黑天、白天位面压制：天地规则压制，无本土免疫，所有凡俗生灵平等承受
    static constexpr Ratio kNativeImmunityRatio = 0.85;

    // ---------------- 三气资源采集 ----------------
    // 5.2 天气：前往黑天/天道秘境搜集；地气：占领五域地脉/福地；人气：白天/人道势力
    struct QiYield { double heaven = 0, earth = 0, human = 0; };
    QiYield collectQi(const Location& loc, double effort = 1.0) const;

    // ---------------- 开局事实校验（供单元测试使用） ----------------
    struct DistributionAudit {
        int whiteHeavenYiren = 0;   // 白天异人洞天数
        int whiteHeavenYizu  = 0;   // 白天异族洞天数
        int blackHeavenTiandao = 0; // 黑天天道秘境/洞天数
        int fangYuanOwned    = 0;   // 方源已吞并
        int tianTingOwned    = 0;   // 天庭已占领
        int fixedInstances   = 0;   // 原著固定唯一实例
        int randomized       = 0;   // 随机生成
        int illegalPlacement = 0;   // 违规落点（应为 0）
        int voidSpaceCaves   = 0;   // 落在虚空/胎壁的洞天（应为 0）
    };
    DistributionAudit audit() const;

private:
    Tick now_ = 0;
    TiangangGale    gale_;
    TwoHeavensState twoHeavens_;
    WorldClimate climate_;
    std::unordered_map<std::string, CaveParadise> caves_;
    std::vector<std::string> caveOrder_;  // 保持插入顺序，保证可复现
};

} // namespace gr
