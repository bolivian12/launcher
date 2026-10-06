<p align="center">
  <img src="site/assets/icon.png" width="96" height="96" alt="Icono de EBALIA Launcher">
</p>

<h1 align="center">EBALIA Launcher</h1>
<p align="center">Minecraft Java, modpacks, Lost Versions y las creaciones de EBALIA.</p>
<p align="center">
  <a href="https://ebalia-launcher.gitgud.site/#descargas">Descargar</a> ·
  <a href="https://github.com/ebalia-real/launcher/releases">Versiones</a> ·
  <a href="https://www.patreon.com/EBALIA">Patreon</a> ·
  <a href="https://github.com/ebalia-real/launcher/issues">Reportar un problema</a>
</p>

[![Compilación y pruebas](https://github.com/ebalia-real/launcher/actions/workflows/ebalia-launcher.yml/badge.svg)](https://github.com/ebalia-real/launcher/actions/workflows/ebalia-launcher.yml)

## Descargar e instalar

La [página oficial](https://ebalia-launcher.gitgud.site/#descargas) detecta el idioma del navegador, muestra la versión publicada y ofrece las descargas sin salir a una página de GitHub. Consulta las versiones de GitHub y GitGud cada minuto mientras está abierta.

| Sistema | Paquete | Instalación |
| --- | --- | --- |
| Windows 10 / 11 · x64 | [Instalador `.exe`](https://github.com/ebalia-real/launcher/releases/latest/download/ebalia-windows-x64-setup.exe) · [Portable `.zip`](https://github.com/ebalia-real/launcher/releases/latest/download/ebalia-windows-x64-portable.zip) | Instalador: ejecutalo y seguí el asistente (incluye Visual C++ y el acceso directo). Portable: descomprimilo en cualquier carpeta o USB y abrí `ebalia-launcher.exe`, sin instalar. |
| macOS · Apple Silicon | [`.dmg`](https://github.com/ebalia-real/launcher/releases/latest/download/ebalia-macos-arm64.dmg) · [Portable `.zip`](https://github.com/ebalia-real/launcher/releases/latest/download/ebalia-macos-arm64-portable.zip) | Abrí la imagen y copiá la aplicación a tus aplicaciones, o descomprimí la versión portable donde quieras. |
| macOS · Intel | [`.dmg`](https://github.com/ebalia-real/launcher/releases/latest/download/ebalia-macos-x64.dmg) · [Portable `.zip`](https://github.com/ebalia-real/launcher/releases/latest/download/ebalia-macos-x64-portable.zip) | Igual que Apple Silicon. |
| Linux · x64 (cualquier distribución) | [AppImage](https://github.com/ebalia-real/launcher/releases/latest/download/ebalia-linux-x86_64.AppImage) · [Portable `.tar.gz`](https://github.com/ebalia-real/launcher/releases/latest/download/ebalia-linux-x86_64-portable.tar.gz) | Trae Qt, libarchive y OpenSSL: `chmod +x ebalia-linux-x86_64.AppImage` y abrilo. Funciona en distribuciones con glibc 2.35+ (Ubuntu 22.04+, Debian 12+, Fedora, Arch, openSUSE…). |
| NixOS | Flake del proyecto | Nix prepara las dependencias y compila el launcher. Usá las instrucciones de abajo. |

Las versiones **portables** guardan instancias, mundos, cuentas y ajustes en la carpeta `ebalia-data` de al lado, gracias al archivo `portable.txt` que incluyen; podés llevarlas en un USB. Los enlaces apuntan a la última versión publicada; los paquetes de una versión nueva aparecen después de que sus compilaciones y pruebas terminen. Windows y macOS se distribuyen actualmente sin firma digital del editor (en macOS, la primera vez abrí la app con clic derecho → Abrir).

### NixOS

```sh
mkdir ebalia-launcher-1.1.0
cd ebalia-launcher-1.1.0
nix --extra-experimental-features 'nix-command flakes' build 'github:ebalia-real/launcher/v1.1.0?dir=ebalia-launcher'
./result/bin/ebalia-launcher
```

Creá la carpeta sin `sudo`. La primera compilación puede tardar y descargar dependencias. Para Bedrock en NixOS también hace falta habilitar Flatpak en la configuración del sistema.

## Novedades de 1.1.0

- **Actualizar y reiniciar:** el launcher descarga y verifica la versión nueva, comprueba su arranque y reemplaza los archivos del programa. No hace falta abrir la web para las actualizaciones compatibles posteriores.
- **Datos conservados:** actualizar mantiene instancias, Lost Versions, mundos, mods, cuentas, ajustes y contenido descargado. La copia anterior del programa y los temporales se eliminan después de un arranque correcto.
- **Guía interactiva inicial:** un recorrido de 14 pasos destaca los controles y explica las secciones. Tiene Atrás, Siguiente y Omitir; se puede repetir desde **Guía y tutorial**.
- **Instalador Windows:** detecta una instalación existente, incluye las DLL de Visual C++ y crea el acceso directo.
- **Lost Versions:** botón para desinstalar con confirmación. Las operaciones muestran su progreso en la barra inferior.
- **Bedrock · Beta:** preparación y apertura de un gestor externo, con requisitos de cuenta diferenciados según el sistema.

Si venís de **1.0.0**, instalá **1.1.0 una vez** para obtener el nuevo actualizador. No hace falta desinstalar previamente ni borrar tus datos. [Notas de la versión](ebalia-launcher/CHANGELOG.md).

## Qué podés hacer

| Sección | Funciones |
| --- | --- |
| Inicio | Fondos rotativos, selector de instancia, Jugar, noticias y Mis Mods. |
| Instancias | Perfiles independientes con mundos, mods y ajustes propios; grupos, copia, importación, exportación y registros. |
| Mods y modpacks | Vanilla, Fabric, Quilt, Forge y NeoForge; integración con Modrinth, CurseForge, ATLauncher, FTB y Technic. Algunas plataformas requieren su configuración de API. |
| Lost Versions | Archivo de versiones de EBALIA, portadas distintas, instalación independiente y diagnóstico. Los paquetes originales de Windows requieren Windows o Wine. |
| Mis packs y skins | Selecciones reutilizables de mods y administración de skins según el tipo de cuenta. |
| Noticias y comunidad | Noticias de Minecraft, publicaciones de EBALIA, Mis Mods y Patreon. El contenido privado requiere una membresía válida vinculada. |
| Ajustes y guía | Configuración de Java, recursos, idioma y acceso permanente al recorrido interactivo. |

La interfaz principal dispone de diez idiomas y se adapta desde 640 × 480. La guía interactiva nueva está disponible en español e inglés. Java se detecta o se prepara según las necesidades de la instancia; el paquete Nix incorpora las rutas necesarias.

## Actualizaciones y archivos del usuario

El launcher consulta versiones estables al iniciar y periódicamente. El aviso permite **Actualizar y reiniciar** o **Más tarde**. Antes de actualizar hay que cerrar las partidas administradas por EBALIA y esperar a que terminen las instalaciones.

Los paquetes se descargan por HTTPS y se verifican con el SHA-256 publicado. La versión nueva se prepara y prueba antes de reemplazar los archivos del programa. Si la preparación falla, la versión actual permanece abierta; si falla el reemplazo, se restauran los archivos anteriores.

El actualizador de Windows/Linux utiliza un manifiesto de archivos del programa: los archivos ajenos a ese manifiesto se conservan. En macOS se reemplaza el bundle de la aplicación. La carpeta de instalación debe permitir escritura para tu usuario. En NixOS se construye la nueva versión con Nix y se cambia la versión activa; las rutas antiguas compartidas del almacén quedan sujetas a la recolección de basura de Nix.

Actualizar el launcher **no actualiza automáticamente la versión de Minecraft de tus instancias**. Tampoco equivale a desinstalar una Lost Version: esa acción separada mueve su carpeta y sus mundos a la papelera, después de una confirmación.

## Bedrock · Beta

Bedrock se integra mediante gestores externos y no incluye el juego ni una licencia.

| Sistema | Gestor | Cuenta necesaria para obtener el juego |
| --- | --- | --- |
| Windows | [BedrockLauncher](https://github.com/BedrockLauncher/BedrockLauncher) | Microsoft con Minecraft para Windows o una suscripción de Game Pass que incluya esa edición. |
| Linux / macOS | [Minecraft Linux Launcher](https://minecraft-linux.github.io/) | Google Play con la edición Android. Microsoft/Xbox se utiliza por separado para funciones del juego. |

**Comprar Bedrock para Windows no habilita la descarga Android en Linux/macOS.** El botón prepara el gestor y las dependencias compatibles; el inicio de sesión y la descarga del juego se completan en ese gestor. Algunas configuraciones requieren una acción del sistema, como habilitar Flatpak o el modo desarrollador de Windows.

Se verificó la instalación del gestor en Linux. No se ha validado una partida real de Bedrock en todos los sistemas ni se garantiza compatibilidad con la última versión del juego en todas las GPU. [Detalles de la integración](ebalia-launcher/BEDROCK.md).

## Compilar y colaborar

El código está en [`ebalia-launcher/`](ebalia-launcher). Requiere C++20, CMake ≥ 3.21, Qt ≥ 6.4 y libarchive. Las pruebas también usan Qt Test.

```sh
git clone https://github.com/ebalia-real/launcher.git
cd launcher/ebalia-launcher
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix package
```

En Linux sin pantalla, ejecutá CTest con `QT_QPA_PLATFORM=offscreen`. Windows usa MSVC/vcpkg; macOS usa Qt y libarchive de Homebrew. El [workflow](.github/workflows/ebalia-launcher.yml) compila y prueba cada plataforma. Las etiquetas de versión publican los paquetes y sus checksums solo después de que las tres plataformas pasen.

- [Documentación técnica y configuración](ebalia-launcher/README.md)
- [Servicio de Patreon](ebalia-launcher/services/patreon/README.md)
- [Código de la web](site/README.md)
- [Repositorio espejo en GitGud](https://gitgud.io/castigarse/launcher)

No publiques contraseñas, tokens ni claves privadas al reportar errores. Incluí el sistema, la versión del launcher y el registro relevante.

Proyecto independiente de EBALIA, no afiliado a Mojang ni Microsoft. Las cuentas y licencias de Minecraft no se incluyen.
