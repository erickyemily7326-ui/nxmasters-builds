#include "dk_gpu.h"
#include "dk_utility_target.h"
#include <iostream>
#include <stdexcept>
#include <unordered_set>

namespace {
struct Binding {
  DkCmdBuf commands;
  uint32_t color_count;
  const DkImageView *color, *depth;
} recorded{};
unsigned checked = 0;
std::unordered_set<const DkImage *> depth_images;
}

extern "C" void dkCmdBufBindRenderTargets(DkCmdBuf commands,
    const DkImageView *const colors[], uint32_t color_count, const DkImageView *depth) {
  if (color_count > DK_MAX_RENDER_TARGETS || (color_count && !colors))
    throw std::invalid_argument("invalid color attachment array");
  for (uint32_t i = 0; i < color_count; ++i)
    if (!colors[i] || !colors[i]->pImage)
      throw std::invalid_argument("view must not be null");
    else if (depth_images.contains(colors[i]->pImage))
      throw std::invalid_argument("attempted to use depth image as color render target");
  if (depth && !depth->pImage) throw std::invalid_argument("depth image must not be null");
  if (depth && !depth_images.contains(depth->pImage))
    throw std::invalid_argument("attempted to use color image as depth render target");
  recorded = {commands, color_count, color_count ? colors[0] : nullptr, depth};
}

int main() {
  try {
    const auto commands = reinterpret_cast<DkCmdBuf>(uintptr_t(0x1000));
    DkImage color_image{}, depth_image{};
    DkImageView color{}, depth{};
    color.pImage = &color_image;
    depth.pImage = &depth_image;
    depth_images.insert(&depth_image);
    struct Pass { const DkImageView *color, *depth; uint32_t color_count; };
    const Pass passes[]{
      {&color, nullptr, 1},
      {nullptr, &depth, 0},
      {&color, &depth, 1},
      {nullptr, &depth, 0},
      {&color, nullptr, 1},
      {nullptr, nullptr, 0},
    };
    for (const auto &pass : passes) {
      nxdk::bind_render_targets(commands, pass.color, pass.depth);
      if (recorded.commands != commands || recorded.color_count != pass.color_count ||
          recorded.color != pass.color || recorded.depth != pass.depth)
        throw std::runtime_error("render-target binding changed the requested attachments");
      ++checked;
    }

    bool rejected = false;
    try { dkCmdBufBindRenderTarget(commands, nullptr, &depth); }
    catch (const std::invalid_argument &) { rejected = true; }
    if (!rejected) throw std::runtime_error("former null color attachment was accepted");
    ++checked;
    bool rejected_depth_color = false;
    try { nxdk::bind_render_targets(commands, &depth, nullptr); }
    catch (const std::invalid_argument &) { rejected_depth_color = true; }
    if (!rejected_depth_color) throw std::runtime_error("logged depth-as-color error was accepted");
    ++checked;

    struct FormatCase { DkImageFormat format; bool depth, writes_depth; };
    const FormatCase formats[]{
      {DkImageFormat_RGBA8_Unorm, false, false},
      {DkImageFormat_RGBA16_Unorm, false, false},
      {DkImageFormat_R8_Unorm, false, false},
      {DkImageFormat_R16_Uint, false, false},
      {DkImageFormat_R32_Uint, false, false},
      {DkImageFormat_R32_Float, false, false},
      {DkImageFormat_S8, true, false},
      {DkImageFormat_Z16, true, true},
      {DkImageFormat_Z24X8, true, true},
      {DkImageFormat_ZF32, true, true},
      {DkImageFormat_Z24S8, true, true},
      {DkImageFormat_ZF32_X24S8, true, true},
    };
    for (const auto &test : formats) {
      nxdk::Image destination{};
      destination.format = test.format;
      const auto attachments = nxdk::utility_targets(&destination);
      const auto dss = attachments.depth_stencil();
      if (attachments.color != (test.depth ? nullptr : &destination) ||
          attachments.depth != (test.depth ? &destination : nullptr) ||
          attachments.color_mask(10) != (test.depth ? 0 : 10) ||
          dss.depthTestEnable != test.writes_depth || dss.depthWriteEnable != test.writes_depth ||
          dss.depthCompareOp != DkCompareOp_Always || dss.stencilTestEnable)
        throw std::runtime_error("utility conversion lost its attachment or depth writes");
      if (test.depth) depth_images.insert(&destination.image);
      else depth_images.erase(&destination.image);
      DkImageView view{};
      dkImageViewDefaults(&view, &destination.image);
      nxdk::bind_render_targets(commands, attachments.color ? &view : nullptr,
                                attachments.depth ? &view : nullptr);
      ++checked;
    }

    nxdk::Image color_target{}, depth_target{}, other_depth{};
    color_target.format = DkImageFormat_RGBA8_Unorm;
    depth_target.format = other_depth.format = DkImageFormat_ZF32_X24S8;
    const auto stencil_pass = nxdk::utility_targets(&depth_target);
    const auto stencil_state = stencil_pass.depth_stencil(true);
    if (stencil_state.depthTestEnable || stencil_state.depthWriteEnable ||
        !stencil_state.stencilTestEnable ||
        stencil_state.stencilFrontPassOp != DkStencilOp_Replace ||
        stencil_state.stencilBackPassOp != DkStencilOp_Replace)
      throw std::runtime_error("stencil conversion must preserve depth and replace stencil");
    ++checked;
    const auto combined = nxdk::utility_targets(&color_target, &depth_target);
    if (combined.color != &color_target || combined.depth != &depth_target)
      throw std::runtime_error("combined utility target lost an attachment");
    ++checked;
    const auto same_depth = nxdk::utility_targets(&depth_target, &depth_target);
    if (same_depth.color || same_depth.depth != &depth_target)
      throw std::runtime_error("same depth destination must produce one attachment");
    ++checked;
    struct InvalidTargets { nxdk::Image *color, *depth; };
    const InvalidTargets invalid_targets[]{
      {&depth_target, &other_depth}, {&color_target, &color_target}};
    for (const auto &bad : invalid_targets) {
      bool rejected_target = false;
      try { nxdk::utility_targets(bad.color, bad.depth); }
      catch (const std::invalid_argument &) { rejected_target = true; }
      if (!rejected_target) throw std::runtime_error("conflicting depth attachments were accepted");
      ++checked;
    }
    std::cout << "Validated " << checked << " render-target/conversion contract cases\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
