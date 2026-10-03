# Patreon de EBALIA, sin dejar tu PC encendida

**Despliegue actual:** [Neon Functions + Neon PostgreSQL](neon/README.md), ambos en Free. Render queda como alternativa. La cuenta creadora está conectada: feed público, OAuth y aviso firmado verificados. Falta probar una cuenta real de pago.

Esta carpeta es un servicio independiente del launcher. Se puede subir sola a un repositorio privado de GitHub. `render.yaml` está preparado para **Render Free** y una base de datos **Neon Free**. No hay servicios de pago en la plantilla.

## 1. Crear la base de datos gratis

1. Entrá a https://console.neon.tech y elegí el plan **Free**.
2. Creá un proyecto llamado `ebalia-patreon`.
3. En **Connect**, copiá la cadena PostgreSQL, con `sslmode=require`. Esa cadena es una contraseña: pegala únicamente en la variable `DATABASE_URL` de Render, no en el launcher ni en GitHub.

## 2. Subir el servicio a Render

1. En GitHub, creá un repositorio privado, por ejemplo `ebalia-patreon-service`.
2. Subí **el contenido de esta carpeta**, con `server.py`, `Dockerfile`, `requirements.txt` y `render.yaml` en la raíz. No subas la carpeta completa del launcher, archivos `.env` ni credenciales.
3. Entrá a https://dashboard.render.com, conectá GitHub y elegí **New → Blueprint**. Seleccioná ese repositorio.
4. Confirmá que el servicio diga **Free**. No agregues discos ni una base Render Postgres: el plan gratuito de esa base vence a los 30 días.
5. Completá las variables solicitadas. Las encontrás en el cliente que ya creaste en el portal de desarrolladores de Patreon:

| Variable de Render | Valor |
| --- | --- |
| `DATABASE_URL` | Cadena de conexión privada de Neon |
| `EBALIA_PATREON_CLIENT_ID` | Client ID de tu aplicación de Patreon |
| `EBALIA_PATREON_CLIENT_SECRET` | Client Secret de la aplicación, solo en Render |
| `EBALIA_PATREON_CREATOR_TOKEN` | Creator Access Token de tu campaña |
| `EBALIA_PATREON_CREATOR_REFRESH` | Creator Refresh Token correspondiente |
| `EBALIA_PATREON_CAMPAIGN_ID` | Identificador numérico de la campaña EBALIA |
| `EBALIA_PATREON_WEBHOOK_SECRET` | Secreto del webhook; podés agregarlo después de crear el webhook |

El token de creador necesita permiso para consultar publicaciones de tu campaña (`campaigns.posts`). Si tu aplicación de Patreon no ofrece ese acceso, hay que habilitarlo en Patreon antes de que el feed funcione; el launcher no extrae contenido privado de las páginas web.

Render dará una URL parecida a `https://TU-SERVICIO.onrender.com`. **Ese nombre es un ejemplo: usá la URL real de tu panel.** `/health` debe responder `{"ok": true}`. Esta comprobación verifica que el servicio está vivo; `/v1/feed` verifica además que Patreon acepte los tokens y permisos.

## 3. Cambiar la redirección de tu captura

En Patreon → editar tu cliente → **URL de redireccionamiento**, agregá:

```
https://TU-SERVICIO.onrender.com/callback
```

La URL debe coincidir exactamente con el dominio de Render. Los `127.0.0.1` de la configuración anterior sirven para pruebas en el propio equipo; no son la dirección de este servicio. No inventes direcciones para los campos de privacidad o términos: usá páginas publicadas con tus políticas reales.

## 4. Activar las actualizaciones al publicar

En https://www.patreon.com/portal/registration/register-webhooks creá un webhook para tu campaña con:

```
https://TU-SERVICIO.onrender.com/v1/webhook
```

Activá `posts:publish`, `posts:update`, `posts:delete` y los eventos `members:*` / `members:pledge:*` disponibles. Copiá el secreto generado por Patreon a `EBALIA_PATREON_WEBHOOK_SECRET` en Render y guardá los cambios. El servicio valida la firma HMAC del aviso, invalida su caché y avisa inmediatamente a los launchers conectados. No envía publicaciones ni datos privados por el canal de avisos; cada launcher consulta de nuevo el feed con su sesión y sus permisos.

## 5. Conectar el launcher

Para probarlo: **Comunidad → EBALIA · Patreon → Configuración del servicio Patreon**; pegá la URL base de Render, sin `/callback` ni `/v1/feed`.

Para distribuirlo sin que cada usuario configure nada: poné la URL base en `patreon-service-url.txt`, junto a `CMakeLists.txt`, y volvé a compilar. También se admite `-DEBALIA_PATREON_SERVICE_URL=https://TU-SERVICIO.onrender.com`.

En **Noticias → EBALIA · Patreon** hay dos apartados:

- **Publicaciones públicas**: no requieren cuenta.
- **Miembros de pago**: requieren enlazar una membresía activa de EBALIA, con el nivel que autoriza cada publicación. Cerrar sesión retira el contenido privado de la interfaz. La validación se realiza en el servidor.

## Límites de la opción gratuita

Render Free se duerme después de 15 minutos sin tráfico y puede tardar alrededor de un minuto en volver. Mientras esté activo y el webhook esté configurado, los cambios llegan automáticamente por una conexión de espera al servidor. Se conserva además una actualización de respaldo cada minuto. No se promete disponibilidad continua ni capacidad ilimitada: ambos planes tienen cuotas. Usá los planes **Free**, sin habilitar ampliaciones de pago. Neon conserva sesiones y tokens aunque Render se reinicie.

Fuentes: [Render Free](https://render.com/docs/free), [Neon Free](https://neon.com/docs/introduction/plans), [API y webhooks de Patreon](https://docs.patreon.com/).

## Pruebas locales del servicio

```sh
python -m unittest discover -s . -p test_server.py -v
```

Las pruebas cubren publicaciones públicas, niveles de pago, membresías vencidas, firmas inválidas, avisos en tiempo real y callbacks OAuth de un solo uso. El inicio real con tu campaña requiere el despliegue y las credenciales anteriores; esas pruebas no sustituyen una verificación con una cuenta de pago y otra gratuita.
