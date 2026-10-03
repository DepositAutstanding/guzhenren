// ============================================================================
//  聚落与建筑（Settlement）—— 人类群居地里有什么
//
//  设计依据（可核验者已注明出处）：
//
//  【南疆·古月山寨】（原著第一章）
//    「从山腰至山脚，闪着许多莹莹的微光……这些光来源于一座座
//      高脚吊楼，虽称不上万家灯火，却也有数千的规模。
//      古月山寨的最中央，是一座大气辉煌的楼阁。」
//    → 高脚吊楼（民居，依山而建）＋ 中央楼阁（后世称「五层家主阁」，
//      常年重兵把守）
//
//  【宗祖祠堂】（原著第 1—2 集，开窍大典）
//    「面对着高高的漆黑台案，这台案有三层，供奉着先祖的牌位，
//      牌位两侧摆着赤铜香炉，香烟袅袅……古月族长最后一个走出宗祖祠堂。」
//    → 宗祖祠堂：开窍大典前的祭祀与开窍之所
//
//  【商家城】（原著第 408 集「商家内城」）
//    「整个商家城就是一座立体的大山」—— 覆盖商量山山体，分外城与内城。
//    第三内城「所有的建筑石料都采用了星星石……在黑暗中能散发出璀璨的
//      星光，甚至连街道上都铺着星星石料所制的石板」。
//    内城照明「火炭石……堆放在墙壁上开凿出的石龛里，没有烟雾产生」。
//    「街道上每隔五百步左右，就会矗立着一根需要数人合抱的巨型圆柱，
//      圆柱表面塑造了螺旋上升的石梯」。
//    入内城须缴元石（第三内城 600 块），守卫蛊师头领为三转。
//    买卖蛊虫的「通幽商铺」：牌匾、雅室、檀木桌椅、雕梁画栋、庭院。
//
//  【中洲·监天塔】天庭九转核心建筑，第五卷末宿命蛊被摧毁后
//    裂纹扩散并崩碎，碎片与蛊仙尸躯坠落（地理研究 A 级）。
//    故第六卷开局时监天塔已不存在 —— 界面上须如实标注，不可仍画为完好。
//
//  原著未系统记载各聚落的完整建筑名录，故除上述可核验者外，
//  其余建筑形制为【工程设定】，一律标 `canon=false`，不冒充原著。
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"

#include <string>
#include <vector>

namespace gr {

// ---------------------------------------------------------------- 建筑类型
enum class BuildingType : std::uint8_t {
    // —— 居住 ——
    StiltHouse   = 0,   // 高脚吊楼：南疆依山民居【原著】
    Tent,               // 帐篷：北原游牧式居所【工程设定】
    EarthFort,          // 土堡：西漠夯土堡垒【工程设定】
    Mansion,            // 府邸：族长 / 家老居所

    // —— 权力 ——
    LordPavilion,       // 家主阁：山寨中央主楼【原著，五层】
    AncestralHall,      // 宗祖祠堂：祭祀与开窍之所【原著】
    CityGate,           // 城门关卡：入城须缴元石【原著】
    Palace,             // 宫殿：王庭 / 宗门中枢

    // —— 交易 ——
    Shop,               // 商铺：买卖蛊虫【原著：通幽商铺】
    Market,             // 坊市：集市摊位
    Auction,            // 拍卖场
    Inn,                // 客栈 / 酒楼：休整

    // —— 修行 ——
    RefineHall,         // 炼蛊房
    Scripture,          // 藏经阁：查蛊方
    School,             // 学堂：学炼蛊手法
    Arena,              // 演武场
    Alchemy,            // 丹房

    // —— 生产 ——
    GuPen,              // 养蛊房
    HerbGarden,         // 药园
    Mine,               // 矿场
    BeastPen,           // 兽栏
    Dock,               // 码头 / 船坞

    // —— 特殊 ——
    Watchtower,         // 塔楼 / 箭塔
    Prison,             // 牢房
    Post,               // 驿站
    Starstone,          // 星星石建筑：内城发光的石构【原著】
};

inline const char* to_string(BuildingType t) {
    switch (t) {
        case BuildingType::StiltHouse:   return "高脚吊楼";
        case BuildingType::Tent:         return "帐篷";
        case BuildingType::EarthFort:    return "土堡";
        case BuildingType::Mansion:      return "府邸";
        case BuildingType::LordPavilion: return "家主阁";
        case BuildingType::AncestralHall:return "宗祖祠堂";
        case BuildingType::CityGate:     return "城门关卡";
        case BuildingType::Palace:       return "宫殿";
        case BuildingType::Shop:         return "商铺";
        case BuildingType::Market:       return "坊市";
        case BuildingType::Auction:      return "拍卖场";
        case BuildingType::Inn:          return "客栈";
        case BuildingType::RefineHall:   return "炼蛊房";
        case BuildingType::Scripture:    return "藏经阁";
        case BuildingType::School:       return "学堂";
        case BuildingType::Arena:        return "演武场";
        case BuildingType::Alchemy:      return "丹房";
        case BuildingType::GuPen:        return "养蛊房";
        case BuildingType::HerbGarden:   return "药园";
        case BuildingType::Mine:         return "矿场";
        case BuildingType::BeastPen:     return "兽栏";
        case BuildingType::Dock:         return "码头";
        case BuildingType::Watchtower:   return "塔楼";
        case BuildingType::Prison:       return "牢房";
        case BuildingType::Post:         return "驿站";
        case BuildingType::Starstone:    return "星星石建筑";
    }
    return "？";
}

// ---------------------------------------------------------------- 建筑用途
//  进入建筑后能做什么 —— 界面按钮、命令校验都据此判定
enum class BuildingUse : std::uint8_t {
    None     = 0,   // 仅可参观
    Trade,          // 买卖蛊虫与材料
    Learn,          // 学炼蛊手法
    Recipe,         // 查阅 / 求得蛊方
    Refine,         // 开炉炼蛊
    Rest,           // 休整（恢复气血）
    Practice,       // 演武（练习战斗）
    Awaken,         // 开窍（凡人开窍大典）
    Gather,         // 采集产出（矿场 / 药园 / 兽栏）
    Passage,        // 通行（城门关卡，须缴元石）
};

inline const char* to_string(BuildingUse u) {
    switch (u) {
        case BuildingUse::None:     return "参观";
        case BuildingUse::Trade:    return "交易";
        case BuildingUse::Learn:    return "求学";
        case BuildingUse::Recipe:   return "查方";
        case BuildingUse::Refine:   return "炼蛊";
        case BuildingUse::Rest:     return "休整";
        case BuildingUse::Practice: return "演武";
        case BuildingUse::Awaken:   return "开窍";
        case BuildingUse::Gather:   return "产出";
        case BuildingUse::Passage:  return "通行";
    }
    return "？";
}

// ---------------------------------------------------------------- 建筑
struct Building {
    std::string  id;
    std::string  name;
    BuildingType type = BuildingType::Mansion;
    BuildingUse  use  = BuildingUse::None;

    std::string  desc;         // 形制与用途说明
    std::string  source;       // 资料溯源
    bool         canon = false; // 是否原著可核验

    // 通行类：入城所需元石（原著：商家城第三内城 600 块）
    double       toll = 0.0;

    // 求学类：可在此习得的手法 id（0 = 无）
    int          teachesTechnique = 0;

    // 开窍类：仅凡人可用
    bool         mortalOnly = false;
};

// ---------------------------------------------------------------- 聚落
//  人类群居地。绑到一个地表地标上。
struct Settlement {
    std::string id;
    std::string name;
    std::string landmarkId;     // 关联地标 id
    Domain      domain = Domain::None;

    //  规模：影响建筑数量与形制
    enum class Scale : std::uint8_t {
        Village = 0,   // 村寨
        Town,          // 集镇
        City,          // 城池
        Sect,          // 宗门
        Capital        // 王庭 / 都城
    };
    Scale scale = Scale::Village;

    std::string desc;
    std::string source;
    bool        canon = false;

    //  已毁：该聚落在第六卷时间线已成废墟（如青茅山古月山寨）。
    //  建筑形制仍可查阅（原著有载），但不可作为出生地、
    //  也不该被当作活着的村寨。
    bool        ruined = false;

    std::vector<Building> buildings;
};

inline const char* to_string(Settlement::Scale s) {
    switch (s) {
        case Settlement::Scale::Village: return "村寨";
        case Settlement::Scale::Town:    return "集镇";
        case Settlement::Scale::City:    return "城池";
        case Settlement::Scale::Sect:    return "宗门";
        case Settlement::Scale::Capital: return "王庭";
    }
    return "？";
}

// ---------------------------------------------------------------- 登记与查询
class SettlementRegistry {
public:
    SettlementRegistry();

    const std::vector<Settlement>& all() const { return all_; }

    //  按地标 id 查聚落（玩家走到该地标时用）
    const Settlement* byLandmark(const std::string& landmarkId) const;

    //  按聚落 id 查
    const Settlement* byId(const std::string& id) const;

    //  玩家当前所在地标上的聚落（无则 nullptr）
    const Settlement* current(const std::string& landmarkId) const {
        return byLandmark(landmarkId);
    }

    std::size_t count() const { return all_.size(); }
    std::size_t buildingCount() const;

private:
    std::vector<Settlement> all_;
};

//  依建筑类型的默认用途（登记时不必逐条写）
BuildingUse defaultUseOf(BuildingType t);

} // namespace gr
