#include "GpuInfo.hpp"
#include "Language.hpp"
#include <QtCore>
namespace {
QString normal(QString s){s=s.toLower();s.remove("(r)");s.remove("(tm)");s.remove("®");s.remove("™");s.remove("nvidia");s.remove("amd");s.remove(QRegularExpression("[^a-z0-9]"));return s;}
#ifdef Q_OS_LINUX
QString readFile(const QString &path){QFile f(path);return f.open(QIODevice::ReadOnly)?QString::fromUtf8(f.readAll()).trimmed():QString();}
#endif
}
namespace GpuInfo {
Gpu describe(const QString &raw){
    Gpu g;g.name=raw.simplified();const auto n=g.name.toLower();
    static const QStringList virtualNames{"basic display","basic render","vmware","virtualbox","hyper-v","qxl","red hat","parallels","virtio","llvmpipe","remote display","citrix","parsec","spacedesk","bochs","cirrus"};
    for(const auto &v:virtualNames)if(n.contains(v)){g.virtualAdapter=true;return g;}
    if(n.contains("nvidia")||n.contains("geforce")||n.contains("quadro")||n.contains("rtx")||n.contains("gtx"))g.vendor="NVIDIA";
    else if(n.contains("amd")||n.contains("radeon")||n.contains("ati "))g.vendor="AMD";
    else if(n.contains("intel"))g.vendor="Intel";
    if(g.vendor=="Intel")g.integrated=!QRegularExpression(R"(arc.*\b[ab]\d{3})").match(n).hasMatch(); // Arc A770 / B580 are cards; "Arc Graphics" is in the processor
    if(g.vendor=="AMD")g.integrated=!n.contains(" rx ")&&!n.contains("pro w")&&!n.contains("radeon pro")&&!n.contains("vii");
    return g;
}
QList<Gpu> detect(){
    QList<Gpu> out;
#if defined(Q_OS_WIN)
    QSettings display(R"(HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318})",QSettings::Registry64Format);
    for(const auto &key:display.childGroups()){if(!QRegularExpression("^\\d{4}$").match(key).hasMatch())continue;const auto name=display.value(key+"/DriverDesc").toString();if(!name.isEmpty())out<<describe(name);}
#elif defined(Q_OS_LINUX)
    for(const auto &card:QDir("/sys/class/drm").entryList({"card?","card??"},QDir::Dirs|QDir::System)){
        const auto device=QFileInfo("/sys/class/drm/"+card+"/device").canonicalFilePath();if(device.isEmpty())continue;
        const auto vendorId=readFile(device+"/vendor");QString name;
        if(auto lspci=QStandardPaths::findExecutable("lspci");!lspci.isEmpty()){QProcess p;p.start(lspci,{"-mm","-s",QFileInfo(device).fileName()});if(p.waitForFinished(3000)){const auto fields=QString::fromUtf8(p.readAllStandardOutput()).split('"');if(fields.size()>5)name=fields[3]+" "+fields[5];}}
        if(name.isEmpty())name=vendorId=="0x10de"?"NVIDIA graphics":vendorId=="0x1002"?"AMD Radeon graphics":vendorId=="0x8086"?"Intel graphics":"Graphics";
        auto g=describe(name);if(g.vendor.isEmpty())g.vendor=vendorId=="0x10de"?"NVIDIA":vendorId=="0x1002"?"AMD":vendorId=="0x8086"?"Intel":QString();
        bool seen=false;for(const auto &o:out)seen|=o.name==g.name;if(!seen)out<<g;
    }
#elif defined(Q_OS_MACOS)
    out<<Gpu{"Apple / Mac","Apple",false,false};
#endif
    return out;
}
Computer computer(){
    Computer c;
#if defined(Q_OS_WIN)
    QSettings bios(R"(HKEY_LOCAL_MACHINE\HARDWARE\DESCRIPTION\System\BIOS)",QSettings::Registry64Format);
    c.manufacturer=bios.value("SystemManufacturer").toString().trimmed();c.model=bios.value("SystemProductName").toString().trimmed();
#elif defined(Q_OS_LINUX)
    c.manufacturer=readFile("/sys/class/dmi/id/sys_vendor");c.model=readFile("/sys/class/dmi/id/product_name");
#endif
    return c;
}
Gpu pick(const QList<Gpu> &gpus,const QString &vendor){
    QList<Gpu> real;for(const auto &g:gpus)if(!g.virtualAdapter)real<<g;
    for(const auto &g:real)if(!vendor.isEmpty()&&g.vendor==vendor)return g;
    for(const auto &g:real)if(!g.integrated&&!g.vendor.isEmpty())return g;
    if(!real.isEmpty())return real.first();
    return gpus.isEmpty()?Gpu{}:gpus.first();
}
DriverLink autoDetect(const QString &vendor){
    if(vendor=="NVIDIA")return {"https://www.nvidia.com/en-us/software/nvidia-app/",Language::text("NVIDIA App (detecta e instala tu driver)","NVIDIA App (finds and installs your driver)","NVIDIA App (detecta e instala seu driver)"),false};
    if(vendor=="AMD")return {"https://www.amd.com/en/support/download/drivers.html",Language::text("AMD Auto-Detect (detecta e instala tu driver)","AMD Auto-Detect (finds and installs your driver)","AMD Auto-Detect (detecta e instala seu driver)"),false};
    if(vendor=="Intel")return {"https://www.intel.com/content/www/us/en/support/detect.html",Language::text("Intel Driver & Support Assistant (detecta tu driver)","Intel Driver & Support Assistant (finds your driver)","Intel Driver & Support Assistant (detecta seu driver)"),false};
    return {};
}
DriverLink driverLink(const Gpu &gpu,Fetch fetch,QString system){
    if(system.isEmpty()){
#if defined(Q_OS_WIN)
        system="windows";
#elif defined(Q_OS_MACOS)
        system="macos";
#else
        system="linux";
#endif
    }
    const auto n=gpu.name.toLower();
    if(system=="macos")return {}; // macOS updates its graphics drivers itself
    if(system=="linux"){
        if(gpu.vendor=="NVIDIA")return {"https://www.nvidia.com/en-us/drivers/unix/",Language::text("Driver NVIDIA para Linux (o el paquete de tu distribución)","NVIDIA driver for Linux (or your distribution's package)","Driver NVIDIA para Linux (ou o pacote da sua distribuição)"),false};
        return {}; // AMD and Intel drivers (Mesa) come with the system updates
    }
    if(gpu.vendor=="NVIDIA"){
        // NVIDIA's own driver search: product id by name, then the newest Game Ready driver for it.
        try{
            const auto xml=QString::fromUtf8(fetch(QUrl("https://www.nvidia.com/Download/API/lookupValueSearch.aspx?TypeID=3")));
            static const QRegularExpression entry(R"re(<LookupValue ParentID="(\d+)">\s*<Name>([^<]*)</Name>\s*<Value>(\d+)</Value>)re");
            QString psid,pfid;for(auto it=entry.globalMatch(xml);it.hasNext();){auto m=it.next();if(normal(m.captured(2))==normal(gpu.name)){psid=m.captured(1);pfid=m.captured(3);break;}}
            if(!pfid.isEmpty()){
                const auto json=QJsonDocument::fromJson(fetch(QUrl("https://gfwsl.geforce.com/services_toolkit/services/com/nvidia/services/AjaxDriverService.php?func=DriverManualLookup&psid="+psid+"&pfid="+pfid+"&osID=57&languageCode=1033&isWHQL=1&dch=1&sort1=0&numberOfResults=1"))).object();
                const auto info=json["IDS"].toArray().first().toObject()["downloadInfo"].toObject();const QUrl details(info["DetailsURL"].toString());
                if(details.scheme()=="https"&&details.host().endsWith("nvidia.com"))return {details.toString(),Language::text("Driver NVIDIA %1 para %2","NVIDIA driver %1 for %2","Driver NVIDIA %1 para %2").arg(info["Version"].toString(),gpu.name),true};
            }
        }catch(...){}
        return autoDetect("NVIDIA");
    }
    if(gpu.vendor=="AMD"){
        // AMD's page for each Radeon RX card: /radeon-rx/radeon-rx-7000-series/amd-radeon-rx-7600.html (checked before use).
        static const QRegularExpression rx(R"(rx\s+(\d)(\d{3})(m?)(?:\s+(xtx|xt|gre))?)");
        if(auto m=rx.match(n);m.hasMatch()&&!gpu.integrated){
            const auto model=m.captured(1)+m.captured(2)+m.captured(3)+(m.captured(4).isEmpty()?QString():"-"+m.captured(4));
            const QUrl page("https://www.amd.com/en/support/downloads/drivers.html/graphics/radeon-rx/radeon-rx-"+m.captured(1)+"000"+m.captured(3)+"-series/amd-radeon-rx-"+model+".html");
            try{if(!fetch(page).isEmpty())return {page.toString(),Language::text("Driver AMD para %1","AMD driver for %1","Driver AMD para %1").arg(gpu.name),true};}catch(...){}
        }
        return autoDetect("AMD"); // also the right tool for Radeon graphics inside Ryzen processors
    }
    if(gpu.vendor=="Intel"){
        if(!gpu.integrated)return {"https://www.intel.com/content/www/us/en/download/785597/intel-arc-graphics-windows.html",Language::text("Driver Intel Arc para %1","Intel Arc driver for %1","Driver Intel Arc para %1").arg(gpu.name),true};
        // 6th–10th generation processors (HD/UHD 5xx/6xx): their own driver package.
        if(QRegularExpression(R"((uhd|hd) graphics (5\d\d|6\d\d|p6\d\d))").match(n).hasMatch()||n.contains("iris plus graphics 6")||n.contains("iris graphics 5")||n.contains("iris plus graphics 64"))
            return {"https://www.intel.com/content/www/us/en/download/776137/intel-7th-10th-gen-processor-graphics-windows.html",Language::text("Driver Intel para gráficos de 7.ª a 10.ª generación","Intel driver for 7th–10th gen processor graphics","Driver Intel para gráficos de 7.ª a 10.ª geração"),true};
        return autoDetect("Intel");
    }
    return {};
}
DriverLink manufacturerSupport(const Computer &c){
    const auto m=c.manufacturer.toLower();
    struct Maker{QStringList names;QString brand,url;};
    static const QList<Maker> makers{
        {{"dell","alienware"},"Dell","https://www.dell.com/support/home/"},{{"hp","hewlett"},"HP","https://support.hp.com/drivers"},{{"lenovo"},"Lenovo","https://pcsupport.lenovo.com/"},
        {{"asus"},"ASUS","https://www.asus.com/support/download-center/"},{{"acer"},"Acer","https://www.acer.com/us-en/support/drivers-and-manuals"},{{"micro-star","msi"},"MSI","https://www.msi.com/support/download"},
        {{"gigabyte","aorus"},"GIGABYTE","https://www.gigabyte.com/Support"},{{"samsung"},"Samsung","https://www.samsung.com/us/support/downloads/"}};
    for(const auto &maker:makers)for(const auto &name:maker.names)if(m.startsWith(name)||m.contains(name+" ")||m==name)
        return {maker.url,Language::text("Soporte de %1 (drivers para tu %2)","%1 support (drivers for your %2)","Suporte da %1 (drivers para seu %2)").arg(maker.brand,c.model.isEmpty()?Language::text("equipo","computer","computador"):c.model),false};
    return {};
}
}
