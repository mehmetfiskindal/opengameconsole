# NES ROM kütüphanesi

Tarih: 2026-10-05

Önceki spec: `2026-10-05-sailfish-nes-console-design.md`. Bu spec oradaki iki kararı değiştirir: “ROM: yalnızca gömülü pad denemesi, klasör taraması yok” ve kapsam dışındaki “ROM klasörü”. Geri kalan her şey (yerleşim, tuşlar, çizim kancası, ses, derleme, kurulum) aynen geçerlidir.

## Amaç

Kullanıcı telefona kendi koyduğu `.nes` dosyalarını kabuktaki bir listeden açabilir. Pad demo listede kalır. Pil destekli kayıt belleği (SRAM) olan oyunların ilerlemesi saklanır.

## Kilitlenen kararlar

- Klasörler: `~/Documents/NES` ve `~/Downloads`. Alt klasörlere inilmez.
- Dosya: adı `.nes` ile biten (büyük/küçük harf farketmez) düz dosya. En çok 4 MiB.
- Listeyi native taraf tutar. TypeScript yalnızca sayıyı, adları ve sıra numarasını görür; dosya yolu TypeScript’e çıkmaz.
- Kayıt: yalnızca SRAM. Anlık kayıt (save state) yok.
- Kayıt yeri: uygulamanın Sailjail veri klasörü, `~/.local/share/org.opengameconsole/opengameconsole/saves/`.
- İzinler: `["Audio", "Documents", "Downloads"]`.
- RPM içinde pad demo dışında ROM yoktur. Uygulama ROM indirmez.

## Mimari

Yeni bir native birim eklenir: `native/rom_library.cpp`, `native/rom_library.h`. SDL’e, libretro’ya ve Gea’ya bağlı değildir; yalnızca C++ standart kütüphanesini ve POSIX dosya çağrılarını kullanır. Bu sayede telefondan bağımsız test edilir.

`native/nes_host.cpp` ROM baytlarını ve kayıt dosyalarını bu birimden alır. libretro yaşam döngüsü, tuşlar, ses ve çizim kancası `nes_host.cpp` içinde kalır.

### `rom_library`

Bir giriş üç şeydir: görünen ad, kaynak (gömülü, Belgeler, İndirilenler) ve dosya yolu. Gömülü girişin yolu yoktur.

- `scan(home)`: `home + "/Documents/NES"` yoksa oluşturmayı dener; oluşturamazsa sessizce geçer. İki klasörü tarar, listeyi döndürür.
  - İlk giriş her zaman pad demodur, adı “Pad Demo”.
  - Diğerleri görünen ada göre, büyük/küçük harf farketmeden sıralanır. Ad eşitse Belgeler önce gelir.
  - Görünen ad uzantısız dosya adıdır. Aynı görünen ad iki kez geçiyorsa İndirilenler’deki girişin adına ` (Downloads)` eklenir.
  - Okunamayan klasör boş sayılır.
- `readRom(entry, out)`: gömülü girişte baytları diziden, dosya girişinde dosyadan okur. 4 MiB’dan büyük veya okunamayan dosyada `false` döner.
- `saveFileName(displayName, bytes)`: `<güvenli-ad>-<crc32>.sav`. Güvenli ad, `A-Z a-z 0-9 . _ -` ve boşluk dışındaki karakterlerin `_` olduğu görünen addır. CRC32 tüm ROM baytları üzerinden, 8 küçük onaltılık hanedir. Böylece iki klasörde aynı adlı iki farklı ROM birbirinin kaydını ezmez.
- `readSave(path, size, out)`: dosya varsa ve boyutu tam `size` ise okur, değilse `false`.
- `writeSave(path, bytes)`: klasörü yoksa oluşturur, önce `path + ".tmp"` dosyasına yazar, sonra `rename` eder. Yarım kalan yazma eski kaydı bozmaz.

Kayıt klasörü `$HOME/.local/share/org.opengameconsole/opengameconsole/saves`. `$HOME` yoksa kayıt devre dışıdır, oyun yine açılır.

### `nes_host` değişiklikleri

- Liste `nes_host.cpp` içinde tutulur; `refreshRoms()` onu `scan(getenv("HOME"))` ile yeniler.
- `play(index)` girişin baytlarını `std::vector` içine okur. `persistent_data` true kaldığı için bu vektör oyun kapanana kadar yaşar. `full_path`, `dir`, `name`, `ext` girişin gerçek dosyasından doldurulur.
- Oyun yüklendikten sonra `retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)` sıfırdan büyükse kayıt dosyası okunur ve `retro_get_memory_data` alanına kopyalanır.
- Kayıt üç anda yazılır: `stop()` içinde çekirdek kapanmadan önce, `play()` başka bir oyuna geçmeden önce ve oyun açıkken çizim kancasında 30 saniyede bir. Hepsinde SRAM son yazılan kopyayla karşılaştırılır; aynıysa dosyaya dokunulmaz. Uygulama oyun açıkken öldürülürse en çok son 30 saniye kaybolur.

## Host çağrıları

| Çağrı | Anlam |
| --- | --- |
| `nesRefreshRoms(): number` | Klasörleri tarar, listeyi yeniler. Pad demo dahil giriş sayısını döner, en az 1. |
| `nesRomName(index: number): string` | Girişin görünen adı. Aralık dışında boş metin. |
| `nesPlay(index: number): number` | Girişi yükler ve çekirdeği başlatır. 0 başarı, 1 dosya NES ROM’u değil, 2 çekirdek başlamadı, 3 dosya okunamadı veya 4 MiB’dan büyük, aralık dışı sıra numarası da 3. Zaten çalışıyorsa önce kaydı yazar ve durdurur. |
| `nesSetButton(name, down): void` | Değişmez. |
| `nesStop(): void` | Kaydı yazar, sonra eskisi gibi kapatır. |

`nesPlay` artık bir argüman alır; `src/nes-host.d.ts`, `scripts/nes-host-plugin.mjs` ve `nes_host.h` buna göre güncellenir.

## Kabuk ekranı

- Üstte başlık “Opengameconsole” ve sağında “Yenile” düğmesi.
- Altında kaydırılabilir oyun listesi. Her satır bir düğmedir, görünen adı gösterir; dokunmak `nesPlay(index)` çağırır.
- Liste uygulama açılınca, Yenile’ye basınca ve Geri ile kabuğa dönünce yenilenir.
- Liste yalnızca pad demodan oluşuyorsa altında ipucu: “ROM’ları ~/Documents/NES veya ~/Downloads klasörüne koy”.
- Hata satırı listenin altında kalır. Metinler: 1 “Bu dosya bir NES ROM’u değil”, 2 “NES başlamadı”, 3 “Dosya okunamadı”.

Oyun ekranı ve tuş bandı değişmez.

## Hatalar

- Klasör yok veya izin yok: o klasör boş sayılır, liste yine pad demoyu gösterir.
- Liste yenilendikten sonra dosya silinmişse `nesPlay` 3 döner.
- Kayıt okunamaz veya boyutu uymazsa oyun kayıtsız başlar; eski dosya silinmez, ilk yazmada üzerine yazılır.
- Kayıt yazılamazsa (`$HOME` yok, disk dolu) hata stderr’e yazılır, oyun sürer.
- Tarayıcıda (`gea dev`) host çağrıları yoktur; orada yalnızca yerleşim denenir.

## Test

- `scripts/nes-host-plugin.test.mjs`: beş host çağrısının C++ karşılıkları.
- `native/tests/rom_library_test.cpp`: geçici bir `HOME` altında
  - `.nes` / `.NES` alınır, `.txt` ve alt klasördeki `.nes` alınmaz;
  - sıralama ve ` (Downloads)` eki;
  - `Documents/NES` yoksa oluşturulur;
  - 4 MiB’dan büyük dosya `readRom` ile reddedilir;
  - `saveFileName` güvenli ad ve bilinen bir CRC32 değeri üretir;
  - `writeSave` sonrası `readSave` aynı baytları verir, yanlış boyut reddedilir.
- Bu test Sailfish SDK derleme kabuğunda i486 hedefiyle derlenip çalıştırılır; bir PowerShell betiği bunu tek komuta bağlar.
- Telefonda: lisansı serbest bir homebrew `.nes` dosyası `~/Documents/NES` içine `scp` ile kopyalanır, listede görünür ve oynanır. SRAM kullanan bir oyun bulunursa Geri sonrası yeniden açıldığında ilerleme durur.

## Kapsam dışı

Alt klasörler, zip/7z, anlık kayıt, oyun kapağı görselleri, son oynananlar, arama, başka sistemler, uygulama içinden ROM indirme.
