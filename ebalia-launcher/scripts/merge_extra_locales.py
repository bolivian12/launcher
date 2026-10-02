import pathlib,json
root=pathlib.Path(__file__).resolve().parents[1]
extra=json.loads((root/'scripts/extra-locales.json').read_text())
for index,code in enumerate(['es','en','pt','de','fr','it','ru','ja','ko','zh']):
 path=root/f'resources/locales/{code}.json';catalog=json.loads(path.read_text())
 for key,translations in extra.items():
  assert len(translations)==10,key
  catalog[key]=translations[index]
 path.write_text(json.dumps(catalog,ensure_ascii=False,indent=2)+'\n')
