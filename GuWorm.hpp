// ============================================================================
//  十一、仙蛊唯一体系（完整保留优化）
//    1. 六转至九转仙蛊全域唯一
//    2. 封印、抢夺、封存不清除注册表
//    3. 彻底毁灭才可重生
//    4. 仿伪蛊、尊者幻象为唯二特例
//    5. NPC、玩家完全平等
//  另：凡蛊（一至五转）同种可大量存在，能力单一；六转及以上为仙蛊
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"

#include <string>
#include <vector>

namespace gr {

using GuId     = std::uint64_t;
using RecipeId = std::uint64_t;

// 仙蛊在注册表中的存在状态
enum class GuState : std::uint8_t {
    Active   = 0,  // 在世、可用
    Sealed   = 1,  // 被封印（不清除注册表，仍占唯一名额）
    Stored   = 2,  // 被封存/收藏（不清除注册表）
    Destroyed= 3   // 彻底毁灭 —— 唯一可让同名仙蛊重生的状态
};

inline const char* to_string(GuState s) {
    switch (s) {
        case GuState::Active:    return "在世";
        case GuState::Sealed:    return "封印中";
        case GuState::Stored:    return "封存中";
        case GuState::Destroyed: return "已彻底毁灭";
    }
    return "？";
}

// 仙蛊唯一的唯二特例
enum class GuSpecial : std::uint8_t {
    None = 0,
    FakeGu,        // 仿伪蛊：不占唯一名额
    ZunZheIllusion // 尊者幻象：不占唯一名额
};

inline const char* to_string(GuSpecial s) {
    switch (s) {
        case GuSpecial::None:           return "常规";
        case GuSpecial::FakeGu:         return "仿伪蛊";
        case GuSpecial::ZunZheIllusion: return "尊者幻象";
    }
    return "？";
}

// ---------------------------------------------------------------- 蛊虫模板
struct GuTemplate {
    GuId        id = 0;
    std::string name;
    Rank        rank = Rank::R1;
    GuCategory  category = GuCategory::Attack;
    Dao         dao = Dao::Refine;      // 所属流派
    std::string effect;                 // 功能描述
    std::string feed;                   // 食物 / 喂养需求
    std::string source;                 // 资料溯源：出自资料库哪张表 / 研究报告 / 需求说明书
    GuSpecial   special = GuSpecial::None;

    // 转数是否为资料库确认值。false = 原著「转数待考/不详」，
    // 此时 rank 仅为占位，任何依赖其转数的判定都应走「不可判定」分支，
    // 不得拿占位值参与强弱比较（遵循资料库的「不编造原则」）。
    bool        rankConfirmed = true;

    // 是否为【工程原创】蛊虫。
    // 原著并未记载这些蛊，是依原著设定推导出来、用于填补玩法空白的。
    // 必须为 true 时在 source 写明推导依据，且界面/文档中显著标注「非原著」，
    // 严禁与 canon 条目混同 —— 见 docs/原创蛊虫设计.md。
    bool        original = false;

    GuTier tier() const { return tier_of(rank); }
    bool isImmortal() const { return tier() == GuTier::Immortal; }
};

// ---------------------------------------------------------------- 蛊虫实例
struct GuInstance {
    GuId     templateId = 0;
    GuId     instanceId = 0;
    GuState  state      = GuState::Active;
    std::string holder;                 // 持有者 id
    int      refinementLevel = 0;       // 升炼次数
    bool     defective = false;         // 残次品 / 残缺品
    double   integrity = 1.0;           // 完整度 0~1，影响威力与崩解概率

    // —— 喂养（蛊虫平时要吃饭，不喂则降功效乃至饿死）——
    double   fullness = 1.0;            // 饱食度 0~1
    int      starveTicks = 0;           // 饱食度归零后累计的天数

    bool usable() const { return state == GuState::Active && integrity > 0.0; }
};

// ---------------------------------------------------------------- 蛊方
enum class RecipeIntegrity : std::uint8_t {
    Complete    = 0,  // 完整蛊方：可正常炼制
    Incomplete  = 1,  // 残缺蛊方：只能炼残缺品、残次蛊、失败品
    Speculative = 2   // 无据推演 —— 等价于「无蛊方」，绝对无法炼制
};

inline const char* to_string(RecipeIntegrity r) {
    switch (r) {
        case RecipeIntegrity::Complete:    return "完整蛊方";
        case RecipeIntegrity::Incomplete:  return "残缺蛊方";
        case RecipeIntegrity::Speculative: return "无据推演（不可炼）";
    }
    return "？";
}

struct MaterialRequirement {
    std::string name;
    double      amount = 0.0;
};

struct GuRecipe {
    RecipeId    id = 0;
    std::string name;                       // 目标蛊名
    Rank        targetRank = Rank::R1;
    Dao         dao = Dao::Refine;
    RecipeIntegrity integrity = RecipeIntegrity::Complete;
    std::vector<MaterialRequirement> materials;
    std::vector<GuId>   componentGu;        // 合炼所需的已有蛊（合炼公式）
    DaoLevel    requiredDao   = DaoLevel::Entry;
    double      requiredEssence = 0.0;      // 炼制消耗

    // 所需炼蛊手法（对应 RefineTechnique::id）。
    //  0 = 无：直接炼制即可，不需特殊手法（如月芒蛊、四味酒蛊）。
    //  非 0 = 须选中对应手法才可确定开炼；手法须先学会。
    int         requiredTechnique = 0;
    bool        canon = true;               // 是否原著/可核验蛊方
    std::string source;                     // 资料溯源：出自资料库哪张表 / 研究报告 / 工程占位

    // 目标是否为仙蛊（六转及以上）。
    // 凡蛊方与仙蛊方在「是否需先求得」上分野：
    // 凡人炼蛊是家常便饭，仙蛊配方才是稀缺资源。
    bool isImmortal() const { return tier_of(targetRank) == GuTier::Immortal; }
};

} // namespace gr
