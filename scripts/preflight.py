from pathlib import Path
root=Path(__file__).resolve().parents[1]
required=['platformio.ini','partitions.csv','src/main.cpp','data/index.html','README.md','ARCHITECTURE.md','BUILD.md','CHANGELOG.md','ROADMAP.md']
missing=[p for p in required if not (root/p).exists()]
if missing: raise SystemExit('Missing: '+', '.join(missing))
text=(root/'platformio.ini').read_text()
assert 'esp32dev' in text and 'M5Unified' in text and '16MB' in text
assert 'M5Paper S3' not in (root/'src/main.cpp').read_text()
print('PaperOS preflight OK')
