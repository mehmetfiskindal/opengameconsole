# Sailfish NES konsolu — uygulama planı

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** aarch64 Sailfish telefonda açılan, dokunmatik tuşlarla gömülü pad ROM’unu FCEUmm üzerinde oynatan tek bir GeaStack uygulaması (`harbour-gea-opengameconsole`).

**Architecture:** Gea kabuğu (TSX → geatsc → C++) üç host çağrısını bir geatsc eklentisiyle `native/nes_host.cpp` içindeki C++ fonksiyonlarına bağlar. `nes_host.cpp` FCEUmm’u libretro API’siyle aynı süreçte çalıştırır ve Sailfish ana döngüsündeki yeni `gea_app_before_refresh` kancasından NES karesini tuvalin üst %62’sine yazar. FCEUmm, Linux hedefine eklenen `gea.sailfish.staticLibraries` ile kendi include yolu olan ayrı bir statik kütüphane olarak derlenir.

**Tech Stack:** GeaStack (`@geastack/core` 0.1.39, `@geastack/compiler`), `@geastack/linux` Sailfish hedefi (`vendor/geastack-linux`, dal `feat/sailfishos-target-fix`), Sailfish SDK `sfdk` (C:\SailfishOS), CMake, SDL2, libretro-fceumm `7a542dab1e87679921962a9f056186eca425c0c2`, Node 24 `node:test`, PowerShell.

## Global Constraints

- Spec: `docs/superpowers/specs/2026-10-05-sailfish-nes-console-design.md`. Çelişkide spec kazanır.
- Hedef: Sailfish OS 5.1, aarch64. RPM adı `harbour-gea-opengameconsole`.
- Kontrol yalnızca dokunmatik. Yerleşim dikey: üst %62 oyun bandı, alt %38 tuş bandı.
- İlk sistem yalnızca NES. Çekirdek FCEUmm, statik bağlı, `dlopen` yok.
- ROM yalnızca bizim pad demomuz. Nintendo ROM’u, kodu, karakteri yok. Klasör taraması yok.
- Host çağrıları: `nesPlay(): number` (0 başarı, 1 ROM bozuk, 2 çekirdek başlamadı), `nesSetButton(name: string, down: number): void`, `nesStop(): void`. Tuş adları: `up`, `down`, `left`, `right`, `a`, `b`, `start`, `select`.
- Hata metinleri birebir: “ROM açılamadı”, “NES başlamadı”.
- `gea.sailfish`: `organizationName` `org.opengameconsole`, `applicationName` `opengameconsole`, `devicePixelRatio` 2, `permissions` `["Audio"]`.
- Telefon: `defaultuser@192.168.1.220`. Parola hiçbir dosyaya, betiğe veya commit’e yazılmaz. SSH anahtarı veya etkileşimli oturum kullanılır.
- `vendor/geastack-linux` değişiklikleri o depoda yerel commit olur. İzin alınmadan push edilmez.
- RPM GPL-2.0 olarak dağıtılır (`package.json` `license`: `GPL-2.0-only`). `native/fceumm/COPYING` durur.
- Kabuk komutları PowerShell’dir: `&&` yok, `;` ve `if ($LASTEXITCODE -ne 0) { ... }` kullanılır.
- Tüm komutlar, aksi yazmadıkça, uygulama kökünden çalışır: `c:\Users\mfisk\opengameconsole\opengameconsole`.

## Dosya haritası

| Dosya | Sorumluluk |
| --- | --- |
| `.gitignore`, `.gitattributes`, `tsconfig.json` | Depo kuralları, LF satır sonu, tsc’nin vendor ve build çıktısını taramaması |
| `vendor/geastack-linux/targets/sailfish-os/prepare-lib.mjs` | `expandSourceEntries`, `normalizeStaticLibraries`, `staticLibrariesCmake`, `compilerPluginArgs` |
| `vendor/geastack-linux/targets/sailfish-os/prepare.mjs` | Yeni alanları okur, derleyiciye eklenti iletir, statik kütüphane köklerini sahneler |
| `vendor/geastack-linux/targets/sailfish-os/CMakeLists.txt` | Her statik kütüphane için `add_library` ve bağlama |
| `vendor/geastack-linux/targets/sailfish-os/main/sailfish_main.cpp` | `gea_app_before_refresh` kancası ve koşullu present |
| `vendor/geastack-linux/targets/sailfish-os/main/sailfish_display.cpp` | `sailfish_display_write_rgb565`, `sailfish_display_present_count` |
| `vendor/geastack-linux/targets/sailfish-os/README.md` | Yeni alanların ve kancanın belgesi |
| `scripts/nes/pad-demo.mjs` | 6502 assembler, pad programı, iNES paketleme, C dizisi üretimi |
| `scripts/nes/write-pad-demo.mjs` | `assets/nes/pad-demo.nes` ve `native/pad_demo_rom.c` dosyalarını yazar |
| `scripts/nes-host-plugin.mjs` | geatsc eklentisi: TS adı → C++ adı ve `#include "nes_host.h"` |
| `src/nes-host.d.ts` | Host çağrılarının TS bildirimi |
| `native/nes_host.h`, `native/nes_host.cpp` | libretro frontend, tuşlar, ses, kare zamanlama, çizim kancası |
| `native/fceumm/` | Upstream `src/`, `COPYING`, `UPSTREAM.txt` |
| `src/index.tsx`, `src/styles.css` | Kabuk ve oyun ekranı |
| `assets/fonts/Inter-Regular.ttf`, `assets/fonts/Inter-LICENSE.txt` | `styles.css`’in beklediği font |
| `scripts/make-icon.mjs`, `assets/icon.png` | 512×512 ikon |
| `scripts/deploy-sailfish.ps1` | En yeni aarch64 RPM’i `scp` ile gönderir, `ssh -t` ile kurar |

---

### Task 1: Depo kurulumu

**Files:**
- Create: `.gitattributes`
- Modify: `.gitignore`, `tsconfig.json`
- Git: uygulama kökünde `git init`, `vendor/geastack-linux` submodule

**Interfaces:**
- Consumes: yok.
- Produces: commit atılabilen bir depo. Sonraki her görev `git add` / `git commit` kullanır. `vendor/geastack-linux` ayrı bir git deposu olarak kalır; vendor commit’leri orada atılır, ardından uygulama deposunda submodule işaretçisi güncellenir.

- [ ] **Step 1: `.gitattributes` oluştur**

```gitattributes
* text=auto eol=lf
*.ps1 text eol=crlf
*.nes binary
*.png binary
*.ttf binary
```

- [ ] **Step 2: `.gitignore` içinden vendor satırını çıkar**

Dosyanın yeni hali:

```gitignore
node_modules/
dist/
.gea/build/
.gea-sailfish/
.superpowers/
.env
```

- [ ] **Step 3: `tsconfig.json` `exclude` listesine vendor ve Sailfish çıktısını ekle**

```json
  "exclude": [
    "node_modules",
    "dist",
    ".gea",
    ".gea-sailfish",
    "vendor",
    "vite.config.ts",
    "vite.web.config.ts"
  ]
```

- [ ] **Step 4: tsc hâlâ geçiyor mu**

Run: `npm run check`
Expected: çıkış kodu 0, hata yok.

- [ ] **Step 5: Depoyu başlat ve vendor’u submodule yap**

```powershell
git init -b main
git submodule add -b feat/sailfishos-target-fix https://github.com/mehmetfiskindal/linux vendor/geastack-linux
git -C vendor/geastack-linux rev-parse --abbrev-ref HEAD
```

Expected: son komut `feat/sailfishos-target-fix` yazar. `git submodule add` mevcut klasör için “Adding existing repo at 'vendor/geastack-linux' to the index” der.

- [ ] **Step 6: İlk commit**

```powershell
git add .
git status --short
git commit -m "chore: initial opengameconsole repo with geastack-linux submodule"
```

Expected: `git status --short` içinde `node_modules`, `.gea-sailfish`, `.superpowers` yoktur.

---

### Task 2: Linux hedefi — `staticLibraries` ve `compilerPlugins`

**Files:**
- Modify: `vendor/geastack-linux/targets/sailfish-os/prepare-lib.mjs`
- Modify: `vendor/geastack-linux/targets/sailfish-os/prepare.mjs:7-10, 39-43, 70-75, 118, 149, 152-156`
- Modify: `vendor/geastack-linux/targets/sailfish-os/CMakeLists.txt:43`
- Modify: `vendor/geastack-linux/targets/sailfish-os/README.md:72-96`
- Test: `vendor/geastack-linux/targets/sailfish-os/prepare.test.mjs`

**Interfaces:**
- Consumes: `appRelative(appDir, rel)`, `nativeKind(file)` (mevcut, `prepare-lib.mjs`).
- Produces:
  - `expandSourceEntries(appDir: string, entries: string[], label: string): string[]` — dosya veya dizin girdilerini app-relative, `/` ayraçlı C/C++ dosya listesine açar. Dizinler özyinelemesizdir, dosyalar alfabetik sıralıdır.
  - `normalizeStaticLibraries(appDir: string, value: unknown): StaticLibrary[]` — `StaticLibrary = { name, root, sources, includePaths, publicIncludePaths, defines }`, hepsi string veya string dizisi.
  - `staticLibrariesCmake(libraries: StaticLibrary[]): string` — `sources.cmake` için `GEA_STATIC_LIBRARY_NAMES` ve `GEA_STATIC_LIBRARY_<NAME>_{SOURCES,PRIVATE_INCLUDES,PUBLIC_INCLUDES,DEFINES}`.
  - `compilerPluginArgs(appDir: string, value: unknown): string[]` — `['--extra-geatsc-plugin', <abs path>, ...]`.
  - `package.json` alanları: `gea.compilerPlugins: string[]`, `gea.sailfish.staticLibraries: [{ name, root, sources, includePaths?, publicIncludePaths?, defines? }]`.
  - CMake hedef adı: `gea_static_<name>`.

- [ ] **Step 1: Başarısız testleri yaz**

`prepare.test.mjs` içindeki import’u şöyle değiştir:

```js
import {
  appRelative, assertPackageVersion, compilerPluginArgs, expandSourceEntries, nativeKind, normalizeStaticLibraries,
  resolveGeastackPackage, shouldCopyStagedPath, staticLibrariesCmake, validateLibraryName,
} from './prepare-lib.mjs'
```

`fs.rmSync(root, { recursive: true, force: true })` satırının hemen üstüne ekle:

```js
const libApp = path.join(root, 'libapp')
fs.mkdirSync(path.join(libApp, 'native', 'core', 'src', 'boards'), { recursive: true })
fs.mkdirSync(path.join(libApp, 'native', 'core', 'include'), { recursive: true })
fs.writeFileSync(path.join(libApp, 'native', 'core', 'src', 'b.c'), '')
fs.writeFileSync(path.join(libApp, 'native', 'core', 'src', 'a.c'), '')
fs.writeFileSync(path.join(libApp, 'native', 'core', 'src', 'a.h'), '')
fs.writeFileSync(path.join(libApp, 'native', 'core', 'src', 'boards', 'm.c'), '')
fs.writeFileSync(path.join(libApp, 'native', 'core', 'extra.cpp'), '')
fs.writeFileSync(path.join(libApp, 'native', 'outside.c'), '')

assert.deepEqual(expandSourceEntries(libApp, ['native/core/src', 'native/core/extra.cpp'], 'x'),
  ['native/core/src/a.c', 'native/core/src/b.c', 'native/core/extra.cpp'])
assert.throws(() => expandSourceEntries(libApp, ['native/core/include'], 'x'), /no C or C\+\+ sources/)
assert.throws(() => expandSourceEntries(libApp, ['native/core/missing.c'], 'x'), /entry missing/)

const libs = normalizeStaticLibraries(libApp, [{
  name: 'core',
  root: 'native/core',
  sources: ['native/core/src', 'native/core/src/boards'],
  includePaths: ['native/core/src'],
  publicIncludePaths: ['native/core/include'],
  defines: ['__CORE__', 'PATH_MAX=1024'],
}])
assert.deepEqual(libs, [{
  name: 'core',
  root: 'native/core',
  sources: ['native/core/src/a.c', 'native/core/src/b.c', 'native/core/src/boards/m.c'],
  includePaths: ['native/core/src'],
  publicIncludePaths: ['native/core/include'],
  defines: ['__CORE__', 'PATH_MAX=1024'],
}])
assert.deepEqual(normalizeStaticLibraries(libApp, undefined), [])
const lib = (extra) => [{ name: 'core', root: 'native/core', sources: ['native/core/src'], ...extra }]
assert.throws(() => normalizeStaticLibraries(libApp, {}), /must be an array/)
assert.throws(() => normalizeStaticLibraries(libApp, lib({ name: 'Core' })), /lowercase C identifier/)
assert.throws(() => normalizeStaticLibraries(libApp, [...lib(), ...lib()]), /duplicated: core/)
assert.throws(() => normalizeStaticLibraries(libApp, lib({ root: 'native/none' })), /root directory missing/)
assert.throws(() => normalizeStaticLibraries(libApp, lib({ sources: ['native/outside.c'] })), /must stay inside native\/core/)
assert.throws(() => normalizeStaticLibraries(libApp, lib({ defines: ['A;B'] })), /NAME or NAME=value/)
assert.throws(() => normalizeStaticLibraries(libApp, lib({ includePaths: ['native/core/nope'] })), /directory missing/)

const cmake = staticLibrariesCmake(libs)
assert.match(cmake, /set\(GEA_STATIC_LIBRARY_NAMES "core"\)/)
assert.match(cmake, /set\(GEA_STATIC_LIBRARY_CORE_SOURCES\n  "\$\{CMAKE_CURRENT_SOURCE_DIR\}\/app_native\/native\/core\/src\/a\.c"/)
assert.match(cmake, /set\(GEA_STATIC_LIBRARY_CORE_PUBLIC_INCLUDES\n  "\$\{CMAKE_CURRENT_SOURCE_DIR\}\/app_native\/native\/core\/include"\n\)/)
assert.match(cmake, /set\(GEA_STATIC_LIBRARY_CORE_DEFINES\n  "__CORE__"\n  "PATH_MAX=1024"\n\)/)
assert.equal(staticLibrariesCmake([]), 'set(GEA_STATIC_LIBRARY_NAMES)\n')

assert.deepEqual(compilerPluginArgs(libApp, undefined), [])
assert.deepEqual(compilerPluginArgs(libApp, ['native/core/extra.cpp']),
  ['--extra-geatsc-plugin', path.join(libApp, 'native', 'core', 'extra.cpp')])
assert.throws(() => compilerPluginArgs(libApp, ['missing.mjs']), /gea.compilerPlugins file missing: missing.mjs/)
assert.throws(() => compilerPluginArgs(libApp, 'x'), /must be an array/)
```

- [ ] **Step 2: Testin başarısız olduğunu gör**

Run: `node vendor/geastack-linux/targets/sailfish-os/prepare.test.mjs`
Expected: FAIL, `SyntaxError: The requested module './prepare-lib.mjs' does not provide an export named 'compilerPluginArgs'`.

- [ ] **Step 3: `prepare-lib.mjs` sonuna yardımcıları ekle**

```js
const sourceExtension = /\.(c|cc|cpp|cxx)$/i

export function expandSourceEntries(appDir, entries, label) {
  const files = []
  for (const rel of entries) {
    const abs = appRelative(appDir, rel)
    if (!fs.existsSync(abs)) throw new Error(`${label} entry missing: ${rel}`)
    const posixRel = rel.replaceAll('\\', '/').replace(/\/+$/, '')
    if (fs.statSync(abs).isDirectory()) {
      const names = fs.readdirSync(abs)
        .filter((name) => sourceExtension.test(name) && fs.statSync(path.join(abs, name)).isFile())
        .sort()
      if (names.length === 0) throw new Error(`${label} directory has no C or C++ sources: ${rel}`)
      for (const name of names) files.push(`${posixRel}/${name}`)
    } else {
      if (nativeKind(rel) === 'header') continue
      files.push(posixRel)
    }
  }
  return [...new Set(files)]
}

export function normalizeStaticLibraries(appDir, value) {
  if (value === undefined) return []
  if (!Array.isArray(value)) throw new Error('gea.sailfish.staticLibraries must be an array')
  const seen = new Set()
  return value.map((entry, index) => {
    const where = `gea.sailfish.staticLibraries[${index}]`
    if (!entry || typeof entry !== 'object') throw new Error(`${where} must be an object`)
    const { name, root, sources = [], includePaths = [], publicIncludePaths = [], defines = [] } = entry
    if (typeof name !== 'string' || !/^[a-z][a-z0-9_]*$/.test(name)) throw new Error(`${where}.name must be a lowercase C identifier`)
    if (seen.has(name)) throw new Error(`${where}.name is duplicated: ${name}`)
    seen.add(name)
    if (typeof root !== 'string') throw new Error(`${where}.root must be an app-relative directory`)
    const rootAbs = appRelative(appDir, root)
    if (!fs.statSync(rootAbs, { throwIfNoEntry: false })?.isDirectory()) throw new Error(`${where}.root directory missing: ${root}`)
    const strings = (list, field) => {
      if (!Array.isArray(list) || !list.every((item) => typeof item === 'string')) throw new Error(`${where}.${field} must be an array of strings`)
      return list
    }
    const insideRoot = (rel, field) => {
      const abs = appRelative(appDir, rel)
      if (abs !== rootAbs && !abs.startsWith(rootAbs + path.sep)) throw new Error(`${where}.${field} must stay inside ${root}: ${rel}`)
      return rel.replaceAll('\\', '/').replace(/\/+$/, '')
    }
    const directories = (list, field) => strings(list, field).map((rel) => {
      const posixRel = insideRoot(rel, field)
      if (!fs.statSync(appRelative(appDir, rel), { throwIfNoEntry: false })?.isDirectory()) throw new Error(`${where}.${field} directory missing: ${rel}`)
      return posixRel
    })
    for (const define of strings(defines, 'defines')) {
      if (!/^[A-Za-z_][A-Za-z0-9_]*(=[A-Za-z0-9_.+-]*)?$/.test(define)) throw new Error(`${where}.defines entry must be NAME or NAME=value: ${define}`)
    }
    const files = expandSourceEntries(appDir, strings(sources, 'sources'), `${where}.sources`).map((rel) => insideRoot(rel, 'sources'))
    if (files.length === 0) throw new Error(`${where}.sources has no C or C++ files`)
    return {
      name,
      root: root.replaceAll('\\', '/').replace(/\/+$/, ''),
      sources: files,
      includePaths: directories(includePaths, 'includePaths'),
      publicIncludePaths: directories(publicIncludePaths, 'publicIncludePaths'),
      defines: [...defines],
    }
  })
}

export function staticLibrariesCmake(libraries) {
  const staged = (values) => values.map((value) => `  "\${CMAKE_CURRENT_SOURCE_DIR}/app_native/${value}"`)
  const plain = (values) => values.map((value) => `  "${value}"`)
  const list = (key, lines) => `set(${key}\n${lines.map((line) => `${line}\n`).join('')})\n`
  let out = `set(GEA_STATIC_LIBRARY_NAMES${libraries.map((library) => ` "${library.name}"`).join('')})\n`
  for (const library of libraries) {
    const key = `GEA_STATIC_LIBRARY_${library.name.toUpperCase()}`
    out += list(`${key}_SOURCES`, staged(library.sources))
    out += list(`${key}_PRIVATE_INCLUDES`, staged(library.includePaths))
    out += list(`${key}_PUBLIC_INCLUDES`, staged(library.publicIncludePaths))
    out += list(`${key}_DEFINES`, plain(library.defines))
  }
  return out
}

export function compilerPluginArgs(appDir, value) {
  if (value === undefined) return []
  if (!Array.isArray(value) || !value.every((item) => typeof item === 'string')) {
    throw new Error('gea.compilerPlugins must be an array of app-relative files')
  }
  return value.flatMap((rel) => {
    const abs = appRelative(appDir, rel)
    if (!fs.statSync(abs, { throwIfNoEntry: false })?.isFile()) throw new Error(`gea.compilerPlugins file missing: ${rel}`)
    return ['--extra-geatsc-plugin', abs]
  })
}
```

- [ ] **Step 4: Testin geçtiğini gör**

Run: `node vendor/geastack-linux/targets/sailfish-os/prepare.test.mjs`
Expected: `sailfish prepare tests passed`.

- [ ] **Step 5: `prepare.mjs` içine bağla**

Import bloğunu değiştir:

```js
import {
  appRelative, assertPackageVersion, compilerPluginArgs, frameworkPackageNames, nativeKind, normalizeStaticLibraries,
  readPackageVersion, resolveGeastackPackage, shouldCopyStagedPath, staticLibrariesCmake, validateLibraryName,
} from './prepare-lib.mjs'
```

`for (const name of libraries) validateLibraryName(name)` satırının altına:

```js
const staticLibraries = normalizeStaticLibraries(appDir, sailfish.staticLibraries)
const pluginArgs = compilerPluginArgs(appDir, app.compilerPlugins)
```

`args` dizisinin son elemanını `'--runtime-ttf-fonts', ...pluginArgs]` yap:

```js
  '--font-viewport-width', '410', '--font-viewport-height', '502', '--font-device-pixel-ratio', String(devicePixelRatio),
  '--runtime-ttf-fonts', ...pluginArgs]
```

`includePaths` sahneleme döngüsünün (`stagedIncludes.push(...)` ile biten `for`) hemen altına:

```js
for (const library of staticLibraries) {
  copyTree(appRelative(appDir, library.root), path.join(nativeRoot, library.root), true)
  for (const file of library.sources) {
    if (!fs.existsSync(path.join(nativeRoot, file))) throw new Error(`static library source missing after staging: ${file}`)
  }
}
```

`sources.cmake` yazımında son satırı değiştir:

```js
  cmakeList('GEA_INCLUDE_DIRS', [...new Set(includes)]) + cmakeList('GEA_C_SOURCES', c) + cmakeList('GEA_CXX_SOURCES', cxx) +
  staticLibrariesCmake(staticLibraries))
```

- [ ] **Step 6: `CMakeLists.txt` içine kütüphane döngüsünü ekle**

`target_link_options(${GEA_PACKAGE_NAME} PRIVATE -Wl,--export-dynamic)` satırının hemen üstüne:

```cmake
# App static libraries keep their include paths private, so vendored sources
# whose headers share names with framework headers (input.h, video.h) do not
# shadow them. Only PUBLIC_INCLUDES reach the app.
foreach(GEA_STATIC_LIBRARY ${GEA_STATIC_LIBRARY_NAMES})
  string(TOUPPER "${GEA_STATIC_LIBRARY}" GEA_STATIC_LIBRARY_KEY)
  set(GEA_STATIC_LIBRARY_TARGET "gea_static_${GEA_STATIC_LIBRARY}")
  add_library(${GEA_STATIC_LIBRARY_TARGET} STATIC ${GEA_STATIC_LIBRARY_${GEA_STATIC_LIBRARY_KEY}_SOURCES})
  target_include_directories(${GEA_STATIC_LIBRARY_TARGET}
    PRIVATE ${GEA_STATIC_LIBRARY_${GEA_STATIC_LIBRARY_KEY}_PRIVATE_INCLUDES}
    PUBLIC ${GEA_STATIC_LIBRARY_${GEA_STATIC_LIBRARY_KEY}_PUBLIC_INCLUDES})
  target_compile_definitions(${GEA_STATIC_LIBRARY_TARGET} PRIVATE ${GEA_STATIC_LIBRARY_${GEA_STATIC_LIBRARY_KEY}_DEFINES})
  target_compile_options(${GEA_STATIC_LIBRARY_TARGET} PRIVATE -funwind-tables)
  target_link_libraries(${GEA_PACKAGE_NAME} PRIVATE ${GEA_STATIC_LIBRARY_TARGET})
endforeach()
```

- [ ] **Step 7: README’yi güncelle**

`README.md` “App native sources” bölümündeki JSON örneğini ve altındaki paragrafı şöyle genişlet (mevcut paragraf aynen kalır, altına iki paragraf eklenir):

````markdown
```json
{
  "gea": {
    "compilerPlugins": ["scripts/host-plugin.mjs"],
    "sailfish": {
      "nativeSources": ["native/host.cpp", "native/host.h"],
      "includePaths": ["native/vendor"],
      "libraries": ["openssl"],
      "staticLibraries": [{
        "name": "core",
        "root": "native/core",
        "sources": ["native/core/src", "native/core/extra.c"],
        "includePaths": ["native/core/src"],
        "publicIncludePaths": ["native/core/include"],
        "defines": ["CORE_STATIC=1"]
      }]
    }
  }
}
```
````

```markdown
`staticLibraries` builds vendored C or C++ as a separate CMake static library
named `gea_static_<name>` and links it into the app. `root` is copied into the
staged project. `sources` entries are files or directories; a directory adds
the C and C++ files directly inside it, not its subdirectories. Every path
must stay inside `root`. `includePaths` and `defines` apply only to the
library, so its headers cannot shadow framework headers with the same name.
`publicIncludePaths` are also added to the app.

`compilerPlugins` lists app-relative geatsc plugin modules. Each one is passed
to the compiler with `--extra-geatsc-plugin`. Use one to bind `declare
function` globals to C++ through `hostFunctions` and `hostPreambles`.
```

- [ ] **Step 8: Testleri yeniden çalıştır ve vendor’da commit at**

```powershell
node vendor/geastack-linux/targets/sailfish-os/prepare.test.mjs
git -C vendor/geastack-linux add targets/sailfish-os/prepare-lib.mjs targets/sailfish-os/prepare.mjs targets/sailfish-os/prepare.test.mjs targets/sailfish-os/CMakeLists.txt targets/sailfish-os/README.md
git -C vendor/geastack-linux commit -m "sailfish: add staticLibraries and compilerPlugins app settings"
git add vendor/geastack-linux
git commit -m "chore: bump geastack-linux for static libraries and compiler plugins"
```

Expected: `sailfish prepare tests passed`, iki commit.

---

### Task 3: Linux hedefi — çizim kancası

**Files:**
- Modify: `vendor/geastack-linux/targets/sailfish-os/main/sailfish_display.cpp:182-189, 243-246`
- Modify: `vendor/geastack-linux/targets/sailfish-os/main/sailfish_main.cpp:56-69, 663-669`
- Modify: `vendor/geastack-linux/targets/sailfish-os/README.md` (“Runtime” bölümünün sonu)

**Interfaces:**
- Consumes: yok.
- Produces (C ABI, `extern "C"`):
  - `int gea_app_before_refresh(void)` — zayıf varsayılan 0 döner. Uygulama güçlü tanım verebilir.
  - `void sailfish_display_write_rgb565(const std::uint16_t *pixels, int x, int y, int width, int height)` — satırları tuvale kopyalar ve dokuya yükler, sunmaz.
  - `unsigned sailfish_display_present_count(void)` — şimdiye kadarki present sayısı.

Bu görevin C++ derlemesi Windows’ta yapılamaz. Doğrulama Task 9’daki `sfdk` derlemesidir. Burada değişiklik okunarak gözden geçirilir.

- [ ] **Step 1: `sailfish_display.cpp` — present sayacı**

`bool g_vsync_waited = false;` satırının altına:

```cpp
unsigned g_present_count = 0;
```

`present_texture()` gövdesini değiştir:

```cpp
void present_texture()
{
	if (!g_sdl_ok) return;
	SDL_RenderClear(g_renderer);
	SDL_RenderCopy(g_renderer, g_texture, nullptr, nullptr);
	SDL_RenderPresent(g_renderer);
	g_vsync_waited = true;
	g_present_count++;
}
```

- [ ] **Step 2: `sailfish_display.cpp` — dışa açık yazma ve sayaç**

`extern "C" void sailfish_display_present()` fonksiyonunun hemen altına:

```cpp
// Lets an app paint straight into the canvas (an emulator frame) outside the
// Gea tree. Uploads without presenting; the main loop presents.
extern "C" void sailfish_display_write_rgb565(const std::uint16_t *pixels, int x, int y, int width, int height)
{
	if (!pixels || width <= 0 || height <= 0) return;
	copy_rgb565_rows_to_canvas(pixels, x, y, width, height);
	upload_rect(x, y, x + width - 1, y + height - 1);
}

extern "C" unsigned sailfish_display_present_count() { return g_present_count; }
```

- [ ] **Step 3: `sailfish_main.cpp` — bildirimler ve zayıf varsayılan**

`extern "C" void sailfish_install_wifi_driver();` satırının altına:

```cpp
extern "C" unsigned sailfish_display_present_count();

// Apps that paint into the canvas themselves (emulators, video) define this.
// Return nonzero after sailfish_display_write_rgb565 so the loop presents even
// when the Gea tree had nothing dirty.
extern "C" __attribute__((weak)) int gea_app_before_refresh(void) { return 0; }
```

- [ ] **Step 4: `sailfish_main.cpp` — döngüde çağrı**

Şu bloğu:

```cpp
		int root = tree.mountedRoot();
		if (root >= 0) {
			gea::embedded::ui::NodeHandle(root).style().width(w);
			gea::embedded::ui::NodeHandle(root).style().height(h);
			tree.refresh(root, w, h);
		}
```

şununla değiştir:

```cpp
		const int appDrew = gea_app_before_refresh();
		const unsigned presentsBeforeRefresh = sailfish_display_present_count();
		int root = tree.mountedRoot();
		if (root >= 0) {
			gea::embedded::ui::NodeHandle(root).style().width(w);
			gea::embedded::ui::NodeHandle(root).style().height(h);
			tree.refresh(root, w, h);
		}
		if (appDrew && sailfish_display_present_count() == presentsBeforeRefresh) sailfish_display_present();
```

- [ ] **Step 5: README — kanca belgesi**

`README.md` içinde “## Device check” başlığının hemen üstüne:

```markdown
## App drawing hook

An app may define `extern "C" int gea_app_before_refresh(void)`. The main loop
calls it every turn after `Application::frame` and before `tree.refresh`. To
paint, write RGB565 pixels with `sailfish_display_write_rgb565(pixels, x, y,
width, height)` and return nonzero; the loop presents if the tree refresh did
not, so one turn waits for vsync once. Gea repaints its own dirty regions after
the hook, so paint only over an element whose content does not change, such as
an empty `div`. `sailfish_display_present_count()` counts presents.
```

- [ ] **Step 6: Gözden geçir ve commit at**

Run: `git -C vendor/geastack-linux diff`
Expected: yalnızca yukarıdaki üç dosya. `gea_app_before_refresh` tek yerde zayıf tanımlı. `sailfish_display_present` döngüde yalnızca `appDrew` ve sayaç değişmemişse çağrılıyor.

```powershell
git -C vendor/geastack-linux add targets/sailfish-os/main/sailfish_display.cpp targets/sailfish-os/main/sailfish_main.cpp targets/sailfish-os/README.md
git -C vendor/geastack-linux commit -m "sailfish: add gea_app_before_refresh drawing hook"
git add vendor/geastack-linux
git commit -m "chore: bump geastack-linux for the drawing hook"
```

---

### Task 4: Pad demo ROM

**Files:**
- Create: `scripts/nes/pad-demo.mjs`
- Create: `scripts/nes/write-pad-demo.mjs`
- Create (üretilen): `assets/nes/pad-demo.nes`, `native/pad_demo_rom.c`
- Modify: `package.json` (`scripts.test`, `scripts.rom`)
- Test: `scripts/nes/pad-demo.test.mjs`

**Interfaces:**
- Consumes: yok.
- Produces:
  - `assemble(origin: number, program: (asm) => void): { bytes: Uint8Array, labels: Map<string, number> }`
  - `buildPadDemoRom(): { rom: Uint8Array, labels: Map<string, number> }` — 24592 bayt, NROM-128, mapper 0, dikey yansıma.
  - `romToC(rom: Uint8Array): string`
  - C sembolleri (`native/pad_demo_rom.c`): `const unsigned char opengameconsole_pad_demo_rom[24592]`, `const size_t opengameconsole_pad_demo_rom_size`.
  - ROM davranışı: zemin `$10`, Start/Select kenarında `$21` ile değişir. 16×16 kare (tile 1, 4 sprite) yön tuşuyla 1 px/kare hareket eder. A sprite paleti 0 (renk `$16` kırmızı), B sprite paleti 1 (renk `$2A` yeşil).

- [ ] **Step 1: Başarısız testi yaz**

`scripts/nes/pad-demo.test.mjs`:

```js
import { test } from 'node:test'
import assert from 'node:assert/strict'
import fs from 'node:fs'
import { assemble, buildPadDemoRom, romToC, ROM_SIZE, PRG_SIZE } from './pad-demo.mjs'

const appRoot = new URL('../../', import.meta.url)

test('assembler resolves forward and backward branches', () => {
  const { bytes, labels } = assemble(0xc000, (a) => {
    a.label('top')
    a.beq('ahead')
    a.inx()
    a.label('ahead')
    a.bne('top')
    a.jmp('top')
  })
  assert.deepEqual([...bytes], [0xf0, 0x01, 0xe8, 0xd0, 0xfb, 0x4c, 0x00, 0xc0])
  assert.equal(labels.get('ahead'), 0xc003)
})

test('assembler rejects unknown labels and long branches', () => {
  assert.throws(() => assemble(0xc000, (a) => a.jmp('nowhere')), /unknown label: nowhere/)
  assert.throws(() => assemble(0xc000, (a) => {
    a.label('far')
    a.bytes(...new Array(200).fill(0xea))
    a.bne('far')
  }), /branch to far out of range/)
})

test('pad demo is an NROM-128 iNES image', () => {
  const { rom } = buildPadDemoRom()
  assert.equal(rom.length, ROM_SIZE)
  assert.equal(ROM_SIZE, 24592)
  assert.deepEqual([...rom.subarray(0, 8)], [0x4e, 0x45, 0x53, 0x1a, 1, 1, 0x01, 0x00])
})

test('vectors point at the reset, nmi and irq handlers', () => {
  const { rom, labels } = buildPadDemoRom()
  const prg = rom.subarray(16, 16 + PRG_SIZE)
  const word = (offset) => prg[offset] | (prg[offset + 1] << 8)
  const at = (address) => prg[address - 0xc000]
  assert.equal(word(PRG_SIZE - 6), labels.get('nmi'))
  assert.equal(word(PRG_SIZE - 4), labels.get('reset'))
  assert.equal(word(PRG_SIZE - 2), labels.get('irq'))
  assert.equal(at(labels.get('reset')), 0x78)
  assert.equal(at(labels.get('nmi')), 0x48)
  assert.equal(at(labels.get('irq')), 0x40)
})

test('palettes and background colours match the spec', () => {
  const { rom, labels } = buildPadDemoRom()
  const prg = rom.subarray(16, 16 + PRG_SIZE)
  const palette = prg.subarray(labels.get('palette') - 0xc000, labels.get('palette') - 0xc000 + 32)
  assert.equal(palette[0], 0x10)
  assert.equal(palette[16], palette[0])
  assert.equal(palette[17], 0x16)
  assert.equal(palette[21], 0x2a)
  const bg = labels.get('bgColors') - 0xc000
  assert.deepEqual([...prg.subarray(bg, bg + 2)], [0x10, 0x21])
})

test('CHR tile 1 is solid colour 1 and tile 0 is empty', () => {
  const { rom } = buildPadDemoRom()
  const chr = rom.subarray(16 + PRG_SIZE)
  assert.ok(chr.subarray(0, 16).every((b) => b === 0))
  assert.ok(chr.subarray(16, 24).every((b) => b === 0xff))
  assert.ok(chr.subarray(24, 32).every((b) => b === 0))
})

test('C array holds the same bytes', () => {
  const { rom } = buildPadDemoRom()
  const c = romToC(rom)
  assert.match(c, /const unsigned char opengameconsole_pad_demo_rom\[24592\] = \{\n  0x4e, 0x45, 0x53, 0x1a,/)
  assert.match(c, /const size_t opengameconsole_pad_demo_rom_size = sizeof\(opengameconsole_pad_demo_rom\);/)
})

test('committed ROM files are up to date', () => {
  const { rom } = buildPadDemoRom()
  const nes = fs.readFileSync(new URL('assets/nes/pad-demo.nes', appRoot))
  assert.deepEqual(new Uint8Array(nes), rom, 'run npm run rom')
  const c = fs.readFileSync(new URL('native/pad_demo_rom.c', appRoot), 'utf8').replaceAll('\r\n', '\n')
  assert.equal(c, romToC(rom), 'run npm run rom')
})
```

`package.json` `scripts` bölümüne ekle:

```json
    "test": "node --test \"scripts/**/*.test.mjs\"",
    "rom": "node scripts/nes/write-pad-demo.mjs"
```

- [ ] **Step 2: Testin başarısız olduğunu gör**

Run: `npm test`
Expected: FAIL, `Cannot find module ... scripts/nes/pad-demo.mjs`.

- [ ] **Step 3: `scripts/nes/pad-demo.mjs` yaz**

```js
export const PRG_SIZE = 16384
export const CHR_SIZE = 8192
export const ROM_SIZE = 16 + PRG_SIZE + CHR_SIZE
const PRG_ORIGIN = 0xc000

const BUTTONS = 0x00
const X_POS = 0x01
const Y_POS = 0x02
const SPRITE_PALETTE = 0x03
const BG_INDEX = 0x04
const PREV_BUTTONS = 0x05
const NMI_FLAG = 0x06

const BUTTON_A = 0x80
const BUTTON_B = 0x40
const BUTTON_START_OR_SELECT = 0x30
const BUTTON_UP = 0x08
const BUTTON_DOWN = 0x04
const BUTTON_LEFT = 0x02
const BUTTON_RIGHT = 0x01

export function assemble(origin, program) {
  const bytes = []
  const labels = new Map()
  const fixups = []
  const emit = (...values) => { for (const value of values) bytes.push(value & 0xff) }
  const absolute = (opcode, target) => {
    if (typeof target === 'string') {
      fixups.push({ at: bytes.length + 1, label: target, kind: 'abs' })
      emit(opcode, 0, 0)
    } else {
      emit(opcode, target, target >> 8)
    }
  }
  const relative = (opcode, label) => {
    fixups.push({ at: bytes.length + 1, label, kind: 'rel' })
    emit(opcode, 0)
  }
  const asm = {
    label(name) {
      if (labels.has(name)) throw new Error(`duplicate label: ${name}`)
      labels.set(name, origin + bytes.length)
    },
    bytes: (...values) => emit(...values),
    sei: () => emit(0x78), cld: () => emit(0xd8), clc: () => emit(0x18),
    txs: () => emit(0x9a), txa: () => emit(0x8a), tya: () => emit(0x98), tax: () => emit(0xaa), tay: () => emit(0xa8),
    inx: () => emit(0xe8), dex: () => emit(0xca), dey: () => emit(0x88),
    pha: () => emit(0x48), pla: () => emit(0x68), lsr: () => emit(0x4a), rti: () => emit(0x40),
    ldaImm: (v) => emit(0xa9, v), ldxImm: (v) => emit(0xa2, v), ldyImm: (v) => emit(0xa0, v),
    andImm: (v) => emit(0x29, v), eorImm: (v) => emit(0x49, v), adcImm: (v) => emit(0x69, v), cpxImm: (v) => emit(0xe0, v),
    ldaZp: (z) => emit(0xa5, z), ldxZp: (z) => emit(0xa6, z), staZp: (z) => emit(0x85, z), andZp: (z) => emit(0x25, z),
    incZp: (z) => emit(0xe6, z), decZp: (z) => emit(0xc6, z), rolZp: (z) => emit(0x26, z),
    ldaAbs: (a) => absolute(0xad, a), staAbs: (a) => absolute(0x8d, a), stxAbs: (a) => absolute(0x8e, a),
    bitAbs: (a) => absolute(0x2c, a), ldaAbsX: (a) => absolute(0xbd, a), staAbsX: (a) => absolute(0x9d, a),
    jmp: (a) => absolute(0x4c, a),
    bpl: (label) => relative(0x10, label), bne: (label) => relative(0xd0, label), beq: (label) => relative(0xf0, label),
  }
  program(asm)
  for (const fix of fixups) {
    const target = labels.get(fix.label)
    if (target === undefined) throw new Error(`unknown label: ${fix.label}`)
    if (fix.kind === 'abs') {
      bytes[fix.at] = target & 0xff
      bytes[fix.at + 1] = target >> 8
    } else {
      const offset = target - (origin + fix.at + 1)
      if (offset < -128 || offset > 127) throw new Error(`branch to ${fix.label} out of range: ${offset}`)
      bytes[fix.at] = offset & 0xff
    }
  }
  return { bytes: Uint8Array.from(bytes), labels }
}

function padDemoProgram(a) {
  a.label('reset')
  a.sei(); a.cld()
  a.ldxImm(0x40); a.stxAbs(0x4017)
  a.ldxImm(0xff); a.txs()
  a.inx()
  a.stxAbs(0x2000); a.stxAbs(0x2001); a.stxAbs(0x4010)
  a.label('vblank1'); a.bitAbs(0x2002); a.bpl('vblank1')

  a.label('clearMemory')
  a.ldaImm(0)
  for (const page of [0x0000, 0x0100, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700]) a.staAbsX(page)
  a.ldaImm(0xff); a.staAbsX(0x0200)
  a.inx(); a.bne('clearMemory')
  a.label('vblank2'); a.bitAbs(0x2002); a.bpl('vblank2')

  a.ldaImm(124); a.staZp(X_POS)
  a.ldaImm(112); a.staZp(Y_POS)

  a.ldaAbs(0x2002)
  a.ldaImm(0x3f); a.staAbs(0x2006); a.ldaImm(0x00); a.staAbs(0x2006)
  a.ldxImm(0)
  a.label('paletteLoop'); a.ldaAbsX('palette'); a.staAbs(0x2007); a.inx(); a.cpxImm(32); a.bne('paletteLoop')

  a.ldaImm(0x20); a.staAbs(0x2006); a.ldaImm(0x00); a.staAbs(0x2006)
  a.ldaImm(0); a.ldyImm(4); a.ldxImm(0)
  a.label('nametableLoop'); a.staAbs(0x2007); a.inx(); a.bne('nametableLoop'); a.dey(); a.bne('nametableLoop')

  a.ldaImm(0); a.staAbs(0x2005); a.staAbs(0x2005)
  a.ldaImm(0x80); a.staAbs(0x2000)
  a.ldaImm(0x1e); a.staAbs(0x2001)

  a.label('main')
  a.label('waitNmi'); a.ldaZp(NMI_FLAG); a.beq('waitNmi')
  a.ldaImm(0); a.staZp(NMI_FLAG)

  a.ldaImm(1); a.staAbs(0x4016); a.ldaImm(0); a.staAbs(0x4016)
  a.ldxImm(8)
  a.label('readPad'); a.ldaAbs(0x4016); a.lsr(); a.rolZp(BUTTONS); a.dex(); a.bne('readPad')

  a.ldaZp(BUTTONS); a.andImm(BUTTON_UP); a.beq('noUp'); a.decZp(Y_POS)
  a.label('noUp'); a.ldaZp(BUTTONS); a.andImm(BUTTON_DOWN); a.beq('noDown'); a.incZp(Y_POS)
  a.label('noDown'); a.ldaZp(BUTTONS); a.andImm(BUTTON_LEFT); a.beq('noLeft'); a.decZp(X_POS)
  a.label('noLeft'); a.ldaZp(BUTTONS); a.andImm(BUTTON_RIGHT); a.beq('noRight'); a.incZp(X_POS)
  a.label('noRight'); a.ldaZp(BUTTONS); a.andImm(BUTTON_A); a.beq('noA'); a.ldaImm(0); a.staZp(SPRITE_PALETTE)
  a.label('noA'); a.ldaZp(BUTTONS); a.andImm(BUTTON_B); a.beq('noB'); a.ldaImm(1); a.staZp(SPRITE_PALETTE)
  a.label('noB'); a.ldaZp(PREV_BUTTONS); a.eorImm(0xff); a.andZp(BUTTONS); a.andImm(BUTTON_START_OR_SELECT); a.beq('noToggle')
  a.ldaZp(BG_INDEX); a.eorImm(1); a.staZp(BG_INDEX)
  a.label('noToggle'); a.ldaZp(BUTTONS); a.staZp(PREV_BUTTONS)

  a.ldaZp(Y_POS); a.staAbs(0x0200); a.staAbs(0x0204)
  a.clc(); a.adcImm(8); a.staAbs(0x0208); a.staAbs(0x020c)
  a.ldaImm(1); a.staAbs(0x0201); a.staAbs(0x0205); a.staAbs(0x0209); a.staAbs(0x020d)
  a.ldaZp(SPRITE_PALETTE); a.staAbs(0x0202); a.staAbs(0x0206); a.staAbs(0x020a); a.staAbs(0x020e)
  a.ldaZp(X_POS); a.staAbs(0x0203); a.staAbs(0x020b)
  a.clc(); a.adcImm(8); a.staAbs(0x0207); a.staAbs(0x020f)
  a.jmp('main')

  a.label('nmi')
  a.pha(); a.txa(); a.pha(); a.tya(); a.pha()
  a.ldaImm(0x00); a.staAbs(0x2003); a.ldaImm(0x02); a.staAbs(0x4014)
  a.ldaAbs(0x2002)
  a.ldaImm(0x3f); a.staAbs(0x2006); a.ldaImm(0x00); a.staAbs(0x2006)
  a.ldxZp(BG_INDEX); a.ldaAbsX('bgColors'); a.staAbs(0x2007)
  a.ldaImm(0); a.staAbs(0x2005); a.staAbs(0x2005)
  a.ldaImm(0x80); a.staAbs(0x2000)
  a.ldaImm(1); a.staZp(NMI_FLAG)
  a.pla(); a.tay(); a.pla(); a.tax(); a.pla()
  a.rti()

  a.label('irq')
  a.rti()

  a.label('palette')
  a.bytes(0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00)
  a.bytes(0x10, 0x16, 0x27, 0x30, 0x10, 0x2a, 0x27, 0x30, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00)
  a.label('bgColors')
  a.bytes(0x10, 0x21)
}

export function buildPadDemoRom() {
  const { bytes: code, labels } = assemble(PRG_ORIGIN, padDemoProgram)
  if (code.length > PRG_SIZE - 6) throw new Error('pad demo program does not fit in PRG')
  const prg = new Uint8Array(PRG_SIZE).fill(0xff)
  prg.set(code, 0)
  const vector = (offset, name) => {
    const address = labels.get(name)
    prg[offset] = address & 0xff
    prg[offset + 1] = address >> 8
  }
  vector(PRG_SIZE - 6, 'nmi')
  vector(PRG_SIZE - 4, 'reset')
  vector(PRG_SIZE - 2, 'irq')
  const chr = new Uint8Array(CHR_SIZE)
  chr.fill(0xff, 16, 24)
  const rom = new Uint8Array(ROM_SIZE)
  rom.set([0x4e, 0x45, 0x53, 0x1a, 1, 1, 0x01, 0x00], 0)
  rom.set(prg, 16)
  rom.set(chr, 16 + PRG_SIZE)
  return { rom, labels }
}

export function romToC(rom) {
  const rows = []
  for (let offset = 0; offset < rom.length; offset += 16) {
    const row = Array.from(rom.subarray(offset, offset + 16), (b) => `0x${b.toString(16).padStart(2, '0')}`)
    rows.push(`  ${row.join(', ')},`)
  }
  return `/* Generated by scripts/nes/write-pad-demo.mjs. Do not edit. */\n#include <stddef.h>\n\n` +
    `const unsigned char opengameconsole_pad_demo_rom[${rom.length}] = {\n${rows.join('\n')}\n};\n` +
    `const size_t opengameconsole_pad_demo_rom_size = sizeof(opengameconsole_pad_demo_rom);\n`
}
```

- [ ] **Step 4: `scripts/nes/write-pad-demo.mjs` yaz**

```js
import fs from 'node:fs'
import { buildPadDemoRom, romToC } from './pad-demo.mjs'

const appRoot = new URL('../../', import.meta.url)
const { rom } = buildPadDemoRom()
fs.mkdirSync(new URL('assets/nes/', appRoot), { recursive: true })
fs.mkdirSync(new URL('native/', appRoot), { recursive: true })
fs.writeFileSync(new URL('assets/nes/pad-demo.nes', appRoot), rom)
fs.writeFileSync(new URL('native/pad_demo_rom.c', appRoot), romToC(rom))
console.log(`pad-demo.nes ${rom.length} bytes`)
```

- [ ] **Step 5: Üret ve testleri çalıştır**

```powershell
npm run rom
npm test
```

Expected: `pad-demo.nes 24592 bytes`, ardından 8 test PASS.

- [ ] **Step 6: Commit**

```powershell
git add scripts/nes package.json assets/nes/pad-demo.nes native/pad_demo_rom.c
git commit -m "feat: add generated pad demo NES ROM"
```

---

### Task 5: Host eklentisi ve TS bildirimi

**Files:**
- Create: `scripts/nes-host-plugin.mjs`
- Create: `src/nes-host.d.ts`
- Modify: `package.json` (`gea.compilerPlugins`)
- Test: `scripts/nes-host-plugin.test.mjs`

**Interfaces:**
- Consumes: `@geastack/compiler/plugin` → `noPluginCapabilities`; `@geastack/compiler/dist/plugins/load.js` → `loadCliPlugins(specifiers)`; Task 2’nin `gea.compilerPlugins` alanı.
- Produces:
  - Eklenti adı `opengameconsole-nes-host`.
  - `nesPlay` → `opengameconsole::nes::play`, `nesSetButton` → `opengameconsole::nes::setButton`, `nesStop` → `opengameconsole::nes::stop`. Her biri `#include "nes_host.h"` önsözünü ister.
  - TS: `declare function nesPlay(): number`, `declare function nesSetButton(name: string, down: number): void`, `declare function nesStop(): void`.

- [ ] **Step 1: Başarısız testi yaz**

`scripts/nes-host-plugin.test.mjs`:

```js
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { fileURLToPath } from 'node:url'
import { loadCliPlugins } from '@geastack/compiler/dist/plugins/load.js'

const pluginPath = fileURLToPath(new URL('./nes-host-plugin.mjs', import.meta.url))

test('geatsc accepts the nes host plugin and maps the three calls', async () => {
  const plugins = await loadCliPlugins([pluginPath])
  const plugin = plugins.find((item) => item.name === 'opengameconsole-nes-host')
  assert.ok(plugin, 'plugin loaded')
  const { capabilities } = plugin.instantiate(new Map())
  assert.deepEqual([...capabilities.hostFunctions], [
    ['nesPlay', 'opengameconsole::nes::play'],
    ['nesSetButton', 'opengameconsole::nes::setButton'],
    ['nesStop', 'opengameconsole::nes::stop'],
  ])
  for (const spelling of capabilities.hostFunctions.values()) {
    assert.deepEqual(capabilities.hostPreambles.get(spelling), ['#include "nes_host.h"'])
  }
})
```

- [ ] **Step 2: Testin başarısız olduğunu gör**

Run: `npm test`
Expected: nes-host-plugin testi FAIL, `--plugin ...nes-host-plugin.mjs: Cannot find module`.

- [ ] **Step 3: Eklentiyi yaz**

`scripts/nes-host-plugin.mjs`:

```js
import { noPluginCapabilities } from '@geastack/compiler/plugin'

const preamble = ['#include "nes_host.h"']
const hostFunctions = [
  ['nesPlay', 'opengameconsole::nes::play'],
  ['nesSetButton', 'opengameconsole::nes::setButton'],
  ['nesStop', 'opengameconsole::nes::stop'],
]

export default {
  name: 'opengameconsole-nes-host',
  instantiate: () => ({
    producers: () => [],
    lower: () => false,
    capabilities: {
      ...noPluginCapabilities,
      hostFunctions: new Map(hostFunctions),
      hostPreambles: new Map(hostFunctions.map(([, spelling]) => [spelling, preamble])),
    },
  }),
}
```

`src/nes-host.d.ts`:

```ts
declare function nesPlay(): number
declare function nesSetButton(name: string, down: number): void
declare function nesStop(): void
```

`package.json` `gea` nesnesine, `entry` satırının altına:

```json
    "compilerPlugins": ["scripts/nes-host-plugin.mjs"],
```

- [ ] **Step 4: Testleri ve tsc’yi çalıştır**

```powershell
npm test
npm run check
```

Expected: tüm testler PASS, tsc hatasız.

- [ ] **Step 5: Commit**

```powershell
git add scripts/nes-host-plugin.mjs scripts/nes-host-plugin.test.mjs src/nes-host.d.ts package.json
git commit -m "feat: bind nes host calls through a geatsc plugin"
```

---

### Task 6: FCEUmm’u vendor’la

**Files:**
- Create: `native/fceumm/src/**` (upstream kopya), `native/fceumm/COPYING`, `native/fceumm/UPSTREAM.txt`
- Modify: `package.json` (`gea.sailfish.staticLibraries`)

**Interfaces:**
- Consumes: Task 2’nin `normalizeStaticLibraries`.
- Produces: `gea_static_fceumm` CMake hedefi; uygulamaya açık include `native/fceumm/src/drivers/libretro/libretro-common/include` (içinde `libretro.h`). libretro sembolleri: `retro_set_environment`, `retro_set_video_refresh`, `retro_set_audio_sample`, `retro_set_audio_sample_batch`, `retro_set_input_poll`, `retro_set_input_state`, `retro_init`, `retro_deinit`, `retro_load_game`, `retro_unload_game`, `retro_get_system_av_info`, `retro_set_controller_port_device`, `retro_run`.

- [ ] **Step 1: Sabit commit’i indir ve kopyala**

```powershell
$src = Join-Path $env:TEMP 'fceumm-vendor'
Remove-Item -Recurse -Force $src -ErrorAction SilentlyContinue
git init $src
git -C $src remote add origin https://github.com/libretro/libretro-fceumm
git -C $src fetch --depth 1 origin 7a542dab1e87679921962a9f056186eca425c0c2
git -C $src checkout FETCH_HEAD
git -C $src rev-parse HEAD
New-Item -ItemType Directory -Force native/fceumm | Out-Null
Copy-Item -Recurse "$src/src" native/fceumm/src
Copy-Item "$src/Copying" native/fceumm/COPYING
```

Expected: `rev-parse` çıktısı `7a542dab1e87679921962a9f056186eca425c0c2`.

- [ ] **Step 2: `native/fceumm/UPSTREAM.txt` yaz**

```text
https://github.com/libretro/libretro-fceumm
7a542dab1e87679921962a9f056186eca425c0c2

Copied: src/ unchanged, Copying as COPYING.
Build: gea.sailfish.staticLibraries "fceumm" in package.json, matching
Makefile.common with HAVE_NTSC and HAVE_HDPACK off and STATIC_LINKING unset.
License: GPL-2.0. The opengameconsole RPM that links it is GPL-2.0.
```

- [ ] **Step 3: `package.json` içine `gea.sailfish` ve statik kütüphaneyi ekle**

`gea` nesnesine, `targets` nesnesinin üstüne:

```json
    "sailfish": {
      "staticLibraries": [
        {
          "name": "fceumm",
          "root": "native/fceumm",
          "sources": [
            "native/fceumm/src",
            "native/fceumm/src/boards",
            "native/fceumm/src/input",
            "native/fceumm/src/drivers/libretro/libretro.c",
            "native/fceumm/src/drivers/libretro/libretro_dipswitch.c",
            "native/fceumm/src/drivers/libretro/libretro-common/streams/memory_stream.c",
            "native/fceumm/src/drivers/libretro/libretro-common/compat/compat_posix_string.c",
            "native/fceumm/src/drivers/libretro/libretro-common/compat/compat_snprintf.c",
            "native/fceumm/src/drivers/libretro/libretro-common/compat/compat_strcasestr.c",
            "native/fceumm/src/drivers/libretro/libretro-common/compat/compat_strl.c",
            "native/fceumm/src/drivers/libretro/libretro-common/compat/fopen_utf8.c",
            "native/fceumm/src/drivers/libretro/libretro-common/encodings/encoding_utf.c",
            "native/fceumm/src/drivers/libretro/libretro-common/file/file_path.c",
            "native/fceumm/src/drivers/libretro/libretro-common/file/file_path_io.c",
            "native/fceumm/src/drivers/libretro/libretro-common/streams/file_stream.c",
            "native/fceumm/src/drivers/libretro/libretro-common/streams/file_stream_transforms.c",
            "native/fceumm/src/drivers/libretro/libretro-common/string/stdstring.c",
            "native/fceumm/src/drivers/libretro/libretro-common/time/rtime.c",
            "native/fceumm/src/drivers/libretro/libretro-common/vfs/vfs_implementation.c"
          ],
          "includePaths": [
            "native/fceumm/src/drivers/libretro",
            "native/fceumm/src",
            "native/fceumm/src/input",
            "native/fceumm/src/boards"
          ],
          "publicIncludePaths": [
            "native/fceumm/src/drivers/libretro/libretro-common/include"
          ],
          "defines": ["__LIBRETRO__", "PATH_MAX=1024", "FCEU_VERSION_NUMERIC=9900", "FRONTEND_SUPPORTS_RGB565"]
        }
      ]
    },
```

- [ ] **Step 4: Ayarı Linux hedefinin doğrulayıcısıyla dene**

```powershell
node --input-type=module -e "import fs from 'node:fs'; import { normalizeStaticLibraries } from './vendor/geastack-linux/targets/sailfish-os/prepare-lib.mjs'; const app = JSON.parse(fs.readFileSync('package.json','utf8')).gea; const [lib] = normalizeStaticLibraries(process.cwd(), app.sailfish.staticLibraries); console.log(lib.name, lib.sources.length, lib.sources.includes('native/fceumm/src/x6502.c'), lib.sources.includes('native/fceumm/src/drivers/libretro/libretro.c'))"
```

Expected: `fceumm <N> true true`; N yüzün üstündedir (23 çekirdek dosyası + boards + input + 16 libretro dosyası). Hata verirse mesajdaki yolu upstream ağaçta düzelt.

- [ ] **Step 5: Commit**

```powershell
git add native/fceumm package.json
git commit -m "chore: vendor libretro-fceumm 7a542dab as a static library"
```

---

### Task 7: `nes_host` — libretro frontend

**Files:**
- Create: `native/nes_host.h`, `native/nes_host.cpp`
- Modify: `package.json` (`gea.sailfish.nativeSources`)

**Interfaces:**
- Consumes: Task 3’ün `sailfish_display_write_rgb565`, `gea_app_before_refresh`; Task 4’ün `opengameconsole_pad_demo_rom`, `opengameconsole_pad_demo_rom_size`; Task 5’in C++ adları; Task 6’nın libretro sembolleri ve `libretro.h`; Sailfish hedefinin `sailfish_canvas_width()`, `sailfish_canvas_height()`.
- Produces:
  - `namespace opengameconsole::nes { double play(); void setButton(const std::string &name, double down); void stop(); }`
  - `extern "C" int gea_app_before_refresh(void)` güçlü tanımı.

C++ Windows’ta derlenmez; doğrulama Task 9 `sfdk` derlemesi ve Task 10 telefon denemesidir.

- [ ] **Step 1: `native/nes_host.h` yaz**

```cpp
#pragma once

#include <string>

namespace opengameconsole::nes {

// 0 started, 1 embedded ROM is not an iNES image, 2 core refused it.
double play();
// name: up, down, left, right, a, b, start, select. down: nonzero pressed.
void setButton(const std::string &name, double down);
void stop();

}  // namespace opengameconsole::nes
```

- [ ] **Step 2: `native/nes_host.cpp` yaz**

```cpp
#include "nes_host.h"

#include "libretro.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>

extern "C" const unsigned char opengameconsole_pad_demo_rom[];
extern "C" const size_t opengameconsole_pad_demo_rom_size;
extern "C" int sailfish_canvas_width();
extern "C" int sailfish_canvas_height();
extern "C" void sailfish_display_write_rgb565(const std::uint16_t *pixels, int x, int y, int width, int height);

namespace {

constexpr unsigned kMaxFrameWidth = 256;
constexpr unsigned kMaxFrameHeight = 240;
constexpr int kScreenBandPercent = 62;
constexpr int kMaxFramesPerLoop = 2;
constexpr std::uint32_t kAudioQueueLimitMs = 120;

struct ButtonName {
	const char *name;
	unsigned id;
};

constexpr ButtonName kButtons[] = {
	{"up", RETRO_DEVICE_ID_JOYPAD_UP},
	{"down", RETRO_DEVICE_ID_JOYPAD_DOWN},
	{"left", RETRO_DEVICE_ID_JOYPAD_LEFT},
	{"right", RETRO_DEVICE_ID_JOYPAD_RIGHT},
	{"a", RETRO_DEVICE_ID_JOYPAD_A},
	{"b", RETRO_DEVICE_ID_JOYPAD_B},
	{"start", RETRO_DEVICE_ID_JOYPAD_START},
	{"select", RETRO_DEVICE_ID_JOYPAD_SELECT},
};

bool g_running = false;
std::uint32_t g_buttons = 0;
std::uint16_t g_frame[kMaxFrameWidth * kMaxFrameHeight];
unsigned g_frameWidth = 0;
unsigned g_frameHeight = 0;
bool g_frameChanged = false;
double g_fps = 60.0;
std::int64_t g_startNs = 0;
std::int64_t g_framesRun = 0;
std::vector<std::uint16_t> g_scaled;
SDL_AudioDeviceID g_audio = 0;
std::uint32_t g_audioBytesPerSecond = 0;
retro_game_info_ext g_gameInfoExt{};

std::int64_t nowNs()
{
	timespec ts{};
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return static_cast<std::int64_t>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

bool romLooksValid(const unsigned char *rom, std::size_t size)
{
	return size >= 16 + 16384 && std::memcmp(rom, "NES\x1A", 4) == 0;
}

bool environment(unsigned cmd, void *data)
{
	switch (cmd) {
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
		return *static_cast<const retro_pixel_format *>(data) == RETRO_PIXEL_FORMAT_RGB565;
	case RETRO_ENVIRONMENT_GET_GAME_INFO_EXT:
		*static_cast<const retro_game_info_ext **>(data) = &g_gameInfoExt;
		return true;
	case RETRO_ENVIRONMENT_GET_CAN_DUPE:
		*static_cast<bool *>(data) = true;
		return true;
	default:
		return false;
	}
}

void videoRefresh(const void *data, unsigned width, unsigned height, std::size_t pitch)
{
	if (!data) return;
	width = std::min(width, kMaxFrameWidth);
	height = std::min(height, kMaxFrameHeight);
	const auto *src = static_cast<const std::uint8_t *>(data);
	for (unsigned y = 0; y < height; y++)
		std::memcpy(g_frame + y * kMaxFrameWidth, src + y * pitch, width * sizeof(std::uint16_t));
	g_frameWidth = width;
	g_frameHeight = height;
	g_frameChanged = true;
}

void queueAudio(const std::int16_t *samples, std::size_t frames)
{
	if (!g_audio || frames == 0) return;
	if (SDL_GetQueuedAudioSize(g_audio) > g_audioBytesPerSecond * kAudioQueueLimitMs / 1000) return;
	SDL_QueueAudio(g_audio, samples, static_cast<Uint32>(frames * 2 * sizeof(std::int16_t)));
}

void audioSample(std::int16_t left, std::int16_t right)
{
	const std::int16_t frame[2] = {left, right};
	queueAudio(frame, 1);
}

std::size_t audioSampleBatch(const std::int16_t *data, std::size_t frames)
{
	queueAudio(data, frames);
	return frames;
}

void inputPoll() {}

std::int16_t inputState(unsigned port, unsigned device, unsigned index, unsigned id)
{
	if (port != 0 || device != RETRO_DEVICE_JOYPAD || index != 0) return 0;
	if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return static_cast<std::int16_t>(g_buttons);
	if (id >= 16) return 0;
	return static_cast<std::int16_t>((g_buttons >> id) & 1u);
}

void openAudio(double sampleRate)
{
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
		std::fprintf(stderr, "[nes] audio unavailable: %s\n", SDL_GetError());
		return;
	}
	SDL_AudioSpec want{};
	want.freq = static_cast<int>(sampleRate + 0.5);
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = 1024;
	g_audio = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
	if (!g_audio) {
		std::fprintf(stderr, "[nes] audio device failed: %s\n", SDL_GetError());
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return;
	}
	g_audioBytesPerSecond = static_cast<std::uint32_t>(want.freq) * 2 * sizeof(std::int16_t);
	SDL_PauseAudioDevice(g_audio, 0);
}

void closeAudio()
{
	if (!g_audio) return;
	SDL_CloseAudioDevice(g_audio);
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	g_audio = 0;
}

// Integer-scales the last core frame into the top band of the canvas,
// centred and clipped to the band. Margins stay whatever Gea painted there.
bool drawFrame()
{
	const int canvasWidth = sailfish_canvas_width();
	const int bandHeight = sailfish_canvas_height() * kScreenBandPercent / 100;
	if (g_frameWidth == 0 || g_frameHeight == 0 || canvasWidth <= 0 || bandHeight <= 0) return false;
	const int frameWidth = static_cast<int>(g_frameWidth);
	const int frameHeight = static_cast<int>(g_frameHeight);
	const int scale = std::max(1, std::min(canvasWidth / frameWidth, bandHeight / frameHeight));
	const int left = (canvasWidth - frameWidth * scale) / 2;
	const int top = (bandHeight - frameHeight * scale) / 2;
	const int clipLeft = std::max(0, left);
	const int clipTop = std::max(0, top);
	const int width = std::min(canvasWidth, left + frameWidth * scale) - clipLeft;
	const int height = std::min(bandHeight, top + frameHeight * scale) - clipTop;
	if (width <= 0 || height <= 0) return false;
	g_scaled.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
	int previousSourceRow = -1;
	for (int y = 0; y < height; y++) {
		const int sourceRow = (clipTop + y - top) / scale;
		std::uint16_t *dst = g_scaled.data() + static_cast<std::size_t>(y) * width;
		if (sourceRow == previousSourceRow) {
			std::memcpy(dst, dst - width, static_cast<std::size_t>(width) * sizeof(std::uint16_t));
			continue;
		}
		const std::uint16_t *src = g_frame + static_cast<std::size_t>(sourceRow) * kMaxFrameWidth;
		for (int x = 0; x < width; x++) dst[x] = src[(clipLeft + x - left) / scale];
		previousSourceRow = sourceRow;
	}
	sailfish_display_write_rgb565(g_scaled.data(), clipLeft, clipTop, width, height);
	return true;
}

void unloadCore()
{
	retro_unload_game();
	retro_deinit();
	closeAudio();
	g_running = false;
	g_buttons = 0;
	g_frameWidth = 0;
	g_frameHeight = 0;
	g_frameChanged = false;
}

}  // namespace

namespace opengameconsole::nes {

double play()
{
	if (g_running) unloadCore();
	const unsigned char *rom = opengameconsole_pad_demo_rom;
	const std::size_t size = opengameconsole_pad_demo_rom_size;
	if (!romLooksValid(rom, size)) {
		std::fprintf(stderr, "[nes] embedded ROM is not an iNES image\n");
		return 1;
	}
	g_gameInfoExt = {};
	g_gameInfoExt.full_path = "pad-demo.nes";
	g_gameInfoExt.dir = "";
	g_gameInfoExt.name = "pad-demo";
	g_gameInfoExt.ext = "nes";
	g_gameInfoExt.data = rom;
	g_gameInfoExt.size = size;
	g_gameInfoExt.file_in_archive = false;
	g_gameInfoExt.persistent_data = true;

	retro_set_environment(environment);
	retro_set_video_refresh(videoRefresh);
	retro_set_audio_sample(audioSample);
	retro_set_audio_sample_batch(audioSampleBatch);
	retro_set_input_poll(inputPoll);
	retro_set_input_state(inputState);
	retro_init();
	retro_game_info info{};
	info.path = "pad-demo.nes";
	info.data = rom;
	info.size = size;
	if (!retro_load_game(&info)) {
		std::fprintf(stderr, "[nes] core refused the ROM\n");
		retro_deinit();
		return 2;
	}
	retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
	retro_system_av_info av{};
	retro_get_system_av_info(&av);
	g_fps = av.timing.fps > 1.0 ? av.timing.fps : 60.0;
	openAudio(av.timing.sample_rate > 0 ? av.timing.sample_rate : 48000.0);
	g_buttons = 0;
	g_frameWidth = 0;
	g_frameHeight = 0;
	g_frameChanged = false;
	g_framesRun = 0;
	g_startNs = nowNs();
	g_running = true;
	return 0;
}

void setButton(const std::string &name, double down)
{
	if (!g_running) return;
	for (const ButtonName &button : kButtons) {
		if (name != button.name) continue;
		if (down != 0) g_buttons |= 1u << button.id;
		else g_buttons &= ~(1u << button.id);
		return;
	}
}

void stop()
{
	if (g_running) unloadCore();
}

}  // namespace opengameconsole::nes

// Runs the NES at its own rate on 60, 90 or 120 Hz panels: frames due since
// play() are produced, at most kMaxFramesPerLoop per loop turn; a longer stall
// skips ahead instead of fast-forwarding.
extern "C" int gea_app_before_refresh(void)
{
	if (!g_running) return 0;
	const auto due = static_cast<std::int64_t>(static_cast<double>(nowNs() - g_startNs) * g_fps / 1e9);
	for (int i = 0; i < kMaxFramesPerLoop && g_framesRun < due; i++) {
		retro_run();
		g_framesRun++;
	}
	if (g_framesRun < due) g_framesRun = due;
	if (!g_frameChanged) return 0;
	g_frameChanged = false;
	return drawFrame() ? 1 : 0;
}
```

- [ ] **Step 3: `package.json` `gea.sailfish` içine `nativeSources` ekle**

`"staticLibraries"` satırının üstüne:

```json
      "nativeSources": ["native/nes_host.cpp", "native/nes_host.h", "native/pad_demo_rom.c"],
```

- [ ] **Step 4: Gözden geçir**

Elle kontrol listesi:
- `nes_host.h` yalnızca `<string>` içerir (geatsc’nin ürettiği her C++ dosyasına girer).
- Task 5’teki üç C++ adı ile `nes_host.h` imzaları aynı: `double play()`, `void setButton(const std::string &, double)`, `void stop()`.
- `opengameconsole_pad_demo_rom` adları `native/pad_demo_rom.c` ile aynı.

- [ ] **Step 5: Commit**

```powershell
git add native/nes_host.h native/nes_host.cpp package.json
git commit -m "feat: add libretro frontend for the embedded NES ROM"
```

---

### Task 8: Kabuk ve oyun ekranı

**Files:**
- Modify: `src/index.tsx`, `src/styles.css`
- Create: `assets/fonts/Inter-Regular.ttf`, `assets/fonts/Inter-LICENSE.txt`

**Interfaces:**
- Consumes: Task 5’in `nesPlay`, `nesSetButton`, `nesStop` bildirimleri.
- Produces: `.screen-band` sınıflı boş `div` ekranın üst %62’sini kaplar; Task 7’nin `drawFrame` aynı oranı tuval pikseliyle hesaplar.

- [ ] **Step 1: Inter fontunu ekle**

```powershell
$zip = Join-Path $env:TEMP 'Inter-3.19.zip'
$dir = Join-Path $env:TEMP 'Inter-3.19'
Invoke-WebRequest https://github.com/rsms/inter/releases/download/v3.19/Inter-3.19.zip -OutFile $zip
Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
Expand-Archive $zip -DestinationPath $dir
$ttf = Get-ChildItem $dir -Recurse -Filter Inter-Regular.ttf | Select-Object -First 1
$license = Get-ChildItem $dir -Recurse -Filter LICENSE.txt | Select-Object -First 1
New-Item -ItemType Directory -Force assets/fonts | Out-Null
Copy-Item $ttf.FullName assets/fonts/Inter-Regular.ttf
Copy-Item $license.FullName assets/fonts/Inter-LICENSE.txt
Get-Item assets/fonts/Inter-Regular.ttf | Select-Object Length
```

Expected: `Length` 300 KB civarında, sıfır değil.

- [ ] **Step 2: `src/index.tsx` yaz**

```tsx
import { ReactiveComponent, mount } from '@geastack/core'
import './styles.css'

function playError(code: number): string {
  if (code === 1) return 'ROM açılamadı'
  if (code === 2) return 'NES başlamadı'
  return 'Bilinmeyen hata'
}

export class App extends ReactiveComponent {
  screen = 'shell'
  message = ''

  play() {
    const code = nesPlay()
    if (code === 0) {
      this.message = ''
      this.screen = 'game'
    } else {
      this.message = playError(code)
    }
  }

  back() {
    nesStop()
    this.screen = 'shell'
  }

  template() {
    return (
      <div class="app">
        <div class={this.screen === 'shell' ? 'shell' : 'hidden'}>
          <span class="title">Opengameconsole</span>
          <span class="game-name">Pad Demo</span>
          <button class="play" onClick={() => this.play()}>Oyna</button>
          <span class="message">{this.message}</span>
        </div>
        <div class={this.screen === 'game' ? 'game' : 'hidden'}>
          <div class="screen-band"></div>
          <div class="controls">
            <div class="top-row">
              <button class="back" onClick={() => this.back()}>Geri</button>
            </div>
            <div class="pad-row">
              <div class="dpad">
                <div class="key" onTouchStart={() => nesSetButton('up', 1)} onTouchEnd={() => nesSetButton('up', 0)}>
                  <span>↑</span>
                </div>
                <div class="dpad-middle">
                  <div class="key" onTouchStart={() => nesSetButton('left', 1)} onTouchEnd={() => nesSetButton('left', 0)}>
                    <span>←</span>
                  </div>
                  <div class="key-gap"></div>
                  <div class="key" onTouchStart={() => nesSetButton('right', 1)} onTouchEnd={() => nesSetButton('right', 0)}>
                    <span>→</span>
                  </div>
                </div>
                <div class="key" onTouchStart={() => nesSetButton('down', 1)} onTouchEnd={() => nesSetButton('down', 0)}>
                  <span>↓</span>
                </div>
              </div>
              <div class="face">
                <div class="face-key" onTouchStart={() => nesSetButton('b', 1)} onTouchEnd={() => nesSetButton('b', 0)}>
                  <span>B</span>
                </div>
                <div class="face-key" onTouchStart={() => nesSetButton('a', 1)} onTouchEnd={() => nesSetButton('a', 0)}>
                  <span>A</span>
                </div>
              </div>
            </div>
            <div class="menu-row">
              <div class="menu-key" onTouchStart={() => nesSetButton('start', 1)} onTouchEnd={() => nesSetButton('start', 0)}>
                <span>Start</span>
              </div>
              <div class="menu-key" onTouchStart={() => nesSetButton('select', 1)} onTouchEnd={() => nesSetButton('select', 0)}>
                <span>Select</span>
              </div>
            </div>
          </div>
        </div>
      </div>
    )
  }
}

mount(App)
```

- [ ] **Step 3: `src/styles.css` yaz**

360×720 CSS piksellik (DPR 2’de 720×1440) bir telefonda alt bant 273 px’tir; tuş bandının içeriği 268 px tutar. Ölçüleri büyütürken bu sınırı aşma.

```css
@font-face {
  font-family: 'Inter';
  src: url('../assets/fonts/Inter-Regular.ttf');
}

.app {
  display: flex;
  flex-direction: column;
  width: 100vw;
  height: 100vh;
  background-color: #000000;
  color: #f8fafc;
  font-family: 'Inter';
}

.hidden { display: none; }

.shell {
  display: flex;
  flex-direction: column;
  flex: 1;
  align-items: center;
  justify-content: center;
  gap: 16px;
  background-color: #101418;
}

.title { color: #2dd4bf; font-size: 28px; }
.game-name { font-size: 20px; }
.message { color: #f87171; font-size: 18px; }

.play {
  width: 200px;
  height: 64px;
  border-radius: 12px;
  background-color: #2dd4bf;
  color: #101418;
  font-size: 24px;
}

.game { display: flex; flex-direction: column; flex: 1; }
.screen-band { flex: 62; background-color: #000000; }

.controls {
  display: flex;
  flex-direction: column;
  flex: 38;
  gap: 8px;
  padding: 8px;
  background-color: #182026;
}

.top-row { display: flex; flex-direction: row; }

.back {
  width: 96px;
  height: 36px;
  border-radius: 8px;
  background-color: #334155;
  color: #f8fafc;
  font-size: 16px;
}

.pad-row {
  display: flex;
  flex-direction: row;
  flex: 1;
  align-items: center;
  justify-content: space-between;
  padding-left: 8px;
  padding-right: 8px;
}

.dpad { display: flex; flex-direction: column; align-items: center; gap: 4px; }
.dpad-middle { display: flex; flex-direction: row; gap: 4px; }

.key {
  display: flex;
  width: 52px;
  height: 52px;
  align-items: center;
  justify-content: center;
  border-radius: 10px;
  background-color: #334155;
  font-size: 22px;
}

.key-gap { width: 52px; height: 52px; }

.face { display: flex; flex-direction: row; align-items: center; gap: 16px; }

.face-key {
  display: flex;
  width: 64px;
  height: 64px;
  align-items: center;
  justify-content: center;
  border-radius: 32px;
  background-color: #b91c1c;
  font-size: 22px;
}

.menu-row { display: flex; flex-direction: row; justify-content: center; gap: 24px; }

.menu-key {
  display: flex;
  width: 104px;
  height: 36px;
  align-items: center;
  justify-content: center;
  border-radius: 18px;
  background-color: #475569;
  font-size: 16px;
}
```

- [ ] **Step 4: tsc ve web build**

```powershell
npm run check
npm run build
```

Expected: ikisi de hatasız. (Web build host çağrılarını çalıştırmaz; yalnızca derler.)

- [ ] **Step 5: Yerleşimi tarayıcıda gör (isteğe bağlı)**

Run: `npm run dev`, açılan adreste kabuk görünür. Oyna’ya basmak tarayıcıda `nesPlay is not defined` verir; bu beklenen durumdur.

- [ ] **Step 6: Commit**

```powershell
git add src/index.tsx src/styles.css assets/fonts
git commit -m "feat: add shell and touch controls screens"
```

---

### Task 9: Sailfish ayarları, ikon ve aarch64 RPM

**Files:**
- Create: `scripts/make-icon.mjs`, `assets/icon.png`
- Modify: `package.json` (`license`, `scripts`, `gea.icons`, `gea.sailfish`)

**Interfaces:**
- Consumes: Task 2–8’in tamamı.
- Produces: `.gea-sailfish/build/opengameconsole-aarch64/project/RPMS/**/harbour-gea-opengameconsole-0.1.0-1.aarch64.rpm`.

- [ ] **Step 1: İkon betiği**

`scripts/make-icon.mjs`:

```js
import sharp from 'sharp'
import { fileURLToPath } from 'node:url'

const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="0 0 512 512">
  <rect width="512" height="512" rx="96" fill="#101418"/>
  <rect x="80" y="160" width="352" height="192" rx="64" fill="#2dd4bf"/>
  <rect x="136" y="244" width="88" height="24" rx="4" fill="#101418"/>
  <rect x="168" y="212" width="24" height="88" rx="4" fill="#101418"/>
  <circle cx="336" cy="236" r="20" fill="#b91c1c"/>
  <circle cx="380" cy="276" r="20" fill="#b91c1c"/>
</svg>`

await sharp(Buffer.from(svg)).png().toFile(fileURLToPath(new URL('../assets/icon.png', import.meta.url)))
console.log('assets/icon.png 512x512')
```

Run: `node scripts/make-icon.mjs`
Expected: `assets/icon.png 512x512`.

- [ ] **Step 2: `package.json` son hali**

`license` değerini `"GPL-2.0-only"` yap. `scripts` içine ekle:

```json
    "sailfish:build": "powershell -NoProfile -ExecutionPolicy Bypass -File node_modules/@geastack/linux/targets/sailfish-os/build-sailfish-os.ps1 -AppDirectory . -Architecture aarch64",
    "sailfish:deploy": "powershell -NoProfile -ExecutionPolicy Bypass -File scripts/deploy-sailfish.ps1"
```

`gea` içine, `compilerPlugins` altına:

```json
    "icons": { "512": "assets/icon.png" },
```

`gea.sailfish` nesnesinin başına (mevcut `nativeSources` ve `staticLibraries` korunur):

```json
      "organizationName": "org.opengameconsole",
      "applicationName": "opengameconsole",
      "devicePixelRatio": 2,
      "permissions": ["Audio"],
```

- [ ] **Step 3: Hazırlığı Windows’ta çalıştır**

```powershell
node vendor/geastack-linux/targets/sailfish-os/prepare.mjs . aarch64
$generated = Get-ChildItem .gea-sailfish/generated -Recurse -File
($generated | Select-String -Pattern 'opengameconsole::nes::play').Count
($generated | Select-String -Pattern 'throwReferenceError.*nes(Play|SetButton|Stop)').Count
(Select-String -Path .gea-sailfish/build/opengameconsole-aarch64/project/sources.cmake -Pattern 'GEA_STATIC_LIBRARY_FCEUMM_SOURCES|GEA_STATIC_LIBRARY_NAMES').Count
```

Expected:
- `prepare.mjs` son satırda sahne yolunu yazar.
- İlk sayı 1 veya daha fazla: host çağrısı C++’a bağlanmıştır.
- İkinci sayı 0.
- Üçüncü sayı 2.

- [ ] **Step 4: aarch64 derlemesi**

Run: `npm run sailfish:build`
Expected: derleme biter, `Get-ChildItem .gea-sailfish/build/opengameconsole-aarch64/project/RPMS -Recurse -Filter *.aarch64.rpm` bir dosya listeler.

Derleme hata verirse tek tek düzelt ve yeniden çalıştır. Beklenebilecek hatalar ve yerleri:
- `multiple definition of '<sembol>'`: FCEUmm ile Gea çerçevesi aynı global adı tanımlıyor. Hatada adı geçen FCEUmm dosyasını bul; çakışan sembol yalnızca o dosyada kullanılıyorsa `static` yap ve değişikliği `native/fceumm/UPSTREAM.txt` içinde “Local changes” başlığı altına satır satır yaz.
- `libretro.h: No such file`: `publicIncludePaths` yolu veya Task 2 CMake döngüsü.
- `undefined reference to opengameconsole::nes::...`: `nativeSources` içinde `native/nes_host.cpp` eksik.
- `undefined reference to gea_app_before_refresh` olmaz (zayıf varsayılan var); `sailfish_display_write_rgb565` için olursa Task 3 Step 2.

- [ ] **Step 5: Commit**

```powershell
git add scripts/make-icon.mjs assets/icon.png package.json native/fceumm/UPSTREAM.txt
git commit -m "build: sailfish aarch64 package settings and icon"
```

(Step 4’te FCEUmm kaynağı değiştiyse `git add native/fceumm` de eklenir.)

---

### Task 10: Telefona kurulum ve deneme

**Files:**
- Create: `scripts/deploy-sailfish.ps1`

**Interfaces:**
- Consumes: Task 9’un RPM’i, `npm run sailfish:deploy` betiği.
- Produces: telefonda kurulu `harbour-gea-opengameconsole`.

- [ ] **Step 1: Deploy betiği**

`scripts/deploy-sailfish.ps1`:

```powershell
param(
	[string]$PhoneHost = '192.168.1.220',
	[string]$User = 'defaultuser'
)
$ErrorActionPreference = 'Stop'
$rpmDir = Join-Path $PSScriptRoot '..\.gea-sailfish\build\opengameconsole-aarch64\project\RPMS'
$rpm = Get-ChildItem -Path $rpmDir -Recurse -Filter 'harbour-gea-opengameconsole-*.aarch64.rpm' -ErrorAction SilentlyContinue |
	Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $rpm) { throw "No aarch64 RPM under $rpmDir. Run npm run sailfish:build first." }
$target = "$User@$PhoneHost"
& scp $rpm.FullName "${target}:~/"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ssh -t $target "pkcon install-local -y ~/$($rpm.Name)"
exit $LASTEXITCODE
```

- [ ] **Step 2: Gönder ve kur**

Run: `npm run sailfish:deploy`
Expected: `scp` dosyayı aktarır, `pkcon` “Installing” ve “Finished” yazar. Parola veya onay sorulursa kullanıcı aynı terminalde cevaplar. `pkcon` yetki hatası verirse kullanıcıya sorulur; `devel-su pkcon install-local` seçeneği yalnızca kullanıcı onaylarsa betiğe eklenir.

- [ ] **Step 3: Telefonda deneme (kullanıcı yapar, sonuçları raporlar)**

1. Uygulama listesinde ikon görünür, açılır. Kabukta “Pad Demo” ve Oyna var.
2. Oyna: üst bantta açık gri zemin ve ortada kırmızı 16×16 kare.
3. Yön tuşları kareyi hareket ettirir.
4. B kareyi yeşil, A kırmızı yapar. Start ve Select zemini açık mavi ile açık gri arasında değiştirir.
5. Oyun bandına dokunmak kareyi hareket ettirmez.
6. Geri kabuğa döner. Oyna aynı ROM’u baştan açar.
7. Kare akıcıdır, tuş bandı titremez.

Bir adım başarısızsa log alınır:

```powershell
ssh defaultuser@192.168.1.220 "journalctl --user -b --no-pager | grep -E 'nes|sailfish' | tail -n 80"
```

- [ ] **Step 4: Commit**

```powershell
git add scripts/deploy-sailfish.ps1
git commit -m "build: add sailfish deploy script"
```
