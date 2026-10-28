#include "dk_shader_source.h"
#include <algorithm>
#include <cstdlib>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace nxdk {
int shader_define(std::string_view source, std::string_view name) {
  const std::string key = "#define " + std::string(name) + " ";
  const size_t offset = source.find(key);
  return offset == source.npos ? 0 : std::atoi(std::string(source.substr(offset + key.size(), 16)).c_str());
}

std::string lower_gs_shuffle(std::string source) {
  if (!shader_define(source, "PS_SHUFFLE")) return source;
  const size_t begin = source.find("#if PS_SHUFFLE");
  if (begin == source.npos) throw std::runtime_error("Missing GS channel shuffle block");
  size_t end = begin;
  unsigned depth = 0;
  do {
    const size_t newline = source.find('\n', end);
    if (newline == source.npos) throw std::runtime_error("Unterminated GS channel shuffle block");
    std::string_view line(source.data() + end, newline - end);
    const size_t first = line.find_first_not_of(" \t\r");
    if (first != line.npos) line.remove_prefix(first);
    if (line.starts_with("#if")) ++depth;
    else if (line.starts_with("#endif")) --depth;
    end = newline + 1;
  } while (depth);
  source.replace(begin, end - begin, R"(#if PS_SHUFFLE
    uvec4 nx_color = uvec4(C);
    uvec2 nx_ta = uvec2(TA * 255.0f + 0.5f);
#if PS_READ16_SRC
    uint nx_alpha = nx_color.a;
    uint nx_low = (nx_color.g >> 6) | ((nx_color.b >> 3) << 2);
    C.rb = vec2(float((nx_color.r >> 3) | (((nx_color.g >> 3) & 7u) << 5)));
#else
#if PS_READ_BA
    uint nx_alpha = nx_color.a;
    C.rb = C.bb;
#else
    uint nx_alpha = nx_color.g;
    C.rb = C.rr;
#endif
    uint nx_low = nx_alpha & 127u;
#endif
    uint nx_high = ((nx_ta.x & ~nx_alpha) | (nx_ta.y & nx_alpha)) & 128u;
    C.ga = vec2(float(nx_low | nx_high));
#endif
)");
  return source;
}

ShaderSource lower_shader(std::string_view source, bool vertex,
                         const std::map<std::string, unsigned> &attributes) {
  ShaderSource result;
  const int a = shader_define(source, "PS_BLEND_A"), b = shader_define(source, "PS_BLEND_B");
  const int c = shader_define(source, "PS_BLEND_C"), d = shader_define(source, "PS_BLEND_D");
  const int date = shader_define(source, "PS_DATE");
  const bool blend_reads_rt = (a || b || d) && (a == 1 || b == 1 || c == 1 || d == 1);
  const bool primid_init = date == 1 || date == 2;

  const bool alpha_reads_rt = c == 1 &&
      (shader_define(source, "PS_A_MASKED") || shader_define(source, "PS_CLR_HW") > 3);
  result.reads_framebuffer = shader_define(source, "PS_TEX_IS_FB") || date >= 5 ||
      (!primid_init && (shader_define(source, "PS_FBMASK") || blend_reads_rt || alpha_reads_rt));
  const std::regex declaration(R"(^\s*(?:layout\s*\(([^)]*)\)\s*)?uniform\s+((?:[iu]?sampler\w+)|(?:[iub]?vec[234])|(?:mat[234])|float|int|uint|bool)\s+(\w+)\s*(?:\[\s*(\d+)\s*\])?\s*;)");
  const std::regex binding(R"(binding\s*=\s*(\d+))");
  const std::regex interface(R"(^\s*((?:(?:flat|smooth|noperspective|centroid)\s+)?)(in|out|attribute|varying)\s+([iub]?vec[234]|float)\s+(\w+)\s*;)");
  const std::map<std::string, unsigned> conventional{
      {"PSin_p", 0}, {"PSin_t", 1}, {"PSin_c", 2},
      {"Frag_UV", 0}, {"Frag_Color", 1}, {"v_tex0", 0},
      {"v_pos", 1}, {"v_col0", 2}, {"v_color", 1},
      {"a_position", 0}, {"a_texcoord", 1}, {"a_color", 2},
      {"Position", 0}, {"UV", 1}, {"Color", 2}};
  std::map<std::string, Uniform> uniforms;
  std::istringstream input{std::string(source)};
  std::string line;
  while (std::getline(input, line)) {
    std::smatch match;
    if (!std::regex_search(line, match, declaration)) continue;
    const std::string type = match[2], name = match[3];
    Uniform u;
    u.name = name;
    u.array_count = match[4].matched ? std::stoul(match[4]) : 1;
    if (type.find("sampler") != std::string::npos) {
      std::smatch slot;
      const std::string layout = match[1];
      u.sampler = std::regex_search(layout, slot, binding) ? std::stoi(slot[1]) : 0;
    } else {
      const uint32_t element = type.starts_with("mat") ? (type.back() - '0') * 16 :
          (type.ends_with('2') ? 8 : type.ends_with('3') ? 12 : type.ends_with('4') ? 16 : 4);
      u.size = u.array_count > 1 ? ((element + 15) & ~15u) * u.array_count : element;
    }
    uniforms.emplace(name, std::move(u));
  }
  for (auto &[name, uniform] : uniforms) {
    if (uniform.sampler < 0) {
      uniform.offset = result.uniform_bytes;
      result.uniform_bytes += (uniform.size + 15) & ~15u;
      if (result.uniform_bytes > 65536) throw std::runtime_error("Loose uniforms exceed 64 KiB");
    }
    result.uniforms.push_back(uniform);
  }

  result.glsl = "#version 450 core\n";
  input.clear();
  input.str(std::string(source));
  unsigned fallback_location = 0;
  while (std::getline(input, line)) {
    const auto first = line.find_first_not_of(" \t\r");
    if (first == line.npos) { result.glsl += '\n'; continue; }
    const std::string_view trimmed{line.data() + first, line.size() - first};
    if (trimmed.starts_with("#version") || trimmed.starts_with("precision ")) continue;
    if (trimmed.starts_with("#extension") &&
        (trimmed.find("GL_EXT_blend_func_extended") != trimmed.npos ||
         trimmed.find("GL_EXT_shader_io_blocks") != trimmed.npos ||
         trimmed.find("GL_OES_geometry_shader") != trimmed.npos ||
         trimmed.find("GL_EXT_geometry_shader") != trimmed.npos)) continue;
    std::smatch match;
    if (std::regex_search(line, match, declaration)) {
      const std::string type = match[2], name = match[3];
      const Uniform &u = uniforms.at(name);
      const std::string suffix = line.substr(match.length());
      if (u.sampler >= 0) {
        if (!match[1].matched) line = "layout(binding=" + std::to_string(u.sampler) + ") " + line;
      } else {
        line = "layout(std140,binding=15) uniform NX_Uniform_" + name +
               " { layout(offset=" + std::to_string(u.offset) + ") " + type + " " + name;
        if (match[4].matched) line += "[" + match[4].str() + "]";
        line += "; };" + suffix;
      }
    } else if (std::regex_search(line, match, interface)) {
      const std::string name = match[4];
      auto attribute = attributes.find(name);
      auto known = conventional.find(name);
      const bool vertex_input = vertex && (match[2] == "in" || match[2] == "attribute");
      const unsigned location = vertex_input && attribute != attributes.end() ? attribute->second :
          known != conventional.end() ? known->second : fallback_location++;
      std::string direction = match[2];
      if (direction == "attribute") direction = "in";
      if (direction == "varying") direction = vertex ? "out" : "in";
      line = "layout(location=" + std::to_string(location) + ") " + match[1].str() +
             direction + " " + match[3].str() + " " + name + ";" + line.substr(match.length());
    }
    result.glsl += line + '\n';
  }
  return result;
}
}
