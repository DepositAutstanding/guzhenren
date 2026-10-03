// ============================================================================
//  两天（黑天／白天）各自的地图
//
//  原著：
//
//    九天位于五域上空，有赤橙黄绿青蓝紫黑白九重，九天相互连接，
//    并一直旋转，形成圆柱形的九色天柱。太古时期人祖十子击毁其七，
//    只留下黑白二天。
//
//    如今黑白两天间的气墙消融，新太阳也被发狂的幽魂魔尊毁灭，
//    黑白两天合为【幽天】—— 再无昼夜之分。（第六卷卷初）
//
//  进入方式：五域地表有【天罡气墙裂缝】（Terrain::SkyRift），
//  自裂缝可上达两天。这与洞天不同 —— 洞天是「进去」，
//  两天是「上去」。
//
//  为何要单独成图：
//    此前两天只作为 RealmLayer 的一个枚举值登记，没有地图。
//    玩家在地表看到「气墙裂缝」，点进去却无内容 ——
//    与「洞天进入即秘境」的处理不一致。
//
//  地形集取原著所载的两天特征：云海、罡风、天柱残迹、
//  太古太阳碎片、幽天暗域、气墙裂隙。
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"
#include "gr/world/CaveRealm.hpp"

#include <string>
#include <vector>

namespace gr {

enum class HeavenTerrain : std::uint8_t {
    Void = 0,        // 天外虚空（不可达）
    CloudSea,        // 云海：两天中最常见的可落脚处
    WindBlast,       // 罡风带：天罡风，凡人触及即伤
    PillarRemnant,   // 天柱残迹：九色天柱被击毁七重后的遗迹
    SunFragment,     // 太古太阳碎片：狂蛮魔尊打碎太阳后的残骸
    GloamVoid,       // 幽天暗域：黑白两天合流后的昏暗之处
    QiWallRift,      // 气墙裂隙：通往五域的垂直通道
    RuinSite,        // 遗迹：太古遗存
    SkySpring,       // 天泉：元气凝聚处
    Nest,            // 巢穴：栖于天上的荒兽之巢
};

inline const char* to_string(HeavenTerrain t) {
    switch (t) {
        case HeavenTerrain::Void:         return "天外虚空";
        case HeavenTerrain::CloudSea:     return "云海";
        case HeavenTerrain::WindBlast:    return "罡风";
        case HeavenTerrain::PillarRemnant:return "天柱残迹";
        case HeavenTerrain::SunFragment:  return "太阳碎片";
        case HeavenTerrain::GloamVoid:    return "幽天暗域";
        case HeavenTerrain::QiWallRift:   return "气墙裂隙";
        case HeavenTerrain::RuinSite:     return "太古遗迹";
        case HeavenTerrain::SkySpring:     return "天泉";
        case HeavenTerrain::Nest:         return "天兽巢穴";
    }
    return "？";
}

inline bool is_passable(HeavenTerrain t) {
    return t != HeavenTerrain::Void && t != HeavenTerrain::WindBlast;
}

//  两天地图的一格
struct HeavenTile {
    HeavenTerrain terrain = HeavenTerrain::Void;
    bool          explored = false;
    std::uint8_t  siteId = 0;      // 定点（0 = 无）
};

//  两天地图
struct HeavenMap {
    std::string which;             // "黑天" / "白天" / "幽天"
    int w = 0, h = 0;
    std::vector<HeavenTile> tiles;
    std::vector<CaveSite>  sites;
    int entryX = 0, entryY = 0;    // 自地表裂缝上来的落点

    HeavenTile&       at(int x, int y);
    const HeavenTile& at(int x, int y) const;
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
    int  reveal(int cx, int cy, int r);
    double exploredRatio() const;
    void revealAll();
};

//  生成某一天的地图（确定性：同种同种子必同图）
void generate_heaven_map(HeavenMap& m, const std::string& which, Rng& rng);

//  黑天 / 白天 / 幽天（第六卷后两天合一）
inline const char* heaven_names(int i) {
    switch (i) {
        case 0: return "黑天";
        case 1: return "白天";
        case 2: return "幽天";
    }
    return "？";
}

//  当前时间线上应显示哪一张（第六卷卷初两天已合为幽天）
inline std::string current_heaven_name() { return "幽天"; }

} // namespace gr
