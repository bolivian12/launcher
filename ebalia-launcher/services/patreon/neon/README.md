# EBALIA Patreon en Neon Free

El servicio se despliega directamente en Neon Functions, junto a PostgreSQL. No requiere Render, GitHub ni mantener la PC encendida. El plan gratuito tiene cuotas; no habilitar ampliaciones de pago.

URL desplegada: https://br-frosty-lake-b5hsqiwp-patreon.compute.c-7.us-east-2.aws.neon.tech

- Salud: `/health`. `patreon_configured: false` indica configuración pendiente; no significa que las publicaciones ya funcionen.
- Retorno OAuth: `/callback` (agregar la URL completa al cliente Patreon).
- Webhook firmado: `/v1/webhook`.
- API del launcher: `/v1/login`, `/v1/feed`, `/v1/events`, `/v1/logout`.

Desplegar con `python deploy.py --credentials /ruta/privada/patreon-creator.json --config-dir /ruta/privada/neon-cli`. El script pasa las credenciales por el entorno del proceso; se verificó que la carga mediante `--env` del CLI no aplicaba los valores en este entorno. `neon.ts` declara solamente la función. Nunca subir ese archivo, las credenciales del CLI o tokens al repositorio. DATABASE_URL se inyecta desde Neon.

Variables EBALIA_PATREON_: CLIENT_ID, CLIENT_SECRET, CAMPAIGN_ID, CREATOR_TOKEN, CREATOR_REFRESH, WEBHOOK_SECRET, PUBLIC_URL. Los tokens de creador deben pertenecer a la campaña EBALIA. La cuenta creadora debe habilitar el acceso campaigns.posts.

Las sesiones OAuth se consumen atómicamente en PostgreSQL. La revisión de noticias se comparte entre instancias, con comprobación cada segundo durante la espera larga. Los cambios de membresía se vuelven a comprobar antes de entregar contenido privado. Nunca se deduce acceso a partir de is_paid: ese campo describe facturación por publicación. Publicaciones privadas sin niveles identificables se omiten hasta que se pueda verificar su audiencia.

Pruebas: `npm ci --ignore-scripts` y `npm test`. Se verificaron además PostgreSQL real, callbacks simultáneos de un solo uso, cierre de sesión, firma del webhook y aviso de revisión con respuestas Patreon simuladas. Verificado con la cuenta creadora real: retorno OAuth, feed público y cierre de sesión. El feed público devuelve una publicación y excluye las tres privadas. Webhook registrado con los nueve eventos; probado el aviso firmado contra el servicio desplegado. Falta una prueba con un suscriptor de pago real.

Documentación: https://neon.com/docs/compute/functions/overview y https://docs.patreon.com/

## Niveles y sincronización automática

El catálogo se consulta desde campaigns/{id}?include=tiers y se almacena como máximo 60 segundos. El launcher refresca cada 60 segundos y ante avisos firmados de membresía/publicaciones. Nuevos niveles y cambios de nombre aparecen sin recompilar (normalmente hasta dos minutos con la caché y conectividad disponible). Los permisos usan currently_entitled_tiers por ID, nunca precios ni nombres fijos. Los niveles Exclusive Chad, Explicit Chad y SENIOR CHAD fueron verificados contra la API; altas, cambios, bajas y un nivel futuro están cubiertos por pruebas simuladas. Esto controla los posts; no define qué versiones de Minecraft corresponden a cada nivel.
