# Sailfish NES konsolu

Tarih: 2026-10-05

## Amaç

aarch64 Sailfish telefonda açılan tek bir native uygulama. İlk sürüm bir kabuk ve bir NES oyunudur. Oyuncu Oyna der, ekran tuşlarıyla oynar, Geri ile kabuğa döner.

## Kilitlenen kararlar

- Hedef: Sailfish OS 5.1 aarch64 telefon. Derleme bu Windows makinesinde Sailfish SDK ile yapılır.
- Linux hedefi: `vendor/geastack-linux`, dal `feat/sailfishos-target-fix`.
- Kontrol: yalnızca dokunmatik. Bluetooth kol yok.
- Yerleşim: telefon dikey. Oyun üst bantta, tuşlar alt bantta.
- İlk sistem: NES. SNES, GBA ve MAME bu sürümde yok.
- Çekirdek: FCEUmm, libretro API’si, RPM içine statik bağlanır. `dlopen` yok.
- ROM: depoya koyduğumuz kendi pad denememiz. Nintendo ROM’u yok. Klasör taraması yok.
- Kurulum: RPM, `scp` ile `defaultuser@192.168.1.220` ev dizinine gider, `ssh` üzerinde `pkcon install-local` ile kurulur. Parola depoya yazılmaz.

## Mimari

Tek süreç, tek RPM: `harbour-gea-opengameconsole`.

Üç parça:

1. Gea kabuğu, TypeScript. İki ekran: kabuk ve oyun.
2. `native/nes_host.cpp`. Dört host çağrısını karşılar ve FCEUmm’u çalıştırır.
3. `native/fceumm/`. libretro-fceumm kaynakları. Üst commit `native/fceumm/UPSTREAM.txt` içine tam git hash ve `https://github.com/libretro/libretro-fceumm` adresi olarak yazılır ve o hash’te donar.

Host çağrıları bir geatsc eklentisiyle tanımlanır: `scripts/nes-host-plugin.mjs`. `package.json` içindeki `gea.compilerPlugins` bu dosyayı listeler. `vendor/geastack-linux/targets/sailfish-os/prepare.mjs` listedeki her eklentiyi `--extra-geatsc-plugin` ile derleyiciye iletir. `build-sailfish-os.ps1` aynı `prepare.mjs` betiğini çağırır.

FCEUmm, uygulamanın kendi kaynaklarıyla aynı include yoluna giremez: `src/input.h`, `src/video.h` ve `src/file.h` Gea başlıklarını gölgeler. Bu yüzden `gea.sailfish.staticLibraries` adında yeni bir alan eklenir. Her kayıt kendi include yoluyla ayrı bir CMake statik kütüphanesi olur ve uygulamaya bağlanır. Yalnızca `publicIncludePaths` uygulamaya görünür; FCEUmm için bu `libretro.h`’nin durduğu `libretro-common/include` dizinidir.

Linux hedefindeki değişiklikler kullanıcının çatalında, `feat/sailfishos-target-fix` dalında yerel commit olarak durur. İzin alınmadan push edilmez.

FCEUmm GPL-2.0’dır. Bu çekirdeği bağlayan RPM GPL-2.0 olarak dağıtılır. Gea kabuğunun ve pad ROM’unun kaynak lisansı MIT kalır. `native/fceumm/` içinde upstream `COPYING` durur.

## Ekranlar

`gea.sailfish.devicePixelRatio` 2’dir. Bir CSS pikseli iki tuval pikselidir; böylece tuşlar 720 ve 1080 piksel genişliğindeki telefonlarda benzer boyda kalır. NES çizimi CSS’e bakmaz, tuval pikselleriyle hesaplanır.

Pencere yüksekliğinin üst %62’si oyun bandıdır. Alt %38’i tuş bandıdır. Her iki bant da pencere genişliğinin tamamını kullanır.

Kabuk ekranında oyunun adı “Pad Demo” ve Oyna düğmesi vardır. Oyun bandı boş ve koyudur. Tuş bandı gizlidir.

Oyun ekranında üst bant NES görüntüsüne ayrılır. Alt bandın ilk satırı Geri düğmesidir. Onun altında soldan sağa yön tuşu, B, A, Start ve Select durur. Parmak oyun bandına değince tuş sayılmaz.

NES görüntüsü 256×240’tır. Üst banda sığan en büyük tam sayı ölçekle ortalanır. Kalan kenarlar siyahtır. Ölçek 1’in altına düşmez; bant bundan dar ise görüntü bandın içinde kesilir ve yine ortalanır.

## Host çağrıları

Kabuk bunları çağırır. NES karelerini kabuk değil, aşağıdaki çizim kancası üretir.

| Çağrı | Anlam |
| --- | --- |
| `nesPlay(): number` | Gömülü ROM’u yükler ve çekirdeği başlatır. 0 başarı, 1 ROM bozuk, 2 çekirdek başlamadı. Zaten çalışıyorsa önce durdurur, sonra yükler. |
| `nesSetButton(name: string, down: number): void` | `name` şunlardan biridir: `up`, `down`, `left`, `right`, `a`, `b`, `start`, `select`. `down` 1 basılı, 0 bırakılmış. Tanınmayan ad yok sayılır. Oyun yokken yok sayılır. |
| `nesStop(): void` | Çekirdeği kapatır ve basılı tuşları bırakır. Oyun yokken bir şey yapmaz. |

Ses, çekirdeğin bildirdiği örnek hızında Sailfish sesine yazılır. Cihaz o hızı kabul etmezse veya ses cihazı açılmazsa görüntü ve tuşlar sürer, oyun sessiz kalır. `gea.sailfish.permissions` listesi `Audio` içerir.

## Çizim kancası

`sailfish_main.cpp`, `Application::frame` ile `tree.refresh` arasında zayıf bir sembol çağırır: `int gea_app_before_refresh(void)`. Varsayılan gövde 0 döner.

Oyun çalışırken `nes_host.cpp` içindeki güçlü tanım şunu yapar:

1. Oyun başladığından beri geçen süreye göre sırası gelen NES karelerini `retro_run` ile üretir. Bir döngü turunda en çok iki kare üretir; daha fazla geride kalırsa aradaki kareler atlanır. 90 veya 120 Hz ekranda NES yine saniyede yaklaşık 60 kare koşar.
2. Yeni kare varsa 256×240 RGB565 görüntüyü üst %62 banda, yukarıdaki ölçek kuralıyla `sailfish_display_write_rgb565` üzerinden tuvale yazar ve 1 döner. Alt %38’e dokunmaz.

Döngü, kanca 1 döndüyse ve `tree.refresh` o turda ekranı sunmadıysa `sailfish_display_present()` çağırır. Bunu `sailfish_display_present_count()` sayacından anlar. Böylece bir turda iki kez vsync beklenmez. Oyun kapalıyken kanca 0 döner ve döngü eskisi gibidir.

Görüntü tek kopyadır: çekirdeğin RGB565 karesi `nes_host.cpp` içinde tutulur, ölçeklenmiş hali doğrudan tuvale gider. Gea oyun bandını kendisi yeniden çizerse (ekran geçişi gibi) bant bir sonraki NES karesine kadar, yani en çok bir kare, koyu kalır.

## Bileşenler

- `src/index.tsx`, `src/styles.css` — kabuk ve oyun ekranı.
- `src/nes-host.d.ts` — üç host çağrısının TypeScript bildirimi.
- `scripts/nes-host-plugin.mjs` — üç host çağrısının C++ karşılığı.
- `native/nes_host.cpp`, `native/nes_host.h` — libretro yaşam döngüsü, tuşlar, ses, çizim kancası.
- `native/fceumm/` — vendor çekirdek ve `UPSTREAM.txt`, `COPYING`.
- `scripts/nes/pad-demo.mjs` — pad ROM’unun kaynağı: küçük bir 6502 assembler’ı ve programın kendisi.
- `assets/nes/pad-demo.nes`, `native/pad_demo_rom.c` — o kaynaktan üretilen NROM-128 dosyası ve aynı baytların C dizisi.
- `assets/fonts/Inter-Regular.ttf` — `styles.css` bunu bekler. OFL lisansı yanında durur.
- `assets/icon.png` — 512×512. Sailfish hazırlığı ikon ister.
- `package.json` içinde `gea.compilerPlugins` ve `gea.sailfish`: `organizationName` `org.opengameconsole`, `applicationName` `opengameconsole`, `devicePixelRatio` 2, `permissions` `["Audio"]`, `nativeSources` host dosyaları, `staticLibraries` FCEUmm.

Pad ROM’u uygulama sahibidir. Açık gri zemin, yön tuşuyla hareket eden 16×16 bir kare. A kareyi kırmızı, B yeşil yapar. Start veya Select zemini açık gri ile açık mavi arasında değiştirir. Nintendo kodu, karakteri veya müziği içermez. Baytlar derlemede host’un okuduğu bir diziye gömülür. Telefonda dosya aranmaz.

## Hatalar

Kabuk oyun ekranına geçmeden mesajı kendi ekranında tutar.

- `nesPlay` 1 dönerse metin “ROM açılamadı”.
- `nesPlay` 2 dönerse metin “NES başlamadı”.
- Çekirdek bir turda kare vermezse üst bantta bir önceki kare kalır. Hiç kare yoksa bant koyu kalır.
- `nesStop` oyun yokken sessizdir.
- Tarayıcıda (`gea dev`) host çağrıları yoktur; orada yalnızca yerleşim denenir.
- Ses arızası oyunu durdurmaz.

SSH kapalıysa veya `scp` koparsa RPM yerelde kalır. Kurulum başlamış sayılmaz. Hata, `scp` veya `ssh` çıktısındadır.

## Derleme ve kurulum

SDK `C:\SailfishOS` altındadır. Komut, uygulama kökünden:

```powershell
.\node_modules\@geastack\linux\targets\sailfish-os\build-sailfish-os.ps1 -AppDirectory . -Architecture aarch64
```

RPM yolu: `.gea-sailfish/build/opengameconsole-aarch64/project/RPMS/` altında `harbour-gea-opengameconsole` paketidir.

Kurulum, o RPM dosyası `$rpm` iken:

```powershell
scp $rpm defaultuser@192.168.1.220:~/
ssh defaultuser@192.168.1.220 pkcon install-local ~/<rpm-dosya-adı>
```

PackageKit onay sorarsa cevap o SSH oturumunda verilir.

`npm run sailfish:build` derleme komutunu, `npm run sailfish:deploy` bu iki kurulum komutunu çalıştırır. Deploy betiği en yeni aarch64 RPM’i seçer ve `ssh -t` kullanır ki onay veya parola istemi aynı terminalde görünsün.

Telefon adresi `192.168.1.220`, kullanıcı `defaultuser`. Geliştirici kipinde SSH açıktır. Kimlik, makinede kurulu SSH anahtarı veya etkileşimli oturumdur. Parola, betik ve depoya yazılmaz.

## Telefonda deneme

1. RPM kurulur ve uygulama açılır. Kabukta “Pad Demo” ve Oyna görünür.
2. Oyna, oyunu açar. Kare görünür ve yön tuşuyla hareket eder.
3. B, A, Start ve Select ayrı ayrı basılır. ROM’daki renk ve işaret değişir.
4. Oyun bandına basmak kareyi hareket ettirmez.
5. Geri, kabuğa döner. Oyna aynı ROM’u yeniden açar.
6. Ses cihazı yoksa adım 2 ve 3 yine çalışır.

## Kapsam dışı

SNES, GBA, MAME, ROM klasörü, kayıt durumu, yatay mod, Bluetooth kol, Harbour mağazası, Raspberry Pi derlemesi.
