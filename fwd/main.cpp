// Lanzador por juego (NX masters): abre el nucleo de Tico con el juego ya elegido.
// Al abrirlo desde el Homebrew Menu muestra 5 s una consola: A = poner el icono en el
// menu HOME; sin tocar nada (o cualquier otro boton) abre el juego. El icono del HOME
// vuelve a abrir este mismo .nro con "--play" y entra directo al juego.
#include <switch.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include "fixed.h"
#include "../launcher/source/forwarder.cpp"

#ifndef FWD_AUTHOR
#define FWD_AUTHOR "grupo NX masters"
#endif

static const char *CAND[] = {
    "sdmc:/switch/nxmasters/cores/%s.nro",
    "sdmc:/tico/cores/%s.nro",
    "sdmc:/switch/tico/cores/%s.nro",
    "sdmc:/switch/%s/%s.nro",
    "sdmc:/switch/%s.nro",
    "sdmc:/tico/%s.nro",
};

// Homebrew Menu abierto desde el Album (modo applet): aqui no se puede jugar, solo instalar el icono.
static bool modoApplet()
{
    const AppletType t = appletGetAppletType();
    return t != AppletType_Application && t != AppletType_SystemApplication;
}

static bool existe(const char *p) { struct stat st; return stat(p, &st) == 0; }

static std::string claveIcono()
{
    // Clave unica por juego: sirve para el id del icono del HOME (solo letras, numeros, - y _).
    unsigned h = 5381;
    for (const char *s = FWD_TITLE FWD_ROM; *s; s++) h = h * 33 + (unsigned char)*s;
    char k[48];
    snprintf(k, sizeof k, "%s-%08x", FWD_SLUG, h);
    return k;
}

// Devuelve 0 si lanzo el nucleo; si no, deja un mensaje en pantalla.
static int jugar(PadState &pad, bool consolaLista)
{
    char path[300], args[1024];
    const char *found = nullptr;
    for (unsigned i = 0; i < sizeof(CAND) / sizeof(CAND[0]); i++) {
        snprintf(path, sizeof(path), CAND[i], FWD_CORE, FWD_CORE);
        if (existe(path)) { found = path; break; }
    }
    if (found && existe(FWD_ROM)) {
        // Orden que entienden todos los nucleos de Tico: juego, titulo, consola.
#if FWD_MODE == 0
        snprintf(args, sizeof(args), "\"%s\" \"%s\" \"%s\" %s", found, FWD_ROM, FWD_TITLE, FWD_SLUG);
#else
        snprintf(args, sizeof(args), "\"%s\" \"%s\"", found, FWD_ROM);
#endif
        if (R_SUCCEEDED(envSetNextLoad(found, args))) return 0;
    }
    if (!consolaLista) { consoleInit(NULL); padConfigureInput(1, HidNpadStyleSet_NpadStandard); padInitializeDefault(&pad); }
    consoleClear();
    printf("%s\n\n", FWD_TITLE);
    if (!found) {
        printf("No encontre el nucleo %s.\n", FWD_CORE);
        printf("Ponlo en /switch/nxmasters/cores/ o instalalo desde Tico.\nBusque en:\n");
        for (unsigned i = 0; i < sizeof(CAND) / sizeof(CAND[0]); i++) {
            snprintf(path, sizeof(path), CAND[i], FWD_CORE, FWD_CORE);
            printf("  %s\n", path);
        }
    } else if (!existe(FWD_ROM)) {
        printf("No encontre el juego:\n  %s\n\nRevisa que el nombre sea exacto.\n", FWD_ROM);
    } else {
        printf("No pude abrir el nucleo:\n  %s\n", found);
    }
    printf("\nPulsa + para salir.\n");
    consoleUpdate(NULL);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return 1;
}

static void instalarIcono(PadState &pad)
{
    consoleClear();
    printf("%s\n\nInstalando el icono en el menu HOME...\n", FWD_TITLE);
    consoleUpdate(NULL);
    char err[256] = {0};
    appletSetCpuBoostMode(ApmCpuBoostMode_FastLoad);
    const bool ok = forwarder_create_tico(claveIcono(), FWD_TITLE, FWD_AUTHOR, "romfs:/home-icon.jpg", err, sizeof err);
    appletSetCpuBoostMode(ApmCpuBoostMode_Normal);
    consoleClear();
    printf("%s\n\n", FWD_TITLE);
    if (ok) {
        printf("LISTO. Vuelve al menu HOME: ya esta el icono.\n");
    } else {
        printf("ERROR: %s\n\n", err[0] ? err : "desconocido");
        printf("Abre este programa desde el ALBUM (modo applet) para instalar el icono,\ny revisa que tengas sigpatches.\n");
    }
    if (modoApplet()) printf("\nPara jugar usa el icono del menu HOME (abre en modo titulo).\n\nPulsa + para salir.\n");
    else printf("\nPulsa A para jugar ahora, + para salir.\n");
    consoleUpdate(NULL);
    while (appletMainLoop()) {
        padUpdate(&pad);
        const u64 k = padGetButtonsDown(&pad);
        if (!modoApplet() && (k & HidNpadButton_A)) { consoleExit(NULL); jugar(pad, false); return; }
        if (k & HidNpadButton_Plus) break;
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
}

int main(int argc, char **argv)
{
    if (argc >= 1 && argv[0] && argv[0][0]) g_forwarderSelfPath = argv[0];
    bool play = false;
    for (int i = 1; i < argc; i++) if (argv[i] && !strcmp(argv[i], "--play")) play = true;

    PadState pad;
    if (play) return jugar(pad, false);       // abierto desde el icono del HOME: directo al juego

    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    romfsInit();

    if (modoApplet()) {
        // Sin cuenta regresiva: en applet el nucleo no funciona, asi que solo se ofrece el icono.
        consoleClear();
        printf("\n  %s\n\n  Pulsa A: poner el juego en el menu HOME\n  Pulsa + para salir\n\n"
               "  (Para jugar, usa luego el icono del HOME,\n   o abre este programa en modo titulo: manten R.)\n", FWD_TITLE);
        consoleUpdate(NULL);
        while (appletMainLoop()) {
            padUpdate(&pad);
            const u64 k = padGetButtonsDown(&pad);
            if (k & HidNpadButton_A) { instalarIcono(pad); break; }
            if (k & HidNpadButton_Plus) break;
            consoleUpdate(NULL);
        }
        romfsExit();
        return 0;
    }
    // Al abrir con R apretado (modo titulo) ese boton llega como "pulsado": se ignora todo
    // lo que ya estaba apretado y solo cuenta A pulsado despues.
    padUpdate(&pad);
    u64 retenidos = padGetButtons(&pad);
    const u64 inicio = armGetSystemTick();
    int mostrado = -1;
    while (appletMainLoop()) {
        const int resta = 5 - (int)(armTicksToNs(armGetSystemTick() - inicio) / 1000000000ULL);
        if (resta <= 0) break;
        padUpdate(&pad);
        retenidos &= padGetButtons(&pad);                 // lo que sigue apretado desde el inicio
        const u64 k = padGetButtonsDown(&pad) & ~retenidos;
        if (k & HidNpadButton_A) {
            consoleClear();
            printf("%s\n\nPoner este juego en el menu HOME instala un titulo no oficial.\n"
                   "Usa emuMMC: en sysNAND puede haber riesgo de ban.\n\n"
                   "A: instalar el icono     Otro boton: jugar\n", FWD_TITLE);
            consoleUpdate(NULL);
            for (;;) {
                if (!appletMainLoop()) { consoleExit(NULL); return 0; }
                padUpdate(&pad);
                const u64 k2 = padGetButtonsDown(&pad);
                if (k2 & HidNpadButton_A) { instalarIcono(pad); romfsExit(); return 0; }
                if (k2) break;
                consoleUpdate(NULL);
            }
            break;
        }
        if (resta != mostrado) {
            mostrado = resta;
            consoleClear();
            printf("\n  %s\n\n  Pulsa A: poner el juego en el menu HOME\n  (si no, el juego abre solo en %d s)\n", FWD_TITLE, resta);
        }
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    romfsExit();
    return jugar(pad, false);
}
