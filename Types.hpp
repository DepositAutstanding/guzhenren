// ============================================================================
//  《蛊真人》开放世界单机（预留联机）—— 全局基础类型
//  依据《最终完整版需求说明书》第一章「全局基础设定」实现：
//    · 时间基线固定第六卷开局（疯魔窟大战结束后），前五卷只作历史资料
//    · 世界层级：五域凡界(地表/地下) + 黑天 + 白天 + 世界胎壁
//    · 彻底删除「界外空域」，所有空间收纳于两天五域之内
// ============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace gr {

// ---------------------------------------------------------------- 基本量纲
using Tick     = std::int64_t;   // 世界时间刻度
using Quantity = double;         // 气、仙元、道痕等连续量
using Ratio    = double;         // 0.0 ~ 1.0 的比例/概率

// ---------------------------------------------------------------- 世界层级
enum class RealmLayer : std::uint8_t {
    MortalSurface     = 0,  // 五域凡界·地表
    MortalUnderground = 1,  // 五域凡界·地下
    BlackHeaven       = 2,  // 黑天位面（悬浮覆盖五域上空）
    WhiteHeaven       = 3,  // 白天位面（悬浮覆盖五域上空）
    WorldWombWall     = 4   // 世界胎壁：世界绝对边界，玩家不可突破
};

inline const char* to_string(RealmLayer l) {
    switch (l) {
        case RealmLayer::MortalSurface:     return "五域凡界·地表";
        case RealmLayer::MortalUnderground: return "五域凡界·地下";
        case RealmLayer::BlackHeaven:       return "黑天";
        case RealmLayer::WhiteHeaven:       return "白天";
        case RealmLayer::WorldWombWall:     return "世界胎壁";
    }
    return "未知层级";
}

// 是否属于「两天」（黑天/白天）——两天压制无本土免疫，凡俗生灵平等承受
inline bool isHeavenLayer(RealmLayer l) {
    return l == RealmLayer::BlackHeaven || l == RealmLayer::WhiteHeaven;
}

// ---------------------------------------------------------------- 五域
enum class Domain : std::uint8_t {
    None      = 0,
    ZhongZhou = 1,  // 中洲
    BeiYuan   = 2,  // 北原
    NanJiang  = 3,  // 南疆
    DongHai   = 4,  // 东海
    XiMo      = 5   // 西漠
};

inline const char* to_string(Domain d) {
    switch (d) {
        case Domain::ZhongZhou: return "中洲";
        case Domain::BeiYuan:   return "北原";
        case Domain::NanJiang:  return "南疆";
        case Domain::DongHai:   return "东海";
        case Domain::XiMo:      return "西漠";
        case Domain::None:      return "无域属";
    }
    return "未知域";
}

// ---------------------------------------------------------------- 修为转数
// 一至五转为凡蛊阶段，六转及以上为仙蛊阶段（仙凡鸿沟）
//
//  R0 = 凡夫：未开窍者。
//
//  原著里开窍是件门槛极高的事 —— 需开窍大典、开窍蛊，
//  且资质分甲乙丙丁，无资质者终身为凡人。
//  五域之中，寻常村镇里绝大多数人终其一生都是凡夫：
//  农户、工匠、商贾、猎户未必有修为，更未必有蛊。
//
//  此前所有凡俗 NPC 一律赋一~三转修为，等于说「世间无人不是蛊师」，
//  与原著相悖。故新增 R0 一档：无空窍、无真元、无蛊虫，只有职业。
//
enum class Rank : int {
    R0 = 0,   // 凡夫（未开窍）
    R1 = 1, R2 = 2, R3 = 3, R4 = 4, R5 = 5,
    R6 = 6, R7 = 7, R8 = 8, R9 = 9
};

inline int  rank_value(Rank r)        { return static_cast<int>(r); }
inline bool is_immortal_rank(Rank r)  { return rank_value(r) >= 6; }
inline const char* to_string(Rank r) {
    switch (r) {
        case Rank::R0: return "凡夫";
        case Rank::R1: return "一转"; case Rank::R2: return "二转";
        case Rank::R3: return "三转"; case Rank::R4: return "四转";
        case Rank::R5: return "五转"; case Rank::R6: return "六转";
        case Rank::R7: return "七转"; case Rank::R8: return "八转";
        case Rank::R9: return "九转";
    }
    return "？转";
}

// ---------------------------------------------------------------- 大道流派
// 5.1「道境体系」：每一道分为 入门→小成→大成→圆满→道基→道痕贯通
enum class Dao : std::uint8_t {
    Refine = 0,   // 炼道
    Wisdom,       // 智道
    Qi,           // 气道
    Luck,         // 运道
    Soul,         // 魂道
    Human,        // 人道
    Heaven,       // 天道
    Thunder,      // 雷道
    Sword,        // 剑道
    Strength,     // 力道
    Formation,    // 阵道
    Poison,       // 毒道
    Transform,    // 变化道
    Rule,         // 律道
    Water,        // 水道
    Fire,         // 火道
    Earth,        // 土道
    Wood,         // 木道
    Metal,        // 金道
    Wind,         // 风道
    Light,        // 光道
    Dark,         // 暗道
    Star,         // 星道
    Space,        // 宇道
    Time,         // 宙道（光阴）
    Blade,        // 兵道
    Blood,        // 血道
    Bone,         // 骨道
    Enslave,      // 奴道
    Thief,        // 偷道
    Food,         // 食道
    Cloud,        // 云道
    Dream,        // 梦道
    Sound,        // 音道
    Painting,     // 画道
    Shadow,       // 影道
    Ice,          // 冰道
    Jade,         // 玉道
    Count
};

inline const char* to_string(Dao d) {
    switch (d) {
        case Dao::Refine: return "炼道";   case Dao::Wisdom: return "智道";
        case Dao::Qi: return "气道";       case Dao::Luck: return "运道";
        case Dao::Soul: return "魂道";     case Dao::Human: return "人道";
        case Dao::Heaven: return "天道";   case Dao::Thunder: return "雷道";
        case Dao::Sword: return "剑道";    case Dao::Strength: return "力道";
        case Dao::Formation: return "阵道"; case Dao::Poison: return "毒道";
        case Dao::Transform: return "变化道"; case Dao::Rule: return "律道";
        case Dao::Water: return "水道";    case Dao::Fire: return "火道";
        case Dao::Earth: return "土道";    case Dao::Wood: return "木道";
        case Dao::Metal: return "金道";    case Dao::Wind: return "风道";
        case Dao::Light: return "光道";    case Dao::Dark: return "暗道";
        case Dao::Star: return "星道";     case Dao::Space: return "宇道";
        case Dao::Time: return "宙道";     case Dao::Blade: return "兵道";
        case Dao::Blood: return "血道";    case Dao::Bone: return "骨道";
        case Dao::Enslave: return "奴道";  case Dao::Thief: return "偷道";
        case Dao::Food: return "食道";     case Dao::Cloud: return "云道";
        case Dao::Dream: return "梦道";    case Dao::Sound: return "音道";
        case Dao::Painting: return "画道"; case Dao::Shadow: return "影道";
        case Dao::Ice: return "冰道";      case Dao::Jade: return "玉道";
        case Dao::Count: break;
    }
    return "未知道";
}

// ---------------------------------------------------------------- 道境等级
enum class DaoLevel : std::uint8_t {
    Entry       = 0,  // 入门
    Small       = 1,  // 小成
    Great       = 2,  // 大成
    Perfection  = 3,  // 圆满
    Foundation  = 4,  // 道基
    MarkFusion  = 5   // 道痕贯通
};

inline const char* to_string(DaoLevel l) {
    switch (l) {
        case DaoLevel::Entry:      return "入门";
        case DaoLevel::Small:      return "小成";
        case DaoLevel::Great:      return "大成";
        case DaoLevel::Perfection: return "圆满";
        case DaoLevel::Foundation: return "道基";
        case DaoLevel::MarkFusion: return "道痕贯通";
    }
    return "未入道";
}

// ------------------------------------------------- 流派境界（数值口径库：10 级）
//  与「道境六阶」是两套体系，不可混用：
//    · 道境 DaoLevel   —— 绑定战力（威力增幅、消耗降低、反噬降低、杀招稳定）
//    · 流派境界 FlowLevel —— 绑定成尊条件、吞窍资格、道主身份
//  原作直接锚点为宗师、大宗师、无上大宗师、道主；「准X」阶见于常见整理。
enum class FlowLevel : std::uint8_t {
    Ordinary        = 0,  // 普通
    QuasiMaster     = 1,  // 准大师
    Master          = 2,  // 大师
    QuasiGrandmaster= 3,  // 准宗师
    Grandmaster     = 4,  // 宗师
    QuasiGreat      = 5,  // 准大宗师
    Great           = 6,  // 大宗师
    QuasiSupreme    = 7,  // 准无上大宗师
    Supreme         = 8,  // 无上大宗师
    DaoLord         = 9   // 道主
};

constexpr int kFlowLevelCount = 10;

inline const char* to_string(FlowLevel f) {
    switch (f) {
        case FlowLevel::Ordinary:         return "普通";
        case FlowLevel::QuasiMaster:      return "准大师";
        case FlowLevel::Master:           return "大师";
        case FlowLevel::QuasiGrandmaster: return "准宗师";
        case FlowLevel::Grandmaster:      return "宗师";
        case FlowLevel::QuasiGreat:       return "准大宗师";
        case FlowLevel::Great:            return "大宗师";
        case FlowLevel::QuasiSupreme:     return "准无上大宗师";
        case FlowLevel::Supreme:          return "无上大宗师";
        case FlowLevel::DaoLord:          return "道主";
    }
    return "？";
}

// ---------------------------------------------------------------- 蛊虫品阶
enum class GuTier : std::uint8_t {
    Mortal   = 0,  // 凡蛊：一至五转，同种可大量存在，能力单一
    Immortal = 1   // 仙蛊：六转及以上，同名仙蛊世间唯一
};

inline GuTier tier_of(Rank r) {
    return is_immortal_rank(r) ? GuTier::Immortal : GuTier::Mortal;
}

// ---------------------------------------------------------------- 蛊虫类别
enum class GuCategory : std::uint8_t {
    Attack, Defense, Movement, Reconnaissance, Support, Healing,
    Consumable, Storage, Communication, BeastTaming, Material, Logistics,
    House /* 仙蛊屋 */
};

inline const char* to_string(GuCategory c) {
    switch (c) {
        case GuCategory::Attack:         return "攻击";
        case GuCategory::Defense:        return "防御";
        case GuCategory::Movement:       return "移动";
        case GuCategory::Reconnaissance: return "侦察";
        case GuCategory::Support:        return "辅助";
        case GuCategory::Healing:        return "治疗";
        case GuCategory::Consumable:     return "消耗";
        case GuCategory::Storage:        return "存储";
        case GuCategory::Communication:  return "传信";
        case GuCategory::BeastTaming:    return "奴兽";
        case GuCategory::Material:       return "材料";
        case GuCategory::Logistics:      return "后勤";
        case GuCategory::House:          return "屋蛊";
    }
    return "其他";
}

// ---------------------------------------------------------------- 世界坐标
// 定仙游只认「坐标」，而坐标必须曾被亲眼见过 / 抵达过 / 感知过
struct Location {
    RealmLayer layer  = RealmLayer::MortalSurface;
    Domain     domain = Domain::None;
    std::string region;   // 具体地域标签，如「万兽山」「瘴气谷」「天庭」
    std::string siteId;   // 洞天/福地/秘境实例标识；空串表示野外
    bool isParadiseInterior = false;  // 是否处于洞天/福地/仙窍内部

    std::string key() const {
        return std::to_string(static_cast<int>(layer)) + "/" +
               std::to_string(static_cast<int>(domain)) + "/" +
               region + "/" + siteId;
    }
    bool operator==(const Location& o) const {
        return layer == o.layer && domain == o.domain &&
               region == o.region && siteId == o.siteId &&
               isParadiseInterior == o.isParadiseInterior;
    }
    bool operator!=(const Location& o) const { return !(*this == o); }
};

struct LocationHash {
    std::size_t operator()(const Location& l) const {
        return std::hash<std::string>{}(l.key());
    }
};

// ---------------------------------------------------------------- 通用结果
enum class Err : std::uint8_t {
    Ok = 0,
    // 世界类
    WombWallImpassable,      // 世界胎壁不可突破
    VoidSpaceRemoved,        // 界外空域已被彻底删除，非法层级
    StormFatal,             // 罡风暴风灾期，八转以下撕裂重创
    // 行动类
    LockedBySeclusion,       // 三气闭关期间不可移动/不可战斗
    NotInOwnParadise,        // 未处于自身仙窍/福地内部
    RankTooLow,
    InsufficientEssence,
    // 定仙游类
    CoordinateUnknown,       // 未探索/未抵达/未亲眼看见的坐标
    DingXianYouNotHeld,
    // 炼蛊类
    RecipeMissing,           // 无蛊方 → 绝对无法炼制
    RecipeIncomplete,        // 残缺蛊方
    MaterialMissing,
    DaoLevelTooLow,
    UniqueGuViolation,       // 仙蛊唯一冲突
    // 战斗类
    ImmortalMortalGap,       // 仙凡鸿沟
    MoveUnstable
};

inline const char* to_string(Err e) {
    switch (e) {
        case Err::Ok:                  return "成功";
        case Err::WombWallImpassable:  return "世界胎壁为绝对屏障，不可突破";
        case Err::VoidSpaceRemoved:    return "界外空域设定已删除，该层级非法";
        case Err::StormFatal:          return "暴风灾期：罡风狂暴，八转以下被撕裂重创";
        case Err::LockedBySeclusion:   return "定点闭关调和三气中：无法移动、无法战斗、无法外出";
        case Err::NotInOwnParadise:    return "三气平衡必须居于自身仙窍/自己掌控的福地洞天内部";
        case Err::RankTooLow:          return "修为不足";
        case Err::InsufficientEssence: return "仙元不足";
        case Err::CoordinateUnknown:   return "定仙游仅可前往亲眼见过/抵达过/感知过的坐标";
        case Err::DingXianYouNotHeld:  return "未持有定仙游";
        case Err::RecipeMissing:       return "无对应蛊方，绝对无法炼制";
        case Err::RecipeIncomplete:    return "蛊方残缺，只能炼出残次品/失败品";
        case Err::MaterialMissing:     return "材料不齐";
        case Err::DaoLevelTooLow:      return "道境不足";
        case Err::UniqueGuViolation:   return "仙蛊唯一：同名仙蛊世间已存在";
        case Err::ImmortalMortalGap:   return "仙凡鸿沟：凡俗手段无法撼动";
        case Err::MoveUnstable:        return "杀招崩解";
    }
    return "未知错误";
}

// 轻量结果包装，避免引入异常
template <typename T>
struct Result {
    Err  err = Err::Ok;
    T    value{};
    std::string detail;

    bool ok()   const { return err == Err::Ok; }
    explicit operator bool() const { return ok(); }

    static Result success(T v, std::string d = {}) {
        Result r; r.err = Err::Ok; r.value = std::move(v); r.detail = std::move(d); return r;
    }
    static Result fail(Err e, std::string d = {}) {
        Result r; r.err = e; r.detail = std::move(d); return r;
    }
};

template <>
struct Result<void> {
    Err         err = Err::Ok;
    std::string detail;
    bool ok() const { return err == Err::Ok; }
    explicit operator bool() const { return ok(); }
    static Result success(std::string d = {}) { Result r; r.detail = std::move(d); return r; }
    static Result fail(Err e, std::string d = {}) { Result r; r.err = e; r.detail = std::move(d); return r; }
};

} // namespace gr
