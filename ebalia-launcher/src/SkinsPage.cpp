#include "SkinsPage.hpp"
#include "SkinManager.hpp"
#include "AccountManager.hpp"
#include "McInstanceManager.hpp"
#include "MsAuth.hpp"
#include "Language.hpp"
#include "Ui.hpp"
#include <QtWidgets>
using Language::key;
SkinsPage::SkinsPage(QString root,AccountManager *accounts,McInstanceManager *instances,Work work,QWidget *parent)
    :QWidget(parent),m_root(std::move(root)),m_accounts(accounts),m_instances(instances),m_work(std::move(work)){
    auto outer=new QVBoxLayout(this);outer->setContentsMargins(0,0,0,0);auto row=new Ui::ResponsiveRow(660);outer->addWidget(row);auto layout=row->box();layout->setSpacing(22);
    auto visual=new QVBoxLayout;layout->addLayout(visual,1);m_preview=new QLabel;m_preview->setAlignment(Qt::AlignCenter);m_preview->setMinimumSize(240,320);m_preview->setObjectName("skinPreview");visual->addWidget(m_preview,1);
    auto turn=new QPushButton(key("Front / back"));visual->addWidget(turn);connect(turn,&QPushButton::clicked,this,[this]{m_back=!m_back;showSkin();});
    auto controls=new QVBoxLayout;layout->addLayout(controls,2);m_list=new QListWidget;controls->addWidget(m_list,1);connect(m_list,&QListWidget::currentRowChanged,this,[this]{showSkin();});
    m_details=new QLabel;m_details->setWordWrap(true);m_details->setTextFormat(Qt::PlainText);controls->addWidget(m_details);
    auto add=[&](QString title,std::function<void()> fn){auto b=new QPushButton(title);controls->addWidget(b);connect(b,&QPushButton::clicked,this,fn);return b;};
    add(key("Import PNG skin"),[this]{importSkin();});
    add(key("Apply to Microsoft account"),[this]{apply(false);})->setProperty("play",true);
    add(key("Apply locally to an instance"),[this]{apply(true);});
    add(key("Restore local appearance"),[this]{
        QStringList names;QList<McInstance> targets;for(const auto &i:m_instances->instances())if(!m_instances->isRunning(i.dir)&&!m_instances->isInstalling(i.dir)){names<<i.name+" · "+i.mcVersion;targets<<i;}
        bool ok;auto value=QInputDialog::getItem(this,key("Restore local appearance"),key("Instance"),names,0,false,&ok);if(!ok||names.indexOf(value)<0)return;
        try{SkinManager::removeLocal(targets[names.indexOf(value)].dir);m_details->setText(key("Local appearance restored."));}catch(const std::exception &e){QMessageBox::warning(this,"EBALIA",QString::fromUtf8(e.what()));}
    });
    auto note=new QLabel(key("Microsoft skins are visible on servers. Local skins only change default textures on this client; other default players may also look different."));note->setWordWrap(true);note->setObjectName("muted");controls->addWidget(note);refresh();
}
void SkinsPage::refresh(){m_list->clear();try{m_skins=SkinManager(m_root).skins();for(const auto &v:m_skins){auto o=v.toObject();m_list->addItem(o["name"].toString()+" · "+(o["variant"].toString()=="slim"?key("Slim"):key("Classic")));}if(m_list->count())m_list->setCurrentRow(0);else{m_details->setText(key("Import a 64 × 64 or 64 × 32 PNG to start your skin library."));m_preview->setText(key("Skins"));}}catch(const std::exception &e){m_details->setText(QString::fromUtf8(e.what()));}}
void SkinsPage::showSkin(){int index=m_list->currentRow();if(index<0||index>=m_skins.size())return;auto o=m_skins[index].toObject();QImage image(o["file"].toString());m_preview->setPixmap(QPixmap::fromImage(SkinManager::preview(image,o["variant"].toString(),m_back)));m_details->setText(o["name"].toString());}
void SkinsPage::importSkin(){
    auto file=QFileDialog::getOpenFileName(this,key("Import PNG skin"),{},"PNG (*.png)");if(file.isEmpty())return;QDialog dialog(this);dialog.setWindowTitle(key("Import PNG skin"));QFormLayout form(&dialog);QLineEdit name(QFileInfo(file).completeBaseName());QComboBox variant;variant.addItem(key("Classic"),"classic");variant.addItem(key("Slim"),"slim");form.addRow(key("Name"),&name);form.addRow(key("Model"),&variant);QDialogButtonBox buttons(QDialogButtonBox::Save|QDialogButtonBox::Cancel);form.addRow(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);if(Ui::openWindow(dialog)!=QDialog::Accepted)return;
    try{SkinManager(m_root).importSkin(file,name.text(),variant.currentData().toString());refresh();}catch(const std::exception &e){QMessageBox::warning(this,"EBALIA",QString::fromUtf8(e.what()));}
}
void SkinsPage::apply(bool local){
    int index=m_list->currentRow();if(index<0||index>=m_skins.size())return;auto skin=m_skins[index].toObject();auto file=skin["file"].toString(),variant=skin["variant"].toString();
    if(local){
        QStringList names;QList<McInstance> targets;for(const auto &i:m_instances->instances())if(i.ready&&!m_instances->isRunning(i.dir)&&!m_instances->isInstalling(i.dir)){names<<i.name+" · "+i.mcVersion;targets<<i;}
        if(names.isEmpty()){QMessageBox::information(this,"EBALIA",key("Install an instance and close the game before applying a local skin."));return;}
        bool ok;auto value=QInputDialog::getItem(this,key("Apply locally to an instance"),key("Instance"),names,0,false,&ok);if(!ok)return;auto target=targets[names.indexOf(value)];auto jar=m_instances->mcDir()+"/versions/"+target.mcVersion+"/"+target.mcVersion+".jar";
        m_work(key("Applying skin…"),[=]{SkinManager::applyLocal(file,variant,target.dir,jar);return QJsonObject{};},[this](QJsonObject){m_details->setText(key("Skin applied. Start the game to see it."));});return;
    }
    auto account=m_accounts->active();if(account.type!="msa"){QMessageBox::information(this,"EBALIA",key("Choose a Microsoft account to apply an online skin."));return;}
    QProgressDialog progress(key("Refreshing session…"),key("Cancel"),0,0,this);progress.setWindowModality(Qt::ApplicationModal);MsAuth auth;QString token,refresh,uuid,name;
    connect(&auth,&MsAuth::loginDone,&progress,[&](const QString &t,const QString &r,const QString &u,const QString &n){token=t;refresh=r;uuid=u;name=n;progress.accept();});
    connect(&auth,&MsAuth::loginFailed,&progress,[&](const QString &error){progress.reject();QMessageBox::warning(this,"EBALIA",Language::message(error));});
    QTimer::singleShot(0,&auth,[&]{auth.refresh(account.refreshToken);});progress.exec();auth.cancel();if(token.isEmpty())return;
    m_accounts->addAccount({"msa",name,uuid,token,refresh});
    m_work(key("Applying skin…"),[=]{SkinManager::upload(file,variant,token);return QJsonObject{};},[this](QJsonObject){m_details->setText(key("Skin applied. Start the game to see it."));});
}
