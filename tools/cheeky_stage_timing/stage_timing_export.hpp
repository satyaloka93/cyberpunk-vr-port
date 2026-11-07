#pragma once
#include "diagnostics.hpp"
#include <cstdio>
#include <cstdint>

namespace cheeky::foveated_dlss {
// Cumulative sampled GPU durations. Delta totals/counts between log records;
// categories are independent samples, NOT matched eyes or additive frame costs.
// Pipeline categories contain child operations. No GPU work or sampling changes.
template <typename Emit>
void export_stage_timings(unsigned long pid, long long qpc, long long frequency, Emit emit) noexcept {
    struct Entry { const char* name; DiagnosticGpuTiming metric; };
    static constexpr Entry stages[]{
        {"native_sr", DiagnosticGpuTiming::d3d12_native_dlss},
        {"foveated_sr", DiagnosticGpuTiming::d3d12_foveated_dlss},
        {"nr_after_full", DiagnosticGpuTiming::d3d12_full_dlss_nr},
        {"nr_after_foveated", DiagnosticGpuTiming::d3d12_foveated_dlss_nr},
        {"nr_before_full", DiagnosticGpuTiming::d3d12_before_full_nr},
        {"nr_before_foveated", DiagnosticGpuTiming::d3d12_before_foveated_nr},
        {"peripheral_dlaa", DiagnosticGpuTiming::d3d12_peripheral_dlaa},
        {"pipeline_before", DiagnosticGpuTiming::d3d12_before_pipeline},
        {"pipeline_after", DiagnosticGpuTiming::d3d12_after_pipeline},
    };
    char line[2048]{};
    int used = std::snprintf(line, sizeof(line),
        "GPU stages v1 pid=%lu qpc=%lld frequency=%lld", pid, qpc, frequency);
    if (used < 0 || static_cast<std::size_t>(used) >= sizeof(line)) return;
    for (const auto& stage : stages) {
        const auto total = diagnostic_timing_totals(stage.metric);
        const auto remaining = sizeof(line) - static_cast<std::size_t>(used);
        const int count = std::snprintf(line + used, remaining, " %s=%.6f,%llu",
            stage.name, total.milliseconds, static_cast<unsigned long long>(total.samples));
        if (count < 0 || static_cast<std::size_t>(count) >= remaining) return;
        used += count;
    }
    emit("%s", line);
}
} // namespace cheeky::foveated_dlss
