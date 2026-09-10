#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <map>
#include <string>

#define PROFILE_CONCAT_IMPL(a, b) a##b
#define PROFILE_CONCAT(a, b) PROFILE_CONCAT_IMPL(a, b)

#if LOG_RENDER_PROFILE
namespace RenderProfile
{
constexpr size_t Window = 120;
struct Metric
{
    std::array<double, Window> values{};
    double current = 0;
    unsigned long long calls = 0;
    bool counter = false;
};
struct State
{
    std::map<std::string, Metric> metrics;
    unsigned long long frame = 0;
    size_t samples = 0;
    bool active = false;
};
inline State& GetState() { static thread_local State state; return state; }
inline Metric& GetMetric(const char* name, bool counter = false)
{
    auto& metric = GetState().metrics[name];
    metric.counter = counter;
    return metric;
}
inline void Add(Metric& metric, double value)
{
    if (!GetState().active) return;
    metric.current += value;
    ++metric.calls;
}
class Scope
{
    using Clock = std::chrono::steady_clock;
    Metric& metric;
    Clock::time_point begin;
public:
    explicit Scope(Metric& value) : metric(value), begin(Clock::now()) {}
    ~Scope() { Add(metric, std::chrono::duration<double, std::milli>(Clock::now() - begin).count()); }
};
class Frame
{
public:
    Frame()
    {
        auto& state = GetState();
        ++state.frame;
        state.active = true;
    }
    ~Frame()
    {
        auto& state = GetState();
        state.active = false;
        for (auto& item : state.metrics)
        {
            item.second.values[state.samples] = item.second.current;
            item.second.current = 0;
        }
        if (++state.samples != Window) return;
        std::string output = "[RenderProfileBatch] CPU inclusive; frames=" +
            std::to_string(state.frame - Window + 1) + ".." + std::to_string(state.frame) + "\n";
        for (auto& item : state.metrics)
        {
            auto& metric = item.second;
            if (!metric.calls) continue;
            auto sorted = metric.values;
            std::sort(sorted.begin(), sorted.end());
            double sum = 0;
            for (double value : sorted) sum += value;
            char line[512];
            sprintf_s(line, "[RenderProfileBatch] %s | %s/frame avg=%.4f p99=%.4f max=%.4f calls/frame=%.2f\n",
                item.first.c_str(), metric.counter ? "count" : "ms", sum / Window,
                sorted[118], sorted.back(), static_cast<double>(metric.calls) / Window);
            output += line;
            metric.calls = 0;
            metric.values.fill(0);
        }
        OutputDebugStringA(output.c_str());
        state.samples = 0;
    }
};
}
#define PROFILE_RENDER_SCOPE(name) \
    static thread_local auto& PROFILE_CONCAT(_profileMetric_, __LINE__) = RenderProfile::GetMetric(name); \
    RenderProfile::Scope PROFILE_CONCAT(_profileScope_, __LINE__)(PROFILE_CONCAT(_profileMetric_, __LINE__))
#define PROFILE_RENDER_COUNT(name, value) do { \
    static thread_local auto& metric = RenderProfile::GetMetric(name, true); \
    RenderProfile::Add(metric, static_cast<double>(value)); } while (false)
#define PROFILE_RENDER_FRAME() RenderProfile::Frame profileFrame
#else
#define PROFILE_RENDER_SCOPE(name) ((void)0)
#define PROFILE_RENDER_COUNT(name, value) ((void)0)
#define PROFILE_RENDER_FRAME() ((void)0)
#endif
