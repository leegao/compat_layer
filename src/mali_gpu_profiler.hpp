#ifndef MALI_GPU_PROFILER_HPP
#define MALI_GPU_PROFILER_HPP

#ifdef HAS_LIBGPUCOUNTERS
#include <hwcpipe/hwcpipe.hpp>
#endif

#include <string_view>

class MaliGpuProfiler {
  public:
    MaliGpuProfiler() { Initialize(); }
    void Start();
    void StopAndProcess(const std::string_view shaderLabel);

  private:
    void Initialize();
#ifdef HAS_LIBGPUCOUNTERS
    std::mutex mtx;
    bool started = false;
    std::unique_ptr<hwcpipe::sampler_config> config;
    std::unique_ptr<hwcpipe::sampler<>> sampler;
    int execution_engines;
#endif
};

MaliGpuProfiler &get_mali_gpu_profiler();

#endif // MALI_GPU_PROFILER_HPP
