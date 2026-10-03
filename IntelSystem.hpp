// ============================================================================
//  情报 / 知识系统
//
//  核心原则：**得先知道，才谈得上做。**
//
//  此前界面把蛊方、地点、人物一股脑全列出来，等于默认玩家全知 ——
//  这既不合理，也让「探索」和「打探」失去意义：
//  既然一开始就知道全部配方，何必出门？
//
//  本系统把「信息」本身变成一种资源：
//    · 蛊方：未得蛊方则不见其名、不知其组成蛊、更不能炼制
//    · 地点：未探明则不显示（地图迷雾已实现）
//    · 传闻：如「定仙游在方源手上」，未听说则支线无从触发
//
//  情报 id 采用前缀命名空间，避免不同类别撞名：
//      recipe:<RecipeId>        蛊方
//      site:<siteId>            地点
//      intel:<name>             传闻 / 常识
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"
#include "gr/cultivator/Cultivator.hpp"

#include <string>
#include <vector>

namespace gr {

enum class IntelCategory : std::uint8_t {
    Recipe = 0,     // 蛊方
    Site,           // 地点
    Rumor,          // 传闻 / 常识
    Person          // 人物
};

inline const char* to_string(IntelCategory c) {
    switch (c) {
        case IntelCategory::Recipe: return "蛊方";
        case IntelCategory::Site:   return "地点";
        case IntelCategory::Rumor:  return "传闻";
        case IntelCategory::Person: return "人物";
    }
    return "？";
}

struct IntelEntry {
    std::string   id;
    IntelCategory category = IntelCategory::Rumor;
    std::string   title;        // 已知时显示的标题
    std::string   detail;       // 已知时显示的正文
    std::string   hint;         // 未知时给出的线索（可为空 = 完全不透露存在）
    std::string   source;       // 溯源
    bool          canon = true; // 是否原著可核验

    // 何处可得：蛊方须「求」才有 —— 在人烟处打探，且所在域匹配方可学到。
    // 五域各有传承倾向，这既合原著（各地家族各掌一门），
    // 也让探索有了意义：想凑齐一套方子，得真的走遍五域。
    Domain where = Domain::None;      // None = 不限地域
};

// 蛊方视图：未解锁时涂黑，只留「未得此方」
struct RecipeView {
    std::size_t   index = 0;      // 在完整表中的索引
    bool          known = false;  // 是否已得蛊方
    std::string   name;           // known 才有效
    std::string   rankName;
    std::string   daoName;
    std::string   integrityName;
    std::string   source;
    bool          canRefine = false;
};

class IntelSystem {
public:
    // ---- 登记（世界构建期调用） ----
    void registerIntel(const IntelEntry& e);
    // 由蛊方表批量登记蛊方情报（同时按流派定其流传之域）
    void registerRecipes(const std::vector<const class GuRecipe*>& recipes);

    // 按流派推断蛊方流传之域（五域各有传承倾向）
    static Domain daoToDomain(Dao d);

    // ---- 获知 ----
    // 返回 true 表示这次是「新得知」（此前不知）
    bool learn(Cultivator& c, const std::string& id) const;
    bool knows(const Cultivator& c, const std::string& id) const;

    // 凡蛊方（一至五转）视为「开局即懂」，不必逐条记入玩家所知。
    //
    //  为什么这样做：凡人炼蛊是原著核心内容，凡人家族各有传承。
    //  若凡蛊方也要求情报，而打探又限六转以上，凡人便永远炼不了蛊 ——
    //  这是根本性的设定冲突。故凡蛊方默认可知，仙蛊方才须求得。
    bool isCommonKnowledge(const std::string& id) const;

    // ---- 查询 ----
    const IntelEntry* find(const std::string& id) const;
    std::vector<IntelEntry> knownEntries(const Cultivator& c) const;
    const std::vector<IntelEntry>& all() const { return entries_; }
    std::size_t knownCount(const Cultivator& c) const;
    std::size_t totalCount() const { return entries_.size(); }

    // ---- 便捷 id 构造 ----
    static std::string recipeId(std::size_t recipeId);
    static std::string siteId(const std::string& s);
    static std::string rumorId(const std::string& s);

private:
    std::vector<IntelEntry> entries_;
    // 凡蛊方 id 集合：这些是常识，不须求得
    std::vector<std::string> commonIds_;
};

} // namespace gr
