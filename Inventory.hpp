// ============================================================================
//  背包（Inventory）
//
//  为什么必须单独建：
//    蛊方除了「合炼用蛊」（componentGu）之外还列有 materials（仙材、美酒等），
//    而 RefineRequest::inventory 此前从未被填充 ——
//    实测炼「四味酒虫」必被判「材料不齐：缺 酸甜苦辣四味美酒」，
//    于是凡蛊方在游戏里根本炼不出来。背包是让炼蛊真正跑通的前提，
//    不只是多一个界面。
//
//  与「囊中蛊」的关系：
//    蛊虫是活的、有唯一性与状态，存在 Cultivator::carriedGu；
//    材料是无生命的死物，可堆叠、可计量，存在本背包。
//    两者分开存，界面上并列两栏展示。
// ============================================================================
#pragma once

#include "gr/gu/GuWorm.hpp"

#include <string>
#include <vector>
#include <algorithm>
#include <cmath>

namespace gr {

// 物品分类
enum class ItemKind : std::uint8_t {
    Material   = 0,   // 材料 / 仙材：炼蛊用
    Consumable = 1,   // 消耗品：可用掉（回复、增益）
    Quest      = 2,   // 任务物品：剧情相关
    Treasure   = 3,   // 天材地宝：贵重，可交易
};

inline const char* to_string(ItemKind k) {
    switch (k) {
        case ItemKind::Material:   return "材料";
        case ItemKind::Consumable: return "消耗品";
        case ItemKind::Quest:      return "任务物品";
        case ItemKind::Treasure:   return "天材地宝";
    }
    return "？";
}

struct ItemStack {
    std::string name;
    double      amount = 0.0;
    ItemKind    kind   = ItemKind::Material;
    std::string source;     // 溯源：原著出处 / 工程设定
    std::string desc;       // 用途说明

    bool empty() const { return amount <= 1e-9; }
};

// ---------------------------------------------------------------- 背包
class Inventory {
public:
    // —— 增减 ——
    void add(const std::string& name, double amount,
             ItemKind kind = ItemKind::Material,
             const std::string& source = {},
             const std::string& desc = {});
    //  取出：成功返回 true 并扣减；不足则不扣并返回 false
    bool take(const std::string& name, double amount);
    //  丢弃
    bool drop(const std::string& name, double amount);

    // —— 查询 ——
    double      count(const std::string& name) const;
    bool        has(const std::string& name, double amount) const {
        //  非正数 / 非有限值一律视为「不满足」。
        //  否则 has(x, -1) 恒为真（count+eps 总 >= 负数），
        //  配合 take(x, -1) 就会把物品越取越多 —— 可无限刷。
        if (!std::isfinite(amount) || amount <= 0.0) return false;
        return count(name) + 1e-9 >= amount;
    }
    const ItemStack* find(const std::string& name) const;
    std::size_t      size() const { return stacks_.size(); }
    bool             empty() const { return stacks_.empty(); }

    // 全部物品（按分类、再按名称排列，便于界面稳定显示）
    std::vector<const ItemStack*> items() const;
    std::vector<const ItemStack*> itemsOf(ItemKind k) const;

    // —— 炼蛊联动 ——
    //  校验：某蛊方所需材料是否齐备。缺什么写进 missing。
    bool canAfford(const GuRecipe& r, std::vector<std::string>* missing = nullptr) const;
    //  扣除：炼成后消耗掉该蛊方所需材料。返回是否成功。
    bool consume(const GuRecipe& r);
    //  换算成 RefineRequest 所需的 inventory 视图
    std::vector<MaterialRequirement> asRequirements() const;

    // 总负重（用于界面展示；原著未给负重公式，此处仅为件数×数量概数）
    double totalWeight() const;

private:
    std::vector<ItemStack> stacks_;
};

// 开局随身物品：凡人家族传承所得，量少而实用
std::vector<ItemStack> startingInventory();

} // namespace gr
