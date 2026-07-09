#include "logger.hpp"
#include "mali_gpu_profiler.hpp"
#include <cstdint>
#include <mutex>

#ifdef HAS_LIBGPUCOUNTERS
#include <device/product_id.hpp>
#include <hwcpipe/counter_database.hpp>
#include <hwcpipe/hwcpipe.hpp>
#include <hwcpipe/hwcpipe_counter.h>
#endif

MaliGpuProfiler &get_mali_gpu_profiler() {
    static MaliGpuProfiler instance;
    return instance;
}

void MaliGpuProfiler::Initialize() {
#ifdef HAS_LIBGPUCOUNTERS
    if (sampler)
        return;

    auto gpu = hwcpipe::gpu(0); // mali0
    if (!gpu || !gpu.valid()) {
        Logger::log("error", "/dev/mali0 not found");
        return;
    }

    Logger::log("info", "GPU Counters requested for /dev/mali0");
    Logger::log("info", "    Product Family:    %d", gpu.get_gpu_family());
    Logger::log("info", "    Product ID:        %d", gpu.get_product_id());
    Logger::log("info", "    Shader Cores:      %d", gpu.num_shader_cores());
    Logger::log("info", "    Execution Engines: %d",
                gpu.num_execution_engines());
    Logger::log("info", "    Bus Width:         %d", gpu.bus_width());
    execution_engines = gpu.num_execution_engines();

    auto counter_db = hwcpipe::counter_database{};
    hwcpipe::counter_metadata meta;
    for (hwcpipe_counter counter : counter_db.counters_for_gpu(gpu)) {
        auto ec = counter_db.describe_counter(counter, meta);
        if (ec) {
            Logger::log("error", "describe_counter failed: %s",
                        ec.message().c_str());
            continue;
        }
    }

    config = std::make_unique<hwcpipe::sampler_config>(gpu);
#define ADD_COUNTER(counter)                                                   \
    {                                                                          \
        auto error = config->add_counter(counter);                             \
        if (error) {                                                           \
            Logger::log("error", "Counter " #counter " not supported: %s",     \
                        error.message().c_str());                              \
        }                                                                      \
    }

    ADD_COUNTER(MaliEngFMAPipeUtil);
    ADD_COUNTER(MaliEngCVTPipeUtil);
    ADD_COUNTER(MaliEngSFUPipeUtil);
    ADD_COUNTER(MaliLSUtil);
    ADD_COUNTER(MaliCoreActiveCy);
    ADD_COUNTER(MaliGPUActiveCy);
    ADD_COUNTER(MaliCompQueueActiveCy);
    ADD_COUNTER(MaliEngStarveCy);
    ADD_COUNTER(MaliCoreAllRegsWarpRate);
    ADD_COUNTER(MaliCoreFullWarpRate);
    ADD_COUNTER(MaliNonFragWarp);
    ADD_COUNTER(MaliFragWarp);
    ADD_COUNTER(MaliCoreFullWarp);
    ADD_COUNTER(MaliCoreAllRegsWarp);
    ADD_COUNTER(MaliL2CacheRdMissRate);
    ADD_COUNTER(MaliSCBusLSL2RdBy);
    ADD_COUNTER(MaliSCBusLSExtRdBy);
    ADD_COUNTER(MaliL2CacheRdStallCy);
    ADD_COUNTER(MaliExtBusRdLat0);
    ADD_COUNTER(MaliExtBusRdLat128);
    ADD_COUNTER(MaliExtBusRdLat192);
    ADD_COUNTER(MaliExtBusRdLat256);
    ADD_COUNTER(MaliExtBusRdLat320);
    ADD_COUNTER(MaliExtBusRdLat384);
    ADD_COUNTER(MaliExtBusRdStallRate);
    ADD_COUNTER(MaliEngNarrowInstrRate);
    ADD_COUNTER(MaliEngDivergedInstrRate);
    ADD_COUNTER(MaliEngICacheMiss);
    ADD_COUNTER(MaliExtBusRdBy);
    ADD_COUNTER(MaliExtBusRdOTQ1);
    ADD_COUNTER(MaliExtBusRdOTQ2);
    ADD_COUNTER(MaliExtBusRdOTQ3);
    ADD_COUNTER(MaliExtBusRdOTQ4);

    sampler = std::make_unique<hwcpipe::sampler<>>(*config);
#endif
}

void MaliGpuProfiler::Start() {
#ifdef HAS_LIBGPUCOUNTERS
    std::lock_guard<std::mutex> lock(mtx);
    if (!sampler)
        return;
    if (started)
        return;
    auto error = sampler->start_sampling();
    if (error) {
        Logger::log("error", "start_sampling failed: %s",
                    error.message().c_str());
        return;
    }
    started = true;
#endif
}

void MaliGpuProfiler::StopAndProcess(std::string_view shaderLabel) {
#ifdef HAS_LIBGPUCOUNTERS
    std::lock_guard<std::mutex> lock(mtx);
    if (!sampler)
        return;
    if (!started)
        return;

    auto error = sampler->sample_now();
    if (error) {
        Logger::log("error", "sample_now failed: %s", error.message().c_str());
    }

    error = sampler->stop_sampling();
    if (error) {
        Logger::log("error", "stop_sampling failed: %s",
                    error.message().c_str());
        return;
    }
    started = false;

    hwcpipe::counter_sample sample;
    auto get_value_uint64 = [&](hwcpipe_counter id,
                                std::string_view label) -> uint64_t {
        auto error = sampler->get_counter_value(id, sample);
        if (error == std::error_code{}) {
            // Logger::log("info", "get_counter_value<u64>(%s) = %lu",
            // label.data(), sample.value.uint64);
            if (sample.type == hwcpipe::counter_sample::type::uint64) {
                return sample.value.uint64;
            }
            return static_cast<uint64_t>(sample.value.float64);
        }
        Logger::log("error", "get_counter_value<u64>(%s) error: %s",
                    label.data(), error.message().c_str());
        return 0;
    };
#define GET_VALUE_U64(id) get_value_uint64(id, #id)

    auto get_value_fp64 = [&](hwcpipe_counter id,
                              std::string_view label) -> double {
        auto error = sampler->get_counter_value(id, sample);
        if (error == std::error_code{}) {
            // Logger::log("info", "get_counter_value<fp64>(%s) = %f",
            // label.data(), sample.value.float64);
            if (sample.type == hwcpipe::counter_sample::type::uint64) {
                return static_cast<double>(sample.value.uint64);
            }
            return sample.value.float64;
        }
        Logger::log("error", "get_counter_value<fp64>(%s) error: %s",
                    label.data(), error.message().c_str());
        return 0;
    };
#define GET_VALUE_FP64(id) get_value_fp64(id, #id)

    double fma_util = GET_VALUE_FP64(MaliEngFMAPipeUtil);
    double cvt_util = GET_VALUE_FP64(MaliEngCVTPipeUtil);
    double sfu_util = GET_VALUE_FP64(MaliEngSFUPipeUtil);
    double ls_util = GET_VALUE_FP64(MaliLSUtil);

    uint64_t active_cy = GET_VALUE_U64(MaliCoreActiveCy);
    double gpu_cy = GET_VALUE_FP64(MaliGPUActiveCy);
    double comp_cy = GET_VALUE_FP64(MaliCompQueueActiveCy);
    uint64_t starvation_cy = GET_VALUE_U64(MaliEngStarveCy);

    double reg_pressure_pct = GET_VALUE_FP64(MaliCoreAllRegsWarpRate);
    double full_warp_pct = GET_VALUE_FP64(MaliCoreFullWarpRate);
    uint64_t high_reg_warps = GET_VALUE_U64(MaliCoreAllRegsWarp);
    uint64_t non_frag_warps = GET_VALUE_U64(MaliNonFragWarp);
    uint64_t frag_warps = GET_VALUE_U64(MaliFragWarp);
    uint64_t full_warps = GET_VALUE_U64(MaliCoreFullWarp);

    double l2_rd_miss_pct = GET_VALUE_FP64(MaliL2CacheRdMissRate);
    double ls_l2_bytes = GET_VALUE_FP64(MaliSCBusLSL2RdBy);
    double ls_ext_bytes = GET_VALUE_FP64(MaliSCBusLSExtRdBy);
    uint64_t internal_stalls = GET_VALUE_U64(MaliL2CacheRdStallCy);

    double total_rd_bytes = static_cast<double>(ls_l2_bytes + ls_ext_bytes);
    double cache_locality =
        (total_rd_bytes > 0.0)
            ? (static_cast<double>(ls_l2_bytes) / total_rd_bytes) * 100.0
            : 0.0;
    uint64_t ext_rd_tx = GET_VALUE_U64(MaliExtBusRd);
    uint64_t ext_rd_beats = GET_VALUE_U64(MaliExtBusRdBt);
    double ext_rd_bytes = GET_VALUE_FP64(MaliExtBusRdBy);

    double avg_burst_len =
        (ext_rd_tx > 0) ? static_cast<double>(ext_rd_beats) / ext_rd_tx : 0.0;
    // uint64_t lat_0_127 = GET_VALUE_U64(MaliExtBusRdLat0);
    // uint64_t lat_128_191 = GET_VALUE_U64(MaliExtBusRdLat128);
    // uint64_t lat_192_255 = GET_VALUE_U64(MaliExtBusRdLat192);
    // uint64_t lat_256_319 = GET_VALUE_U64(MaliExtBusRdLat256);
    // uint64_t lat_320_383 = GET_VALUE_U64(MaliExtBusRdLat320);
    double lat_384_plus = GET_VALUE_FP64(MaliExtBusRdLat384);

    uint64_t otq_0_25 = GET_VALUE_U64(MaliExtBusRdOTQ1);
    uint64_t otq_25_50 = GET_VALUE_U64(MaliExtBusRdOTQ2);
    uint64_t otq_50_75 = GET_VALUE_U64(MaliExtBusRdOTQ3);
    double otq_75_100 = GET_VALUE_FP64(MaliExtBusRdOTQ4);

    uint64_t ext_rd_stalls = GET_VALUE_U64(MaliExtBusRdStallCy);
    double ext_rd_stall_pct = GET_VALUE_FP64(MaliExtBusRdStallRate);

    double fp16_math_pct = GET_VALUE_FP64(MaliEngNarrowInstrRate);
    double warp_divergence = GET_VALUE_FP64(MaliEngDivergedInstrRate);
    uint64_t icache_misses = GET_VALUE_U64(MaliEngICacheMiss);

    double comp_purity =
        (gpu_cy > 0) ? (static_cast<double>(comp_cy) / gpu_cy) * 100.0 : 0.0;

    if (active_cy == 0) {
        Logger::log("info", "No active cycles logged");
        return;
    }

    Logger::log("info", "--- Profile: %s ---", shaderLabel.data());
    Logger::log("info", "  Compute Purity:       %.2f%%", comp_purity);
    Logger::log("info", "  ALU FMA Pipe Util:    %.2f%%", fma_util);
    Logger::log("info", "  ALU CVT Pipe Util:    %.2f%%", cvt_util);
    Logger::log("info", "  ALU SFU Pipe Util:    %.2f%%", sfu_util);
    Logger::log("info", "  Load/Store Unit Util: %.2f%%", ls_util);
    Logger::log("info", "  Compute Queue Cycles: %.0f cycles (%llu warps)",
                comp_cy, non_frag_warps);
    Logger::log("info", "  GPU Active Cycles:    %.0f cycles (%llu warps)",
                gpu_cy, non_frag_warps + frag_warps);

    Logger::log("info", "  --- Shader Stats: %s ---", shaderLabel.data());
    Logger::log("info", "    >32 Registers:              %.2f%% (%llu warps)",
                reg_pressure_pct, high_reg_warps);
    Logger::log("info", "    Full Warp Grid Execution:   %.2f%% (%llu warps)",
                full_warp_pct, full_warps);
    Logger::log("info", "    Warp Branch Divergence:     %.2f%%",
                warp_divergence);
    Logger::log("info", "    16-bit Math Density:        %.2f%%",
                fp16_math_pct);
    Logger::log("info", "    Instruction Cache Misses:   %llu", icache_misses);
    Logger::log("info", "    ALU Engine Starvation Rate: %.2f%%",
                static_cast<double>(starvation_cy) / active_cy /
                    execution_engines * 100.0);

    Logger::log("info", "  --- L2 Cache: %s ---", shaderLabel.data());
    Logger::log("info", "    L2 Cache Read Miss Rate:    %.2f%%",
                l2_rd_miss_pct);
    Logger::log("info", "    L2 Cache Read Locality:     %.2f%%",
                cache_locality);
    Logger::log("info", "    Internal Read Stall:        %.2f%% (%llu cycles)",
                static_cast<double>(internal_stalls) / active_cy * 100.0,
                internal_stalls);

    Logger::log("info", "  --- External Reads Profile: %s ---",
                shaderLabel.data());
    Logger::log("info", "    Read Transactions (Packets): %llu", ext_rd_tx);
    Logger::log("info", "    Read Beats (Bus Bursts):    %llu", ext_rd_beats);
    Logger::log("info", "    Read Payload Data Vol:      %llu bytes",
                static_cast<uint64_t>(ext_rd_bytes));
    Logger::log("info", "    Avg Burst Length:           %.2f bytes/tx",
                4 * avg_burst_len);
    Logger::log("info", "    External Read Stall Rate:   %.2f%% (%llu cycles)",
                gpu_cy > 0 ? static_cast<double>(ext_rd_stalls) / gpu_cy * 100.0
                           : 0.0,
                ext_rd_stalls);

    // Logger::log("info", "  --- External Read Beats (16 bytes) Latency: %s
    // ---", shaderLabel.data()); Logger::log("info", "    [000 - 127 cy]: %
    // .2f%% (%llu beats)", lat_0_127 / static_cast<double>(ext_rd_beats) *
    // 100.0, lat_0_127); Logger::log("info", "    [128 - 191 cy]:            %
    // .2f%% (%llu beats)", lat_128_191 / static_cast<double>(ext_rd_beats) *
    // 100.0, lat_128_191); Logger::log("info", "    [192 - 255 cy]: % .2f%%
    // (%llu beats)", lat_192_255 / static_cast<double>(ext_rd_beats) * 100.0,
    // lat_192_255); Logger::log("info", "    [256 - 319 cy]:            % .2f%%
    // (%llu beats)", lat_256_319 / static_cast<double>(ext_rd_beats) * 100.0,
    // lat_256_319); Logger::log("info", "    [320 - 383 cy]:            % .2f%%
    // (%llu beats)", lat_320_383 / static_cast<double>(ext_rd_beats) * 100.0,
    // lat_320_383);
    Logger::log("info", "    Slow External Reads (16b): %.2f%% (%.0f beats)",
                lat_384_plus / static_cast<double>(ext_rd_beats) * 100.0,
                lat_384_plus);

    if (ext_rd_tx > 0) {
        Logger::log("info",
                    "  --- External Read Transaction Queue Load: %s ---",
                    shaderLabel.data());
        Logger::log(
            "info", "    Queue Load [ 0%% -  25%%]:    %.2f%% (%llu txs)",
            otq_0_25 / static_cast<double>(ext_rd_tx) * 100.0, otq_0_25);
        Logger::log(
            "info", "    Queue Load [25%% -  50%%]:    %.2f%% (%llu txs)",
            otq_25_50 / static_cast<double>(ext_rd_tx) * 100.0, otq_25_50);
        Logger::log(
            "info", "    Queue Load [50%% -  75%%]:    %.2f%% (%llu txs)",
            otq_50_75 / static_cast<double>(ext_rd_tx) * 100.0, otq_50_75);
        Logger::log(
            "info", "    Queue Load [75%% - 100%%]:    %.2f%% (%.0f txs)",
            otq_75_100 / static_cast<double>(ext_rd_tx) * 100.0, otq_75_100);
    }

    sampler =
        std::make_unique<hwcpipe::sampler<>>(*config); // Reset the sampler
#endif
}
