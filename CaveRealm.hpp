// ============================================================================
//  洞天 / 福地内部地图（秘境）
//
//  为什么要单独做：洞天不是地表上的一个点，而是一方独立小世界。
//  此前「进入洞天」只是把玩家位置改成某个 siteId，内部全无内容 ——
//  既然原著里洞天自有山川、灵脉、荒兽、仙材，进来就该是另一张地图。
//
//  设计要点：
//    · 每个洞天按需生成自己的小地图，尺寸依层级而定
//      （福地小而精，洞天大而全，九层结构如疯魔窟则逐层生成）
//    · 地形风格随洞天种类走：
//        天道秘境 → 混沌、规则型地貌
//        异人洞天 → 聚落、田园
//        原著福地 → 依原著描述（琅琊福地有荡魂山、落魄谷）
//    · 复用 TileMap 的分块流式机制，只是规模小得多，故全图常驻即可
//    · 出口：回到进入前的位置
//
//  与原著的关系：
//    原著对多数洞天内部的【精确布局】并未给出，只有少量定点描述
//    （如疯魔窟九层、琅琊福地的荡魂山落魄谷、至尊仙窍的小东海）。
//    因此本模块生成的是**结构示意**，不是考据地图：
//       · 原著明确提及的地点 → 定点放置，canon=true
//       · 其余地形 → 依洞天种类推导填充，属工程构造
//    这点必须在界面上说明，不可冒充原著地图。
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"
#include "gr/core/Rng.hpp"
#include "gr/world/TileMap.hpp"

#include <string>
#include <vector>
#include <unordered_map>

namespace gr {

// 秘境地形：洞天内部专用，与五域地表地形不同
enum class CaveTerrain : std::uint8_t {
    Void = 0,        // 洞天边界之外（不可达）
    Floor,           // 普通地面
    SpiritField,     // 灵田 / 药圃
    SpiritSpring,    // 灵泉 / 元泉
    Mountain,        // 洞天内山岳
    Water,           // 河湖
    Forest,          // 林地
    Settlement,      // 聚落 / 居所
    MineVein,        // 矿脉
    BeastNest,       // 荒兽巢穴
    ChaoticRift,     // 混沌裂隙（天道秘境型）
    Ruins,           // 遗迹
    CaveEntrance,    // 入口
    CaveExit,        // 出口（返回外界）
    Landmark,        // 洞天内著名地点
    // —— 原著定点专用 ——
    LavaRock,        // 火岩石滩（疯魔窟二层）
    MistCity,        // 雾都（疯魔窟三层云竹猛兽雾都）
    Bamboo,          // 云竹（疯魔窟三层）
    Rainforest,      // 雨林（疯魔窟一层外部）
    YuanJing,        // 元境（疯魔窟九层）
    BookMountain,    // 书山（疯魔窟九层）
    // —— 大地裂缝专用（B 级推演：宿命蛊与监天塔崩溃后地脉翻涌）——
    //  裂缝不是普通洞天：它是【地表被撕开的口子】，内里是断口岩层、
    //  翻涌的地脉、坠落中的洞天碎片，深处与地脉相连、深不见底。
    CrackEdge,       // 裂口边缘：沿裂缝走向的断口岩层，是唯一稳定的落脚处
    CrackWall,       // 断壁：裂缝两壁的裸露岩层，不可通行
    FallenDebris,    // 坠落碎片：可踏足但危险
    EarthVeinRift,   // 翻涌地脉：暴露在外的地气脉络
    DarkDepths,      // 深不见底：向下延伸的黑暗，望不见底
    // —— 地沟专用（原著实有）——
    BlackOil,        // 黑油河：地沟盛产黑油，食道仙材，交错纠缠如巨蟒
};

inline const char* to_string(CaveTerrain t) {
    switch (t) {
        case CaveTerrain::Void:        return "虚空";
        case CaveTerrain::Floor:       return "地面";
        case CaveTerrain::SpiritField: return "灵田";
        case CaveTerrain::SpiritSpring:return "灵泉";
        case CaveTerrain::Mountain:    return "山岳";
        case CaveTerrain::Water:       return "水域";
        case CaveTerrain::Forest:      return "林地";
        case CaveTerrain::Settlement:  return "聚落";
        case CaveTerrain::MineVein:    return "矿脉";
        case CaveTerrain::BeastNest:   return "兽巢";
        case CaveTerrain::ChaoticRift: return "混沌裂隙";
        case CaveTerrain::Ruins:       return "遗迹";
        case CaveTerrain::CaveEntrance:return "入口";
        case CaveTerrain::CaveExit:    return "出口";
        case CaveTerrain::Landmark:    return "地标";
        case CaveTerrain::LavaRock:    return "火岩石滩";
        case CaveTerrain::MistCity:    return "雾都";
        case CaveTerrain::Bamboo:      return "云竹";
        case CaveTerrain::Rainforest:  return "雨林";
        case CaveTerrain::YuanJing:    return "元境";
        case CaveTerrain::BookMountain:return "书山";
        case CaveTerrain::CrackEdge:   return "裂口边缘";
        case CaveTerrain::CrackWall:   return "断壁";
        case CaveTerrain::FallenDebris:return "坠落碎片";
        case CaveTerrain::EarthVeinRift:return "翻涌地脉";
        case CaveTerrain::DarkDepths:  return "深不见底";
        case CaveTerrain::BlackOil:    return "黑油河";
    }
    return "？";
}

struct CaveTile {
    CaveTerrain terrain = CaveTerrain::Void;
    bool        explored = false;
    std::int16_t landmarkId = -1;
    bool passable() const { return terrain != CaveTerrain::Void && terrain != CaveTerrain::CrackWall; }
};

// 洞天内部地点
struct CaveSite {
    std::string id;
    std::string name;
    std::string desc;
    int  x = 0, y = 0;
    bool canon = true;    // 是否原著可核验地点
    std::string source;
};

// 一层秘境地图
struct CaveFloorMap {
    int level = 1;              // 层数（非多层结构恒为 1）
    std::string name;           // 层名，如「第二层·火岩石滩」
    int w = 0, h = 0;
    std::vector<CaveTile> tiles;
    std::vector<CaveSite> sites;
    int entryX = 0, entryY = 0; // 入口
    int exitX = 0, exitY = 0;   // 出口（返回外界）

    CaveTile&       at(int x, int y);
    const CaveTile& at(int x, int y) const;
    bool inBounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < w && y < h;
    }
    int reveal(int cx, int cy, int r);
    double exploredRatio() const;
    void revealAll();
};

// 一整个洞天的秘境（可能多层）
class CaveRealm {
public:
    CaveRealm() = default;
    CaveRealm(const std::string& siteId, const std::string& name, int levels = 1);

    const std::string& siteId() const { return siteId_; }
    const std::string& name()   const { return name_; }
    int levelCount() const { return static_cast<int>(floors_.size()); }

    CaveFloorMap&       floor(int lv);
    const CaveFloorMap& floor(int lv) const;
    std::vector<CaveFloorMap>& floors() { return floors_; }

    bool generated() const { return generated_; }
    void markGenerated() { generated_ = true; }

    // 玩家在秘境中的位置
    void setPos(int lv, int x, int y) { lv_ = lv; px_ = x; py_ = y; }
    int  level() const { return lv_; }
    int  x() const { return px_; }
    int  y() const { return py_; }

private:
    std::string siteId_, name_;
    std::vector<CaveFloorMap> floors_;
    bool generated_ = false;
    int lv_ = 0, px_ = 0, py_ = 0;
};

// ---------------------------------------------------------------------------
//  秘境生成器
// ---------------------------------------------------------------------------
class CaveRealmGenerator {
public:
    struct Result { bool ok = false; std::string detail; };

    // 依洞天种类生成内部地图
    Result generate(CaveRealm& realm, const class CaveParadise& cave, Rng& rng);

private:
    Result genSingleFloor(CaveFloorMap& f, const CaveParadise& cave,
                          int level, int levels, Rng& rng);
    // 原著定点：依 siteId 放置原著明确提及的内部地点
    void placeCanonSites(CaveFloorMap& f, const CaveParadise& cave, int level);
};

} // namespace gr
