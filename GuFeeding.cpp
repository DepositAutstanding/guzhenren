// 喂养机制实现
#include "gr/gu/GuFeeding.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <unordered_set>

namespace gr {

// ---------------------------------------------------------------- 状态与功效
FeedState feedStateOf(double fullness, int starveTicks) {
    if (!std::isfinite(fullness)) return FeedState::Starving;  // 不可判定 → 按最差
    if (starveTicks >= kStarveDaysBeforeDeath) return FeedState::Dead;
    if (fullness <= 0.0)    return FeedState::Starving;
    if (fullness < 0.5)     return FeedState::Hungry;
    return FeedState::Full;
}

double efficacyOf(double fullness, int starveTicks) {
    //  NaN 防御：NaN 与任何数比较皆 false，会一路穿透所有 if，
    //  最终作为「功效」返回并污染后续一切计算（杀招威力、产出等）。
    //  此处一律按「最差」处理 —— 不可判定时不可当成可用。
    if (!std::isfinite(fullness)) return 0.0;
    if (starveTicks >= kStarveDaysBeforeDeath) return 0.0;   // 已饿死
    if (fullness <= 0.0) return 0.3;                          // 濒死：只剩三成
    if (fullness >= 0.5) return 1.0;                          // 饱食：无损
    // 0 → 0.3，0.5 → 1.0：线性
    return 0.3 + (fullness / 0.5) * 0.7;
}

// ---------------------------------------------------------------- 食物名解析
namespace {

//  描述串里常见、但不构成食物名的片段
const std::unordered_set<std::string>& noiseWords() {
    static const std::unordered_set<std::string> k = {
        "不详", "天然采集", "天然生成", "无", "-", "未知", "待考",
    };
    return k;
}

//  去掉数量与量词：「每日早晚各2片」→「片」这类片段本身不是食物名，
//  真正的食物名通常出现在分隔符之前。
std::string cleanFragment(std::string s) {
    // 去括号内容
    while (true) {
        const auto a = s.find("\uFF08");       // 全角（
        if (a == std::string::npos) break;
        const auto b = s.find("\uFF09", a);    // 全角）
        if (b == std::string::npos) { s.erase(a); break; }
        s.erase(a, b - a + 1);
    }
    while (true) {
        const auto a = s.find('(');
        if (a == std::string::npos) break;
        const auto b = s.find(')', a);
        if (b == std::string::npos) { s.erase(a); break; }
        s.erase(a, b - a + 1);
    }
    // 去前导数量：「2片」→「片」
    while (!s.empty() && (std::isdigit(static_cast<unsigned char>(s.front())) ||
                          s.front() == ' '))
        s.erase(s.begin());
    // 常见量词收尾直接丢弃
    //  量词碎片不是食物名。中文一律按 UTF-8 子串比较，
    //  不写宽字符字面量（'等' 会触发 -Wmultichar）。
    static const char* const kQty[] = {
        "\u7247", "\u4e24", "\u65e5", "\u575b", "\u5757", "\u9897",
        "\u53ea", "\u4efd", "\u4e9b", "\u70b9", "\u6b21", "\u679a",
    };
    for (const char* q : kQty) if (s == q) return {};
    // 去尾部空格与「等」
    while (!s.empty() && s.back() == ' ') s.pop_back();
    if (s.size() >= 3 && s.compare(s.size() - 3, 3, "\u7b49") == 0)
        s.erase(s.size() - 3);
    return s;
}

} // namespace

std::vector<std::string> parseFeedNames(const std::string& feed) {
    std::vector<std::string> out;
    if (feed.empty()) return out;

    //
    //  关键：先把【全角分隔符整体替换】为半角，再按单字节切分。
    //  若直接在多字节分隔符串里用 find(ch) 逐字节匹配，
    //  中文字符的中间字节会偶然命中分隔符的某个字节
    //  （如「每」的字节序列撞上「，」的字节），把 UTF-8 切坏 ——
    //  实测「每十日吞食二两玉石」被切成 '每?' '日吞食?' 这样的乱码。
    //
    std::string t = feed;
    static const char* const kFullSep[] = {
        "\uFF0C",   // ，
        "\uFF1B",   // ；
        "\u3001",   // 、
        "\u3002",   // 。
        "\uFF1A",   // ：
    };
    for (const char* sep : kFullSep) {
        const std::size_t sl = std::strlen(sep);
        std::size_t pos = 0;
        while ((pos = t.find(sep, pos)) != std::string::npos) {
            t.replace(pos, sl, ",");
            pos += 1;
        }
    }

    // 按常见分隔符切分
    std::string cur;
    auto flush = [&]() {
        const std::string c = cleanFragment(cur);
        cur.clear();
        //  「每十日吞食二两玉石」：真正的食物在「吞食」之后
        {
            const std::string tun = u8"\u541e\u98df";   // 吞食
            auto pos = c.find(tun);
            if (pos != std::string::npos) {
                std::string food = c.substr(pos + tun.size());
                //  去前导数量：阿拉伯数字与中文数字（一二三…十、两）
                static const char* const kCn[] = {
                    u8"\u4e00", u8"\u4e8c", u8"\u4e09", u8"\u56db", u8"\u4e94",
                    u8"\u516d", u8"\u4e03", u8"\u516b", u8"\u4e5d", u8"\u5341",
                    u8"\u4e24", u8"\u767e", u8"\u5343",
                };
                bool strip = true;
                while (strip && !food.empty()) {
                    strip = false;
                    if (std::isdigit(static_cast<unsigned char>(food.front()))) {
                        food.erase(food.begin()); strip = true; continue;
                    }
                    for (const char* d : kCn) {
                        const std::size_t dl = std::strlen(d);
                        if (food.compare(0, dl, d) == 0) {
                            food.erase(0, dl); strip = true; break;
                        }
                    }
                }
                if (food.size() >= 2) {
                    if (std::find(out.begin(), out.end(), food) == out.end())
                        out.push_back(food);
                    return;
                }
            }
        }
        //  「以X为食」：截去「为食」与前导「以」，取出食物名
        {
            const std::string what = u8"\u4e3a\u98df";   // 为食
            auto pos = c.find(what);
            if (pos != std::string::npos) {
                std::string food = c.substr(0, pos);
                const std::string yi = u8"\u4ee5";        // 以
                if (food.compare(0, yi.size(), yi) == 0) food.erase(0, yi.size());
                if (food.size() >= 2 &&
                    std::find(out.begin(), out.end(), food) == out.end())
                    out.push_back(food);
                return;
            }
        }
        //  「每…」是频率说明（每十日、每日），不是食物名
        {
            const std::string mei = u8"\u6bcf";           // 每
            if (c.compare(0, mei.size(), mei) == 0) return;
        }
        if (c.size() < 3) return;                 // 太短，多半是碎片（中文至少 3 字节）
        if (noiseWords().count(c)) return;
        if (c.find(u8"\u7ef4\u6301") != std::string::npos) return;  // 维持
        if (c.find(u8"\u51cf\u5c11") != std::string::npos) return;  // 减少
        if (std::find(out.begin(), out.end(), c) == out.end()) out.push_back(c);
    };
    for (const char ch : t) {
        //  此时只剩 ASCII 分隔符，单字节比较安全
        if (ch == ',' || ch == ';' || ch == '/') {
            flush();
        } else {
            cur += ch;
        }
    }
    flush();
    return out;
}

std::string primaryFeedName(const std::string& feed) {
    const auto names = parseFeedNames(feed);
    if (names.empty()) return {};
    // 「以酒水为食」这类：去掉「以…为食」后取「酒水」
    for (const auto& n : names) {
        std::string s = n;
        if (s.compare(0, 3, u8"\u4ee5") == 0 && s.size() > 3) s = s.substr(3);
        auto pos = s.find(u8"\u4e3a\u98df");
        if (pos != std::string::npos) s = s.substr(0, pos);
        if (s.size() >= 2) return s;
    }
    return names.front();
}

// ---------------------------------------------------------------- 推进
FeedTickReport tickFeeding(std::vector<GuInstance>& gus,
                           const std::vector<GuTemplate>& tpls,
                           int days,
                           const std::vector<GuId>* skipIds) {
    FeedTickReport rep;
    if (days <= 0) return rep;

    for (auto& g : gus) {
        //  炼制中的材料蛊豁免：正在炉里被炼化，不按日常消耗计
        if (skipIds && std::find(skipIds->begin(), skipIds->end(),
                                 g.instanceId) != skipIds->end())
            continue;
        //  饿死即「彻底毁灭」—— 与 GuState::Destroyed 同义：
        //  蛊已不在世，仙蛊的唯一名额随之释放（可重生同名蛊）。
        if (g.state == GuState::Destroyed) continue;

        //
        //  封印 / 封存中的蛊不消耗、也不会饿死。
        //
        //  这不是偷懒：仙蛊被封印时【不清除唯一注册表】，
        //  而饿死会置 Destroyed 从而【释放名额】—— 若封印中的蛊还会饿死，
        //  就等于「封印着也能悄悄死掉并让同名仙蛊重生」，
        //  与仙蛊唯一的铁律直接冲突。封印即静止，不食不耗。
        //
        if (g.state == GuState::Sealed || g.state == GuState::Stored) continue;

        const GuTemplate* t = nullptr;
        for (const auto& x : tpls) if (x.id == g.templateId) { t = &x; break; }
        const Rank rk = t ? t->rank : Rank::R1;

        //  NaN / 越界兜底：饱食度须是 [0,1] 内的有限数。
        //  外部若写入了 NaN 或超界值，先归位再计，
        //  否则 NaN 会一路穿透并在界面上显示成空白或乱码。
        if (!std::isfinite(g.fullness))    g.fullness = 0.0;
        if (g.fullness > 1.0)              g.fullness = 1.0;
        if (g.fullness < 0.0)              g.fullness = 0.0;
        if (g.starveTicks < 0)             g.starveTicks = 0;

        const FeedState before = feedStateOf(g.fullness, g.starveTicks);

        // 饱食度下降
        const double drain = dailyDrain(rk) * static_cast<double>(days) *
                             static_cast<double>(kDaysPerTick);
        g.fullness -= drain;
        if (g.fullness <= 0.0) {
            g.fullness = 0.0;
            g.starveTicks += days * kDaysPerTick;
        } else {
            // 吃饱了，饥饿计数清零（不至于刚喂一口就累积旧账）
            g.starveTicks = 0;
        }

        const FeedState after = feedStateOf(g.fullness, g.starveTicks);

        if (after == FeedState::Dead && before != FeedState::Dead) {
            g.state = GuState::Destroyed;
            rep.starved.push_back(g.instanceId);
        } else if (after == FeedState::Starving && before != FeedState::Starving) {
            rep.becameStarving.push_back(g.instanceId);
        } else if (after == FeedState::Hungry && before != FeedState::Hungry) {
            rep.becameHungry.push_back(g.instanceId);
        }
    }
    return rep;
}

} // namespace gr
