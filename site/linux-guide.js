(() => {
 const box=document.querySelector('#linux-guide');
 const words={
 es:['Instalación por distribución','Elegí tu distribución','Compilar y abrir','Instalar dependencias','Descargá el TAR.GZ con el botón de arriba. En la carpeta donde lo guardaste, abrí una terminal y ejecutá:','En NixOS usá el flake del proyecto: Nix descarga las dependencias y compila el launcher. El TAR.GZ de Ubuntu no funciona directamente en NixOS. La primera compilación puede tardar y descargar varios GB.','Para evitar incompatibilidades entre bibliotecas, compilá el código de esta versión con las dependencias de tu distribución. Ejecutá los bloques en orden, en una terminal.','Solo Ubuntu 24.04 x64. Para otras versiones o errores de GLIBC/Qt, elegí la opción de compilación Debian/Ubuntu.','Necesitás CMake ≥ 3.21, un compilador C++20, Qt ≥ 6.4 (Widgets, Network, Concurrent) y libarchive. Instalá sus paquetes de desarrollo con el gestor de tu distribución y seguí el bloque de compilación. Alpine/musl y ARM requieren una compilación propia; no están validados con el paquete x64.','Estos pasos no se han ejecutado en todas las distribuciones. El binario publicado se construye en Ubuntu 24.04. No ejecutes el launcher con sudo.','Documentación de paquetes','Compilación','La carpeta de destino debe ser nueva. Repetí los pasos con la versión nueva para actualizar.','Guardá el archivo con su nombre original: ebalia-linux-x64.tar.gz.','Otras distribuciones'],
 en:['Install by distribution','Choose your distribution','Build and launch','Install dependencies','Download the TAR.GZ above. Open a terminal in the folder where you saved it and run:','On NixOS use the project flake: Nix downloads dependencies and builds the launcher. The Ubuntu TAR.GZ does not run directly on NixOS. The first build can take time and download several GB.','Build this release with your distribution’s libraries to avoid compatibility issues. Run the blocks in order in a terminal.','Ubuntu 24.04 x64 only. For other versions or GLIBC/Qt errors, choose the Debian/Ubuntu source build option.','You need CMake ≥ 3.21, a C++20 compiler, Qt ≥ 6.4 (Widgets, Network, Concurrent) and libarchive. Install their development packages using your distribution’s package manager, then follow the build block. Alpine/musl and ARM require a source build; they are not validated with the x64 package.','These steps have not been run on every distribution. The published binary is built on Ubuntu 24.04. Do not run the launcher with sudo.','Package documentation','Source build','Use a new destination folder. Repeat with the new release to update.','Keep the original filename: ebalia-linux-x64.tar.gz.','Other distributions']
 };
 let choice='nixos';
 const deps={
 debian:'sudo apt update\nsudo apt install git build-essential cmake ninja-build qt6-base-dev libarchive-dev qt6-image-formats-plugins',
 arch:'sudo pacman -Syu --needed git base-devel cmake ninja qt6-base qt6-imageformats libarchive',
 fedora:'sudo dnf install git gcc-c++ cmake ninja-build qt6-qtbase-devel qt6-qtimageformats libarchive-devel',
 ubuntu:'sudo apt update\nsudo apt install libqt6widgets6 libqt6network6 libqt6concurrent6 qt6-qpa-plugins qt6-image-formats-plugins libarchive13t64'
 };
 const docs={nixos:'https://wiki.nixos.org/wiki/Flakes',debian:'https://packages.debian.org/bookworm/qt6-base-dev',arch:'https://archlinux.org/packages/extra/x86_64/qt6-base/',fedora:'https://packages.fedoraproject.org/pkgs/qt6-qtbase/qt6-qtbase-devel/',ubuntu:'https://packages.ubuntu.com/noble/libarchive13t64'};
 window.renderLinuxGuide=(platform,version,tag)=>{
  box.hidden=platform!=='linux';if(box.hidden)return;
  // Versions originate in a release API; validate again before showing shell commands.
  if(!/^\d+\.\d+\.\d+$/.test(version)||!/^v?\d+\.\d+\.\d+$/.test(tag))return;
  const w=words[document.documentElement.lang]||words.en;
  box.replaceChildren();
  const el=(type,text)=>{const e=document.createElement(type);e.textContent=text;box.append(e);return e;};
  el('h3',w[0]);const label=el('label',w[1]);label.htmlFor='linux-distro';
  const select=el('select','');select.id='linux-distro';
  for(const [value,name] of [['nixos','NixOS'],['ubuntu','Ubuntu 24.04 / Linux Mint 22 · TAR.GZ'],['debian','Debian 12+ / Ubuntu 24.04+ · '+w[11]],['arch','Arch / EndeavourOS / Manjaro · '+w[11]],['fedora','Fedora · '+w[11]],['other',w[14]]]){const o=document.createElement('option');o.value=value;o.textContent=name;select.append(o);}
  select.value=choice;select.addEventListener('change',()=>{choice=select.value;window.renderLinuxGuide(platform,version,tag);document.querySelector('#linux-distro').focus();});
  const code=text=>{const pre=el('pre','');const c=document.createElement('code');c.textContent=text;pre.append(c);};
  const folder='ebalia-launcher-'+version;
  if(choice==='nixos'){
   el('p',w[5]);el('h4',w[2]);
   code(`mkdir ${folder}\ncd ${folder}\nnix --extra-experimental-features 'nix-command flakes' build 'github:ebalia-real/launcher/${tag}?dir=ebalia-launcher'\n./result/bin/ebalia-launcher`);
  }else if(choice==='ubuntu'){
   el('p',w[7]);el('h4',w[3]);code(deps.ubuntu);el('p',w[4]);el('p',w[13]);
   code(`mkdir ${folder}\ntar -xzf ebalia-linux-x64.tar.gz -C ${folder}\nchmod +x ${folder}/bin/ebalia-launcher\n./${folder}/bin/ebalia-launcher`);
  }else{
   el('p',choice==='other'?w[8]:w[6]);
   if(deps[choice]){el('h4',w[3]);code(deps[choice]);}
   el('h4',w[2]);
   code(`git clone --depth 1 --branch ${tag} https://github.com/ebalia-real/launcher.git ${folder}\ncd ${folder}/ebalia-launcher\ncmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF\ncmake --build build --parallel 2\n./build/ebalia-launcher`);
  }
  el('p',w[12]);el('p',w[9]);
  if(docs[choice]){const a=el('a',w[10]+' ↗');a.href=docs[choice];a.target='_blank';a.rel='noopener noreferrer';}
 };
})();
