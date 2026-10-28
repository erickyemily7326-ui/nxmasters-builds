#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../source/hooks.h"
#include "../../source/prefs.h"
#include "../../source/so_util.h"

so_module emu_mod;

static const size_t image_size = 0xb80560;
static uint8_t *image;
static unsigned checks;

static void prepare(const char *version) {
  memset(image, 0, image_size);
  emu_mod.load_base = image;
  emu_mod.load_size = image_size;
  if (!strcmp(version, "3668"))
    memcpy(image + 0xff01f, "v2.2n-3668", 10);
  else if (!strcmp(version, "4248"))
    memcpy(image + 0xd4436, "v2.2n-4248", 10);
  *(uint32_t *)(image + 0x3cfa98) = 0x14000012;
  *(uint32_t *)(image + 0x3cfad8) = 0x140001d7;
  prefs_init("");
}

static void check_pair(const char *name, uint32_t first, uint32_t second) {
  patch_game();
  if (*(uint32_t *)(image + 0x3cfa98) != first ||
      *(uint32_t *)(image + 0x3cfad8) != second) {
    fprintf(stderr, "%s failed\n", name);
    exit(EXIT_FAILURE);
  }
  checks++;
}

int main(void) {
  image = malloc(image_size);
  if (!image)
    return EXIT_FAILURE;

  prepare("3668");
  if (prefs_get_bool("EmuCore/Patches/XGKickSkip", true))
    return EXIT_FAILURE;
  check_pair("default off", 0x14000012, 0x140001d7);

  prepare("3668");
  prefs_remove("EmuCore/Patches/XGKickSkip");
  check_pair("missing setting", 0x14000012, 0x140001d7);

  prepare("3668");
  prefs_set_bool("EmuCore/Patches/XGKickSkip", false);
  check_pair("explicit off", 0x14000012, 0x140001d7);

  prepare("3668");
  prefs_set_bool("EmuCore/Patches/XGKickSkip", true);
  check_pair("enabled", 0xd503201f, 0xd503201f);
  check_pair("already patched", 0xd503201f, 0xd503201f);

  prepare("3668");
  prefs_set_bool("EmuCore/Patches/XGKickSkip", true);
  *(uint32_t *)(image + 0x3cfa98) = 0x14000013;
  check_pair("first mismatch", 0x14000013, 0x140001d7);

  prepare("3668");
  prefs_set_bool("EmuCore/Patches/XGKickSkip", true);
  *(uint32_t *)(image + 0x3cfad8) = 0x140001d8;
  check_pair("second mismatch", 0x14000012, 0x140001d8);

  prepare("3668");
  prefs_set_bool("EmuCore/Patches/XGKickSkip", true);
  *(uint32_t *)(image + 0x3cfa98) = 0xd503201f;
  check_pair("partially patched image", 0xd503201f, 0x140001d7);

  prepare("4248");
  prefs_set_bool("EmuCore/Patches/XGKickSkip", true);
  check_pair("unsupported core", 0x14000012, 0x140001d7);

  prepare("");
  prefs_set_bool("EmuCore/Patches/XGKickSkip", true);
  check_pair("unknown core", 0x14000012, 0x140001d7);

  prepare("3668");
  prefs_set_bool("EmuCore/Patches/XGKickSkip", true);
  prefs_set_bool("Wrapper/BootBIOS", true);
  check_pair("BIOS boot", 0x14000012, 0x140001d7);

  prepare("3668");
  prefs_set_bool("EmuCore/Patches/XGKickSkip", true);
  emu_mod.load_size = 0x3cfa9b;
  check_pair("first out of range", 0x14000012, 0x140001d7);

  prepare("3668");
  prefs_set_bool("EmuCore/Patches/XGKickSkip", true);
  emu_mod.load_size = 0x3cfadb;
  check_pair("second out of range", 0x14000012, 0x140001d7);

  prepare("3668");
  prefs_set_string("EmuCore/Patches/XGKickSkip", "invalid");
  check_pair("invalid setting", 0x14000012, 0x140001d7);

  prepare("3668");
  prefs_set_bool("EmuCore/Gamefixes/XgKickHack", true);
  check_pair("independent XGKick sync", 0x14000012, 0x140001d7);

  free(image);
  printf("%u core patch checks passed\n", checks);
  return EXIT_SUCCESS;
}
