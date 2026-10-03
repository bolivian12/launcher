# EBALIA Launcher 4

Reconstrucción nativa en C++20 / Qt 6. Barra lateral con ilustraciones de colores propias y la cuenta arriba, como el Minecraft Launcher; los controles usan iconos [Lucide](https://lucide.dev) (licencia ISC). **Inicio** tiene fondos que cambian automáticamente, el selector de instancia que se abre hacia arriba, el botón verde **JUGAR** y las novedades con sus imágenes completas. **Instancias** muestra tarjetas agrupadas, como las páginas de perfiles de los launchers modernos, y cada instancia tiene su propia página sobre su imagen, con mods, paquetes de recursos, shaders y mundos. Las ventanas como Nueva instancia, Ajustes de instancia, Mods o Registro son ventanas normales: se mueven y redimensionan por separado (GNOME pega las ventanas modales a la principal) y mientras están abiertas la principal queda en pausa.

El carrusel de Inicio alterna cuatro fondos cada nueve segundos, con transición, flechas, indicadores y pausa persistente. Deja de rotar al cambiar de página. Cada una de las 15 versiones perdidas tiene una portada distinta, asignada por su identificador. Las 19 imágenes se incluyen localmente para funcionar sin conexión; proceden de los [fondos oficiales de Minecraft](https://www.minecraft.net/en-us/collectibles), con las fuentes de cada archivo en `resources/art/backgrounds/sources.json`. Las portadas del archivo son ilustrativas, no capturas históricas de esos paquetes. Los proveedores de modpacks usan sus logos, con créditos en `resources/icons/providers`.

La ventana admite 640×480 y tamaños mayores. Al reducir el ancho, la barra lateral conserva los iconos y sus ayudas, Jugar pasa a otra fila y los paneles se apilan. En ventanas bajas se desplaza el contenido para conservar el tamaño y la separación de los controles. El creador de instancias mantiene sus botones de confirmación fuera del área desplazable.

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
- **Instancias**: tarjetas por grupo (se pliegan, renombran o quitan), pestañas Todas / Vanilla / Con mods / Modpacks, búsqueda, botón verde Jugar y engranaje de ajustes en cada tarjeta, menú contextual y tecla Supr. La página de una instancia muestra cargador, versión, memoria y grupo, y permite jugar, abrir registro o carpeta, exportar, copiar, cambiar de grupo, buscar mods, guardar los mods como pack y eliminar.
- **Eliminar instancia** mueve la carpeta a la papelera local con sus mundos. Si Windows bloquea la carpeta porque un programa usa un archivo, se intenta la papelera del sistema y, si tampoco se puede, se explica qué cerrar.
- **Copiar instancia** duplica mundos, mods y ajustes en una instancia independiente. **Exportar** genera un ZIP que se vuelve a abrir con Importar.
- **Nueva instancia** con el esquema de Prism Launcher: nombre, grupo e ícono arriba; a la izquierda Personalizado, Importar, ATLauncher, CurseForge, FTB, FTB Legacy, Importar app de FTB, Modrinth y Technic. Cada proveedor se busca dentro de la misma ventana (ícono, descripción y versión; se preselecciona la última estable). El nombre se completa solo con la versión o el nombre del pack.
- **Importar** acepta archivo, carpeta o enlace https, también arrastrándolo a la ventana: Modrinth (.mrpack), CurseForge (.zip), exportaciones o carpetas de Prism Launcher / PolyMC / MultiMC, Technic y ATLauncher (.zip), carpetas de la app de CurseForge y de FTB, exportaciones de EBALIA y packs de EBALIA (.json). Lista las instancias de Prism, PolyMC, MultiMC, CurseForge y la app de FTB que ya existen en el equipo. Las instancias con LiteLoader o mods dentro de minecraft.jar se rechazan con una explicación.
- **Java automático**: se buscan Java en PATH, JAVA_HOME, el registro de Windows (Oracle, Temurin/Adoptium, Microsoft, Zulu, Liberica, Corretto, Semeru), Program Files, los runtimes del Minecraft Launcher (incluida la versión de Microsoft Store), CurseForge, FTB App, Technic, ATLauncher, Modrinth App, GDLauncher, Prism/PolyMC/MultiMC (y las Java configuradas en ellos), Gradle, IntelliJ, SDKMAN, asdf, mise, scoop, /usr/lib/jvm, /opt y Nix. Se usa la versión exacta que pide Minecraft o la más cercana más nueva cuando el juego ya requiere Java 16+. Si no hay ninguna, se descarga la Java oficial de Mojang que indica la versión (con verificación SHA-1), igual que el Minecraft Launcher.
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

Fuera del paquete Nix no hace falta instalar Java a mano: si no se detecta una compatible, la instancia descarga durante la instalación la Java oficial de Mojang (en `mc/java`). En Ajustes de instancia podés elegir otro ejecutable; si no es compatible, el mensaje indica qué versión tiene y cuál pide Minecraft. `EBALIA_JAVA_PATHS` permite pasar varias rutas a ejecutables, separadas con el separador de rutas de la plataforma. En NixOS no se descarga la Java genérica de Mojang (no funciona allí): se usan las del paquete Nix.

## Versiones perdidas

Los paquetes del archivo son `.bat`/`.exe` originales que llaman a `java` con las bibliotecas nativas de Windows. Al pulsar Jugar, EBALIA usa una Java 8 detectada o instala la `jre-legacy` oficial de Mojang. En Linux y macOS se ejecutan en Wine con un prefijo propio (`<datos>/wine`, o `EBALIA_WINEPREFIX`), con la Java 8 de Windows en `WINEPATH`; no se toca tu configuración de Wine. Detener una versión cierra ese prefijo. Se probó Alpha 1.2.7 hasta su pantalla de título en Wine.

**Preparación y diagnóstico** lista las Java detectadas, comprueba Java 8 (o Wine y la Java 8 de Windows), OpenGL y OpenAL, permite instalar la Java 8 oficial con un clic y volver a comprobar todo. Estos paquetes necesitan OpenAL para el sonido: en Windows se verifica `OpenAL32.dll` en el sistema; abrir el instalador no cuenta como instalado hasta que la comprobación lo encuentra.

## Jugar

Jugar instala lo que falte (cliente, cargador, Java) y abre el juego en el mismo paso. Si todavía no hay cuenta, solo se pide un nombre de jugador para un perfil local.

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

El código anterior está respaldado en `legacy/source-v3.tar.gz`. No se modificó el proyecto `PrismLauncher-Cracked-main`; se consultó su organización de componentes como referencia.

## Verificación

```sh
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
EBALIA_LIVE_TESTS=1 QT_QPA_PLATFORM=offscreen ./build/ebalia-tests liveModrinth
EBALIA_DATA_DIR=/tmp/ebalia-install-test QT_QPA_PLATFORM=offscreen ./build/ebalia-launcher --installtest 1.20.1 fabric
```

La prueba de red busca Sodium, lo descarga/verifica, captura un pack, resuelve otra versión y consulta Forge/NeoForge. Las pruebas sin red cubren aislamiento, papelera, argumentos con espacios, reglas, dependencias, hashes inválidos, errores de descarga, archivos locales, packs, extracción y completitud de los diez catálogos. Las pruebas de UI recorren las diez páginas en los diez idiomas y crean dos instancias homónimas mediante los controles reales. También verifican el carrusel, su pausa, las 15 imágenes distintas y la disposición de las páginas entre 640×480 y 1920×1080.

Otras comprobaciones manuales:

```sh
./build/ebalia-launcher --javatest                 # Java detectadas
./build/ebalia-launcher --javadownload 21          # instala la Java oficial de Mojang
./build/ebalia-launcher --lost-launchtest alpha_1.2.7   # Java 8 + Wine/Windows, 40 s de juego
EBALIA_TEST_ARTIFACTS=/tmp/shots QT_QPA_PLATFORM=offscreen ./build/ebalia-ui-tests screenshots
EBALIA_LIVE_TESTS=1 QT_QPA_PLATFORM=offscreen ./build/ebalia-tests liveModpackInstall livePackCatalogs
```

`EBALIA_TEST_ARTIFACTS` guarda capturas de inicio y guía por idioma. `EBALIA_NO_NETWORK=1` evita consultas de inicio durante pruebas de UI. `--selftest <0..6>` guarda una captura y cierra; `EBALIA_SCREENSHOT` define su destino.

Fuentes de integración: [Modrinth API](https://docs.modrinth.com/api/), [Fabric Meta](https://meta.fabricmc.net/), [instalador oficial de Forge](https://github.com/MinecraftForge/Installer).

## Skins

En el modo cliente, la pestaña **Skins** permite importar PNG de 64×64 o 64×32, elegir modelo Classic/Slim y ver el frente o la espalda. Aplicar a Microsoft renueva la sesión y sube el archivo al servicio oficial; se debe verificar con una cuenta real cuando esté configurado el OAuth de EBALIA.

**Aplicar localmente** genera un resource pack por instancia, sin instalar un mod adicional. Necesita PNG de 64×64 y una versión del juego que publique su formato de recursos y sus texturas predeterminadas. El formato se lee del cliente instalado, no de una tabla fija. Se generan texturas para brazos clásicos y finos. Esta opción cambia la apariencia de las skins predeterminadas en ese cliente, por lo que puede afectar a otros jugadores con skin predeterminada; no publica la skin para otros usuarios ni cambia el modelo que el juego asigna al UUID. El botón Restaurar apariencia local desactiva ese pack conservando los demás packs y ajustes.

## CurseForge y Modrinth

En **Explorar mods**, elegí Modrinth o CurseForge y seleccioná uno o varios resultados. Ambos proveedores usan el mismo flujo de revisión, dependencias y archivos verificados. Los packs pueden conservar referencias de los dos orígenes y recordar los mods que este launcher instaló.

CurseForge exige una clave de API para aplicaciones externas. Para esta prueba local se configuró en `curseforge-api-key.txt` la misma clave presente en `PrismLauncher-Cracked-main`, según lo solicitado. Ese archivo está excluido de Git y CMake lo incorpora al compilar, también en el paquete Nix. Para cambiarla, reemplazá el archivo junto a `CMakeLists.txt` y volvé a configurar y compilar, o pasá `-DEBALIA_CURSEFORGE_API_KEY=...` a CMake. Sin clave incorporada, la página de CurseForge muestra un campo para pegarla una vez; también sirve **Ajustes → Configuración de proveedores** o `EBALIA_CURSEFORGE_API_KEY`. Si un autor no permite descarga directa, se ofrece el enlace oficial; después podés agregar el JAR desde Administrar mods. No se fabrican enlaces CDN para eludir la restricción. Un JAR importado manualmente que no esté identificado en el registro del launcher ni en Modrinth no se incluye en un pack por suposiciones sobre el nombre.

El adaptador de CurseForge se prueba con respuestas controladas, dependencias y archivos SHA-1 (el registro local conserva además SHA-512). Las pruebas de red usan la clave configurada: buscan y descargan JEI para Forge 1.20.1, verifican su SHA-1 y consultan modpacks y versiones. La prueba de interfaz busca un modpack de CurseForge y comprueba que se pueda seleccionar una versión y crear la instancia. [Documentación oficial de CurseForge](https://docs.curseforge.com/rest-api/).

```sh
EBALIA_LIVE_TESTS=1 QT_QPA_PLATFORM=offscreen ./build/ebalia-tests liveCurseForge
EBALIA_LIVE_TESTS=1 EBALIA_TEST_ARTIFACTS=./artifacts QT_QPA_PLATFORM=offscreen ./build/ebalia-ui-tests liveCurseForgeBrowser
```

## Mis Mods y noticias de Patreon

Inicio incluye **Mis Mods**, una ventana con los iconos y enlaces originales de `ooo.jar`, `In Your World` y `Secret 01 · SOON`, y una invitación a Patreon que puede cerrarse o desactivarse. La invitación automática aparece una sola vez, cuando Inicio está activo; siempre se puede abrir desde su botón.

**Noticias** separa Minecraft de **EBALIA · Patreon**. Patreon distingue publicaciones públicas y publicaciones autorizadas para una cuenta con membresía activa de pago. El servidor recibe webhooks firmados, avisa a los launchers conectados y vuelve a verificar la membresía antes de entregar publicaciones privadas. Se conserva una consulta de respaldo cada minuto. Sin el servicio configurado no se simula que una cuenta esté enlazada ni que haya contenido privado disponible.

El servicio está en [services/patreon](services/patreon/README.md), con plantilla **Render Free + Neon Free**, contenedor y pruebas. La guía explica qué hacer con la aplicación OAuth de Patreon ya creada, la URL de redirección y el webhook. Los secretos quedan en variables del servidor; el ejecutable incorpora únicamente su URL pública mediante `patreon-service-url.txt` o `EBALIA_PATREON_SERVICE_URL` de CMake.

## Rendimiento e iconos de los paquetes

La decodificación de portadas trabaja fuera del hilo de la interfaz, con dos tareas como máximo. Las miniaturas tienen una caché separada para que un fondo grande no las expulse durante el scroll. Las solicitudes simultáneas se agrupan y los resultados de widgets cerrados se descartan. En la prueba local de 1920×1080, el scroll del archivo pasó de 716 ms a 3,6 ms de media por actualización; estas mediciones no garantizan la misma cifra en cualquier equipo.

El paquete de Windows incorpora `ebalia.ico` con seis tamaños (16–256 px), macOS incluye `ebalia.icns` dentro del bundle y Linux instala PNGs de 16–512 px en el tema hicolor, junto al archivo `.desktop` y la identidad de ventana. `packaging/verify-icons.py` verifica los contenedores y el paquete nativo; el workflow lo ejecuta después de instalar cada plataforma. Linux se verificó localmente; una ejecución de CI en Windows y macOS sigue siendo necesaria para confirmar sus paquetes.

Para analizar la carpeta final de Windows en un equipo con Microsoft Defender activo:

```powershell
./packaging/check-windows-release.ps1 -PackagePath ./package/bin
# En una distribución firmada:
./packaging/check-windows-release.ps1 -PackagePath ./package/bin -RequireSignature
```

La comprobación no modifica Defender ni agrega exclusiones. Un resultado sin detecciones corresponde a esos archivos y esa versión de las firmas. No garantiza resultados futuros ni elimina por sí solo avisos de SmartScreen. La firma requiere un certificado real del editor; no se ha firmado ni escaneado un ejecutable Windows desde este equipo Linux. Los falsos positivos se envían al [portal oficial de Microsoft](https://www.microsoft.com/wdsi/filesubmission).
