#include <QCoreApplication>
#include <QFile>
#include <cstdio>
int main(int argc,char **argv) {
 QCoreApplication app(argc,argv);
 if(app.arguments().contains("--update-probe")){std::puts("{\"version\":\"1.2.0\"}");return 0;}
 if(app.arguments().contains("--finish-update")){
  QFile file(qEnvironmentVariable("EBALIA_DATA_DIR")+"/update-child-started");
  if(!file.open(QIODevice::WriteOnly))return 1;file.write("started");return 0;
 }
 return 1;
}
