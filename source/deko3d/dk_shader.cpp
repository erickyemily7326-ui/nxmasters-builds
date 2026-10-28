#include "dk_shader.h"
#include "dk_shader_blob.h"
#include "compiler/UamBridge.h"
#include <lsfg/common/dksh.hpp>
#include <lsfg/common/sha256.hpp>
#include <lsfg/common/version.hpp>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <sys/stat.h>

extern "C" {
#include "../config.h"
#include "../prefs.h"
}

namespace nxdk {
namespace {
std::mutex compiler_mutex;
std::unordered_map<std::string, std::weak_ptr<ShaderBinary>> binaries;
std::vector<MemoryPtr> code_pages;
uint32_t code_offset = 0;
constexpr uint32_t CacheVersion = 2;
constexpr uint32_t MaximumBlobSize = 4 * 1024 * 1024;
}

std::shared_ptr<ShaderBinary> compile_shader(const ShaderSource &source, DkStage stage,
                                           std::string &diagnostics) {
  std::lock_guard guard(compiler_mutex);
  lsfg::Sha256 hash;
  hash.update_field(CacheVersion);
  hash.update_field(lsfg::version::uam_revision);
  hash.update_field(static_cast<uint64_t>(stage));
  hash.update_field(source.glsl);
  const std::string key = lsfg::to_hex(hash.finish()).data();
  if (auto found = binaries.find(key); found != binaries.end())
    if (auto binary = found->second.lock()) return binary;

  const bool disk_cache = !prefs_get_bool("EmuCore/GS/DisableShaderCache", false);
  if (disk_cache) {
    mkdir(CACHE_DIR, 0777);
    mkdir(CACHE_DIR "/deko3d", 0777);
  }
  const std::string path = std::string(CACHE_DIR "/deko3d/") + key + ".nxdk";
  std::vector<uint8_t> blob;
  if (FILE *file = disk_cache ? fopen(path.c_str(), "rb") : nullptr) {
    if (fseek(file, 0, SEEK_END) == 0) {
      const long size = ftell(file);
      if (size > long(lsfg::sha256_digest_size) && size <= MaximumBlobSize + long(lsfg::sha256_digest_size) &&
          fseek(file, 0, SEEK_SET) == 0) {
        blob.resize(size);
        if (fread(blob.data(), 1, blob.size(), file) != blob.size()) blob.clear();
      }
    }
    fclose(file);
    if (blob.size() > lsfg::sha256_digest_size) {
      const size_t size = blob.size() - lsfg::sha256_digest_size;
      const auto checksum = lsfg::sha256(std::span(blob).first(size));
      if (memcmp(checksum.data(), blob.data() + size, checksum.size())) blob.clear();
      else blob.resize(size);
    }
    if (!validate_shader_blob(blob, stage, ScratchBytes)) blob.clear();
  }
  if (blob.empty()) {
    size_t size = 0;
    char *message = nullptr;
    void *compiled = UamCompileGlsl(source.glsl.c_str(), static_cast<int>(stage), &size, &message);

    diagnostics = message ? message : "";
    free(message);
    if (compiled && size && size <= MaximumBlobSize)
      blob.assign(static_cast<uint8_t *>(compiled), static_cast<uint8_t *>(compiled) + size);
    free(compiled);
    if (!validate_shader_blob(blob, stage, ScratchBytes)) {
      if (diagnostics.empty()) diagnostics = "Invalid DKSH or shader scratch limit exceeded";

      return {};
    }
    const std::string temporary = path + ".tmp";
    if (FILE *file = disk_cache ? fopen(temporary.c_str(), "wb") : nullptr) {
      const auto checksum = lsfg::sha256(blob);
      const bool written = fwrite(blob.data(), 1, blob.size(), file) == blob.size() &&
          fwrite(checksum.data(), 1, checksum.size(), file) == checksum.size();
      const bool closed = fclose(file) == 0;
      if (written && closed) rename(temporary.c_str(), path.c_str());
      else remove(temporary.c_str());
    }
  }
  lsfg::dksh::FileHeader header;
  memcpy(&header, blob.data(), sizeof(header));
  const uint32_t size = align_up(header.code_size + DK_SHADER_CODE_UNUSABLE_SIZE,
                                DK_SHADER_CODE_ALIGNMENT);
  if (code_pages.empty() || uint64_t(code_offset) + size > code_pages.back()->size) {
    auto memory = gpu().allocate(std::max<uint32_t>(4 * 1024 * 1024, size),
        DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code);
    if (!memory) { diagnostics = "No memory for shader code"; return {}; }
    code_pages.push_back(std::move(memory));
    code_offset = 0;
  }
  auto result = std::make_shared<ShaderBinary>();
  result->memory = code_pages.back();
  memcpy(static_cast<uint8_t *>(result->memory->cpu()) + code_offset,
         blob.data() + header.control_size, header.code_size);
  gpu().barrier(DkBarrier_None, DkInvalidateFlags_Shader);
  DkShaderMaker maker;
  dkShaderMakerDefaults(&maker, result->memory->block, code_offset);
  maker.control = blob.data();
  dkShaderInitialize(&result->shader, &maker);
  code_offset += size;
  if (!dkShaderIsValid(&result->shader) || dkShaderGetStage(&result->shader) != stage) {
    diagnostics = "Deko3D rejected shader";
    return {};
  }
  binaries[key] = result;
  return result;
}

void reset_shader_cache() {
  std::lock_guard guard(compiler_mutex);
  binaries.clear();
  code_pages.clear();
  code_offset = 0;
}
}
