// 背包实现
#include "gr/gu/Inventory.hpp"

#include <cmath>

namespace gr {

void Inventory::add(const std::string& name, double amount,
                    ItemKind kind, const std::string& source,
                    const std::string& desc) {
    //  关键防护：非有限值（NaN / Inf）与非正数一律拒绝。
    //  不能只写 amount <= 1e-9 —— NaN 与任何数比较皆为 false，
    //  `NaN <= 1e-9` 为假，于是 NaN 会被放行混入背包；
    //  一旦混入，此后所有 has / take 的数值比较全部失效。
    if (name.empty() || !std::isfinite(amount) || amount <= 0.0) return;
    for (auto& s : stacks_) {
        if (s.name == name) {
            s.amount += amount;
            // 后补的信息只填空缺，不覆盖已有描述
            if (!source.empty() && s.source.empty()) s.source = source;
            if (!desc.empty()   && s.desc.empty())   s.desc   = desc;
            return;
        }
    }
    ItemStack s;
    s.name = name; s.amount = amount; s.kind = kind;
    s.source = source; s.desc = desc;
    stacks_.push_back(s);
}

bool Inventory::take(const std::string& name, double amount) {
    //  负数 / 零 / 非有限值一律拒绝。
    //  这是本轮最严重的漏洞：此前只判 `s.amount + 1e-9 < amount`，
    //  当 amount 为负时该条件恒不成立，于是走到 `s.amount -= amount`
    //  —— 减去负数等于加上正数，物品越「取」越多。
    //  实测：丢弃 灵石 ×(-10) 会让 50 变成 60，可无限刷。
    if (name.empty() || !std::isfinite(amount) || amount <= 0.0) return false;
    for (auto& s : stacks_) {
        if (s.name != name) continue;
        if (s.amount + 1e-9 < amount) return false;
        s.amount -= amount;
        if (s.amount <= 1e-9) {
            // 取空则移除该格，避免界面上留一堆 0
            s = std::move(stacks_.back());
            stacks_.pop_back();
        }
        return true;
    }
    return false;
}

bool Inventory::drop(const std::string& name, double amount) {
    // 丢弃与取出同为扣减，差别只在界面语义
    return take(name, amount);
}

double Inventory::count(const std::string& name) const {
    for (const auto& s : stacks_)
        if (s.name == name) return s.amount;
    return 0.0;
}

const ItemStack* Inventory::find(const std::string& name) const {
    for (const auto& s : stacks_)
        if (s.name == name) return &s;
    return nullptr;
}

std::vector<const ItemStack*> Inventory::items() const {
    std::vector<const ItemStack*> v;
    v.reserve(stacks_.size());
    for (const auto& s : stacks_)
        if (!s.empty()) v.push_back(&s);
    std::sort(v.begin(), v.end(), [](const ItemStack* a, const ItemStack* b) {
        if (a->kind != b->kind) return static_cast<int>(a->kind) < static_cast<int>(b->kind);
        return a->name < b->name;
    });
    return v;
}

std::vector<const ItemStack*> Inventory::itemsOf(ItemKind k) const {
    std::vector<const ItemStack*> v;
    for (const auto& s : stacks_)
        if (!s.empty() && s.kind == k) v.push_back(&s);
    std::sort(v.begin(), v.end(), [](const ItemStack* a, const ItemStack* b) {
        return a->name < b->name;
    });
    return v;
}

bool Inventory::canAfford(const GuRecipe& r, std::vector<std::string>* missing) const {
    bool ok = true;
    for (const auto& need : r.materials) {
        if (!has(need.name, need.amount)) {
            if (missing) missing->push_back(need.name);
            ok = false;
        }
    }
    return ok;
}

bool Inventory::consume(const GuRecipe& r) {
    // 先整体校验再扣 —— 否则会出现「扣了一半才发现不够」的残局
    if (!canAfford(r)) return false;
    for (const auto& need : r.materials) take(need.name, need.amount);
    return true;
}

std::vector<MaterialRequirement> Inventory::asRequirements() const {
    std::vector<MaterialRequirement> v;
    v.reserve(stacks_.size());
    for (const auto& s : stacks_) {
        if (s.empty()) continue;
        MaterialRequirement m;
        m.name = s.name;
        m.amount = s.amount;
        v.push_back(m);
    }
    return v;
}

double Inventory::totalWeight() const {
    double w = 0.0;
    for (const auto& s : stacks_) if (!s.empty()) w += s.amount;
    return w;
}

// ---------------------------------------------------------------- 开局物品
std::vector<ItemStack> startingInventory() {
    //
    //  【溯源说明】原著对「凡人蛊师随身带什么」并无清单式记载。
    //  以下物品依据三条可核验信息推导：
    //    · 四味酒虫方需「酸甜苦辣四味美酒」—— 资料库蛊方表明载，故开局给一份，
    //      否则凡人第一条凡蛊方就炼不成（实测会被判「材料不齐」）。
    //    · 痕石用于炼痕石蛊—— 同为资料库明载材料。
    //    · 其余为工程设定，已标注 canon=false，不得当原著引用。
    std::vector<ItemStack> v;
    auto mk = [](const char* n, double a, ItemKind k, const char* src, const char* d) {
        ItemStack s;
        s.name = n; s.amount = a; s.kind = k; s.source = src; s.desc = d;
        return s;
    };
    v.push_back(mk("酸甜苦辣四味美酒", 2, ItemKind::Material,
                   "蛊方表明载：四味酒虫所需材料",
                   "酸甜苦辣四味交融之酒，炼四味酒虫的主材"));
    v.push_back(mk("痕石", 3, ItemKind::Material,
                   "蛊方表明载：痕石蛊所需材料",
                   "承载道痕的矿石，炼痕石蛊所用"));
    v.push_back(mk("灵石", 50, ItemKind::Treasure,
                   "工程设定",
                   "通用硬通货，可交易"));
    v.push_back(mk("止血草", 5, ItemKind::Consumable,
                   "工程设定",
                   "嚼碎敷于伤处，可缓缓恢复"));
    return v;
}

} // namespace gr
