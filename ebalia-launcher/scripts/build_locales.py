import json,pathlib
root=pathlib.Path(__file__).resolve().parents[1]
keys=json.loads((root/'scripts/locale-keys.json').read_text())
codes=['de','fr','it','ru','ja','ko','zh']
tables={code:{} for code in codes}
for line in (root/'scripts/translations.tsv').read_text().splitlines():
 cells=line.split('\t');assert len(cells)==8,(cells[0],len(cells))
 idx=int(cells[0]);assert idx!=3
 for code,value in zip(codes,cells[1:]):tables[code][keys[idx]]=value
# Guides cover the same six workflows in every language, without machine translation at runtime.
guides={
'de':[
('1. Modus wählen','Meine Instanzen ist der normale Minecraft-Client. Verschollene Versionen öffnet das EBALIA-Archiv.'),
('2. Instanz erstellen','Wähle Name, Spielversion und Loader. Vanilla ist das Originalspiel; Fabric, Quilt, Forge und NeoForge erlauben Mods. Jede Instanz hat eigene Welten und Einstellungen. Der Katalog wird beim Start und alle 30 Minuten aktualisiert. Bestehende Instanzen behalten ihre Version.'),
('3. Mods hinzufügen','Wähle unter Mods entdecken deine Instanz und suche etwa Sodium, Dynamic Lights, FallingTree oder Veinminer. Prüfe die Abhängigkeiten vor der Installation. Veröffentlichte Kompatibilität garantiert nicht, dass alle Mods zusammen funktionieren.'),
('4. Favoriten als Pack speichern','Wähle Mods als Pack speichern in deiner Instanz. Unter Meine Packs kannst du es auf eine andere Instanz anwenden. Der Launcher sucht passende Versionen und zeigt fehlende Mods vor dem Download. Inkompatible JARs werden nicht kopiert. Lokale Mods müssen auf Modrinth erkennbar sein; deaktivierte Mods werden ausgelassen.'),
('5. Spielen','Füge ein Konto hinzu, installiere Java und klicke auf Spielen / Installieren. Klicke nach der Installation erneut auf Spielen. Der Launcher zeigt die benötigte Java-Version an. In den Instanzeinstellungen wählst du Java-Pfad und Arbeitsspeicher. Forge und NeoForge verwenden ihre offiziellen Installer und brauchen Java bereits vor der Installation.'),
('Fehler beheben','Öffne Instanzeinstellungen → Protokoll. Installation reparieren prüft die Spieldateien erneut. Entfernte Instanzen bleiben mit ihren Welten im lokalen Papierkorb. Verschollene Versionen können Wine, älteres Java oder nicht mehr verfügbare Originalpakete benötigen.')],
'fr':[
('1. Choisissez un mode','Mes instances est le client Minecraft classique. Versions perdues ouvre les archives EBALIA.'),
('2. Créez une instance','Choisissez un nom, une version et un chargeur. Vanilla est le jeu original ; Fabric, Quilt, Forge et NeoForge permettent les mods. Chaque instance garde ses mondes et paramètres. Le catalogue est actualisé au démarrage et toutes les 30 minutes. Les instances existantes conservent leur version.'),
('3. Ajoutez des mods','Dans Découvrir des mods, choisissez votre instance et recherchez Sodium, Dynamic Lights, FallingTree ou Veinminer. Vérifiez les dépendances avant l’installation. La compatibilité publiée ne garantit pas que tous les mods fonctionnent ensemble.'),
('4. Enregistrez vos favoris en pack','Dans votre instance, choisissez Enregistrer les mods en pack. Appliquez-le ensuite à une autre instance depuis Mes packs. Le launcher cherche les versions adaptées et affiche les mods indisponibles avant le téléchargement. Aucun JAR incompatible n’est copié. Les mods locaux doivent être identifiables sur Modrinth ; les mods désactivés sont exclus.'),
('5. Jouez','Ajoutez un compte, installez Java et cliquez sur Jouer / Installer. Une fois l’installation terminée, cliquez sur Jouer. Le launcher indique la version de Java requise. Choisissez son chemin et la mémoire dans les paramètres de l’instance. Forge et NeoForge utilisent leurs installateurs officiels et nécessitent Java avant l’installation.'),
('En cas de problème','Ouvrez Paramètres de l’instance → Journal. Réparer l’installation vérifie de nouveau les fichiers du jeu. Une instance retirée conserve ses mondes dans la corbeille locale. Les versions perdues peuvent nécessiter Wine, un ancien Java ou des archives d’origine indisponibles.')],
'it':[
('1. Scegli la modalità','Le mie istanze è il normale client Minecraft. Versioni perdute apre l’archivio EBALIA.'),
('2. Crea un’istanza','Scegli nome, versione e loader. Vanilla è il gioco originale; Fabric, Quilt, Forge e NeoForge supportano le mod. Ogni istanza mantiene mondi e impostazioni separati. Il catalogo si aggiorna all’avvio e ogni 30 minuti. Le istanze esistenti mantengono la propria versione.'),
('3. Aggiungi mod','In Esplora mod, scegli l’istanza e cerca Sodium, Dynamic Lights, FallingTree o Veinminer. Controlla le dipendenze prima dell’installazione. La compatibilità dichiarata non garantisce che tutte le mod funzionino insieme.'),
('4. Salva le preferite come pack','Nella tua istanza, scegli Salva mod come pack. Poi applicalo a un’altra istanza da I miei pack. Il launcher cerca versioni adatte e mostra le mod non disponibili prima del download. Non copia JAR incompatibili. Le mod locali devono essere identificabili su Modrinth; quelle disattivate sono escluse.'),
('5. Gioca','Aggiungi un account, installa Java e premi Gioca / Installa. Al termine dell’installazione, premi Gioca. Il launcher indica quale versione di Java serve. Scegli percorso Java e memoria nelle impostazioni dell’istanza. Forge e NeoForge usano gli installer ufficiali e richiedono Java prima dell’installazione.'),
('Risoluzione dei problemi','Apri Impostazioni istanza → Registro. Ripara installazione verifica nuovamente i file di gioco. Le istanze rimosse e i loro mondi restano nel cestino locale. Le versioni perdute possono richiedere Wine, vecchie versioni di Java o archivi originali non più disponibili.')],
'ru':[
('1. Выберите режим','Мои сборки — обычный клиент Minecraft. Потерянные версии — архив EBALIA.'),
('2. Создайте сборку','Выберите название, версию игры и загрузчик. Vanilla — исходная игра; Fabric, Quilt, Forge и NeoForge поддерживают моды. Миры и настройки у каждой сборки свои. Каталог обновляется при запуске и каждые 30 минут. Версии существующих сборок не меняются.'),
('3. Добавьте моды','В разделе Найти моды выберите сборку и найдите Sodium, Dynamic Lights, FallingTree или Veinminer. Проверьте зависимости перед установкой. Заявленная совместимость не гарантирует, что все моды будут работать вместе.'),
('4. Сохраните любимые моды в набор','В сборке нажмите Сохранить моды как набор. Затем примените его к другой сборке через Мои наборы. Лаунчер ищет подходящие версии и показывает недоступные моды до загрузки. Несовместимые JAR не копируются. Локальные моды должны определяться на Modrinth; отключённые моды не включаются.'),
('5. Играйте','Добавьте учётную запись, установите Java и нажмите Играть / Установить. После установки нажмите Играть. Лаунчер укажет нужную версию Java. Путь к Java и объём памяти задаются в настройках сборки. Forge и NeoForge используют официальные установщики и требуют Java до начала установки.'),
('Если возникла ошибка','Откройте Настройки сборки → Журнал. Восстановление повторно проверяет файлы игры. Удалённая сборка вместе с мирами остаётся в локальной корзине. Потерянным версиям могут понадобиться Wine, старая Java или исходные архивы, которые уже недоступны.')],
'ja':[
('1. モードを選ぶ','マイインスタンスは通常のMinecraftクライアントです。失われたバージョンからEBALIAアーカイブを開けます。'),
('2. インスタンスを作る','名前、ゲームバージョン、ローダーを選びます。Vanillaは元のゲームで、Fabric・Quilt・Forge・NeoForgeはMODに対応します。ワールドと設定はインスタンスごとに独立しています。カタログは起動時と30分ごとに更新され、既存のインスタンスのバージョンは維持されます。'),
('3. MODを追加する','MODを探すでインスタンスを選び、Sodium、Dynamic Lights、FallingTree、Veinminerなどを検索します。依存関係を確認してからインストールしてください。公開された対応情報は、すべてのMODの組み合わせでの動作を保証するものではありません。'),
('4. お気に入りをパックに保存する','インスタンスでMODをパックとして保存を選びます。マイパックから別のインスタンスに適用できます。対象に合う版を探し、利用できないMODはダウンロード前に表示します。非対応のJARはコピーしません。ローカルMODはModrinthで識別できる必要があります。無効化したMODは含まれません。'),
('5. プレイする','アカウントを追加してJavaをインストールし、プレイ・インストールを押します。完了後にもう一度プレイを押してください。必要なJavaのバージョンはランチャーに表示されます。Javaのパスとメモリーはインスタンス設定で選べます。ForgeとNeoForgeは公式インストーラーを使い、インストール前にJavaが必要です。'),
('問題が起きたら','インスタンス設定 → ログを確認してください。修復はゲームファイルを再検証します。削除したインスタンスとワールドはローカルのゴミ箱に残ります。失われたバージョンにはWine、古いJava、または現在入手できない元のパッケージが必要な場合があります。')],
'ko':[
('1. 모드 선택','내 인스턴스는 일반 Minecraft 클라이언트입니다. 잃어버린 버전은 EBALIA 보관소를 엽니다.'),
('2. 인스턴스 만들기','이름, 게임 버전, 로더를 선택하세요. Vanilla는 원본 게임이며 Fabric, Quilt, Forge, NeoForge는 모드를 지원합니다. 월드와 설정은 인스턴스마다 분리됩니다. 카탈로그는 시작할 때와 30분마다 갱신되며 기존 인스턴스의 버전은 유지됩니다.'),
('3. 모드 추가','모드 찾기에서 인스턴스를 선택하고 Sodium, Dynamic Lights, FallingTree 또는 Veinminer를 검색하세요. 의존성을 확인한 뒤 설치하세요. 공개된 호환 정보가 모든 모드 조합의 정상 작동을 보장하지는 않습니다.'),
('4. 즐겨 쓰는 모드를 팩으로 저장','인스턴스에서 모드를 팩으로 저장을 선택하세요. 내 팩에서 다른 인스턴스에 적용할 수 있습니다. 대상에 맞는 버전을 찾고 사용할 수 없는 모드는 다운로드 전에 표시합니다. 호환되지 않는 JAR은 복사하지 않습니다. 로컬 모드는 Modrinth에서 식별 가능해야 하며 비활성화된 모드는 제외됩니다.'),
('5. 플레이','계정을 추가하고 Java를 설치한 뒤 플레이 / 설치를 누르세요. 설치가 끝나면 플레이를 누르세요. 필요한 Java 버전은 런처에 표시됩니다. Java 경로와 메모리는 인스턴스 설정에서 선택하세요. Forge와 NeoForge는 공식 설치 프로그램을 사용하며 설치 전에 Java가 필요합니다.'),
('문제 해결','인스턴스 설정 → 로그를 여세요. 설치 복구는 게임 파일을 다시 검증합니다. 제거된 인스턴스와 월드는 로컬 휴지통에 남습니다. 잃어버린 버전은 Wine, 이전 Java 또는 더 이상 제공되지 않는 원본 패키지가 필요할 수 있습니다.')],
'zh':[
('1. 选择模式','我的实例是常规 Minecraft 客户端。失落版本可打开 EBALIA 档案库。'),
('2. 创建实例','选择名称、游戏版本和加载器。Vanilla 为原版游戏；Fabric、Quilt、Forge 和 NeoForge 支持模组。每个实例的世界和设置相互独立。目录在启动时及每隔 30 分钟自动刷新，现有实例的版本保持不变。'),
('3. 添加模组','在探索模组中选择实例，搜索 Sodium、Dynamic Lights、FallingTree 或 Veinminer。安装前请检查依赖。作者公布的兼容信息不保证所有模组组合都能正常运行。'),
('4. 将常用模组保存为包','在实例中选择将模组保存为模组包，然后从我的模组包应用到其他实例。启动器会查找适合目标版本和加载器的模组，并在下载前显示不可用项。不会复制不兼容的 JAR。本地模组必须能在 Modrinth 上被识别；停用的模组不会被包含。'),
('5. 开始游戏','添加账户、安装 Java，然后点击开始游戏／安装。安装完成后点击开始游戏。启动器会提示所需的 Java 版本。可在实例设置中选择 Java 路径和内存。Forge 和 NeoForge 使用官方安装器，安装前需要先准备 Java。'),
('遇到问题时','打开实例设置 → 日志。修复安装会重新校验游戏文件。移除的实例及其世界会保留在本地回收站。失落版本可能需要 Wine、旧版 Java 或已经不可用的原始安装包。')]
}
for code in codes:
 tables[code][keys[3]]=''.join(f'<h2>{h}</h2><p>{p}</p>' for h,p in guides[code])
 assert set(tables[code])==set(keys),(code,set(keys)-set(tables[code]))
 (root/f'resources/locales/{code}.json').write_text(json.dumps(tables[code],ensure_ascii=False,indent=2)+'\n')
print('10 complete UI catalogs generated')
