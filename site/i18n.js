const translations = {
  "es": {
    "kindInstaller": "Instalador",
    "kindInstallerHelp": "Se instala en tu usuario, crea el acceso directo e incluye Visual C++. Se actualiza solo.",
    "kindPortable": "Portable",
    "kindPortableHelp": "Sin instalar: descomprimilo en cualquier carpeta o USB y abrí ebalia-launcher.exe. Tus datos quedan en esa carpeta.",
    "kindMacArm": "Apple Silicon",
    "kindMacArmHelp": "Para Mac con chip M1 o posterior.",
    "kindMacIntel": "Intel",
    "kindMacIntelHelp": "Para Mac con procesador Intel.",
    "kindMacPortableArm": "Portable · Apple Silicon",
    "kindMacPortableIntel": "Portable · Intel",
    "kindMacPortableHelp": "La app y sus datos juntos en una carpeta que podés mover.",
    "kindAppimage": "AppImage",
    "kindAppimageHelp": "Para cualquier distribución: dale permiso de ejecución y abrila.",
    "kindLinuxPortable": "Portable",
    "kindLinuxPortableHelp": "El AppImage con sus datos en la misma carpeta.",
    "kindPackage": "Paquete",
    "hashAll": "Todas las sumas",
    "hashPending": "Calculando la suma de verificación…",
    "hashCopy": "Copiar",
    "hashCopied": "Copiado ✓",
    "hashHelp": "Para comprobar que la descarga está intacta, calculá su SHA-256 en una terminal y comparalo con este valor:",
    "navDownload": "Descargar",
    "source": "Código ↗",
    "hero": "Tu mundo.<br>Tu próxima <em>aventura.</em>",
    "intro": "Minecraft, modpacks y versiones perdidas.<br>Todo en un lugar, con el universo de EBALIA a un clic.",
    "choose": "Elegí tu plataforma <span>↓</span>",
    "home": "EBALIA / INICIO",
    "feature1": "01 / A TU MANERA",
    "worlds": "Un lugar para tus mundos",
    "worldsBody": "Organizá instancias y explorá modpacks de CurseForge, Modrinth y otras plataformas.",
    "feature2": "02 / MÁS POR DESCUBRIR",
    "lostBody": "Explorá el archivo de versiones de EBALIA desde una biblioteca visual.",
    "feature3": "03 / CERCA DEL DESARROLLO",
    "connected": "La comunidad, conectada",
    "connectedBody": "Noticias de Minecraft y Patreon, con publicaciones exclusivas según tu membresía.",
    "start": "EMPEZÁ ACÁ",
    "where": "Elegí dónde jugar.",
    "notes": "Notas de la versión ↗",
    "checking": "Consultando los paquetes publicados…",
    "availability": "Ver disponibilidad ↗",
    "windowsBody": "Instalador .exe o versión portable .zip: descomprimila en cualquier carpeta o USB y abrí ebalia-launcher.exe, sin instalar.",
    "macBody": "DMG para Apple Silicon e Intel. La versión portable .zip guarda todo junto a la aplicación.",
    "linuxArch": "x64 · AppImage para cualquier distribución",
    "linuxBody": "AppImage con Qt y libarchive incluidos: dale permiso de ejecución y abrila, sin instalar dependencias. En NixOS, usá el flake.",
    "requirements": "Requisitos y otras distribuciones de Linux",
    "requirementsBody": "El AppImage incluye Qt, libarchive y OpenSSL y funciona en distribuciones x86_64 con glibc 2.35 o más reciente (Ubuntu 22.04+, Debian 12+, Fedora, Arch, openSUSE y otras). Alpine/musl y ARM requieren compilar el código con CMake, C++20, Qt 6 y libarchive. En NixOS usá el flake. Consultá el README para más detalles.",
    "fine": "Los paquetes se muestran cuando terminan las compilaciones y se adjuntan a la versión. Windows y macOS pueden distribuirse sin firma digital. No se incluyen cuentas de Minecraft.",
    "made": "HECHO POR EBALIA",
    "follow": "Seguí lo que viene.",
    "followBody": "Descubrí mods, novedades y contenido para la comunidad.",
    "patreon": "Conocer Patreon ↗",
    "issue": "Reportar un problema",
    "independent": "Proyecto independiente. No afiliado a Mojang ni Microsoft.",
    "title": "EBALIA Launcher — Tu próxima aventura",
    "description": "EBALIA Launcher: Minecraft, modpacks y Lost Versions. Descargas oficiales para Windows, macOS y Linux.",
    "previewAlt": "Interfaz de EBALIA Launcher con instancias y noticias",
    "language": "Idioma",
    "automatic": "Automático (sistema)",
    "pending": "Paquete en preparación",
    "download": "Descargar {size} MB ↓",
    "ready": "Versión {version} · {count} paquetes disponibles",
    "preparing": "Versión {version} · Los paquetes se están preparando.",
    "unavailable": "Consultá la release para ver las descargas disponibles.",
    "releaseEyebrow": "Novedades de la versión 1.1.0",
    "releaseHeading": "Bedrock experimental y mejoras al instalar",
    "releaseBedrock": "Gestor externo de Bedrock: Windows requiere Minecraft para Windows; Linux y macOS requieren la edición Android de Google Play. La compra de Windows no habilita Android. Las partidas aún no están verificadas en todos los sistemas.",
    "releaseFixes": "Guía interactiva en la primera apertura y botón Actualizar y reiniciar dentro del launcher. La actualización conserva instancias, Lost Versions y descargas, y elimina los archivos anteriores del programa al terminar. Windows incluye las DLL necesarias y detecta instalaciones existentes. Si usás 1.0.0, instalá 1.1.0 una vez para habilitar las próximas actualizaciones internas."
  },
  "en": {
    "kindInstaller": "Installer",
    "kindInstallerHelp": "Installs for your user, creates the shortcut and includes Visual C++. Updates itself.",
    "kindPortable": "Portable",
    "kindPortableHelp": "No installation: extract it to any folder or USB drive and open ebalia-launcher.exe. Your data stays in that folder.",
    "kindMacArm": "Apple Silicon",
    "kindMacArmHelp": "For Macs with an M1 chip or later.",
    "kindMacIntel": "Intel",
    "kindMacIntelHelp": "For Macs with an Intel processor.",
    "kindMacPortableArm": "Portable · Apple Silicon",
    "kindMacPortableIntel": "Portable · Intel",
    "kindMacPortableHelp": "The app and its data together in a folder you can move.",
    "kindAppimage": "AppImage",
    "kindAppimageHelp": "For any distribution: make it executable and open it.",
    "kindLinuxPortable": "Portable",
    "kindLinuxPortableHelp": "The AppImage with its data in the same folder.",
    "kindPackage": "Package",
    "hashAll": "All checksums",
    "hashPending": "Loading the checksum…",
    "hashCopy": "Copy",
    "hashCopied": "Copied ✓",
    "hashHelp": "To check that the download is intact, compute its SHA-256 in a terminal and compare it with this value:",
    "navDownload": "Download",
    "source": "Source ↗",
    "hero": "Your world.<br>Your next <em>adventure.</em>",
    "intro": "Minecraft, modpacks and lost versions.<br>Everything in one place, with the EBALIA universe a click away.",
    "choose": "Choose your platform <span>↓</span>",
    "home": "EBALIA / HOME",
    "feature1": "01 / YOUR WAY",
    "worlds": "A place for your worlds",
    "worldsBody": "Organize instances and explore modpacks from CurseForge, Modrinth and other platforms.",
    "feature2": "02 / MORE TO DISCOVER",
    "lostBody": "Explore the EBALIA version archive in a visual library.",
    "feature3": "03 / FOLLOW DEVELOPMENT",
    "connected": "A connected community",
    "connectedBody": "Minecraft and Patreon news, with exclusive posts based on your membership.",
    "start": "START HERE",
    "where": "Choose where to play.",
    "notes": "Release notes ↗",
    "checking": "Checking published packages…",
    "availability": "Check availability ↗",
    "windowsBody": ".exe installer or portable .zip: extract it to any folder or USB drive and open ebalia-launcher.exe, no installation needed.",
    "macBody": "DMG for Apple Silicon and Intel. The portable .zip keeps everything next to the app.",
    "linuxArch": "x64 · AppImage for any distribution",
    "linuxBody": "AppImage with Qt and libarchive bundled: make it executable and open it, no dependencies to install. On NixOS, use the flake.",
    "requirements": "Requirements and other Linux distributions",
    "requirementsBody": "The AppImage bundles Qt, libarchive and OpenSSL and runs on x86_64 distributions with glibc 2.35 or newer (Ubuntu 22.04+, Debian 12+, Fedora, Arch, openSUSE and others). Alpine/musl and ARM need a source build with CMake, C++20, Qt 6 and libarchive. On NixOS use the flake. See the README for details.",
    "fine": "Packages appear once builds finish and are attached to the release. Windows and macOS packages may be unsigned. Minecraft accounts are not included.",
    "made": "MADE BY EBALIA",
    "follow": "Follow what’s next.",
    "followBody": "Discover mods, news and community content.",
    "patreon": "Explore Patreon ↗",
    "issue": "Report an issue",
    "independent": "Independent project. Not affiliated with Mojang or Microsoft.",
    "title": "EBALIA Launcher — Your next adventure",
    "description": "EBALIA Launcher: Minecraft, modpacks and Lost Versions. Official downloads for Windows, macOS and Linux.",
    "previewAlt": "EBALIA Launcher interface with instances and news",
    "language": "Language",
    "automatic": "Automatic (system)",
    "pending": "Package in preparation",
    "download": "Download {size} MB ↓",
    "ready": "Version {version} · {count} packages available",
    "preparing": "Version {version} · Packages are being prepared.",
    "unavailable": "Check the release for available downloads.",
    "releaseEyebrow": "New in version 1.1.0",
    "releaseHeading": "Experimental Bedrock and installation improvements",
    "releaseBedrock": "External Bedrock manager: Windows requires Minecraft for Windows; Linux and macOS require the Android edition from Google Play. A Windows purchase does not unlock Android. Game sessions have not been verified on all platforms.",
    "releaseFixes": "Interactive first-run tour and an in-app Update and restart button. Updates preserve instances, Lost Versions and downloads, then remove the previous program files after successful startup. Windows includes the required DLLs and detects existing installations. If you use 1.0.0, install 1.1.0 once to enable future in-app updates."
  },
  "pt": {
    "kindInstaller": "Instalador",
    "kindInstallerHelp": "Instala no seu usuário, cria o atalho e inclui o Visual C++. Atualiza sozinho.",
    "kindPortable": "Portátil",
    "kindPortableHelp": "Sem instalar: extraia em qualquer pasta ou pendrive e abra o ebalia-launcher.exe. Seus dados ficam nessa pasta.",
    "kindMacArm": "Apple Silicon",
    "kindMacArmHelp": "Para Mac com chip M1 ou posterior.",
    "kindMacIntel": "Intel",
    "kindMacIntelHelp": "Para Mac com processador Intel.",
    "kindMacPortableArm": "Portátil · Apple Silicon",
    "kindMacPortableIntel": "Portátil · Intel",
    "kindMacPortableHelp": "O app e seus dados juntos em uma pasta que você pode mover.",
    "kindAppimage": "AppImage",
    "kindAppimageHelp": "Para qualquer distribuição: dê permissão de execução e abra.",
    "kindLinuxPortable": "Portátil",
    "kindLinuxPortableHelp": "O AppImage com seus dados na mesma pasta.",
    "kindPackage": "Pacote",
    "hashAll": "Todas as somas",
    "hashPending": "Carregando a soma de verificação…",
    "hashCopy": "Copiar",
    "hashCopied": "Copiado ✓",
    "hashHelp": "Para conferir se o download está íntegro, calcule o SHA-256 em um terminal e compare com este valor:",
    "navDownload": "Baixar",
    "source": "Código ↗",
    "hero": "Seu mundo.<br>Sua próxima <em>aventura.</em>",
    "intro": "Minecraft, modpacks e versões perdidas.<br>Tudo em um só lugar, com o universo EBALIA a um clique.",
    "choose": "Escolha sua plataforma <span>↓</span>",
    "home": "EBALIA / INÍCIO",
    "feature1": "01 / DO SEU JEITO",
    "worlds": "Um lugar para seus mundos",
    "worldsBody": "Organize instâncias e explore modpacks do CurseForge, Modrinth e outras plataformas.",
    "feature2": "02 / MAIS PARA DESCOBRIR",
    "lostBody": "Explore o arquivo de versões EBALIA em uma biblioteca visual.",
    "feature3": "03 / ACOMPANHE O DESENVOLVIMENTO",
    "connected": "Uma comunidade conectada",
    "connectedBody": "Notícias do Minecraft e Patreon, com publicações exclusivas conforme sua assinatura.",
    "start": "COMECE AQUI",
    "where": "Escolha onde jogar.",
    "notes": "Notas da versão ↗",
    "checking": "Consultando os pacotes publicados…",
    "availability": "Ver disponibilidade ↗",
    "windowsBody": "Instalador .exe ou versão portátil .zip: extraia em qualquer pasta ou pendrive e abra o ebalia-launcher.exe, sem instalar.",
    "macBody": "DMG para Apple Silicon e Intel. A versão portátil .zip guarda tudo junto ao aplicativo.",
    "linuxArch": "x64 · AppImage para qualquer distribuição",
    "linuxBody": "AppImage com Qt e libarchive incluídos: dê permissão de execução e abra, sem instalar dependências. No NixOS, use o flake.",
    "requirements": "Requisitos e outras distribuições Linux",
    "requirementsBody": "O AppImage inclui Qt, libarchive e OpenSSL e funciona em distribuições x86_64 com glibc 2.35 ou mais recente (Ubuntu 22.04+, Debian 12+, Fedora, Arch, openSUSE e outras). Alpine/musl e ARM exigem compilar o código com CMake, C++20, Qt 6 e libarchive. No NixOS, use o flake. Veja o README para mais detalhes.",
    "fine": "Os pacotes aparecem após a conclusão das compilações e sua inclusão na versão. Pacotes Windows e macOS podem não ter assinatura digital. Contas Minecraft não estão incluídas.",
    "made": "FEITO POR EBALIA",
    "follow": "Acompanhe as novidades.",
    "followBody": "Descubra mods, novidades e conteúdo para a comunidade.",
    "patreon": "Conhecer o Patreon ↗",
    "issue": "Relatar um problema",
    "independent": "Projeto independente. Sem afiliação à Mojang ou Microsoft.",
    "title": "EBALIA Launcher — Sua próxima aventura",
    "description": "EBALIA Launcher: Minecraft, modpacks e Lost Versions. Downloads oficiais para Windows, macOS e Linux.",
    "previewAlt": "Interface do EBALIA Launcher com instâncias e notícias",
    "language": "Idioma",
    "automatic": "Automático (sistema)",
    "pending": "Pacote em preparação",
    "download": "Baixar {size} MB ↓",
    "ready": "Versão {version} · {count} pacotes disponíveis",
    "preparing": "Versão {version} · Os pacotes estão sendo preparados.",
    "unavailable": "Consulte a versão para ver os downloads disponíveis.",
    "releaseEyebrow": "Novidades da versão 1.1.0",
    "releaseHeading": "Bedrock experimental e melhorias na instalação",
    "releaseBedrock": "Gestor externo de Bedrock: Windows exige Minecraft para Windows; Linux e macOS exigem a edição Android do Google Play. A compra para Windows não libera Android. As partidas ainda não foram verificadas em todos os sistemas.",
    "releaseFixes": "Guia interativo na primeira abertura e botão Atualizar e reiniciar no launcher. A atualização preserva instâncias, Lost Versions e downloads e remove os arquivos anteriores do programa após iniciar corretamente. Windows inclui as DLLs necessárias e detecta instalações existentes. Se você usa 1.0.0, instale 1.1.0 uma vez para ativar as próximas atualizações internas."
  },
  "fr": {
    "kindInstaller": "Installateur",
    "kindInstallerHelp": "S’installe pour votre utilisateur, crée le raccourci et inclut Visual C++. Se met à jour tout seul.",
    "kindPortable": "Portable",
    "kindPortableHelp": "Sans installation : extrayez-le dans n’importe quel dossier ou clé USB et ouvrez ebalia-launcher.exe. Vos données restent dans ce dossier.",
    "kindMacArm": "Apple Silicon",
    "kindMacArmHelp": "Pour les Mac avec puce M1 ou plus récente.",
    "kindMacIntel": "Intel",
    "kindMacIntelHelp": "Pour les Mac avec processeur Intel.",
    "kindMacPortableArm": "Portable · Apple Silicon",
    "kindMacPortableIntel": "Portable · Intel",
    "kindMacPortableHelp": "L’app et ses données réunies dans un dossier déplaçable.",
    "kindAppimage": "AppImage",
    "kindAppimageHelp": "Pour toutes les distributions : rendez-la exécutable et ouvrez-la.",
    "kindLinuxPortable": "Portable",
    "kindLinuxPortableHelp": "L’AppImage avec ses données dans le même dossier.",
    "kindPackage": "Paquet",
    "hashAll": "Toutes les sommes",
    "hashPending": "Chargement de la somme de contrôle…",
    "hashCopy": "Copier",
    "hashCopied": "Copié ✓",
    "hashHelp": "Pour vérifier que le téléchargement est intact, calculez son SHA-256 dans un terminal et comparez-le à cette valeur :",
    "navDownload": "Télécharger",
    "source": "Code source ↗",
    "hero": "Votre monde.<br>Votre prochaine <em>aventure.</em>",
    "intro": "Minecraft, modpacks et versions perdues.<br>Tout au même endroit, avec l’univers EBALIA à portée de clic.",
    "choose": "Choisissez votre plateforme <span>↓</span>",
    "home": "EBALIA / ACCUEIL",
    "feature1": "01 / À VOTRE FAÇON",
    "worlds": "Un espace pour vos mondes",
    "worldsBody": "Organisez vos instances et explorez les modpacks de CurseForge, Modrinth et d’autres plateformes.",
    "feature2": "02 / ENCORE À DÉCOUVRIR",
    "lostBody": "Explorez les archives des versions EBALIA dans une bibliothèque visuelle.",
    "feature3": "03 / SUIVEZ LE DÉVELOPPEMENT",
    "connected": "Une communauté connectée",
    "connectedBody": "Actualités Minecraft et Patreon, avec des publications exclusives selon votre abonnement.",
    "start": "COMMENCEZ ICI",
    "where": "Choisissez où jouer.",
    "notes": "Notes de version ↗",
    "checking": "Recherche des paquets publiés…",
    "availability": "Voir la disponibilité ↗",
    "windowsBody": "Installateur .exe ou version portable .zip : extrayez-la dans n’importe quel dossier ou clé USB et ouvrez ebalia-launcher.exe, sans installation.",
    "macBody": "DMG pour Apple Silicon et Intel. La version portable .zip garde tout à côté de l’application.",
    "linuxArch": "x64 · AppImage pour toutes les distributions",
    "linuxBody": "AppImage avec Qt et libarchive inclus : rendez-la exécutable et ouvrez-la, sans dépendances à installer. Sur NixOS, utilisez le flake.",
    "requirements": "Configuration requise et autres distributions Linux",
    "requirementsBody": "L’AppImage inclut Qt, libarchive et OpenSSL et fonctionne sur les distributions x86_64 avec glibc 2.35 ou plus récente (Ubuntu 22.04+, Debian 12+, Fedora, Arch, openSUSE et d’autres). Alpine/musl et ARM nécessitent une compilation depuis les sources avec CMake, C++20, Qt 6 et libarchive. Sur NixOS, utilisez le flake. Consultez le README pour plus de détails.",
    "fine": "Les paquets apparaissent une fois compilés et joints à la version. Les paquets Windows et macOS peuvent ne pas être signés. Aucun compte Minecraft n’est inclus.",
    "made": "CRÉÉ PAR EBALIA",
    "follow": "Suivez la suite.",
    "followBody": "Découvrez les mods, les nouveautés et le contenu pour la communauté.",
    "patreon": "Découvrir Patreon ↗",
    "issue": "Signaler un problème",
    "independent": "Projet indépendant. Non affilié à Mojang ou Microsoft.",
    "title": "EBALIA Launcher — Votre prochaine aventure",
    "description": "EBALIA Launcher : Minecraft, modpacks et Lost Versions. Téléchargements officiels pour Windows, macOS et Linux.",
    "previewAlt": "Interface EBALIA Launcher avec instances et actualités",
    "language": "Langue",
    "automatic": "Automatique (système)",
    "pending": "Paquet en préparation",
    "download": "Télécharger {size} Mo ↓",
    "ready": "Version {version} · {count} paquets disponibles",
    "preparing": "Version {version} · Les paquets sont en préparation.",
    "unavailable": "Consultez la version pour voir les téléchargements disponibles.",
    "releaseEyebrow": "Nouveautés de la version 1.1.0",
    "releaseHeading": "Bedrock expérimental et installation améliorée",
    "releaseBedrock": "Gestionnaire Bedrock externe : Windows nécessite Minecraft pour Windows ; Linux et macOS nécessitent l’édition Android de Google Play. L’achat Windows ne donne pas accès à Android. Les parties ne sont pas encore vérifiées sur tous les systèmes.",
    "releaseFixes": "Guide interactif au premier lancement et bouton Mettre à jour et redémarrer dans le launcher. Les mises à jour conservent les instances, Lost Versions et téléchargements, puis suppriment les anciens fichiers du programme après un démarrage réussi. Windows inclut les DLL nécessaires et détecte les installations existantes. Installez 1.1.0 une fois depuis 1.0.0 pour activer les futures mises à jour intégrées."
  },
  "de": {
    "kindInstaller": "Installer",
    "kindInstallerHelp": "Installiert für deinen Benutzer, legt die Verknüpfung an und enthält Visual C++. Aktualisiert sich selbst.",
    "kindPortable": "Portabel",
    "kindPortableHelp": "Ohne Installation: in einen beliebigen Ordner oder auf einen USB-Stick entpacken und ebalia-launcher.exe öffnen. Deine Daten bleiben in diesem Ordner.",
    "kindMacArm": "Apple Silicon",
    "kindMacArmHelp": "Für Macs mit M1-Chip oder neuer.",
    "kindMacIntel": "Intel",
    "kindMacIntelHelp": "Für Macs mit Intel-Prozessor.",
    "kindMacPortableArm": "Portabel · Apple Silicon",
    "kindMacPortableIntel": "Portabel · Intel",
    "kindMacPortableHelp": "App und Daten zusammen in einem verschiebbaren Ordner.",
    "kindAppimage": "AppImage",
    "kindAppimageHelp": "Für jede Distribution: ausführbar machen und öffnen.",
    "kindLinuxPortable": "Portabel",
    "kindLinuxPortableHelp": "Das AppImage mit seinen Daten im selben Ordner.",
    "kindPackage": "Paket",
    "hashAll": "Alle Prüfsummen",
    "hashPending": "Prüfsumme wird geladen…",
    "hashCopy": "Kopieren",
    "hashCopied": "Kopiert ✓",
    "hashHelp": "Um zu prüfen, ob der Download unverändert ist, berechne seinen SHA-256 im Terminal und vergleiche ihn mit diesem Wert:",
    "navDownload": "Herunterladen",
    "source": "Quellcode ↗",
    "hero": "Deine Welt.<br>Dein nächstes <em>Abenteuer.</em>",
    "intro": "Minecraft, Modpacks und verschollene Versionen.<br>Alles an einem Ort, das EBALIA-Universum nur einen Klick entfernt.",
    "choose": "Wähle deine Plattform <span>↓</span>",
    "home": "EBALIA / START",
    "feature1": "01 / AUF DEINE ART",
    "worlds": "Ein Ort für deine Welten",
    "worldsBody": "Verwalte Instanzen und entdecke Modpacks von CurseForge, Modrinth und anderen Plattformen.",
    "feature2": "02 / MEHR ZU ENTDECKEN",
    "lostBody": "Entdecke das EBALIA-Versionsarchiv in einer visuellen Bibliothek.",
    "feature3": "03 / ENTWICKLUNG MITERLEBEN",
    "connected": "Eine vernetzte Community",
    "connectedBody": "Neuigkeiten von Minecraft und Patreon mit exklusiven Beiträgen je nach Mitgliedschaft.",
    "start": "HIER GEHT’S LOS",
    "where": "Wähle, wo du spielst.",
    "notes": "Versionshinweise ↗",
    "checking": "Verfügbare Pakete werden gesucht…",
    "availability": "Verfügbarkeit prüfen ↗",
    "windowsBody": "EXE-Installer oder portable .zip: In einen beliebigen Ordner oder auf einen USB-Stick entpacken und ebalia-launcher.exe öffnen, ohne Installation.",
    "macBody": "DMG für Apple Silicon und Intel. Die portable .zip speichert alles neben der App.",
    "linuxArch": "x64 · AppImage für jede Distribution",
    "linuxBody": "AppImage mit Qt und libarchive: ausführbar machen und öffnen, keine Abhängigkeiten nötig. Unter NixOS den Flake verwenden.",
    "requirements": "Anforderungen und andere Linux-Distributionen",
    "requirementsBody": "Das AppImage enthält Qt, libarchive und OpenSSL und läuft auf x86_64-Distributionen mit glibc 2.35 oder neuer (Ubuntu 22.04+, Debian 12+, Fedora, Arch, openSUSE und weitere). Alpine/musl und ARM erfordern einen eigenen Build mit CMake, C++20, Qt 6 und libarchive. Unter NixOS den Flake verwenden. Details stehen in der README.",
    "fine": "Pakete erscheinen nach der Kompilierung und dem Anhängen an die Veröffentlichung. Windows- und macOS-Pakete können unsigniert sein. Minecraft-Konten sind nicht enthalten.",
    "made": "VON EBALIA",
    "follow": "Entdecke, was kommt.",
    "followBody": "Entdecke Mods, Neuigkeiten und Inhalte für die Community.",
    "patreon": "Patreon entdecken ↗",
    "issue": "Problem melden",
    "independent": "Unabhängiges Projekt. Nicht mit Mojang oder Microsoft verbunden.",
    "title": "EBALIA Launcher — Dein nächstes Abenteuer",
    "description": "EBALIA Launcher: Minecraft, Modpacks und Lost Versions. Offizielle Downloads für Windows, macOS und Linux.",
    "previewAlt": "EBALIA-Launcher-Oberfläche mit Instanzen und Neuigkeiten",
    "language": "Sprache",
    "automatic": "Automatisch (System)",
    "pending": "Paket wird vorbereitet",
    "download": "{size} MB herunterladen ↓",
    "ready": "Version {version} · {count} Pakete verfügbar",
    "preparing": "Version {version} · Die Pakete werden vorbereitet.",
    "unavailable": "Verfügbare Downloads findest du auf der Veröffentlichungsseite.",
    "releaseEyebrow": "Neu in Version 1.1.0",
    "releaseHeading": "Experimentelles Bedrock und verbesserte Installation",
    "releaseBedrock": "Externer Bedrock-Manager: Windows benötigt Minecraft für Windows; Linux und macOS benötigen die Android-Version von Google Play. Ein Windows-Kauf schaltet Android nicht frei. Spielsitzungen wurden noch nicht auf allen Systemen überprüft.",
    "releaseFixes": "Interaktive Einführung beim ersten Start und eine Schaltfläche zum Aktualisieren und Neustarten im Launcher. Updates behalten Instanzen, Lost Versions und Downloads und entfernen nach erfolgreichem Start die alten Programmdateien. Windows enthält die benötigten DLLs und erkennt bestehende Installationen. Installiere 1.1.0 einmal über 1.0.0, um zukünftige integrierte Updates zu aktivieren."
  }
};
Object.entries({
 es: {availability:'Ver descarga',unavailable:'Los paquetes aún no están disponibles. Podés comprobar su estado aquí.', panelPending:'El paquete para esta plataforma todavía no está publicado. Volvé a comprobarlo en unos minutos.',panelError:'No pudimos comprobar la disponibilidad. Intentá nuevamente en unos momentos.',retry:'Comprobar de nuevo',close:'Cerrar',panelReady:'Tu descarga está lista. Pulsá el botón para guardar el archivo.'},
 en: {availability:'View download',unavailable:'Packages are not available yet. Check their status here.',panelPending:'The package for this platform has not been published yet. Check again in a few minutes.',panelError:'We could not check availability. Please try again shortly.',retry:'Check again',close:'Close',panelReady:'Your download is ready. Press the button to save the file.'},
 pt: {availability:'Ver download',unavailable:'Os pacotes ainda não estão disponíveis. Consulte o estado aqui.',panelPending:'O pacote desta plataforma ainda não foi publicado. Confira novamente em alguns minutos.',panelError:'Não foi possível verificar a disponibilidade. Tente novamente em instantes.',retry:'Verificar novamente',close:'Fechar',panelReady:'Seu download está pronto. Clique no botão para salvar o arquivo.'},
 fr: {availability:'Voir le téléchargement',unavailable:'Les paquets ne sont pas encore disponibles. Consultez leur état ici.',panelPending:'Le paquet de cette plateforme n’est pas encore publié. Réessayez dans quelques minutes.',panelError:'Impossible de vérifier la disponibilité. Réessayez dans un instant.',retry:'Vérifier à nouveau',close:'Fermer',panelReady:'Votre téléchargement est prêt. Cliquez pour enregistrer le fichier.'},
 de: {availability:'Download ansehen',unavailable:'Die Pakete sind noch nicht verfügbar. Prüfe den Status hier.',panelPending:'Das Paket für diese Plattform wurde noch nicht veröffentlicht. Prüfe es in einigen Minuten erneut.',panelError:'Die Verfügbarkeit konnte nicht geprüft werden. Versuche es gleich erneut.',retry:'Erneut prüfen',close:'Schließen',panelReady:'Dein Download ist bereit. Klicke auf die Schaltfläche, um die Datei zu speichern.'}
}).forEach(([language, values]) => Object.assign(translations[language], values));
(() => {
  const key = 'ebalia-language';
  const toggle = document.querySelector('#language-toggle');
  const menu = document.querySelector('#language-menu');
  const buttons = [...menu.querySelectorAll('[data-language]')];
  const supported = code => Object.hasOwn(translations, code);
  const detect = () => {
    for (const locale of navigator.languages?.length ? navigator.languages : [navigator.language || 'en']) {
      const language = locale.toLowerCase().split(/[-_]/)[0];
      if (supported(language)) return language;
    }
    return 'en';
  };
  let preference = 'auto';
  try { const saved = localStorage.getItem(key); if (saved === 'auto' || supported(saved)) preference = saved; } catch {}
  let language;
  function apply() {
    language = preference === 'auto' ? detect() : preference;
    document.documentElement.lang = language;
    document.title = translations[language].title;
    document.querySelector('meta[name="description"]').content = translations[language].description;
    document.querySelector('.preview img').alt = translations[language].previewAlt;
    document.querySelector('nav').setAttribute('aria-label', translations[language].navDownload);
    toggle.setAttribute('aria-label', translations[language].language);
    document.querySelector('#language-current').textContent = language.toUpperCase();
    buttons.forEach(button => button.setAttribute('aria-pressed', String(button.dataset.language === preference)));
    for (const element of document.querySelectorAll('[data-i18n]')) {
      // Only our bundled, trusted translations contain markup. No remote HTML is inserted.
      element.innerHTML = translations[language][element.dataset.i18n];
    }
    window.dispatchEvent(new Event('ebalia-language-change'));
  }
  window.ebaliaI18n = {
    t: (key, values = {}) => translations[language][key].replace(/\{(\w+)\}/g, (_, name) => values[name] ?? ''),
    number: value => new Intl.NumberFormat(language, { maximumFractionDigits: 1, minimumFractionDigits: 1 }).format(value)
  };
  function closeMenu(restoreFocus = false) {
    menu.hidden = true;
    toggle.setAttribute('aria-expanded', 'false');
    if (restoreFocus) toggle.focus();
  }
  toggle.addEventListener('click', () => {
    const opening = menu.hidden;
    menu.hidden = !opening;
    toggle.setAttribute('aria-expanded', String(opening));
    if (opening) buttons.find(button => button.dataset.language === preference).focus();
  });
  buttons.forEach(button => button.addEventListener('click', () => {
    preference = button.dataset.language;
    try { localStorage.setItem(key, preference); } catch {}
    apply();
    closeMenu(true);
  }));
  menu.addEventListener('keydown', event => {
    const index = buttons.indexOf(document.activeElement);
    let next;
    if (event.key === 'ArrowDown') next = (index + 1) % buttons.length;
    if (event.key === 'ArrowUp') next = (index + buttons.length - 1) % buttons.length;
    if (event.key === 'Home') next = 0;
    if (event.key === 'End') next = buttons.length - 1;
    if (next !== undefined) { event.preventDefault(); buttons[next].focus(); }
  });
  document.addEventListener('keydown', event => {
    if (event.key === 'Escape' && !menu.hidden) { event.preventDefault(); closeMenu(true); }
  });
  document.addEventListener('click', event => {
    if (!event.target.closest('.language-picker')) closeMenu();
  });
  document.querySelector('.language-picker').addEventListener('focusout', event => {
    if (!event.currentTarget.contains(event.relatedTarget)) closeMenu();
  });
  window.addEventListener('languagechange', () => { if (preference === 'auto') apply(); });
  apply();
})();
