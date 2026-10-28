// Puerta de arranque (NX masters): al abrir el icono del HOME espera 5 s.
// Si tocas la pantalla abre Cemu-nx normal (biblioteca, con Y = ajustes del juego).
// Si no tocas, abre el juego directo. Solo al arrancar.
#include <switch.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "../asce_installer/source/game_config.h"

#define CEMU "sdmc:/switch/cemu/cemu.nro"
#define ESPERA_MS 5000

int main(int argc, char **argv)
{
    char game[600] = {0};
    if (argc >= 2 && argv[1]) snprintf(game, sizeof game, "%s", argv[1]);
    else {   // abierto desde el Homebrew Menu: usa el juego de este paquete
        struct stat st;
        if (stat(GAME_PATH, &st) == 0) snprintf(game, sizeof game, "%s", GAME_PATH);
        else if (GAME_PATH_ALT[0] && stat(GAME_PATH_ALT, &st) == 0) snprintf(game, sizeof game, "%s", GAME_PATH_ALT);
    }

    consoleInit(NULL);
    hidInitializeTouchScreen();
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    u64 t0 = armGetSystemTick();
    bool settings = false;
    int shown = -1;
    while (appletMainLoop()) {
        u64 ms = armTicksToNs(armGetSystemTick() - t0) / 1000000ULL;
        if (ms >= ESPERA_MS) break;
        HidTouchScreenState st = {0};
        if (hidGetTouchScreenStates(&st, 1) && st.count > 0) { settings = true; break; }
        int left = (int)((ESPERA_MS - ms + 999) / 1000);
        if (left != shown) {
            shown = left;
            consoleClear();
            printf("\n\n  Toca la pantalla para abrir los ajustes\n");
            printf("  (si no, el juego arranca solo en %d s)\n", left);
        }
        consoleUpdate(NULL);
    }
    consoleExit(NULL);

    char args[1300];
    if (settings || !game[0]) snprintf(args, sizeof args, "\"%s\"", CEMU);
    else snprintf(args, sizeof args, "\"%s\" \"%s\"", CEMU, game);
    envSetNextLoad(CEMU, args);
    return 0;
}
