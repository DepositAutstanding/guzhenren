// ============================================================================
//  炼蛊台（RefineBench）—— 交互式炼蛊会话
//
//  本模块对应需求 8：把「炼蛊」从【一条指令】升级为【一套交互流程】：
//
//    点击炼蛊
//      → ① 预留 2 秒：人物形象原地等待（Preparing）
//      → ② 画面切到地图并覆盖半透明灰层（Setup）
//           · 上部：选原材料（弹背包，点一次选中、两次放弃；
//                    蛊方列表可折叠，悬浮显示组成蛊虫，点击自动取料）
//           · 中部：选炼蛊手法（下拉 + 滚轮；只列已学手法）
//           · 下部：确定 / 取消
//      → ③ 炼制中（Refining）：三部分末行换成
//           剩余时间（按世界刻度计）、终止炼蛊、一心多用
//      → ④ 完成：界面消失，蛊自动归入囊中
//
//  为什么单独成模块：炼制过程跨越多帧、带状态与倒计时，
//  绝非「一条指令 + 立刻出结果」能表达。状态机放在界面层会与渲染纠缠，
//  放在规则层则界面读不到进度 —— 故独立为炼蛊台，由会话层持有。
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"
#include "gr/gu/GuWorm.hpp"
#include "gr/gu/Refining.hpp"

#include <string>
#include <vector>

namespace gr {

// ---------------------------------------------------------------- 炼蛊手法
//
//  需求：手法列表中所有手法均为【玩家已学过】的手法；
//  手法可通过阅读书籍、拜师等方式学习。
//
//  原著依据说明：原著确有「炼蛊手法」这一概念（手法高低直接影响成蛊率，
//  方源前世五百年所积即包含大量手法），但资料库未逐条收录手法名录。
//  故下列具体手法名属【工程设定】，已标注 canon=false，
//  不得当作原著设定引用。id == 0 恒为「无」。
struct RefineTechnique {
    int         id = 0;
    std::string name;
    std::string desc;
    bool        canon = false;      // 是否原著可核验
    std::string source;
};

// 手法总表（工程设定；id 0 为「无」，恒可用、无需学习）
const std::vector<RefineTechnique>& allRefineTechniques();
const RefineTechnique* refineTechnique(int id);

// ---------------------------------------------------------------- 阶段
enum class RefineStage : std::uint8_t {
    Idle      = 0,   // 未开始
    Preparing,       // 人物原地等待（2 秒）
    Setup,           // 选材料与手法
    Refining,        // 炼制中
    Finished,        // 已完成（等待界面关闭后归零）
};

inline const char* to_string(RefineStage s) {
    switch (s) {
        case RefineStage::Idle:      return "未开始";
        case RefineStage::Preparing: return "准备中";
        case RefineStage::Setup:     return "备料";
        case RefineStage::Refining:  return "炼制中";
        case RefineStage::Finished:  return "已完成";
    }
    return "？";
}

// ---------------------------------------------------------------- 一次炼制
struct RefineSession {
    // —— 配置（Setup 阶段由玩家填）——
    RecipeId            recipeId = 0;
    std::string         recipeName;
    // 已投入的原材料：背包中的蛊【实例 id】。
    //  必须存实例而非模板 —— 有些方子要「两只同一种蛊」，
    //  若按模板记就分不出是一只还是两只，材料齐备性会判错。
    std::vector<GuId>   materialInst;
    int                 techniqueId = 0;      // 0 = 无（默认）

    // —— 状态 ——
    RefineStage         stage = RefineStage::Idle;
    double              waitRemaining = 0.0;  // Preparing 剩余秒数

    // —— 炼制计时（单位为世界刻度 Tick）——
    Tick                baseTicks   = 0;      // 手法/蛊方决定的总时长
    Tick                remainTicks = 0;
    Tick                cooldownTicks = 0;    // 冷却：已炼时间不足此值则不可终止
    bool                stable = true;        // 当前是否稳定态
    bool                terminated = false;   // 是否为「提前收炉」而非自然成蛊
    //  本炉是否已结算过。
    //  必须记录 —— 世界每推进一次都会扫到已完成的炉，
    //  若无此标记就会被反复收蛊（一炉变十炉、材料持续被扣）。
    bool                harvested  = false;
    bool                timeGuUsed = false;   // 是否已投入光阴蛊
    double              speedMul = 1.0;       // 光阴蛊加成后的速率

    // —— 结果 ——
    RefineOutcome       outcome = RefineOutcome::Blocked;
    std::string         productName;
    std::string         detail;

    // —— 派生 ——
    bool    active() const { return stage != RefineStage::Idle &&
                                    stage != RefineStage::Finished; }
    bool    running() const { return stage == RefineStage::Refining; }
    Tick    elapsed() const { return baseTicks - remainTicks; }
    // 已炼时间小于冷却则不可终止
    bool    canTerminate() const { return running() && elapsed() >= cooldownTicks; }
    double  progress() const {
        return baseTicks <= 0 ? 0.0
             : std::clamp(1.0 - static_cast<double>(remainTicks) /
                                static_cast<double>(baseTicks), 0.0, 1.0);
    }
};

// ---------------------------------------------------------------- 炼蛊台
//
//  一心多用：同时开多个槽位炼多只蛊，可在槽位间切换查看。
//  魂魄强度不足时须借助「一心多用」系列蛊。
class RefineBench {
public:
    static constexpr int kMaxSlots = 4;

    // —— 开启 / 关闭 ——
    //  start：进入 Preparing（人物原地等待 2 秒）
    void open(RecipeId recipe, const std::string& name);
    void close(int slot = -1);          // -1 = 关闭当前槽
    void closeAll() { slots_.clear(); curSlot_ = 0; }

    bool     isOpen() const { return !slots_.empty(); }
    int      slotCount() const { return static_cast<int>(slots_.size()); }
    int      curSlot() const { return curSlot_; }
    void     setCurSlot(int i);
    RefineSession* cur();
    const RefineSession* cur() const;
    std::vector<RefineSession>& slots() { return slots_; }
    const std::vector<RefineSession>& slots() const { return slots_; }

    // —— Setup 阶段操作 ——
    //  选择/放弃一件原材料：返回 true 表示本次为「选中」，false 表示「放弃」
    bool   toggleMaterial(int slot, GuId instId);
    bool   hasMaterial(int slot, GuId instId) const;
    void   setTechnique(int slot, int techniqueId);

    // —— 炼制 ——
    //  begin：确定 —— 校验通过后进入 Refining 并计总时长。
    //  selTplIds：已选实例各自对应的模板 id（可重复，用于齐备性判定）
    bool   begin(int slot, const GuRecipe& recipe,
                 const std::vector<GuId>& selTplIds);

    //  applyTimeGu：投入光阴蛊加速（每只蛊只能用一次）
    bool   applyTimeGu(int slot);

    //  tickPrepare：按【真实秒】推进「原地等待」阶段。
    //  需求写的是「预留 2s」，那是实际观感上的 2 秒，不是世界刻度，
    //  故与世界推进分开计时 —— 否则玩家不推进世界就永远等不到开炉。
    void   tickPrepare(double dtSeconds, std::vector<std::string>& notes);

    //  tick：推进 dtTicks 个【世界刻度】。剩余时间以世界时间计。
    void   tick(Tick dtTicks, std::vector<std::string>& notes);

    //  terminate：终止当前炼制。稳定态出半成品，不稳定则失败。
    //  冷却未过或已炼时间不足时拒绝。
    bool   terminate(int slot, std::string& why);

    //  一心多用：魂魄强度不足时须有对应的一心N用蛊
    //  soulStrength：魂魄强度；hasMultiGu：是否持有对应的一心N用蛊
    static bool canMultiTask(int soulStrength, int currentSlots, bool hasMultiGu);

    // 开一个新槽（一心多用）。成功返回槽号，失败返回 -1。
    int    openSlot(RecipeId recipe, const std::string& name,
                    int soulStrength, bool hasMultiGu);

private:
    std::vector<RefineSession> slots_;
    int curSlot_ = 0;
};

// ---------------------------------------------------------------- 配置
//  覆盖层透明度等显示设置（需求：可在「设置—显示设置」里调节）
struct DisplaySettings {
    float refineOverlayAlpha = 0.72f;   // 炼蛊覆盖层灰度不透明度 0~1
};

} // namespace gr
