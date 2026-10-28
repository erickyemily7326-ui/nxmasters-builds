# Aplica la puerta de 5 s sobre el arbol ya parcheado (Cemu-nx + instalador).
import re
def sub(path, old, new, count=1):
    s = open(path).read()
    assert old in s, (path, old)
    open(path, 'w').write(s.replace(old, new, count))

f = 'switch_launcher/source/forwarder.cpp'
sub(f, 'std::string g_forwarderDirectGamePath;',
       'std::string g_forwarderDirectGamePath;\nstd::string g_forwarderGatePath;')
sub(f, 'const std::string nroPath = resolveLauncherPath();\n    if (nroPath.empty())\n        return kForwarderIoError;\n    const bool quotePath',
       'const std::string nroPath = (!g_forwarderDirectGamePath.empty() && !g_forwarderGatePath.empty())\n                                    ? g_forwarderGatePath : resolveLauncherPath();\n    if (nroPath.empty())\n        return kForwarderIoError;\n    const bool quotePath')

m = 'asce_installer/source/main.cpp'
sub(m, 'static bool exists(', 'extern std::string g_forwarderGatePath;\n\nstatic bool exists(')
sub(m, '        bool done = forwarder_create_direct(',
       '        g_forwarderGatePath = GATE_PATH;\n        bool done = forwarder_create_direct(')
sub(m, '    if (!exists("sdmc:/switch/cemu/cemu.nro")) {',
       '    if (!exists(GATE_PATH)) {\n        printf("  FALTA: " GATE_PATH "\\n");\n        printf("  Copia la carpeta switch del zip a la raiz de la SD.\\n\\n");\n        ok = false;\n    }\n    if (!exists("sdmc:/switch/cemu/cemu.nro")) {')

b = 'tools/build-game.sh'
sub(b, "#define GAME_PATH_ALT %s\n''' % (", "#define GAME_PATH_ALT %s\n#define GATE_PATH \"sdmc:/switch/cemu-%s/gate.nro\"\n''' % (")
sub(b, '"""${GAME_PATH_ALT:-}"""))', '"""${GAME_PATH_ALT:-}"""), "$ROOT_NAME")')
sub(b, 'echo "[4/4] Zip para la SD"',
       'echo "[3b] Puerta de 5 s"\nmake -C wiiu-gate clean\nmake -C wiiu-gate TARGET=gate APP_TITLE="$APP_NAME" APP_AUTHOR="$APP_AUTHOR"\n\necho "[4/4] Zip para la SD"')
sub(b, 'cp "$I/instalar-$ROOT_NAME.nro" "$OUT/switch/cemu-$ROOT_NAME/"',
       'cp "$I/instalar-$ROOT_NAME.nro" "$OUT/switch/cemu-$ROOT_NAME/"\ncp wiiu-gate/gate.nro "$OUT/switch/cemu-$ROOT_NAME/gate.nro"')
sub(b, '"4. Vuelve al HOME: ya esta el icono. Se abre directo en el juego." \\',
       '"4. Vuelve al HOME: ya esta el icono. Al abrirlo tienes 5 segundos para" \\\n  "   tocar la pantalla y abrir los ajustes; si no, entra directo al juego." \\\n  "   SIN sigpatches: no hace falta el instalador; abre \\"$APP_NAME\\" directo desde el Homebrew Menu (mismo conteo)." \\')
