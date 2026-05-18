#pragma once

#include <memory>
#include <string>
#include <vector>

#include "SpektraParameters.h"

namespace spektrafilm {

struct MetalPassDiagnostics {
  std::string name;
  double gpuMs = 0.0;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t depth = 1;
  uint32_t threadgroupWidth = 0;
  uint32_t threadgroupHeight = 0;
  uint64_t estimatedBytes = 0;
  bool gpuTimeAvailable = false;
};

struct MetalRenderDiagnostics {
  double cpuSetupMs = 0.0;
  double sourceCopyMs = 0.0;
  double commandBufferMs = 0.0;
  double outputCopyMs = 0.0;
  uint64_t staticAllocationBytes = 0;
  uint64_t staticAllocationCount = 0;
  uint64_t scratchAllocationBytes = 0;
  uint64_t scratchAllocationCount = 0;
  uint64_t sharedScratchAllocationBytes = 0;
  uint64_t sharedScratchAllocationCount = 0;
  uint64_t privateScratchAllocationBytes = 0;
  uint64_t privateScratchAllocationCount = 0;
  uint64_t uploadBytes = 0;
  uint32_t passCount = 0;
  bool sourceNoCopy = false;
  bool destinationNoCopy = false;
  bool passGpuTimingEnabled = false;
  bool passGpuTimingAvailable = false;
  bool privateScratchEnabled = false;
  bool renderSerialized = false;
  bool halationPath = false;
  bool cameraDiffusionPath = false;
  bool printDiffusionPath = false;
  bool dirPath = false;
  bool productionGrainPath = false;
  bool grainSynthesisPath = false;
  bool finalPostProcessPath = false;
  bool scannerTextureIntermediates = false;
  uint32_t diffusionGroupSize = 1;
  std::string threadgroupMode;
  std::string passTimingMode;
  std::vector<MetalPassDiagnostics> passes;
};

class MetalRenderer {
public:
  MetalRenderer();
  ~MetalRenderer();

  MetalRenderer(const MetalRenderer &) = delete;
  MetalRenderer &operator=(const MetalRenderer &) = delete;

  bool isAvailable() const;
  const std::string &lastError() const;
  const MetalRenderDiagnostics &lastDiagnostics() const;

  bool render(
    const ImageView &source,
    const MutableImageView &destination,
    const RenderWindow &window,
    const RenderParams &params,
    double time
  );

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace spektrafilm
