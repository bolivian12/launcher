import re,json,pathlib
root=pathlib.Path(__file__).resolve().parents[1]
p=re.compile(r'text\(\s*("(?:[^"\\]|\\.)*")\s*,\s*("(?:[^"\\]|\\.)*")\s*,\s*("(?:[^"\\]|\\.)*")\s*\)',re.S)
messages={}
for source in (root/'src').glob('*.cpp'):
 for match in p.finditer(source.read_text()):
  es,en,pt=map(json.loads,match.groups());messages[en]=(es,en,pt)
for idx,code in enumerate(['es','en','pt']):
 (root/f'resources/locales/{code}.json').write_text(json.dumps({k:v[idx] for k,v in sorted(messages.items())},ensure_ascii=False,indent=2)+'\n')
(root/'scripts/locale-keys.json').write_text(json.dumps(list(sorted(messages)),ensure_ascii=False,indent=2))
print(len(messages),'messages')
for i,k in enumerate(sorted(messages)):
 print(i,k[:200])
