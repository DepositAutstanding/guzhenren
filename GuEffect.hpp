// ============================================================================
//  蛊虫攻击特效：交互预留
//
//  现阶段只定义【数据结构与播放队列】，不接入真实图片／粒子资源。
//  如此，日后接美术资源时只需替换绘制分支（spriteReady 为真走贴图），
//  战斗结算与 UI 布局都不必改动。
//
//  设计要点：
//    · 特效由【蛊虫 / 杀招】触发，而非凭空生成 —— 故 req 带 guName 与流派
//    · 起止坐标以【格子】为单位，UI 自行换算屏幕坐标
//    · 播放按时间推进（duration），过期自动出队
// ============================================================================
#pragma once

#include "gr/core/Types.hpp"

#include <string>
#include <vector>

namespace gr {

enum class GuEffectKind : std::uint8_t {
    Beam = 0,       // 光束：月芒、剑气一类
    Burst,          // 爆裂：自伤式、炉炸一类
    Projectile,     // 飞行物：飞刃、箭蛊一类
    Mist,           // 雾气：瘴气、毒道一类
    Shockwave,      // 冲击波：力道、巨力一类
    Aura,           // 光环：护体、增益一类
};

inline const char* to_string(GuEffectKind k) {
    switch (k) {
        case GuEffectKind::Beam:       return "光束";
        case GuEffectKind::Burst:      return "爆裂";
        case GuEffectKind::Projectile: return "飞行物";
        case GuEffectKind::Mist:       return "雾气";
        case GuEffectKind::Shockwave:  return "冲击波";
        case GuEffectKind::Aura:       return "光环";
    }
    return "特效";
}

// 一次攻击所请求的特效
struct GuEffectRequest {
    GuEffectKind kind = GuEffectKind::Beam;
    std::string  guName;        // 触发的蛊虫 / 杀招名
    Dao          dao = Dao::Refine;
    int   fromX = 0, fromY = 0; // 起点（格）
    int   toX   = 0, toY   = 0; // 终点（格）
    float intensity = 1.0f;     // 强度，影响尺寸与不透明度
    std::string spriteId;       // 预留：贴图资源 id（为空则用几何示意）
};

// 正在播放的特效实例
struct GuEffectPlayback {
    GuEffectRequest req;
    float elapsed  = 0.0f;
    float duration = 0.7f;      // 秒
    // 归一化进度 0~1；用于淡出与扩散
    float progress() const {
        return duration > 0.f ? (elapsed / duration) : 1.f;
    }
};

class GuEffectSystem {
public:
    void emit(const GuEffectRequest& r) {
        GuEffectPlayback p;
        p.req = r;
        p.duration = (r.kind == GuEffectKind::Aura) ? 1.2f : 0.7f;
        queue_.push_back(p);
    }

    // 推进时间，剔除已播完的
    void update(float dt) {
        for (auto& p : queue_) p.elapsed += dt;
        std::vector<GuEffectPlayback> keep;
        for (const auto& p : queue_)
            if (p.elapsed < p.duration) keep.push_back(p);
        queue_.swap(keep);
    }

    const std::vector<GuEffectPlayback>& active() const { return queue_; }
    bool empty() const { return queue_.empty(); }
    void clear() { queue_.clear(); }

    // 预留：贴图资源是否已就位。现阶段恒为 false，一律走几何示意。
    bool spriteReady(const std::string& id) const {
        (void)id;
        return false;
    }

private:
    std::vector<GuEffectPlayback> queue_;
};

} // namespace gr
