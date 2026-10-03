# Actualizaciones del launcher

El launcher consulta automáticamente Releases al iniciar (5 segundos después) y cada seis horas. GitHub: https://github.com/bolivian12/launcher/releases ; respaldo GitGud: https://gitgud.io/castigarse/launcher/-/releases . No requiere credenciales del usuario.

Publicar una release estable con tag vMAJOR.MINOR.PATCH, por ejemplo v4.0.1, y paquetes para cada sistema. Incrementar PROJECT_VERSION en CMakeLists.txt antes de compilar: esa es la versión incluida en el ejecutable. Commits, ramas, borradores y prereleases no activan avisos. En GitGud crear una Release, no solo un tag. Usar el mismo tag y binarios para ambos sitios.

El botón de actualización abre la página oficial de la release para descargar el paquete de Windows, Linux o macOS. Esta implementación detecta versiones automáticamente; no reemplaza ejecutables ni instala silenciosamente. No marca una descarga como instalación completada. La versión se compara con el ejecutable realmente iniciado.

No hay releases publicadas actualmente. La detección se verificó con versiones simuladas; falta una release real con sus paquetes para probar una actualización completa. GitGud fue creado con README inicial; todavía no contiene el código local del launcher. No hay sincronización automática del código entre repositorios.
