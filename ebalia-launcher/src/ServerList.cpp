#include "ServerList.hpp"
#include <QFile>
#include <QSaveFile>
#include <QtEndian>
#include <stdexcept>
namespace {
enum Type:quint8{End,Byte,Short,Int,Long,Float,Double,ByteArray,String,List,Compound,IntArray,LongArray};
struct Entry{quint8 type;QByteArray name,payload;};
using Entries=QList<Entry>;
struct Document{QByteArray rootName;Entries root;QList<Entries> servers;};
[[noreturn]] void fail(const char *message){throw std::runtime_error(message);}
class Reader {
public:
 explicit Reader(const QByteArray &data):m_data(data){}
 quint8 byte(){need(1);return quint8(m_data[m_pos++]);}
 quint16 u16(){need(2);auto v=qFromBigEndian<quint16>(m_data.constData()+m_pos);m_pos+=2;return v;}
 qint32 i32(){need(4);auto v=qFromBigEndian<qint32>(m_data.constData()+m_pos);m_pos+=4;return v;}
 QByteArray take(qsizetype n){need(n);auto v=m_data.mid(m_pos,n);m_pos+=n;return v;}
 QByteArray name(){return take(u16());}
 // Returns the raw payload of a tag, validating its structure.
 QByteArray payload(quint8 type,int depth=0){
  if(depth>64)fail("servers.dat is nested too deeply");
  const auto start=m_pos;skip(type,depth);return m_data.mid(start,m_pos-start);
 }
 Entries compound(int depth=0){
  Entries entries;
  for(quint8 type=byte();type!=End;type=byte()){auto name=this->name();entries.append({type,name,payload(type,depth+1)});}
  return entries;
 }
 qsizetype pos() const {return m_pos;}
private:
 void need(qsizetype n){if(n<0||m_pos+n>m_data.size())fail("servers.dat is truncated");}
 void count(qint32 n,qsizetype size){if(n<0)fail("servers.dat has a negative length");need(qsizetype(n)*size);m_pos+=qsizetype(n)*size;}
 void skip(quint8 type,int depth){
  switch(type){
  case Byte:need(1);m_pos+=1;break;case Short:need(2);m_pos+=2;break;
  case Int:case Float:need(4);m_pos+=4;break;case Long:case Double:need(8);m_pos+=8;break;
  case ByteArray:count(i32(),1);break;case IntArray:count(i32(),4);break;case LongArray:count(i32(),8);break;
  case String:take(u16());break;
  case List:{const auto element=byte();const auto n=i32();if(n<0)fail("servers.dat has a negative length");for(qint32 i=0;i<n;++i)skip(element,depth+1);break;}
  case Compound:compound(depth);break;
  default:fail("servers.dat contains an unknown tag");
  }
 }
 const QByteArray &m_data;qsizetype m_pos=0;
};
void put16(QByteArray &out,quint16 v){char b[2];qToBigEndian(v,b);out.append(b,2);}
void put32(QByteArray &out,qint32 v){char b[4];qToBigEndian(v,b);out.append(b,4);}
QByteArray text(const QString &value){auto bytes=value.toUtf8();if(bytes.size()>65535)fail("Text is too long");QByteArray out;put16(out,quint16(bytes.size()));return out+bytes;}
void writeEntries(QByteArray &out,const Entries &entries){
 for(const auto &e:entries){out.append(char(e.type));put16(out,quint16(e.name.size()));out+=e.name;out+=e.payload;}
 out.append(char(End));
}
Document load(const QString &file){
 Document doc;QFile in(file);
 if(!in.exists())return doc;
 if(!in.open(QIODevice::ReadOnly)||in.size()>16*1024*1024)fail("Could not read servers.dat");
 const auto data=in.readAll();if(data.isEmpty())return doc;
 Reader reader(data);if(reader.byte()!=Compound)fail("servers.dat is not an NBT compound");
 doc.rootName=reader.name();
 for(const auto &e:reader.compound()) {
  if(e.type==List&&e.name=="servers") {
   Reader list(e.payload);const auto element=list.byte();const auto n=list.i32();
   if(n>0&&element!=Compound)fail("servers.dat has an unexpected server list");
   for(qint32 i=0;i<n;++i)doc.servers.append(list.compound(1));
  } else doc.root.append(e);
 }
 return doc;
}
void save(const QString &file,const Document &doc){
 QByteArray list;list.append(char(Compound));put32(list,qint32(doc.servers.size()));
 for(const auto &server:doc.servers)writeEntries(list,server);
 Entries root=doc.root;root.append({List,"servers",list});
 QByteArray out;out.append(char(Compound));put16(out,quint16(doc.rootName.size()));out+=doc.rootName;writeEntries(out,root);
 QSaveFile f(file);if(!f.open(QIODevice::WriteOnly)||f.write(out)!=out.size()||!f.commit())fail("Could not save servers.dat");
}
QString string(const Entries &entries,const char *name){
 for(const auto &e:entries)if(e.type==String&&e.name==name){Reader r(e.payload);return QString::fromUtf8(r.take(r.u16()));}
 return {};
}
}
namespace ServerList {
QList<Server> read(const QString &file){
 QList<Server> out;
 try{
  for(const auto &entries:load(file).servers) {
   Server s{string(entries,"name"),string(entries,"ip"),{}};
   const auto icon=QByteArray::fromBase64(string(entries,"icon").toLatin1());
   if(!icon.isEmpty())s.icon.loadFromData(icon,"PNG");
   if(!s.address.isEmpty())out.append(s);
  }
 }catch(const std::exception &){}
 return out;
}
void add(const QString &file,const QString &name,const QString &address){
 auto doc=load(file);
 doc.servers.append({{String,"name",text(name.trimmed().isEmpty()?address.trimmed():name.trimmed())},{String,"ip",text(address.trimmed())}});
 save(file,doc);
}
void remove(const QString &file,int index){
 auto doc=load(file);
 // Indices follow read(), which leaves out entries without an address.
 int visible=-1;
 for(int i=0;i<doc.servers.size();++i)if(!string(doc.servers[i],"ip").isEmpty()&&++visible==index){doc.servers.removeAt(i);save(file,doc);return;}
 fail("Server not found");
}
}
