#include "dk_abi.h"
#include "dk_shader_source.h"
#include "dk_shader_blob.h"
#include "UamBridge.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {
unsigned compiled = 0;
std::string read(const std::filesystem::path &path) {
  std::ifstream stream(path);
  if (!stream) throw std::runtime_error("Could not read " + path.string());
  return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
void check(const std::string &source, int stage, const std::string &label) {
  const auto native = stage == UamStage_Fragment ? nxdk::lower_gs_shuffle(source) : source;
  const auto lowered = nxdk::lower_shader(native, stage == UamStage_Vertex);
  if (stage == UamStage_Vertex &&
      (lowered.glsl.find("nx_core_main") != std::string::npos ||
       lowered.glsl.find("2.0 * gl_Position.z - gl_Position.w") != std::string::npos))
    throw std::runtime_error(label + ": lossy GS depth remapping returned");
  size_t size = 0;
  char *log = nullptr;
  void *blob = UamCompileGlsl(lowered.glsl.c_str(), stage, &size, &log);
  const std::string diagnostic = log ? log : "";
  std::free(log);
  if (!blob || size < 256) {
    std::free(blob);
    throw std::runtime_error(label + ": " + diagnostic);
  }
  const auto bytes = std::span(static_cast<const uint8_t *>(blob), size);
  if (!nxdk::validate_shader_blob(bytes, stage) ||
      nxdk::validate_shader_blob(bytes.first(size - 1), stage) ||
      nxdk::validate_shader_blob(bytes, (stage + 1) % 6)) {
    std::free(blob);
    throw std::runtime_error(label + ": stage or bounds validation failed");
  }
  uint32_t header[6], program[16];
  std::memcpy(header, blob, sizeof(header));
  if (header[0] != 0x48534b44 || header[2] + uint64_t(header[3]) != size ||
      header[4] + 64 > header[2] || header[5] != 1) {
    std::free(blob);
    throw std::runtime_error(label + ": invalid DKSH");
  }
  std::memcpy(program, static_cast<const uint8_t *>(blob) + header[4], sizeof(program));
  std::free(blob);
  if (program[5] > 65536 || program[2] > 255) throw std::runtime_error(label + ": exceeds GPU limits");
  ++compiled;
}

std::string header(int stage, const std::string &entry) {
  const char *define = stage == 0 ? "VERTEX_SHADER" : stage == 3 ? "GEOMETRY_SHADER" :
                       stage == 4 ? "FRAGMENT_SHADER" : "COMPUTE_SHADER";
  return std::string("#version 450 core\n#define pGL_ES 1\n#define HAS_CLIP_CONTROL 1\n") +
         "#define HAS_FRAMEBUFFER_FETCH 0\n#define " + define + " 1\n" +
         (entry == "main" ? "" : "#define " + entry + " main\n");
}
void validate(const std::filesystem::path &assets) {
  const auto gl = assets / "shaders/opengl";
  const auto common = read(gl / "common_header.glsl");
  const auto vgs = read(gl / "tfx_vgs.glsl");
  for (unsigned selector = 0; selector < 8; ++selector) {
    const std::string defines = "#define VS_INT_FST " + std::to_string(selector & 1) +
        "\n#define VS_IIP " + std::to_string((selector >> 1) & 1) + "\n#define VS_POINT_SIZE " +
        std::to_string((selector >> 2) & 1) + "\n#define VS_POINT_SIZE_VALUE 2.0\n";
    check(header(0,"vs_main") + defines + common + vgs, 0, "VS " + std::to_string(selector));
  }
  for (unsigned topology = 0; topology < 3; ++topology)
    for (unsigned iip = 0; iip < 2; ++iip) {
      const std::string defines = "#define GS_POINT " + std::to_string(topology == 0) +
          "\n#define GS_LINE " + std::to_string(topology == 1) + "\n#define GS_IIP " + std::to_string(iip) + "\n";
      check(header(3,"gs_main") + defines + common + vgs, 3, "GS " + std::to_string(topology));
    }
  const auto convert = read(gl / "convert.glsl");
  check(header(0,"vs_main") + common + convert, 0, "conversion VS");
  const std::regex entry(R"(void\s+(ps_\w+)\s*\()");
  for (std::sregex_iterator i(convert.begin(), convert.end(), entry), end; i != end; ++i) {
    const std::string name = (*i)[1];
    check(header(4,name) + "#define PS_SCALE_FACTOR 2.0\n" + common + convert, 4, name);
  }
  for (unsigned mode = 0; mode < 2; ++mode)
    check(header(4,"main") + "#define ps_stencil_image_init_" + std::to_string(mode) +
          " 1\n" + common + convert, 4, "DATE initialization");
  for (const char *file : {"interlace.glsl", "merge.glsl", "present.glsl", "shadeboost.glsl"}) {
    const auto source = read(gl / file);
    for (std::sregex_iterator i(source.begin(), source.end(), entry), end; i != end; ++i) {
      const std::string name = (*i)[1];
      check(header(4,name) + common + source, 4, name);
    }
  }
  check(header(4,"main") + "#define FXAA_GLSL_130 1\n" + common +
        read(assets / "shaders/common/fxaa.fx"), 4, "FXAA");
  auto cas = read(gl / "cas.glsl");
  for (const char *name : {"ffx_a.h", "ffx_cas.h"}) {
    const std::string include = "#include \"" + std::string(name) + "\"";
    cas.replace(cas.find(include), include.size(), read(assets / "shaders/common" / name));
  }
  for (unsigned sharpen = 0; sharpen < 2; ++sharpen)
    check(header(5,"main") + "#define CAS_SHARPEN_ONLY " + (sharpen ? "true\n" : "false\n") + cas, 5, "CAS");

  const auto tfx = read(gl / "tfx_fs.glsl");
  std::set<std::string> macros;
  const std::regex macro(R"(\bPS_[A-Z0-9_]+\b)");
  for (std::sregex_iterator i(tfx.begin(), tfx.end(), macro), end; i != end; ++i) macros.insert(i->str());
  std::array<std::pair<const char *,unsigned>,18> variants{{
      {"PS_TFX",4}, {"PS_FST",1}, {"PS_DATE",3}, {"PS_DATE",5}, {"PS_DATE",1},
      {"PS_DATE",2}, {"PS_HDR",1}, {"PS_FBMASK",1}, {"PS_SHUFFLE",1},
      {"PS_PAL_FMT",1}, {"PS_DEPTH_FMT",1}, {"PS_DITHER",1}, {"PS_COLCLIP",1},
      {"PS_CHANNEL_FETCH",6}, {"PS_SCANMSK",2}, {"PS_TEX_IS_FB",1},
      {"PS_A_MASKED",1}, {"PS_CLR_HW",4}}};
  for (const auto &[changed,value] : variants) {
    if (!macros.contains(changed)) continue;
    std::string defines;
    const bool alpha_optimization = std::string_view(changed) == "PS_A_MASKED" ||
                                     std::string_view(changed) == "PS_CLR_HW";
    for (const auto &name : macros) {
      if (name == "PS_PRIMID_INIT") continue;
      const unsigned selected = name == changed ? value :
          alpha_optimization && name == "PS_BLEND_C" ? 1 : name == "PS_SCALE_FACTOR" ? 2 :
          name == "PS_TFX" ? 0 : name == "PS_IIP" ? 1 : 0;
      defines += "#define " + name + " " + std::to_string(selected) + "\n";
    }
    const std::string source = header(4,"ps_main") + defines + common + tfx;
    const bool reads_rt = alpha_optimization || std::string_view(changed) == "PS_TEX_IS_FB" ||
        std::string_view(changed) == "PS_FBMASK" || (std::string_view(changed) == "PS_DATE" && value >= 5);
    if (nxdk::lower_shader(source, false).reads_framebuffer != reads_rt)
      throw std::runtime_error(std::string(changed) + ": incorrect framebuffer-read dependency");
    check(source, 4, changed + std::to_string(value));
  }
  for (unsigned read16 = 0; read16 < 2; ++read16)
    for (unsigned read_ba = 0; read_ba < 2; ++read_ba)
      for (unsigned fbmask = 0; fbmask < 2; ++fbmask) {
        std::string defines;
        for (const auto &name : macros) {
          if (name == "PS_PRIMID_INIT") continue;
          const unsigned value = name == "PS_READ16_SRC" ? read16 :
              name == "PS_READ_BA" ? read_ba : name == "PS_FBMASK" ? fbmask :
              name == "PS_SHUFFLE" || name == "PS_FST" || name == "PS_TFX" ||
              name == "PS_TCC" || name == "PS_NO_COLOR1" ? 1 :
              name == "PS_SCALE_FACTOR" ? 3 : 0;
          defines += "#define " + name + " " + std::to_string(value) + "\n";
        }
        check(header(4,"ps_main") + defines + common + tfx, 4, "GS channel shuffle");
      }
  std::cout << assets.string() << ": shader compilation passed\n";
}
}

int main(int argc, char **argv) {
  try {
    if (argc < 2) throw std::runtime_error("Provide one or more extracted APK assets directories");
    for (int i = 1; i < argc; ++i) validate(argv[i]);
    std::cout << "Compiled " << compiled << " DKSH shader variants; AArch64 GS layouts checked\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
