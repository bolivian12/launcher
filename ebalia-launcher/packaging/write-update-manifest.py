"""Record files owned by the launcher; the updater never removes unlisted files."""
import argparse, json, pathlib
p = argparse.ArgumentParser()
p.add_argument('--root', required=True)
a = p.parse_args()
root = pathlib.Path(a.root).resolve()
files = []
for file in sorted(root.rglob('*')):
    if file.is_symlink():
        raise SystemExit('Update packages must not contain symbolic links: ' + str(file))
    if file.is_file() and file.name != 'update-files.json':
        files.append(file.relative_to(root).as_posix())
files.append('update-files.json')
(root / 'update-files.json').write_text(json.dumps({'format': 1, 'files': files}, indent=2) + '\n')
