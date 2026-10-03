// 情报 / 知识系统实现
#include "gr/core/IntelSystem.hpp"
#include "gr/gu/GuWorm.hpp"

#include <algorithm>

namespace gr {

// ---------------------------------------------------------------------------
//  id 构造
// ---------------------------------------------------------------------------
std::string IntelSystem::recipeId(std::size_t rid) {
    return "recipe:" + std::to_string(rid);
}
std::string IntelSystem::siteId(const std::string& s)  { return "site:" + s; }
std::string IntelSystem::rumorId(const std::string& s) { return "intel:" + s; }

// ---------------------------------------------------------------------------
//  登记
// ---------------------------------------------------------------------------
void IntelSystem::registerIntel(const IntelEntry& e) {
    if (find(e.id)) return;          // 已登记则跳过，避免重复
    entries_.push_back(e);
}

// 五域传承倾向：依地理研究各域环境与原著势力分布推导。
// 这不是原著明文的「某地必出某道」，而是让蛊方有处可求的合理分配 ——
// 水道归东海、冰道归北原、毒道瘴气归南疆、土道矿藏归西漠，中洲为交流中枢。
Domain IntelSystem::daoToDomain(Dao d) {
    switch (d) {
        case Dao::Water:  return Domain::DongHai;
        case Dao::Ice:    return Domain::BeiYuan;
        case Dao::Poison: return Domain::NanJiang;
        case Dao::Wood:   return Domain::NanJiang;
        case Dao::Earth:  return Domain::XiMo;
        case Dao::Light:  return Domain::NanJiang;
        case Dao::Food:   return Domain::NanJiang;
        default:          return Domain::ZhongZhou;
    }
}

void IntelSystem::registerRecipes(const std::vector<const GuRecipe*>& recipes) {
    for (const GuRecipe* r : recipes) {
        if (!r) continue;
        IntelEntry e;
        e.id       = recipeId(r->id);
        e.category = IntelCategory::Recipe;
        e.title    = r->name + "（蛊方）";
        e.detail   = "目标：" + r->name + "；流派：" + to_string(r->dao);
        e.hint     = "据闻有一纸蛊方流传于世";
        e.source   = r->source;
        e.canon    = r->canon;
        e.where    = daoToDomain(r->dao);
        registerIntel(e);
        // 凡蛊方属常识，开局即懂；仙蛊方才须求得
        if (!r->isImmortal()) commonIds_.push_back(e.id);
    }
}

// ---------------------------------------------------------------------------
//  获知
// ---------------------------------------------------------------------------
bool IntelSystem::knows(const Cultivator& c, const std::string& id) const {
    // 凡蛊方等常识：不须记入玩家所知，直接视为已知
    if (isCommonKnowledge(id)) return true;
    return c.knowsIntel(id);
}

bool IntelSystem::isCommonKnowledge(const std::string& id) const {
    return std::find(commonIds_.begin(), commonIds_.end(), id) != commonIds_.end();
}

bool IntelSystem::learn(Cultivator& c, const std::string& id) const {
    if (c.knowsIntel(id)) return false;
    // 只登记已存在的情报，避免塞入无意义字符串
    if (!find(id)) return false;
    c.learnIntel(id);
    return true;
}

// ---------------------------------------------------------------------------
//  查询
// ---------------------------------------------------------------------------
const IntelEntry* IntelSystem::find(const std::string& id) const {
    for (const auto& e : entries_) if (e.id == id) return &e;
    return nullptr;
}

std::vector<IntelEntry> IntelSystem::knownEntries(const Cultivator& c) const {
    std::vector<IntelEntry> out;
    for (const auto& e : entries_) if (c.knowsIntel(e.id)) out.push_back(e);
    return out;
}

std::size_t IntelSystem::knownCount(const Cultivator& c) const {
    std::size_t n = 0;
    for (const auto& e : entries_) if (c.knowsIntel(e.id)) ++n;
    return n;
}

} // namespace gr
