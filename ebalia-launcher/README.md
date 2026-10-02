# EBALIA Launcher 4

Reconstrucción nativa en C++20 / Qt 6. El inicio sigue el flujo de Minecraft Launcher: selector de instancia, acceso directo a su carpeta y botón principal para instalar/jugar. La pestaña de instalaciones conserva las herramientas avanzadas.

## Incluido

- Dos modos: cliente Minecraft y archivo de versiones perdidas de EBALIA.
- Instancias con directorios UUID independientes, aunque compartan nombre y versión. Mundos, mods, opciones, Java y memoria separados.
- Catálogo oficial de Minecraft al iniciar y cada 30 minutos; copia local ante fallos de conexión. Snapshots y versiones antiguas son opcionales.
- Vanilla, Fabric, Quilt, Forge y NeoForge. Versiones del cargador consultables; selección automática o explícita. Forge/NeoForge ejecutan sus instaladores oficiales completos, incluidos los procesadores, en vez de limitarse a mezclar JSON.
- Noticias oficiales de Minecraft con copia local y enlaces a Minecraft.net. Los artículos conservan el idioma publicado por Mojang.
- Búsqueda Modrinth filtrada por Minecraft/cargador, resolución de dependencias obligatorias y vista previa. Archivos verificados con SHA-512 y descargas preparadas antes de modificar mods instalados.
- Packs personales guardados como referencias a proyectos: al aplicarlos se buscan ediciones compatibles con el destino. Una ausencia o dependencia contradictoria se muestra antes de instalar. Se puede elegir omitir los no disponibles.
- Captura de mods locales mediante su hash de Modrinth. No se adivina la compatibilidad de archivos desconocidos. Los desactivados no se incluyen.
- Exportación/importación JSON de packs, activación/desactivación, JAR locales y carpeta de mods retirados.
- Papelera reversible de instancias, reparación del cliente y visor de registros.
- Diez idiomas: español, inglés, portugués, alemán, francés, italiano, ruso, japonés, coreano y chino simplificado. Interfaz y tutorial incluidos localmente; no se traduce mediante servicios externos durante el uso. Los mensajes técnicos de Java, proveedores y algunos errores del motor conservan su texto original.

## Ejecutar en NixOS / Linux de este equipo

Desde `ebalia-launcher`:

```sh
nix build
./result/bin/ebalia-launcher
```

El paquete Nix incorpora rutas a Java 8, 17, 21 y 25 para que el launcher elija la versión que declara Minecraft. Las instancias existentes no se actualizan automáticamente de versión; esto evita cambiar mundos o mods sin revisión.

Para desarrollo, evitá que `nix develop` genere rutas de salida en un directorio con espacios: ejecutá el entorno desde `/tmp` y pasá rutas absolutas a CMake. `scripts/build-local.sh` automatiza esto.

## Compilar en Windows, Linux o macOS

Requisitos: CMake 3.21+, compilador C++20, Qt 6.4+ (Widgets, Network, Concurrent, Test) y libarchive.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix package
```

En Windows se usa MSVC y libarchive mediante vcpkg. En macOS se usa Qt/libarchive de Homebrew y `macdeployqt`. El workflow `.github/workflows/build.yml` compila, prueba y empaqueta por separado en Ubuntu, Windows y macOS. Este workflow debe ejecutarse en un repositorio de EBALIA; su presencia no equivale a una ejecución exitosa en las tres plataformas. Las builds de CI sin credenciales de firma se identifican explícitamente como unsigned.

Fuera del paquete Nix, instalá Java para las versiones que uses. El launcher indica la versión requerida por los metadatos. En Ajustes de instancia podés elegir el ejecutable; también se detectan JAVA_HOME, PATH y ubicaciones habituales. `EBALIA_JAVA_PATHS` permite pasar varias rutas a ejecutables, separadas con el separador de rutas de la plataforma.

## Cuentas Microsoft

Los perfiles locales están disponibles. El acceso Microsoft requiere registrar una aplicación propia de EBALIA como cliente público y habilitar el acceso correspondiente a Minecraft/Xbox. Configurá `EBALIA_MS_CLIENT_ID` o `auth/microsoftClientId` en QSettings. No se reutiliza el identificador de aplicación de Prism. No se guarda una contraseña Microsoft; se usan device code y refresh tokens. En sistemas POSIX el archivo de cuentas solo tiene permisos de lectura/escritura para su propietario. No es un almacén cifrado de credenciales.

El inicio de sesión con una cuenta real debe verificarse con el registro OAuth propio. Los tokens se renuevan antes de lanzar una instancia autenticada.

## Windows, firma y antivirus

El ejecutable tiene icono, metadatos de versión y manifiesto `asInvoker`; no requiere administrador, no usa UPX ni un empaquetador de ejecutables. El paquete nuevo no incorpora los ejecutables auxiliares del launcher anterior.

Para distribuirlo, firmá el ejecutable final con un certificado Authenticode confiable o un servicio de firma de artefactos y verificá la firma. `packaging/sign-windows.ps1` usa un certificado que ya esté en el almacén de Windows. No se incluyen claves privadas ni se inventa una firma.

La firma **no garantiza** que desaparezcan los avisos de SmartScreen ni las detecciones de antivirus. Microsoft documenta que incluso un binario nuevo firmado puede mostrar advertencias mientras adquiere reputación: https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation . Un falso positivo real debe enviarse al proveedor para revisión; no se desactiva Defender ni se agregan exclusiones.

macOS necesita Developer ID y notarización para distribución sin avisos habituales de Gatekeeper. `packaging/sign-macos.sh` prepara ese proceso usando una identidad y un perfil de llavero existentes.

## Datos, compatibilidad y recuperación

Se conserva la ubicación anterior `QStandardPaths::AppDataLocation` de `EBALIA / EBALIA Launcher`. El cliente usa `mc/instances`, comparte descargas verificadas en `mc/libraries` y `mc/assets`, y guarda los packs en `packs`. `EBALIA_DATA_DIR` permite probar todo con un directorio independiente.

Una instancia del motor anterior necesita **Reparar instalación** una vez para generar su `launch-profile.json`. Los mundos y mods no se borran. Las versiones perdidas requieren volver a instalarse una vez para crear su marcador de instalación; la carpeta previa se conserva como `.backup-<fecha>`.

La extracción de ZIP rechaza rutas que salen del destino y enlaces simbólicos. Los paquetes del archivo de EBALIA conservan su comando original; los `.exe`/`.bat` necesitan Windows o Wine. No todos los paquetes históricos o enlaces originales están necesariamente disponibles. El catálogo histórico permanece incorporado; la consulta automática de versiones nuevas corresponde al catálogo oficial de Minecraft.

Forge antiguo puede usar un instalador sin modo de cliente por consola; esos instaladores muestran un error y su registro, no una instalación falsamente exitosa. La compatibilidad de un mod se determina por los metadatos publicados en Modrinth, no garantiza que cualquier combinación funcione en el juego. No se distribuyen archivos de mods dentro del JSON de un pack, ni configuraciones o mundos.

El código anterior está respaldado en `legacy/source-v3.tar.gz`. No se modificó el proyecto `PrismLauncher-Cracked-main`; se consultó su organización de componentes como referencia, sin copiar su código ni sus credenciales.

## Verificación

```sh
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
EBALIA_LIVE_TESTS=1 QT_QPA_PLATFORM=offscreen ./build/ebalia-tests liveModrinth
EBALIA_DATA_DIR=/tmp/ebalia-install-test QT_QPA_PLATFORM=offscreen ./build/ebalia-launcher --installtest 1.20.1 fabric
```

La prueba de red busca Sodium, lo descarga/verifica, captura un pack, resuelve otra versión y consulta Forge/NeoForge. Las pruebas sin red cubren aislamiento, papelera, argumentos con espacios, reglas, dependencias, hashes inválidos, errores de descarga, archivos locales, packs, extracción y completitud de los diez catálogos. La prueba de UI recorre las siete páginas en los diez idiomas y crea dos instancias homónimas mediante los controles reales.

`EBALIA_TEST_ARTIFACTS` guarda capturas de inicio y guía por idioma. `EBALIA_NO_NETWORK=1` evita consultas de inicio durante pruebas de UI. `--selftest <0..6>` guarda una captura y cierra; `EBALIA_SCREENSHOT` define su destino.

Fuentes de integración: [Modrinth API](https://docs.modrinth.com/api/), [Fabric Meta](https://meta.fabricmc.net/), [instalador oficial de Forge](https://github.com/MinecraftForge/Installer). La imagen de portada proviene de los recursos que ya incluía este proyecto.

## Skins

En el modo cliente, la pestaña **Skins** permite importar PNG de 64×64 o 64×32, elegir modelo Classic/Slim y ver el frente o la espalda. Aplicar a Microsoft renueva la sesión y sube el archivo al servicio oficial; se debe verificar con una cuenta real cuando esté configurado el OAuth de EBALIA.

**Aplicar localmente** genera un resource pack por instancia, sin instalar un mod adicional. Necesita PNG de 64×64 y una versión del juego que publique su formato de recursos y sus texturas predeterminadas. El formato se lee del cliente instalado, no de una tabla fija. Se generan texturas para brazos clásicos y finos. Esta opción cambia la apariencia de las skins predeterminadas en ese cliente, por lo que puede afectar a otros jugadores con skin predeterminada; no publica la skin para otros usuarios ni cambia el modelo que el juego asigna al UUID. El botón Restaurar apariencia local desactiva ese pack conservando los demás packs y ajustes.

## CurseForge y Modrinth

En **Explorar mods**, elegí Modrinth o CurseForge y seleccioná uno o varios resultados. Ambos proveedores usan el mismo flujo de revisión, dependencias y archivos verificados. Los packs pueden conservar referencias de los dos orígenes y recordar los mods que este launcher instaló.

CurseForge exige una clave de API para aplicaciones externas. Configurala en **Cuentas y ajustes → Configuración de proveedores**, o con `EBALIA_CURSEFORGE_API_KEY`. El registro debe corresponder a EBALIA. Si un autor no permite descarga directa, se ofrece el enlace oficial; después podés agregar el JAR desde Administrar mods. No se fabrican enlaces CDN para eludir la restricción. Un JAR importado manualmente que no esté identificado en el registro del launcher ni en Modrinth no se incluye en un pack por suposiciones sobre el nombre.

El adaptador de CurseForge se prueba con respuestas controladas, dependencias y archivos SHA-1 (el registro local conserva además SHA-512). La prueba de red real requiere la clave propia. [Documentación oficial de CurseForge](https://docs.curseforge.com/rest-api/).
