// ============================================================================
//  二维世界地图（分块 + 外存流式）
//
//  ── 为什么改成分块 ────────────────────────────────────────────────────────
//  上一版是「整幅地图常驻内存」，尺寸只能做到 128×96，否则内存与绘制都吃紧，
//  结果细节严重不足 —— 五域只剩色块，谈不上地理。
//
//  本版把地图放大到 1024×768（约 78 万格），并改为：
//
//    · 地形：由噪声函数【纯函数现算】，不占内存、不需落盘；
//            同一 seed 永远得到同一地形，故「不存」不等于「丢失」。
//    · 探索状态：唯一需要持久化的信息，按块（64×64）序列化到外存目录。
//    · 内存：仅保留视口附近若干块，超出上限的块写回磁盘后淘汰（LRU）。
//
//  这样地图可以进一步放大而内存占用基本不变，界面也只绘制视口内的局部。
//
//  ── 地理依据 ──────────────────────────────────────────────────────────────
//  严格按《蛊真人两天五域地理研究》：
//    · 四环围一中心：中洲居中，东海在东、西漠在西、北原在北、南疆在南
//    · 五域各有一套界壁：中洲圣贤、南疆瘴气、北原甘草、西漠狂炎、东海苍水
//    · 地貌：中洲山川平原河川深渊、北原草原冰原风雪、东海群岛海域海底潜流、
//           西漠沙漠戈壁绿洲移动沙丘、南疆十万大山瘴气江河
//    · 疯魔窟为九层嵌套（北原），非普通山谷
//    · 地标须区分「五域地表 / 两天之上 / 洞天内部 / 宙道域外」四类，
//      仙窍内部坐标（荡魂山、落魄谷、逆流河）不得落在地表图上
//
//  ── 升级空间 ──────────────────────────────────────────────────────────────
//    · 生成器为接口抽象，可整体替换（河流侵蚀、山脉走向、气候带）
//    · 地图预留「地表 / 黑天 / 白天」三层，当前只填地表层
//    · 地标表独立存放，可挂载事件、任务、洞天入口
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"
#include "gr/core/Rng.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <list>

namespace gr {

// ------------------------------------------------------------------ 地形
enum class Terrain : std::uint8_t {
    Void = 0,        // 世界之外的虚空（不可达）
    Plain,           // 平原
    Mountain,        // 山川 / 十万大山
    Water,           // 江河 / 湖泊
    Sea,             // 海域
    Island,          // 海岛
    Desert,          // 沙漠戈壁
    Oasis,           // 绿洲
    Grassland,       // 草原
    IceField,        // 冰原
    ToxicForest,     // 瘴气林 / 毒泽
    Wall,            // 界壁（五域分隔）
    SkyRift,         // 天罡气墙裂缝 —— 通往两天的垂直通道
    Landmark,        // 著名地标
    // —— 依地理研究新增 ——
    Rainforest,      // 雨林（湿润山地，如疯魔窟外层、南疆谷地）
    LavaRock,        // 火岩石滩（疯魔窟第二层）
    MistCity,        // 雾都（云竹、云雾、猛兽并存之地）
    Bamboo,          // 云竹山
    Abyss,           // 地渊 / 深渊（中洲深壑）
    SandDune,        // 移动沙丘（西漠·怎渡丘）
    Undercurrent,    // 海底潜流（东海天然高速航路）
    SpiritVein,      // 灵脉 / 地脉节点
    EarthRift,       // 地沟：大地裂开的沟壑（B 级推演，可下探）
    // —— 需求「细化地形图」新增：垂直分带与水陆过渡 ——
    SnowPeak,        // 雪峰：雪线以上的高峰
    Foothill,        // 山麓：山体与平原之间的缓坡地带
    Hill,            // 丘陵：起伏和缓的低矮山地
    Forest,          // 森林：林线以下、草木丰茂
    Lake,            // 湖泊：洼地积水，与江河有别
    Wetland,         // 湿地 / 沼泽：水陆之间
    Gobi,            // 戈壁砾石：荒漠中的砾石地带
    Shoal,           // 浅滩：海陆交界的水浅处
    Coast,           // 海岸：潮间带
    // —— 依「地点总表」新增：西漠火山天坑、瀑布洞穴等 ——
    Volcano,         // 火山：西漠龙吟火山
    Sinkhole,        // 天坑：西漠羽化天坑（董家福地内）
    FlyingIsland,    // 飞岛：西漠香风飞岛
    Waterfall,       // 瀑布：南疆毒瘟瀑布（高十八丈）
    Cave,            // 洞窟：中洲玄玉蛇窟（洞窟万千）、藏龙窟
    StoneForest,     // 石林：中洲地气石林
    Spring,          // 泉：中洲灵浒泉
};

inline const char* to_string(Terrain t) {
    switch (t) {
        case Terrain::Void:        return "虚空";
        case Terrain::Plain:       return "平原";
        case Terrain::Mountain:    return "山川";
        case Terrain::Water:       return "江河";
        case Terrain::Sea:         return "海域";
        case Terrain::Island:      return "海岛";
        case Terrain::Desert:      return "沙漠";
        case Terrain::Oasis:       return "绿洲";
        case Terrain::Grassland:   return "草原";
        case Terrain::IceField:    return "冰原";
        case Terrain::ToxicForest: return "瘴气林";
        case Terrain::Wall:        return "界壁";
        case Terrain::SkyRift:     return "气墙裂缝";
        case Terrain::Landmark:    return "地标";
        case Terrain::Rainforest:  return "雨林";
        case Terrain::LavaRock:    return "火岩石滩";
        case Terrain::MistCity:    return "雾都";
        case Terrain::Bamboo:      return "云竹山";
        case Terrain::Abyss:       return "深渊";
        case Terrain::SandDune:    return "移动沙丘";
        case Terrain::Undercurrent:return "海底潜流";
        case Terrain::EarthRift:   return "地沟";
        case Terrain::SnowPeak:    return "雪峰";
        case Terrain::Foothill:    return "山麓";
        case Terrain::Hill:        return "丘陵";
        case Terrain::Forest:      return "森林";
        case Terrain::Lake:        return "湖泊";
        case Terrain::Wetland:     return "湿地";
        case Terrain::Gobi:        return "戈壁";
        case Terrain::Shoal:       return "浅滩";
        case Terrain::Coast:       return "海岸";
        case Terrain::Volcano:     return "火山";
        case Terrain::Sinkhole:    return "天坑";
        case Terrain::FlyingIsland:return "飞岛";
        case Terrain::Waterfall:   return "瀑布";
        case Terrain::Cave:        return "洞窟";
        case Terrain::StoneForest: return "石林";
        case Terrain::Spring:      return "泉";
        case Terrain::SpiritVein:  return "灵脉";
    }
    return "？";
}

// ------------------------------------------------------------------ 原著固定地形
//
//  为什么要有这一层：
//  此前地形全由噪声随机生成，原著明载的地形（落天河、罐河、十万大山、
//  怎渡丘、龙鱼海域……）只是钉在上面一个个孤立的地标点，
//  周围的「河」「山」「沙丘」却是随机长出来的 ——
//  于是玩家可能在东海岸边看到沙漠，在南疆腹地看到冰原。
//
//  原著对五域地形有明确记载，这些**必须在固定地点、以固定类型生成**，
//  不能交给噪声。故此处把原著地形登记为区域（矩形 / 带状），
//  生成时优先于噪声强制铺设。
enum class CanonShape : std::uint8_t {
    Rect = 0,   // 矩形区域
    Band,       // 带状：以线段为轴、给定半宽（适合河流、沙丘带、潜流）
};

struct CanonTerrainZone {
    std::string id;
    std::string name;
    Domain      domain = Domain::None;
    CanonShape  shape  = CanonShape::Rect;

    // —— Rect：相对坐标 0..1 ——
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;

    // —— Band：线段 (ax,ay)-(bx,by) 加半宽（相对全图宽度的比例）——
    double ax = 0, ay = 0, bx = 0, by = 0, halfWidth = 0.01;

    Terrain  terrain = Terrain::Plain;
    std::string source;          // 溯源：出自地理研究哪一节
    bool     canon = true;       // 是否原著可核验（false = 工程构造填充）

    //
    //  覆写型：由【事件】造成的地貌改变，优先于一切自然地物。
    //
    //  例：青茅山冰域 —— 白凝冰自爆把郁郁葱葱的青茅山化为冰域绝地。
    //  原本此处是山林，黄龙江又从边上流过；若按「面积最小优先」，
    //  狭长的江带会盖过冰域，青茅山就仍显示为江河 —— 与「已毁成冰域」冲突。
    //  事件改变地貌，理应盖住原本的自然地物，故设此标志。
    //
    bool     overlay = false;
};

// ------------------------------------------------------------------ 大地貌起伏
//
//  需求 10：山一类范围较大的地貌，不得铺成一块死板的同色块，
//  须按高程分出峰、坡、谷、涧，比例接近真实山地。
//
//  阈值依实测标定：对整幅 1024×768 采样 fbm 取分位，
//     p09≈0.308  p37≈0.447  p82≈0.644
//  故按 峰 18% / 坡 45% / 谷 28% / 涧 9% 的比例切分。
Terrain reliefSubTerrain(Terrain base, double elevation01);

//  某地形是否属于 base 这一地貌族的起伏亚型。
//  用于「原著固定地形」校验：山系内部出现谷地与山涧是正常的，
//  不能因为谷地是 Plain 就判定该处不是十万大山。
bool isReliefVariantOf(Terrain actual, Terrain base);

// ------------------------------------------------------------------ 地块
struct Tile {
    Terrain    terrain = Terrain::Void;
    Domain     domain  = Domain::None;
    bool       explored = false;
    std::uint8_t elevation = 0;
    std::int16_t landmarkId = -1;
    bool       canonZone = false;   // 是否落在原著固定地形区内

    bool passable() const { return terrain != Terrain::Void; }
};


// ------------------------------------------------------------------ 地标
struct Landmark {
    std::string id;
    std::string name;
    std::string desc;
    Domain      domain = Domain::None;
    int         x = 0, y = 0;
    std::string siteId;
    std::string source;          // 资料溯源
    bool        canon = true;    // 是否原著可核验

    // —— 依「两天五域地点总表」新增 ——
    //
    //  证据等级：A＝正文明确写过；B＝有伏笔或旁证推演；
    //            C＝读者推测、同人或不可靠转述。
    //  总表自身即以此分级，此处照录，不自行升降。
    //
    enum class Evidence : std::uint8_t { A = 0, B, C, Unrated };
    Evidence evidence = Evidence::Unrated;

    //  第六卷时状况（截至第368节「方源、巨阳战星宿」）
    enum class Status : std::uint8_t {
        Unknown = 0,   // 原文未明
        Intact,        // 存续
        Ruined,        // 毁坏 / 荒废
        ChangedOwner,  // 易主
        Relocated,     // 迁址
        Absorbed,      // 被方源吞并 / 搬入至尊仙窍
        Battlefield,   // 成为战场
        Declined       // 衰败 / 空置
    };
    Status status = Status::Unknown;

    std::string scaleDesc;   // 规模／占地面积（照抄原文，未给则「原文未明」）
    std::string statusDesc;  // 状况原文描述
    //  原文是否给出方位。false = 「具体方位原文未明」，坐标为工程落点。
    bool positionCanon = false;
    //  已毁：该地在第六卷时间线已不复原貌，不可作为出生点。
    //  例：青茅山 —— 第一卷白凝冰自爆北冥冰魄体，
    //  「从山顶到山脚，将原本郁郁葱葱的青茅山化为一片冰霜风雪的险恶之地」。
    bool        ruined = false;
    // 地理研究第五节：地点分四类，避免把仙窍内部误标为地表
    enum class Kind : std::uint8_t {
        Surface = 0,     // 五域地表
        TwoHeavens,      // 五域上方两天
        InsideCave,      // 洞天内部
        OuterRule        // 宙道 / 域外规则空间
    };
    Kind kind = Kind::Surface;
};
inline const char* to_string(Landmark::Evidence e) {
    switch (e) {
        case Landmark::Evidence::A:       return "A";
        case Landmark::Evidence::B:       return "B";
        case Landmark::Evidence::C:       return "C";
        case Landmark::Evidence::Unrated: return "—";
    }
    return "—";
}

inline const char* to_string(Landmark::Status s) {
    switch (s) {
        case Landmark::Status::Unknown:      return "未明";
        case Landmark::Status::Intact:       return "存续";
        case Landmark::Status::Ruined:       return "毁坏";
        case Landmark::Status::ChangedOwner: return "易主";
        case Landmark::Status::Relocated:    return "迁址";
        case Landmark::Status::Absorbed:     return "被方源吞并";
        case Landmark::Status::Battlefield:  return "战场";
        case Landmark::Status::Declined:     return "衰败/空置";
    }
    return "未明";
}




inline const char* to_string(Landmark::Kind k) {
    switch (k) {
        case Landmark::Kind::Surface:    return "五域地表";
        case Landmark::Kind::TwoHeavens: return "两天之上";
        case Landmark::Kind::InsideCave: return "洞天内部";
        case Landmark::Kind::OuterRule:  return "域外规则空间";
    }
    return "？";
}

enum class MapLayer : std::uint8_t {
    Surface = 0,
    BlackHeaven,
    WhiteHeaven,
};

struct MapGenResult {
    bool ok = false;
    std::string detail;
};

class IMapGenerator {
public:
    virtual ~IMapGenerator() = default;
    virtual const char* name() const = 0;
    virtual MapGenResult generate(class TileMap& map, Rng& rng) = 0;
};

// ------------------------------------------------------------------ 地图
class TileMap {
public:
    // 分块尺寸：块内地形现算，只有探索状态落盘
    static constexpr int kChunk = 64;
    //
    //  内存中最多保留的块数，超出则写回外存并淘汰。
    //
    //  原为固定 24 —— 在 1024×768（16×12=192 块）时够用，
    //  地图放大到 2048×1536（32×24=768 块）后严重不足：
    //
    //    实测「1/16 采样遍历」耗时 11.7 秒，其中绝大部分是
    //    【同一块被反复淘汰又重建】。行优先遍历一行需 32 块，
    //    而缓存只有 24 块 —— 单行之内就开始抖动。
    //
    //  故改为按【一行块数】自适应：至少容纳两行，且不低于 64。
    //  8 MB 上下（每块 64×64×8B = 32KB），容器内存完全承受得起。
    //
    static constexpr int kMaxResidentChunks = 256;

    TileMap() = default;

    void resize(int w, int h);
    int  width()  const { return w_; }
    int  height() const { return h_; }

    // 外存目录：探索状态按块落盘于此
    void setCacheDir(std::string dir);
    const std::string& cacheDir() const { return cacheDir_; }

    MapGenResult generate(Rng& rng, IMapGenerator* gen = nullptr);

    // 取得地块（必要时从外存载入或现算地形）
    Tile&       at(int x, int y);
    const Tile& at(int x, int y) const;
    bool inBounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < w_ && y < h_;
    }

    // ---------------- 探索 ----------------
    int  reveal(int cx, int cy, int r);
    bool isExplored(int x, int y) const;
    double exploredRatio() const;

    // ========================================================================
    //  合并扫描 —— 一次遍历完成多项统计
    //
    //  地图扩至 2048×1536（315 万格）后，逐格访问本身成了开销：
    //  此前各统计（域占比、地类分布、探索度、地标域校验）
    //  各自跑一遍全图，等于把同一片地扫了四遍。
    //
    //  这些任务【相近且互不影响】—— 都只读格子、不改动地图，
    //  故可并进同一次遍历。实测合并后由四遍降为一遍。
    //
    //  step 为采样步长：1 = 逐格（精确），4 = 每 4 格取 1（快 16 倍）。
    //  统计占比用采样即可，校验地标另走全量（因其是定点而非抽样）。
    //
    // ========================================================================
    struct MapScan {
        long long sampled = 0;              // 实际取样格数
        long long explored = 0;             // 其中已探索
        long long byDomain[8]  = {};        // 按 Domain 枚举计数
        long long byTerrain[64] = {};       // 按 Terrain 枚举计数
        int       terrainKinds = 0;         // 出现过的地类数
        int       landmarkBad  = 0;         // 域错位的地表地标数
        int       landmarkChecked = 0;      // 受检的地表地标数

        double exploredRatio() const {
            return sampled > 0 ? static_cast<double>(explored) / sampled : 0.0;
        }
        double domainRatio(int d) const {
            return sampled > 0 ? 100.0 * byDomain[d] / sampled : 0.0;
        }
    };

    //  一次遍历完成上述全部统计。doLandmarkCheck = true 时顺带校验地标。
    MapScan scanAll(int step = 4, bool doLandmarkCheck = true) const;
    void revealAll();
    // 已探明格数（缓存计数）
    long long exploredCount() const { return exploredTotal_; }

    // ---------------- 地标 ----------------
    void addLandmark(const Landmark& lm);
    const std::vector<Landmark>& landmarks() const { return landmarks_; }
    const Landmark* landmarkAt(int x, int y) const;
    const Landmark* landmarkById(const std::string& id) const;

    Domain domainAt(int x, int y) const;

    //  自然域：只按域界划分得到的域，【不含】原著地形区的覆盖。
    //  用于校验「某个原著地形区是否跨出了它所属的域」——
    //  地形区在 fillTile 里会覆盖 domain，故直接读 at() 永远自洽，
    //  看不出跨域问题。
    Domain naturalDomainAt(int x, int y) const;

    bool generated() const { return generated_; }
    const char* generatorName() const { return genName_.c_str(); }

    MapLayer layer() const { return layer_; }
    void setLayer(MapLayer l) { layer_ = l; }

    void setPlayerPos(int x, int y) { px_ = x; py_ = y; }
    int  playerX() const { return px_; }
    int  playerY() const { return py_; }

    // ---------------- 外存 ----------------
    // 把所有驻留块的探索状态写回磁盘
    void flush();
    // 统计：已落盘的块数
    std::size_t persistedChunkCount() const;
    // 扫描外存，一次性统计全部已探明格数。
    //
    //  为什么需要：分块流式下 exploredTotal_ 只在块被载入时才累加，
    //  于是重新进入游戏时探索度显示为 0，要等玩家走到那些块附近才慢慢涨回来 ——
    //  从玩家角度看就像进度丢了。故开局先扫一遍外存，把总数补上。
    void recountFromDisk();
    // 供生成器写入种子
    void setSeed(std::uint32_t s) { seed_ = s; }
    std::uint32_t seed() const { return seed_; }

    // 分区矩形（生成器与绘制共用），公开以便 UI 画域界
    struct Zone { Domain d; int x0, y0, x1, y1; };
    const std::vector<Zone>& zones() const { return zones_; }
    void setZones(std::vector<Zone> z) { zones_ = std::move(z); }


    // 原著固定地形：命中则强制铺设，优先于噪声
    const std::vector<CanonTerrainZone>& canonZones() const { return canonZones_; }
    void addCanonZone(const CanonTerrainZone& z) { canonZones_.push_back(z); }

    // 丢弃所有已构建的块，令其按需重建。
    //
    //  为什么需要：块是【懒加载】的，首次 at() 时才构建并缓存。
    //  而 addLandmark 内部会调 at() —— 若它在原著地形区登记之前触发，
    //  那些块就在「还没有原著地形数据」的状态下被固化下来，
    //  此后即便补登记也不会重算，于是部分区域缺了原著地形
    //  （表现为：落天河一段是水、另一段变草原）。
    //  故配置全部就绪后须整体失效一次，让所有块按完整配置重建。
    void invalidateChunks();
    // 该格是否落在某原著固定地形内（返回 nullptr 表示否）
    const CanonTerrainZone* canonZoneAt(int x, int y) const;

    int chunkCols() const;
    int chunkRows() const;

private:
    struct Chunk {
        std::vector<Tile> tiles;    // kChunk*kChunk
        bool dirty = false;
        bool persisted = false;
    };

    int chunkId(int cx, int cy) const { return cy * chunkCols() + cx; }

    mutable std::unordered_map<int, Chunk> resident_;
    mutable std::list<int> lru_;                 // 前面 = 最近使用
    mutable std::unordered_map<int, std::list<int>::iterator> lruPos_;

    Chunk& touchChunk(int cid) const;
    void evictIfNeeded() const;
    void buildChunk(Chunk& c, int cx, int cy) const;
    // 外存读写
    bool loadChunkFromDisk(int cid, Chunk& c) const;
    void saveChunkToDisk(int cid, const Chunk& c) const;
    std::string chunkPath(int cid) const;

    int w_ = 0, h_ = 0;
    std::vector<Landmark> landmarks_;
    //  坐标 → 地标下标。
    //  必须有这张表：块是懒加载且可被 invalidateChunks 整体失效重建的，
    //  而 buildChunk 会用 Tile{} 重置每一格。若重建后不按此表写回，
    //  landmarkId 就会被抹掉 —— 表现为地标在地图上「凭空消失」，
    //  玩家走到青茅山却查不到任何地标。
    std::unordered_map<std::int64_t, std::int16_t> landmarkAtPos_;
    std::vector<Zone> zones_;
    std::vector<CanonTerrainZone> canonZones_;
    bool generated_ = false;
    std::string genName_;
    MapLayer layer_ = MapLayer::Surface;
    int px_ = -1, py_ = -1;
    mutable long long exploredTotal_ = 0;   // touchChunk 为 const，需在载入时修正计数
    std::string cacheDir_;
    std::uint32_t seed_ = 1;
    // 已从外存计入 exploredTotal_ 的块。
    // 块被淘汰后若重新载入，不可把已有的探索格再数一遍 ——
    // 否则重载存档后探索度会被重复累加，数字虚高。
    mutable std::unordered_set<int> countedChunks_;
};

class SimpleMapGenerator : public IMapGenerator {
public:
    const char* name() const override { return "简易生成器（规则分区＋分形噪声，分块流式）"; }
    MapGenResult generate(TileMap& map, Rng& rng) override;

    // 单格地形：纯函数，供分块按需现算
    static void fillTile(Tile& t, int x, int y, int W, int H,
                         std::uint32_t seed,
                         const std::vector<TileMap::Zone>& zones,
                         const TileMap* map = nullptr);
};

} // namespace gr
