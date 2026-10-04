"""Generate an NSIS installer from an already deployed and tested Windows package."""
import argparse,os,pathlib,subprocess
p=argparse.ArgumentParser();p.add_argument('--package',required=True);p.add_argument('--output',required=True);p.add_argument('--version',default='1.0.0');p.add_argument('--makensis',default='makensis');a=p.parse_args()
import re
if not re.fullmatch(r'\d+\.\d+\.\d+',a.version):p.error('Expected a stable numeric version')
root=pathlib.Path(a.package).resolve();out=pathlib.Path(a.output).resolve();out.parent.mkdir(parents=True,exist_ok=True)
for f in ['ebalia-launcher.exe','Qt6Core.dll','platforms/qwindows.dll','archive.dll']:
 if not (root/f).is_file():p.error('Missing package file: '+f)
def q(s):return str(s).replace('$','$$').replace('"','$\\"')
files=sorted(f for f in root.rglob('*') if f.is_file());dirs=sorted({str(f.parent.relative_to(root)).replace('/','\\') for f in files if f.parent!=root},key=lambda s:len(s),reverse=True)
icon=pathlib.Path(__file__).resolve().parents[1]/'resources/ebalia.ico'
script=f'''Unicode True
!include "MUI2.nsh"
Name "EBALIA Launcher"
OutFile "{q(out)}"
InstallDir "$LOCALAPPDATA\\Programs\\EBALIA Launcher"
RequestExecutionLevel user
SetCompressor zlib
Icon "{q(icon)}"
UninstallIcon "{q(icon)}"
VIProductVersion "{a.version}.0"
VIAddVersionKey /LANG=1033 "ProductName" "EBALIA Launcher"
VIAddVersionKey /LANG=1033 "FileDescription" "EBALIA Launcher Setup"
VIAddVersionKey /LANG=1033 "FileVersion" "{a.version}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "EBALIA"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Spanish"
Section "EBALIA Launcher"
SetShellVarContext current
SetOverwrite on
'''
for f in files:
 rel=f.relative_to(root);parent=str(rel.parent).replace('/','\\');dest='$INSTDIR'+('\\'+q(parent) if parent!='.' else '')
 script+=f'SetOutPath "{dest}"\nFile "{q(f)}"\n'
script+='''SetOutPath "$INSTDIR"
WriteUninstaller "$INSTDIR\\Uninstall.exe"
CreateDirectory "$SMPROGRAMS\\EBALIA Launcher"
CreateShortcut "$SMPROGRAMS\\EBALIA Launcher\\EBALIA Launcher.lnk" "$INSTDIR\\ebalia-launcher.exe"
WriteRegStr HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\EBALIA Launcher" "DisplayName" "EBALIA Launcher"
WriteRegStr HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\EBALIA Launcher" "UninstallString" '"$INSTDIR\\Uninstall.exe"'
WriteRegStr HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\EBALIA Launcher" "DisplayIcon" "$INSTDIR\\ebalia-launcher.exe"
'''
script+=f'WriteRegStr HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\EBALIA Launcher" "DisplayVersion" "{a.version}"\n'
script+='''WriteRegDWORD HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\EBALIA Launcher" "NoModify" 1
WriteRegDWORD HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\EBALIA Launcher" "NoRepair" 1
SectionEnd
Section "Uninstall"
SetShellVarContext current
'''
# Only remove files installed by this package. Never recursively delete user folders.
for f in files:script+='Delete "$INSTDIR\\'+q(str(f.relative_to(root)).replace('/','\\'))+'"\n'
for d in dirs:script+='RMDir "$INSTDIR\\'+q(d)+'"\n'
script+='''Delete "$INSTDIR\\Uninstall.exe"
RMDir "$INSTDIR"
Delete "$SMPROGRAMS\\EBALIA Launcher\\EBALIA Launcher.lnk"
RMDir "$SMPROGRAMS\\EBALIA Launcher"
DeleteRegKey HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\EBALIA Launcher"
SectionEnd
'''
nsi=out.with_suffix('.nsi');nsi.write_text(script);subprocess.run([a.makensis,'/V2' if os.name == 'nt' else '-V2',str(nsi)],check=True)
print(out)
