#include "PerfStats.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "../../logger.h"

namespace PerfStats
{
    namespace
    {
        constexpr double kSlowMs      = 4.0;
        constexpr double kLogMs       = 8.0;
        constexpr auto   kLogInterval = std::chrono::seconds(30);

        struct Stat
        {
            std::uint64_t                         calls = 0;
            std::uint64_t                         slowCalls = 0;
            double                                totalMs = 0.0;
            double                                maxMs = 0.0;
            double                                lastMs = 0.0;
            std::chrono::steady_clock::time_point lastLog{};
        };

        std::mutex                            s_mutex;
        std::unordered_map<std::string, Stat> s_stats;
        std::chrono::steady_clock::time_point s_since = std::chrono::steady_clock::now();
    }

    void Record(const std::string& a_key, double a_ms)
    {
        bool log = false;
        {
            std::scoped_lock lock(s_mutex);
            auto& s = s_stats[a_key];
            ++s.calls;
            s.totalMs += a_ms;
            s.lastMs = a_ms;
            s.maxMs  = std::max(s.maxMs, a_ms);
            if (a_ms > kSlowMs)
                ++s.slowCalls;
            if (a_ms > kLogMs) {
                const auto now = std::chrono::steady_clock::now();
                if (now - s.lastLog >= kLogInterval) {
                    s.lastLog = now;
                    log       = true;
                }
            }
        }
        if (log)
            logger::warn("[Perf] '{}' took {:.1f} ms on the game thread", a_key, a_ms);
    }

    nlohmann::json Read()
    {
        struct Row
        {
            std::string key;
            Stat        stat;
        };
        std::vector<Row> rows;
        double           since = 0.0;
        {
            std::scoped_lock lock(s_mutex);
            rows.reserve(s_stats.size());
            for (const auto& [key, stat] : s_stats)
                rows.push_back({ key, stat });
            since = std::chrono::duration<double>(std::chrono::steady_clock::now() - s_since).count();
        }
        std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.stat.maxMs > b.stat.maxMs; });

        nlohmann::json entries = nlohmann::json::array();
        for (const auto& r : rows) {
            // Round to 0.01 ms: enough precision, smaller JSON.
            auto round2 = [](double v) { return std::round(v * 100.0) / 100.0; };
            entries.push_back({
                { "key", r.key },
                { "calls", r.stat.calls },
                { "avgMs", round2(r.stat.calls ? r.stat.totalMs / static_cast<double>(r.stat.calls) : 0.0) },
                { "maxMs", round2(r.stat.maxMs) },
                { "lastMs", round2(r.stat.lastMs) },
                { "slowCalls", r.stat.slowCalls },
            });
        }
        return { { "sinceSeconds", static_cast<std::int64_t>(since) }, { "entries", std::move(entries) } };
    }

    void Reset()
    {
        std::scoped_lock lock(s_mutex);
        s_stats.clear();
        s_since = std::chrono::steady_clock::now();
    }
}
