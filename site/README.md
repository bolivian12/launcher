# EBALIA download website

Public website: https://ebalia-launcher.gitgud.site/

The deployment source is the `site` directory of https://gitgud.io/ebalia-launcher/ebalia-launcher.gitgud.site.
GitGud Pages publishes it using `.gitlab-ci.yml`.

The language follows the browser's preferred languages unless the visitor chooses a language manually. The preference is saved locally. Supported languages: Spanish, English, Portuguese, French and German.

The launcher icon and preview are embedded in index.html because separate image requests returned HTTP 403 from the hosting provider. Their original files are retained in assets for editing. Keep the embedded copies in sync when replacing artwork.

Download links are populated from the latest stable release in GitHub or GitGud, checked every minute while the page is visible. No package is advertised as downloadable before its asset exists.
