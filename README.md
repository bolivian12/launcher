# launcher

Código de **EBALIA Launcher 4** (C++20 / Qt 6) en [`ebalia-launcher/`](ebalia-launcher/README.md): instancias de Minecraft con Vanilla, Fabric, Quilt, Forge y NeoForge, modpacks de Modrinth, CurseForge, ATLauncher, FTB y Technic, importación desde Prism Launcher, detección y descarga automática de Java, y el archivo de versiones perdidas de EBALIA.

```sh
cd ebalia-launcher
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
./build/ebalia-launcher
```

En NixOS: `nix build ./ebalia-launcher && ./result/bin/ebalia-launcher`. Requisitos y detalles en el README de la carpeta.

Los ejecutables del launcher anterior (`Bexe16.4`), el proyecto de AutoPlay (`codigo generacion`), las compilaciones y las capturas de prueba no se guardan en el repositorio.
