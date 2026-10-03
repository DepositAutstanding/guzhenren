// 炼蛊台实现
#include "gr/gu/RefineBench.hpp"

#include <algorithm>
#include <cmath>

namespace gr {

// ---------------------------------------------------------------- 手法总表
const std::vector<RefineTechnique>& allRefineTechniques() {
    // 【工程设定】原著确有「炼蛊手法」概念，但资料库未逐条收录手法名录，
    // 故下列名称属二次创作，canon=false，不得当原著引用。
    // id == 0 恒为「无」：直接炼制、无需特殊手法（如月芒蛊、四味酒蛊）。
    static const std::vector<RefineTechnique> k = {
        {0,  "无",   "直接炼制，不需特殊手法。多数凡蛊（如月芒蛊、四味酒蛊）皆如此",
             false, "工程设定：对应需求「默认认为无」"},
        {1,  "文火慢炼", "以温和火候徐徐炼制，成蛊率高而耗时较长",
             false, "工程设定"},
        {2,  "猛火快炼", "以猛烈火候急速炼制，耗时短而不稳",
             false, "工程设定"},
        {3,  "水磨功夫", "反复淬洗磨炼，去杂质，宜炼凡蛊",
             false, "工程设定"},
        {4,  "血祭炼法", "以血气催炼，魔道常用，凶险而威力大",
             false, "工程设定"},
        {5,  "三气调和炼法", "以三气平衡为基炼制，须蛊仙方可施展",
             false, "工程设定"},
    };
    return k;
}

const RefineTechnique* refineTechnique(int id) {
    for (const auto& t : allRefineTechniques())
        if (t.id == id) return &t;
    return nullptr;
}

// ---------------------------------------------------------------- 槽位访问
void RefineBench::setCurSlot(int i) {
    if (slots_.empty()) { curSlot_ = 0; return; }
    curSlot_ = std::clamp(i, 0, static_cast<int>(slots_.size()) - 1);
}

RefineSession* RefineBench::cur() {
    if (slots_.empty()) return nullptr;
    if (curSlot_ < 0 || curSlot_ >= static_cast<int>(slots_.size())) curSlot_ = 0;
    return &slots_[curSlot_];
}

const RefineSession* RefineBench::cur() const {
    if (slots_.empty()) return nullptr;
    const int i = std::clamp(curSlot_, 0, static_cast<int>(slots_.size()) - 1);
    return &slots_[i];
}

// ---------------------------------------------------------------- 开启
void RefineBench::open(RecipeId recipe, const std::string& name) {
    slots_.clear();
    RefineSession s;
    s.recipeId   = recipe;
    s.recipeName = name;
    s.stage      = RefineStage::Preparing;
    // 需求：预留 2 秒人物形象原地等待
    s.waitRemaining = 2.0;
    slots_.push_back(s);
    curSlot_ = 0;
}

int RefineBench::openSlot(RecipeId recipe, const std::string& name,
                          int soulStrength, bool hasMultiGu) {
    if (!canMultiTask(soulStrength, slotCount(), hasMultiGu)) return -1;
    if (slotCount() >= kMaxSlots) return -1;
    RefineSession s;
    s.recipeId   = recipe;
    s.recipeName = name;
    s.stage      = RefineStage::Preparing;
    s.waitRemaining = 2.0;
    slots_.push_back(s);
    curSlot_ = static_cast<int>(slots_.size()) - 1;
    return curSlot_;
}

void RefineBench::close(int slot) {
    if (slots_.empty()) return;
    if (slot < 0) slot = curSlot_;
    if (slot < 0 || slot >= static_cast<int>(slots_.size())) return;
    slots_.erase(slots_.begin() + slot);
    if (slots_.empty()) curSlot_ = 0;
    else curSlot_ = std::min(curSlot_, static_cast<int>(slots_.size()) - 1);
}

// ---------------------------------------------------------------- 备料
bool RefineBench::toggleMaterial(int slot, GuId instId) {
    if (slot < 0 || slot >= static_cast<int>(slots_.size())) return false;
    auto& ms = slots_[slot].materialInst;
    auto it = std::find(ms.begin(), ms.end(), instId);
    if (it != ms.end()) {
        ms.erase(it);          // 第二次点击 = 放弃选择
        return false;
    }
    ms.push_back(instId);      // 第一次点击 = 选中
    return true;
}

bool RefineBench::hasMaterial(int slot, GuId instId) const {
    if (slot < 0 || slot >= static_cast<int>(slots_.size())) return false;
    const auto& ms = slots_[slot].materialInst;
    return std::find(ms.begin(), ms.end(), instId) != ms.end();
}

void RefineBench::setTechnique(int slot, int techniqueId) {
    if (slot < 0 || slot >= static_cast<int>(slots_.size())) return;
    // 只允许已收录手法；未收录则回落为「无」
    slots_[slot].techniqueId = refineTechnique(techniqueId) ? techniqueId : 0;
}

// ---------------------------------------------------------------- 开始炼制
bool RefineBench::begin(int slot, const GuRecipe& recipe,
                        const std::vector<GuId>& selTplIdsIn) {
    if (slot < 0 || slot >= static_cast<int>(slots_.size())) return false;
    std::vector<GuId> selTplIds = selTplIdsIn;   // 本地副本，逐个核销
    RefineSession& s = slots_[slot];

    // 齐备性：配方要几只同种蛊，就得有几只（可重复计数）
    for (GuId need : recipe.componentGu) {
        auto it = std::find(selTplIds.begin(), selTplIds.end(), need);
        if (it == selTplIds.end()) return false;
        selTplIds.erase(it);   // 核销一只；同蛊多只则依次核销
    }

    // 时长：蛊方转数越高越久；手法会影响
    double ticks = 30.0 + static_cast<double>(static_cast<int>(recipe.targetRank)) * 18.0;
    if (s.techniqueId == 2)      ticks *= 0.65;   // 猛火快炼
    else if (s.techniqueId == 1) ticks *= 1.35;   // 文火慢炼
    else if (s.techniqueId == 3) ticks *= 1.15;   // 水磨功夫

    s.baseTicks   = std::max<Tick>(10, static_cast<Tick>(std::lround(ticks)));
    s.remainTicks = s.baseTicks;
    // 冷却：前 1/5 时间不可终止（刚起炉就撤手，理应不许）
    s.cooldownTicks = std::max<Tick>(1, s.baseTicks / 5);
    s.stage     = RefineStage::Refining;
    // 稳定与否：残缺蛊方或猛火手法更易不稳
    s.stable    = (recipe.integrity == RecipeIntegrity::Complete) && (s.techniqueId != 2);
    s.speedMul  = 1.0;
    s.outcome   = RefineOutcome::Blocked;
    return true;
}

// ---------------------------------------------------------------- 光阴蛊加速
bool RefineBench::applyTimeGu(int slot) {
    if (slot < 0 || slot >= static_cast<int>(slots_.size())) return false;
    RefineSession& s = slots_[slot];
    if (!s.running()) return false;
    if (s.timeGuUsed) return false;      // 每炉只能用一次
    s.timeGuUsed = true;
    s.speedMul   = 2.0;                  // 光阴蛊：流速加倍
    return true;
}

// ---------------------------------------------------------------- 推进
void RefineBench::tickPrepare(double dtSeconds, std::vector<std::string>& notes) {
    for (auto& s : slots_) {
        if (s.stage != RefineStage::Preparing) continue;
        s.waitRemaining -= dtSeconds;
        if (s.waitRemaining <= 0.0) {
            s.waitRemaining = 0.0;
            s.stage = RefineStage::Setup;
            notes.push_back("炉火已备：切至炼蛊台");
        }
    }
}

void RefineBench::tick(Tick dtTicks, std::vector<std::string>& notes) {
    for (auto& s : slots_) {
        if (s.stage == RefineStage::Refining) {
            const Tick dt = static_cast<Tick>(std::lround(
                static_cast<double>(dtTicks) * s.speedMul));
            s.remainTicks -= dt;
            if (s.remainTicks <= 0) {
                s.remainTicks = 0;
                s.stage   = RefineStage::Finished;
                s.outcome = s.stable ? RefineOutcome::Success
                                     : RefineOutcome::Defective;
                notes.push_back("炼成 " + s.recipeName +
                                (s.outcome == RefineOutcome::Success ? "" : "（残次品）"));
            }
        }
    }
}

// ---------------------------------------------------------------- 终止
bool RefineBench::terminate(int slot, std::string& why) {
    if (slot < 0 || slot >= static_cast<int>(slots_.size())) { why = "无此炉"; return false; }
    RefineSession& s = slots_[slot];
    if (!s.running()) { why = "未在炼制中"; return false; }
    // 需求：若炼蛊时间小于冷却，则不可终止
    if (!s.canTerminate()) {
        why = "起炉未稳（已炼 " + std::to_string(s.elapsed()) +
              " / 冷却 " + std::to_string(s.cooldownTicks) + " 刻），不可撤手";
        return false;
    }
    // 稳定态 → 半成品；不稳定 → 失败
    s.outcome = s.stable ? RefineOutcome::Defective : RefineOutcome::Failure;
    s.terminated = true;
    s.stage   = RefineStage::Finished;
    s.detail  = s.stable ? "提前收炉，得一半成品" : "火候失控，炼蛊失败";
    return true;
}

// ---------------------------------------------------------------- 一心多用
bool RefineBench::canMultiTask(int soulStrength, int currentSlots, bool hasMultiGu) {
    // 魂魄强度决定天然可同时驾驭几炉
    //  魂魄为负属异常数据，先夹到非负再判 ——
    //  否则 -30/30 = -1，clamp 后仍为 1，负数魂魄也能开炉。
    const int soul    = std::max(0, soulStrength);
    const int natural = std::clamp(soul / 30, 1, kMaxSlots);
    if (currentSlots < natural) return true;
    // 超出天赋则须借助「一心多用」系列蛊
    return hasMultiGu;
}

} // namespace gr
