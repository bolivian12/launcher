"""Create versioned-release assets from the successful platform CI artifacts."""
import argparse, hashlib, pathlib, shutil, tarfile, zipfile
p = argparse.ArgumentParser()
p.add_argument('--artifacts', required=True)
p.add_argument('--output', required=True)
a = p.parse_args()
artifacts, out = pathlib.Path(a.artifacts), pathlib.Path(a.output)
out.mkdir(parents=True, exist_ok=True)
linux = artifacts / 'ebalia-linux-x64'
windows = artifacts / 'ebalia-windows-x64-unsigned'
for root, exe in [(linux, 'bin/ebalia-launcher'), (windows, 'ebalia-launcher.exe')]:
    if not (root / exe).is_file() or not (root / 'update-files.json').is_file():
        raise SystemExit('Missing executable or update manifest: ' + str(root))
(linux / 'bin/ebalia-launcher').chmod(0o755)
with tarfile.open(out / 'ebalia-linux-x64.tar.gz', 'w:gz') as archive:
    for file in sorted(linux.iterdir()):
        archive.add(file, arcname=file.name)
for folder, name in [(linux, 'ebalia-linux-x64-update.zip'), (windows, 'ebalia-windows-x64.zip')]:
    with zipfile.ZipFile(out / name, 'w', zipfile.ZIP_DEFLATED) as archive:
        for file in sorted(folder.rglob('*')):
            if file.is_symlink():
                raise SystemExit('Unexpected symlink in update package')
            if file.is_file():
                archive.write(file, file.relative_to(folder).as_posix())
# Portable Windows build: the same files plus portable.txt, so data stays in the folder.
portable_note = pathlib.Path(__file__).with_name('portable.txt')
with zipfile.ZipFile(out / 'ebalia-windows-x64-portable.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
    for file in sorted(windows.rglob('*')):
        if file.is_file():
            archive.write(file, 'EBALIA Launcher/' + file.relative_to(windows).as_posix())
    archive.write(portable_note, 'EBALIA Launcher/portable.txt')
setup = list((artifacts / 'ebalia-windows-installer').glob('*.exe'))
if len(setup) != 1:
    raise SystemExit('Missing or ambiguous Windows installer')
shutil.copyfile(setup[0], out / 'ebalia-windows-x64-setup.exe')
for arch in ('arm64', 'x64'):
    folder = artifacts / f'ebalia-macos-{arch}-unsigned'
    dmg, portable = list(folder.glob('*.dmg')), list(folder.glob('*-portable.zip'))
    if len(dmg) != 1 or len(portable) != 1:
        raise SystemExit('Missing or ambiguous macOS package: ' + arch)
    shutil.copyfile(dmg[0], out / f'ebalia-macos-{arch}.dmg')
    shutil.copyfile(portable[0], out / f'ebalia-macos-{arch}-portable.zip')
appimage = artifacts / 'ebalia-linux-appimage'
for name in ('ebalia-linux-x86_64.AppImage', 'ebalia-linux-x86_64-portable.tar.gz'):
    if not (appimage / name).is_file():
        raise SystemExit('Missing Linux package: ' + name)
    shutil.copyfile(appimage / name, out / name)
(out / 'ebalia-linux-x86_64.AppImage').chmod(0o755)
lines = []
for file in sorted(out.iterdir()):
    if file.name != 'SHA256SUMS.txt' and file.is_file():
        with file.open('rb') as stream:
            digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        lines.append(f'{digest}  {file.name}\n')
(out / 'SHA256SUMS.txt').write_text(''.join(lines))
print('Prepared', len(lines), 'release packages with SHA-256 checksums')
