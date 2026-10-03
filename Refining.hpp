// ============================================================================
//  六、炼蛊系统全新硬性规则
//    6.1 炼蛊前置铁律
//      1. 无蛊方 → 绝对无法炼制、无法推演、无法瞎炼
//      2. 残缺蛊方 → 只能炼残缺品、残次蛊、失败品
//      3. 完整蛊方 + 齐全材料 + 炼道境界 → 可正常炼制
//      4. 仙蛊炼制依旧遵守「仙蛊唯一」底层规则
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"
#include "gr/gu/GuWorm.hpp"
#include "gr/gu/GuRegistry.hpp"
#include "gr/cultivator/DaoRealm.hpp"
#include "gr/world/Climate.hpp"

#include <string>
#include <vector>
#include <algorithm>

namespace gr {

enum class RefineOutcome : std::uint8_t {
    Success        = 0,  // 正常炼制成功
    Defective      = 1,  // 残次蛊 / 残缺品
    Failure        = 2,  // 失败品
    Blocked        = 3   // 被铁律挡下（无蛊方 / 唯一冲突 / 道境不足 / 材料不齐）
};

inline const char* to_string(RefineOutcome o) {
    switch (o) {
        case RefineOutcome::Success:   return "成功";
        case RefineOutcome::Defective: return "残次品";
        case RefineOutcome::Failure:   return "失败品";
        case RefineOutcome::Blocked:   return "被铁律挡下";
    }
    return "？";
}

struct RefineRequest {
    RecipeId recipeId = 0;
    std::string refinerId;
    bool   refinerIsPlayer = false;
    DaoLevel refinerDao = DaoLevel::Entry;   // 炼道境界
    double refinerEssence = 0.0;
    std::vector<MaterialRequirement> inventory;   // 背包材料
    std::vector<GuInstance> componentGu;          // 背包中的合炼用蛊
    double rngRoll = 0.5;                         // [0,1)，由调用方注入以保证可复现
};

struct RefineResult {
    RefineOutcome outcome = RefineOutcome::Blocked;
    Err           err     = Err::Ok;
    GuInstance    product;         // outcome != Blocked/Failure 时有效
    std::string   guName;
    GuId          registryId = 0;  // 仙蛊注册 id（凡蛊为 0）
    double        essenceSpent = 0.0;
    std::string   detail;
    bool ok() const {
        return outcome == RefineOutcome::Success || outcome == RefineOutcome::Defective;
    }
};

// 炼蛊台：持有蛊方表与蛊虫模板表，是炼制判定的唯一入口
class Refinery {
public:
    void addRecipe(const GuRecipe& r);
    void addTemplate(const GuTemplate& t);

    const GuRecipe*   recipe(RecipeId id) const;
    // 全部蛊方（UI 列表用，按 id 顺序）
    std::vector<const GuRecipe*> recipes() const {
        std::vector<const GuRecipe*> v;
        v.reserve(recipes_.size());
        for (const auto& r : recipes_) v.push_back(&r);
        std::sort(v.begin(), v.end(),
                  [](const GuRecipe* a, const GuRecipe* b) { return a->id < b->id; });
        return v;
    }
    const GuTemplate* guTemplate(GuId id) const;
    std::vector<const GuRecipe*> recipesFor(const std::string& guName) const;
    std::vector<GuTemplate> templates() const { return templates_; }

    // 核心：执行一次炼制
    RefineResult refine(const RefineRequest& req, ImmortalGuRegistry& registry);

private:
    std::vector<GuRecipe>   recipes_;
    std::vector<GuTemplate> templates_;
    GuId nextInstanceId_ = 100000;

    GuId            allocInstanceId() { return nextInstanceId_++; }
    const GuTemplate* templateByName(const std::string& n) const;
};

// 原著 / 资料库可核验蛊虫模板样例（资料库收录 450 凡蛊 + 362 仙蛊，此处为骨架样本）
std::vector<GuTemplate> build_canon_gu_templates();

//  资料库【全表】补充条目 —— 由 tools/gen_gu_data.py 生成。
//  工程目的是还原蛊世界，故资料库明载的蛊虫应全收，而非只取手工精编的几十条。
//  idBase 需避开 build_canon_gu_templates 已占用的 id。
std::vector<GuTemplate> buildCanonGuData(std::uint32_t idBase);
// 原著 / 资料库可核验蛊方样例
std::vector<GuRecipe> build_canon_recipes(const std::vector<GuTemplate>& tpls);

//  资料库【全表】蛊方 —— 由 tools/gen_recipe_data.py 生成。
//  蛊方取自资料库「炼制方法」列的合炼公式与「炼制材料」列。
//  按蛊名登记、运行时查 id；与手工精编蛊方重名者以精编版为准。
std::vector<GuRecipe> buildCanonRecipeData(const std::vector<GuTemplate>& tpls,
                                           RecipeId idBase);

} // namespace gr
