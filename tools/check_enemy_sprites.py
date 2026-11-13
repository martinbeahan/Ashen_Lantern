#!/usr/bin/env python3
"""Check that every Bestiary ID (app/src/main/cpp/Bestiary.h) resolves to its own drawable.

Asserts, for MainActivity.enemySprites:
  * every table ID has an entry, mapped to R.drawable.<same id>;
  * res/drawable/<id>.xml exists and all 27 files are distinct (by name AND by content);
  * calm (reduce-flash) variants exist, differ from their base art and from every other entry.
Usage: python3 tools/check_enemy_sprites.py   (exit 1 on failure)
"""
import hashlib, os, re, sys

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
bestiary = open(os.path.join(root, 'app/src/main/cpp/Bestiary.h')).read()
kt = open(os.path.join(root, 'app/src/main/java/com/fintrack/dndbeginnerremote/MainActivity.kt')).read()
draw = os.path.join(root, 'app/src/main/res/drawable')

ids = re.findall(r'^\s*\{"((?:enemy|boss)_[a-z_]+)",\s*"', bestiary, re.M)
def kmap(name):
    body = re.search(r'private val %s: Map<String, Int> = mapOf\((.*?)\n    \)' % name, kt, re.S).group(1)
    return dict(re.findall(r'"([a-z_]+)" to R\.drawable\.([a-z0-9_]+)', body))
sprites, calm = kmap('enemySprites'), kmap('calmEnemySprites')

errors = []
def digest(res):
    path = os.path.join(draw, res + '.xml')
    if not os.path.exists(path):
        errors.append('missing drawable %s.xml' % res); return None
    return hashlib.sha256(open(path, 'rb').read()).hexdigest()

if len(ids) != 27 or len(set(ids)) != 27:
    errors.append('expected 27 unique Bestiary IDs, found %d' % len(set(ids)))
hashes = {}
for i in ids:
    res = sprites.get(i)
    if res != i:
        errors.append('%s -> %s (expected R.drawable.%s)' % (i, res, i)); continue
    hashes[i] = digest(res)
if len(set(sprites[i] for i in ids if i in sprites)) != len(ids):
    errors.append('drawable names are not distinct')
if len(set(hashes.values())) != len(hashes):
    errors.append('two IDs share identical drawable content')
for i, res in calm.items():
    h = digest(res)
    if i not in ids: errors.append('calm entry for unknown id %s' % i)
    if h in hashes.values(): errors.append('calm variant %s duplicates an existing drawable' % res)

for e in errors: print('FAIL:', e)
print('%d IDs -> %d distinct drawables (+%d calm variants): %s'
      % (len(ids), len(set(hashes.values())), len(calm), 'OK' if not errors else 'FAILED'))
sys.exit(1 if errors else 0)
