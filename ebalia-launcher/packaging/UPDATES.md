# Actualizaciones del launcher

El launcher consulta automáticamente Releases al iniciar (5 segundos después) y cada seis horas. GitHub: https://github.com/ebalia-real/launcher/releases ; respaldo GitGud: https://gitgud.io/castigarse/launcher/-/releases . No requiere credenciales del usuario.

Publicar una release estable con tag vMAJOR.MINOR.PATCH, por ejemplo v1.0.1, y paquetes para cada sistema. Incrementar PROJECT_VERSION en CMakeLists.txt antes de compilar: esa es la versión incluida en el ejecutable. Commits, ramas, borradores y prereleases no activan avisos. En GitGud crear una Release, no solo un tag. Usar el mismo tag y binarios para ambos sitios.

El botón de actualización abre la página oficial de la release para descargar el paquete de Windows, Linux o macOS. Esta implementación detecta versiones automáticamente; no reemplaza ejecutables ni instala silenciosamente. No marca una descarga como instalación completada. La versión se compara con el ejecutable realmente iniciado.

La versión 1.0.0 está publicada en GitHub y GitGud. Los paquetes Linux x64 y macOS ARM64 están disponibles; Windows x64 también está publicado tras superar sus pruebas. No hay sincronización automática del código entre repositorios.

La web https://ebalia-launcher.gitgud.site consulta las releases estables de ambos repositorios cada 60 segundos mientras está visible y al volver a la pestaña. Elige la versión semántica más reciente, descarta borradores y prereleases, y muestra únicamente los paquetes de esa versión. Cuando un paquete todavía no existe, el panel indica que se está preparando. Publicar un commit o subir código sin crear una release no cambia la versión mostrada.
