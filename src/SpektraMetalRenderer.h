#pragma once

#include <memory>
#include <string>

#include "SpektraRenderer.h"

namespace spektrafilm {

using MetalPassDiagnostics = RendererPassDiagnostics;
using MetalRenderDiagnostics = RendererDiagnostics;

class MetalRenderer final : public Renderer {
public:
  MetalRenderer();
  ~MetalRenderer() override;

  MetalRenderer(const MetalRenderer &) = delete;
  MetalRenderer &operator=(const MetalRenderer &) = delete;

  bool isAvailable() const override;
  const MetalRenderDiagnostics &lastDiagnostics() const override;
  const std::string &lastError() const override;

  bool render(
    const ImageView &source,
    const MutableImageView &destination,
    const RenderWindow &window,
    const RenderParams &params,
    double time
  ) override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace spektrafilm
