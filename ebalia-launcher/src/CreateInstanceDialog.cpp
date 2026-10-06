#include "CreateInstanceDialog.hpp"
#include "Language.hpp"
#include "Loaders.hpp"
#include "PackBrowser.hpp"
#include "PackService.hpp"
#include "ModRepository.hpp"
#include "InstanceIcons.hpp"
#include <QtWidgets>
#include <QtConcurrent>
namespace {
QString t(const char *s){return Language::key(QString::fromUtf8(s));}
QString loaderTitle(const QString &key){return key=="vanilla"||key.isEmpty()?QString():key=="neoforge"?QString("NeoForge"):key.left(1).toUpper()+key.mid(1);}
const QStringList sourceKeys{"custom","import","atlauncher","curseforge","ftb","legacy_ftb","import_ftb","modrinth","technic"};
QString describeLocal(const QJsonObject &o){auto loader=loaderTitle(o["loader"].toString());return o["name"].toString()+"\n"+o["source"].toString()+" · Minecraft "+o["mcVersion"].toString()+(loader.isEmpty()?QString():" · "+loader+" "+o["loaderVersion"].toString());}
}
CreateInstanceDialog::CreateInstanceDialog(McInstanceManager *manager,const QList<McVersion> &catalog,const QJsonArray &packs,const McInstance &source,bool copy,QWidget *parent):QDialog(parent),m_manager(manager),m_catalog(catalog){
    setObjectName("createInstanceDialog");setWindowTitle(t("New instance"));resize(1040,760);setMinimumSize(640,480);setAcceptDrops(true);
    auto windowLayout=new QVBoxLayout(this);windowLayout->setSpacing(12);auto scroll=new QScrollArea;scroll->setObjectName("instanceFormScroll");scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);windowLayout->addWidget(scroll,1);
    auto content=new QWidget;auto outer=new QVBoxLayout(content);outer->setContentsMargins(0,0,0,0);outer->setSpacing(12);scroll->setWidget(content);
    auto header=new QHBoxLayout;outer->addLayout(header);
    if(copy){try{m_iconKey=ModRepository::read(source.dir+"/instance.json")["icon"].toString(m_iconKey);if(m_iconKey=="custom"){m_customIcon.load(source.dir+"/instance-icon.png");if(m_customIcon.isNull())m_iconKey="grass";}}catch(...){}}
    m_icon=new QToolButton;m_icon->setObjectName("instanceIcon");m_icon->setIconSize(QSize(56,56));m_icon->setFixedSize(78,78);m_icon->setPopupMode(QToolButton::InstantPopup);m_icon->setToolTip(t("Choose an icon"));m_icon->setIcon(m_iconKey=="custom"?QIcon(QPixmap::fromImage(m_customIcon)):InstanceIcons::icon(m_iconKey));
    m_icon->setMenu(InstanceIcons::menu(m_icon,[this](const QString &key,const QImage &custom){
        m_iconKey=key;m_customIcon=custom;m_icon->setIcon(key=="custom"?QIcon(QPixmap::fromImage(custom)):InstanceIcons::icon(key));}));header->addWidget(m_icon,0,Qt::AlignTop);
    auto details=new QFormLayout;header->addLayout(details,1);
    m_name=new QLineEdit(copy?source.name+" (2)":"");m_name->setObjectName("instanceName");details->addRow(t("Name"),m_name);
    m_group=new QComboBox;m_group->setEditable(true);m_group->setObjectName("instanceGroup");m_group->addItem(t("No group"),"");
    QSet<QString> groups;for(auto i:manager->instances()){try{auto g=ModRepository::read(i.dir+"/instance.json")["group"].toString();if(!g.isEmpty())groups.insert(g);}catch(...){}}
    auto sortedGroups=groups.values();sortedGroups.sort(Qt::CaseInsensitive);for(auto g:sortedGroups)m_group->addItem(g,g);
    if(copy){try{auto g=ModRepository::read(source.dir+"/instance.json")["group"].toString();if(!g.isEmpty())m_group->setCurrentIndex(m_group->findData(g));}catch(...){}}
    details->addRow(t("Group"),m_group);

    auto body=new QHBoxLayout;outer->addLayout(body,1);
    m_sources=new QListWidget;m_sources->setObjectName("instanceSources");m_sources->setFixedWidth(228);m_sources->setIconSize(QSize(28,28));m_sources->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);m_sources->setTextElideMode(Qt::ElideRight);
    const QStringList labels{t("Custom"),t("Import"),"ATLauncher","CurseForge","FTB","FTB Legacy",t("Import FTB App"),"Modrinth","Technic"};
    for(int i=0;i<labels.size();++i){auto item=new QListWidgetItem(InstanceIcons::provider(sourceKeys[i]),labels[i],m_sources);item->setToolTip(labels[i]);item->setData(Qt::AccessibleTextRole,labels[i]);item->setSizeHint(QSize(200,42));}
    body->addWidget(m_sources);
    m_pages=new QStackedWidget;m_pages->setObjectName("instanceSourcePages");body->addWidget(m_pages,1);
    auto root=QFileInfo(m_manager->mcDir()).absolutePath();
    m_pages->addWidget(customPage(packs,copy));m_pages->addWidget(importPage());
    auto addBrowser=[this,root](const QString &provider){auto browser=new PackBrowser(root,provider);int index=m_pages->addWidget(browser);m_browsers[index]=browser;browser->changed=[this,index]{if(m_pages->currentIndex()==index)validate();};};
    for(auto provider:{"atlauncher","curseforge","ftb","legacy_ftb"})addBrowser(provider);
    m_pages->addWidget(ftbAppPage());
    for(auto provider:{"modrinth","technic"})addBrowser(provider);

    auto options=new QHBoxLayout;outer->addLayout(options);options->addWidget(new QLabel(t("Maximum memory")));
    m_memory=new QSpinBox;m_memory->setRange(512,65536);m_memory->setSingleStep(512);m_memory->setValue(copy?source.xmx:4096);m_memory->setSuffix(" MB");options->addWidget(m_memory);options->addStretch();
    auto note=new QLabel(t("Java is detected automatically. If the right version is missing, EBALIA downloads the official one. Each instance keeps its own worlds, mods and settings."));note->setWordWrap(true);note->setObjectName("muted");outer->addWidget(note);
    m_buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);m_buttons->button(QDialogButtonBox::Ok)->setText(t("Create instance"));m_buttons->button(QDialogButtonBox::Ok)->setProperty("play",true);windowLayout->addWidget(m_buttons);
    connect(m_buttons,&QDialogButtonBox::accepted,this,[this]{validate();if(m_buttons->button(QDialogButtonBox::Ok)->isEnabled())accept();});connect(m_buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(m_name,&QLineEdit::textChanged,this,[this]{validate();});
    connect(m_sources,&QListWidget::currentRowChanged,this,[this](int row){
        m_pages->setCurrentIndex(row);if(m_browsers.contains(row))m_browsers[row]->activate();if(row==Import||row==FTBApp)listLocalInstances();validate();
    });
    connect(manager,&McInstanceManager::manifestReady,this,[this](QList<McVersion> versions){m_catalog=versions;populateVersions();m_status->setText(t("Choose Minecraft, then a compatible mod loader."));});
    connect(manager,&McInstanceManager::manifestFailed,this,[this](QString error){m_status->setText(Language::message(error));});

    if(copy)for(auto v:catalog)if(v.id==source.mcVersion&&m_types.contains(v.type))m_types[v.type]->setChecked(true);
    populateVersions();
    if(copy){for(int i=0;i<m_versions->topLevelItemCount();++i)if(m_versions->topLevelItem(i)->text(0)==source.mcVersion)m_versions->setCurrentItem(m_versions->topLevelItem(i));if(auto radio=findChild<QRadioButton*>(source.loader+"Loader"))radio->click();}
    m_sources->setCurrentRow(Custom);validate();
}
void CreateInstanceDialog::resizeEvent(QResizeEvent *event){
    QDialog::resizeEvent(event);const bool compact=width()<900;m_sources->setFixedWidth(compact?62:228);
    m_sources->setStyleSheet(compact?"QListWidget#instanceSources {padding:4px;} QListWidget#instanceSources::item {padding:6px 4px;}":QString());
    for(int i=0;i<m_sources->count();++i){auto item=m_sources->item(i);item->setText(compact?QString():item->toolTip());}
}
QWidget *CreateInstanceDialog::customPage(const QJsonArray &packs,bool copy){
    auto page=new QWidget;page->setObjectName("customPage");auto right=new QVBoxLayout(page);right->setContentsMargins(0,0,0,0);
    m_status=new QLabel(t("Choose Minecraft, then a compatible mod loader."));m_status->setWordWrap(true);right->addWidget(m_status);
    auto gameArea=new QHBoxLayout;right->addLayout(gameArea,3);auto game=new QVBoxLayout;gameArea->addLayout(game,1);
    m_versions=new QTreeWidget;m_versions->setObjectName("minecraftVersions");m_versions->setColumnCount(3);m_versions->setHeaderLabels({t("Version"),t("Release date"),t("Type")});m_versions->setRootIsDecorated(false);
    m_versions->setSelectionMode(QAbstractItemView::SingleSelection);m_versions->header()->setSectionResizeMode(0,QHeaderView::Stretch);m_versions->header()->setSectionResizeMode(1,QHeaderView::ResizeToContents);m_versions->setMinimumHeight(150);game->addWidget(m_versions,1);
    m_search=new QLineEdit;m_search->setObjectName("versionSearch");m_search->setPlaceholderText(t("Search versions…"));m_search->setClearButtonEnabled(true);game->addWidget(m_search);
    auto filters=new QVBoxLayout;gameArea->addLayout(filters);filters->addWidget(new QLabel(t("Filter")));
    for(auto entry:{qMakePair("release","Releases"),qMakePair("snapshot","Snapshots"),qMakePair("old_beta","Betas"),qMakePair("old_alpha","Alphas")}){
        auto c=new QCheckBox(t(entry.second));c->setObjectName(QString(entry.first)+"Filter");c->setChecked(QString(entry.first)=="release");m_types[entry.first]=c;filters->addWidget(c);connect(c,&QCheckBox::toggled,this,[this]{populateVersions();});
    }
    filters->addStretch();auto refresh=new QPushButton(t("Refresh"));filters->addWidget(refresh);connect(refresh,&QPushButton::clicked,this,[this]{m_status->setText(t("Checking versions…"));m_manager->fetchManifest();});
    auto loaderArea=new QHBoxLayout;right->addLayout(loaderArea,2);auto versions=new QVBoxLayout;loaderArea->addLayout(versions,1);
    m_loaderStatus=new QLabel;m_loaderStatus->setWordWrap(true);versions->addWidget(m_loaderStatus);
    m_loaders=new QListWidget;m_loaders->setObjectName("loaderVersions");m_loaders->setMinimumHeight(110);versions->addWidget(m_loaders,1);
    m_loaderSearch=new QLineEdit;m_loaderSearch->setPlaceholderText(t("Search loader versions…"));m_loaderSearch->setClearButtonEnabled(true);versions->addWidget(m_loaderSearch);
    connect(m_loaderSearch,&QLineEdit::textChanged,this,[this](QString query){for(int i=0;i<m_loaders->count();++i)m_loaders->item(i)->setHidden(!m_loaders->item(i)->text().contains(query,Qt::CaseInsensitive));});
    auto choices=new QVBoxLayout;loaderArea->addLayout(choices);choices->addWidget(new QLabel(t("Mod loader")));auto buttons=new QButtonGroup(this);
    for(auto kind:{"vanilla","neoforge","forge","fabric","quilt"}){
        auto key=QString(kind);auto radio=new QRadioButton(key=="vanilla"?t("None"):loaderTitle(key));radio->setObjectName(key+"Loader");buttons->addButton(radio);choices->addWidget(radio);radio->setChecked(key=="vanilla");
        connect(radio,&QRadioButton::clicked,this,[this,key]{m_loader=key;fetchLoaders();});
    }
    choices->addStretch();auto reload=new QPushButton(t("Refresh"));choices->addWidget(reload);connect(reload,&QPushButton::clicked,this,[this]{fetchLoaders();});
    auto packRow=new QHBoxLayout;right->addLayout(packRow);packRow->addWidget(new QLabel(t("Initial pack")));
    m_pack=new QComboBox;m_pack->setObjectName("initialPack");m_pack->addItem(t("No pack"));for(auto v:packs)m_pack->addItem(v.toObject()["name"].toString(),v.toObject());packRow->addWidget(m_pack,1);if(copy)m_pack->setEnabled(false);
    connect(m_search,&QLineEdit::textChanged,this,[this]{populateVersions();});connect(m_versions,&QTreeWidget::currentItemChanged,this,[this]{fetchLoaders();});connect(m_loaders,&QListWidget::currentRowChanged,this,[this]{validate();});
    return page;
}
QWidget *CreateInstanceDialog::importPage(){
    auto page=new QWidget;page->setObjectName("importPage");auto l=new QVBoxLayout(page);l->setContentsMargins(0,0,0,0);
    l->addWidget(new QLabel(t("Local file, folder or direct download link")));
    auto row=new QHBoxLayout;l->addLayout(row);m_importPath=new QLineEdit;m_importPath->setObjectName("importPath");m_importPath->setPlaceholderText("pack.mrpack  ·  pack.zip  ·  https://…");m_importPath->setClearButtonEnabled(true);row->addWidget(m_importPath,1);
    auto file=new QPushButton(t("Choose file…"));row->addWidget(file);auto folder=new QPushButton(t("Choose folder…"));row->addWidget(folder);
    auto formats=new QLabel(t("Supported: Modrinth (.mrpack), CurseForge (.zip), Prism Launcher and MultiMC (.zip or folder), Technic and ATLauncher (.zip), CurseForge App and FTB App folders, EBALIA exports (.zip) and EBALIA packs (.json). You can also drag a file onto this window."));
    formats->setWordWrap(true);formats->setObjectName("muted");l->addWidget(formats);
    m_importStatus=new QLabel;m_importStatus->setWordWrap(true);l->addWidget(m_importStatus);
    l->addSpacing(6);l->addWidget(new QLabel(t("Instances found on this computer")));
    m_found=new QListWidget;m_found->setObjectName("localInstances");m_found->setIconSize(QSize(28,28));l->addWidget(m_found,1);
    connect(file,&QPushButton::clicked,this,[this]{auto path=QFileDialog::getOpenFileName(this,t("Import"),{},t("Modpacks")+" (*.mrpack *.zip *.json);;"+t("All files")+" (*)");if(!path.isEmpty())setImportPath(path);});
    connect(folder,&QPushButton::clicked,this,[this]{auto path=QFileDialog::getExistingDirectory(this,t("Import"));if(!path.isEmpty())setImportPath(path);});
    connect(m_importPath,&QLineEdit::textChanged,this,[this]{
        auto path=m_importPath->text().trimmed();QFileInfo info(path);
        m_importStatus->setText(path.isEmpty()?QString():path.startsWith("https://",Qt::CaseInsensitive)?t("The pack will be downloaded when you create the instance."):!info.exists()?t("This file or folder does not exist."):t("The pack defines the Minecraft version and mod loader."));
        validate();
    });
    connect(m_found,&QListWidget::currentItemChanged,this,[this](QListWidgetItem *item){if(!item||!item->data(Qt::UserRole).isValid())return;m_importPath->setText(item->data(Qt::UserRole).toJsonObject()["path"].toString());});
    return page;
}
QWidget *CreateInstanceDialog::ftbAppPage(){
    auto page=new QWidget;page->setObjectName("ftbAppPage");auto l=new QVBoxLayout(page);l->setContentsMargins(0,0,0,0);
    auto intro=new QLabel(t("Copy an instance installed with the FTB App. Worlds, mods and settings are copied; the FTB App keeps its own copy."));intro->setWordWrap(true);l->addWidget(intro);
    m_ftbList=new QListWidget;m_ftbList->setObjectName("ftbInstances");m_ftbList->setIconSize(QSize(28,28));l->addWidget(m_ftbList,1);
    auto row=new QHBoxLayout;l->addLayout(row);m_ftbFolder=new QLineEdit;m_ftbFolder->setReadOnly(true);m_ftbFolder->setPlaceholderText(t("Instances are searched in the FTB App folder."));row->addWidget(m_ftbFolder,1);
    auto browse=new QPushButton(t("Choose folder…"));row->addWidget(browse);m_ftbStatus=new QLabel;m_ftbStatus->setWordWrap(true);m_ftbStatus->setObjectName("muted");l->addWidget(m_ftbStatus);
    connect(m_ftbList,&QListWidget::currentItemChanged,this,[this]{validate();});
    connect(browse,&QPushButton::clicked,this,[this]{
        auto folder=QFileDialog::getExistingDirectory(this,t("Choose the FTB App instance folder"),QDir::homePath());if(folder.isEmpty())return;m_ftbFolder->setText(folder);
        QStringList candidates{folder};for(auto sub:QDir(folder).entryList(QDir::Dirs|QDir::NoDotAndDotDot))candidates<<folder+"/"+sub;QListWidgetItem *first=nullptr;
        for(auto path:candidates){try{auto o=ModRepository::read(path+"/instance.json");if(o["mcVersion"].toString().isEmpty())continue;QJsonObject e{{"name",o["name"].toString(QFileInfo(path).fileName())},{"mcVersion",o["mcVersion"]},{"path",path},{"source","FTB App"},{"loader",o["modLoader"].toString().section('-',0,0).toLower()},{"loaderVersion",o["modLoader"].toString().section('-',1)}};
            auto item=new QListWidgetItem(InstanceIcons::provider("import_ftb"),describeLocal(e));item->setData(Qt::UserRole,e);m_ftbList->insertItem(0,item);if(!first)first=item;}catch(...){}}
        if(first)m_ftbList->setCurrentItem(first);else QMessageBox::warning(this,"EBALIA",t("This folder does not contain an FTB App instance."));
    });
    return page;
}
void CreateInstanceDialog::listLocalInstances(){
    if(m_localListed)return;m_localListed=true;
    for(auto v:PackService::localInstances()){
        auto o=v.toObject();bool ftb=o["source"]=="FTB App";auto item=new QListWidgetItem(InstanceIcons::provider(ftb?"import_ftb":"import"),describeLocal(o));item->setData(Qt::UserRole,o);(ftb?m_ftbList:m_found)->addItem(item);
    }
    auto empty=[](QListWidget *list,const QString &text){if(list->count())return;auto item=new QListWidgetItem(text,list);item->setFlags(Qt::NoItemFlags);};
    empty(m_found,t("No Prism Launcher, MultiMC or CurseForge instances were found. Choose a file or folder above."));
    empty(m_ftbList,t("No FTB App instances were found. Choose their folder below."));
}
void CreateInstanceDialog::showPage(int page){m_sources->setCurrentRow(page);}
void CreateInstanceDialog::setImportPath(const QString &value){
    auto path=value.trimmed();
    if(path.endsWith(".json",Qt::CaseInsensitive)&&!path.startsWith("https://",Qt::CaseInsensitive)){
        try{
            auto pack=ModRepository::read(path);if(pack["schema"].toInt()!=1||!pack["projects"].isArray()||pack["name"].toString().isEmpty())throw std::runtime_error("Invalid EBALIA pack");
            m_pack->addItem(pack["name"].toString(),pack);m_pack->setCurrentIndex(m_pack->count()-1);showPage(Custom);m_status->setText(t("Pack added. Choose the Minecraft version and loader it will be installed on."));
        }catch(const std::exception &e){QMessageBox::warning(this,"EBALIA",Language::message(QString::fromUtf8(e.what())));}
        return;
    }
    showPage(Import);{QSignalBlocker block(m_found);m_found->setCurrentItem(nullptr);}m_importPath->setText(path);
}
void CreateInstanceDialog::dragEnterEvent(QDragEnterEvent *event){if(event->mimeData()->hasUrls())event->acceptProposedAction();}
void CreateInstanceDialog::dropEvent(QDropEvent *event){auto urls=event->mimeData()->urls();if(urls.isEmpty())return;auto url=urls.first();setImportPath(url.isLocalFile()?url.toLocalFile():url.toString());event->acceptProposedAction();}
void CreateInstanceDialog::populateVersions(){
    auto selected=m_versions->currentItem()?m_versions->currentItem()->text(0):QString();QSignalBlocker block(m_versions);m_versions->clear();QTreeWidgetItem *keep=nullptr;
    for(auto v:m_catalog){
        if(!m_types.contains(v.type)||!m_types[v.type]->isChecked()||!v.id.contains(m_search->text().trimmed(),Qt::CaseInsensitive))continue;
        auto item=new QTreeWidgetItem(m_versions,{v.id,v.releaseTime,Language::key(v.type=="release"?"Release":v.type=="snapshot"?"Snapshot":v.type=="old_beta"?"Beta":"Alpha")});if(v.id==selected)keep=item;
    }
    if(!keep&&m_versions->topLevelItemCount())keep=m_versions->topLevelItem(0);m_versions->setCurrentItem(keep);block.unblock();fetchLoaders();
}
void CreateInstanceDialog::fetchLoaders(){
    ++m_request;m_loading=false;m_loaders->clear();m_loaderSearch->clear();if(!m_versions->currentItem()){m_loaderStatus->setText(t("Choose a Minecraft version."));validate();return;}
    if(m_loader=="vanilla"){m_loaderStatus->setText(t("No mod loader selected. Vanilla uses the original game."));validate();return;}
    auto game=m_versions->currentItem()->text(0),loader=m_loader;int id=m_request;m_loading=true;m_loaderStatus->setText(t("Checking compatible loader versions…"));validate();auto watcher=new QFutureWatcher<QJsonObject>(this);
    connect(watcher,&QFutureWatcher<QJsonObject>::finished,this,[this,watcher,id]{
        auto result=watcher->result();watcher->deleteLater();if(id!=m_request)return;m_loading=false;if(result.contains("error")){m_loaderStatus->setText(result["error"].toString());validate();return;}
        for(auto v:result["versions"].toArray())m_loaders->addItem(v.toString());m_loaderStatus->setText(m_loaders->count()?t("Choose a loader version."):t("No compatible loader versions found."));if(m_loaders->count())m_loaders->setCurrentRow(0);validate();
    });
    watcher->setFuture(QtConcurrent::run([loader,game]{try{QJsonArray versions;for(auto v:Loaders::versions(loader,game))versions.append(v);return QJsonObject{{"versions",versions}};}catch(const std::exception &e){return QJsonObject{{"error",QString::fromUtf8(e.what())}};}}));
}
QString CreateInstanceDialog::suggestedName()const{
    const int page=m_pages->currentIndex();
    if(page==Custom){if(!m_versions->currentItem())return {};auto title=loaderTitle(m_loader);return m_versions->currentItem()->text(0)+(title.isEmpty()?QString():" "+title);}
    if(page==Import){
        auto path=m_importPath->text().trimmed();if(path.isEmpty())return {};
        if(auto item=m_found->currentItem();item&&item->data(Qt::UserRole).toJsonObject()["path"].toString()==path)return item->data(Qt::UserRole).toJsonObject()["name"].toString();
        if(path.startsWith("https://",Qt::CaseInsensitive))return QFileInfo(QUrl(path).fileName()).completeBaseName();
        QFileInfo info(path);return info.isDir()?info.fileName():info.completeBaseName();
    }
    if(page==FTBApp){auto item=m_ftbList->currentItem();return item?item->data(Qt::UserRole).toJsonObject()["name"].toString():QString();}
    if(m_browsers.contains(page))return m_browsers[page]->selectedPack["name"].toString();
    return {};
}
void CreateInstanceDialog::validate(){
    if(!m_buttons)return;
    const auto suggestion=suggestedName();m_name->setPlaceholderText(suggestion.isEmpty()?t("Instance name"):suggestion);
    const int page=m_pages->currentIndex();bool ready=false;
    if(page==Custom)ready=m_versions->currentItem()&&!m_loading&&(m_loader=="vanilla"||m_loaders->currentItem());
    else if(page==Import){auto path=m_importPath->text().trimmed();ready=path.startsWith("https://",Qt::CaseInsensitive)||(!path.isEmpty()&&QFileInfo::exists(path));}
    else if(page==FTBApp)ready=m_ftbList->currentItem()&&m_ftbList->currentItem()->data(Qt::UserRole).isValid();
    else if(m_browsers.contains(page))ready=!m_browsers[page]->selectedPack.isEmpty()&&!m_browsers[page]->selectedVersion.isEmpty();
    const auto name=m_name->text().trimmed().isEmpty()?suggestion:m_name->text().trimmed();
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(ready&&!name.isEmpty());
}
QJsonObject CreateInstanceDialog::configuration()const{
    const int page=m_pages->currentIndex();QJsonObject providerPack,providerVersion;
    if(page==Import){auto path=m_importPath->text().trimmed();if(!path.isEmpty())providerPack={{"provider","import"},{"path",path}};}
    else if(page==FTBApp){if(auto item=m_ftbList->currentItem();item&&item->data(Qt::UserRole).isValid())providerPack={{"provider","import_ftb"},{"path",item->data(Qt::UserRole).toJsonObject()["path"]}};}
    else if(m_browsers.contains(page)){providerPack=m_browsers[page]->selectedPack;providerVersion=m_browsers[page]->selectedVersion;}
    auto name=m_name->text().trimmed();if(name.isEmpty())name=suggestedName();
    auto group=m_group->currentText().trimmed();if(m_group->currentIndex()==0&&group==t("No group"))group.clear();
    return {{"name",name},{"mcVersion",m_versions->currentItem()?m_versions->currentItem()->text(0):QString()},{"providerPack",providerPack},{"providerVersion",providerVersion},{"loader",m_loader},
        {"loaderVersion",m_loaders->currentItem()?m_loaders->currentItem()->text():QString()},{"group",group},{"icon",m_iconKey},{"xmx",m_memory->value()},{"pack",page==Custom?m_pack->currentData().toJsonObject():QJsonObject()}};
}
