# Інструкція: збірка Release та прошивка СКМ‑8 через MSP‑FET / MSP430Flasher

## 1. Призначення

Ця інструкція описує штатний процес:

1. підготовки версії firmware СКМ‑8;
2. збірки Release для **Slot A** і **Slot B**;
3. формування OTA-пакета (`slot_a.ota`, `slot_b.ota`, `*_commands.txt`, `*_flasher.txt`, `manifest.json`, `checksums.sha256`);
4. перевірки результату збірки;
5. резервного копіювання контролера;
6. прошивки через **MSP‑FET** за допомогою **MSP430Flasher**;
7. контрольного читання службових областей після прошивки.

Перевірена конфігурація:

- MCU: **MSP430FR5949**
- CCS: `/home/skydom/ti/ccs2100`
- compiler: `ti-cgt-msp430_21.6.2.LTS`
- `cl430`: `/home/skydom/ti/ccs2100/ccs/tools/compiler/ti-cgt-msp430_21.6.2.LTS/bin/cl430`
- `hex430`: `/home/skydom/ti/ccs2100/ccs/tools/compiler/ti-cgt-msp430_21.6.2.LTS/bin/hex430`
- MSP430Flasher: `/home/skydom/ti/MSPFlasher_1.3.20/MSP430Flasher`
- workspace: `~/workspace_ccstheia/skm-8.7.x_GRP`

---

## 2. Підготовка до Release-збірки

```bash
cd ~/workspace_ccstheia/skm-8.7.x_GRP

git status
git diff
```

Перед Release бажано переконатися, що всі потрібні зміни firmware збережені та зрозумілі.

---

## 3. Підняття версії firmware

Версія зберігається у `firmware_version.h`.

Приклад для `8.78.2`:

```c
#define FW_VERSION_MAJOR        8
#define FW_VERSION_MINOR        78
#define FW_VERSION_PATCH        2

#define FW_VERSION_STRING       "8.78.2"
#define FW_VERSION_DISPLAY      "8.78.2s    >"
#define FW_PRODUCT_STRING       "СКМ-8.78.2.ШГРП"
```

Перед зміною версії:

```bash
cp -a firmware_version.h firmware_version.h.before-NEW_VERSION
```

> Важливо: `tools/ota/build_release.sh` виконує `rm -rf dist/SKM8-${VERSION}`. Не збирати новий код під уже перевіреною production-версією, якщо старий пакет треба зберегти незмінним.

---

## 4. Штатна Release-збірка

Основний скрипт:

```text
tools/ota/build_release.sh
```

Запуск:

```bash
cd ~/workspace_ccstheia/skm-8.7.x_GRP

bash tools/ota/build_release.sh \
  2>&1 | tee Release/build_release_<VERSION>.log
```

Наприклад:

```bash
bash tools/ota/build_release.sh \
  2>&1 | tee Release/build_release_8.78.2.log
```

Скрипт автоматично:

- визначає версію з `firmware_version.h`;
- видаляє та заново створює `dist/SKM8-<VERSION>`;
- виконує `make clean`;
- збирає Slot A;
- перевіряє версію в `.out`;
- формує Slot A OTA image;
- запускає `tools/ota/build_slot_b_ota.sh`;
- збирає Slot B через `lnk_msp430fr5949_slot_b.cmd`;
- формує обидва OTA images;
- генерує `*_commands.txt` і `*_flasher.txt`;
- створює `manifest.json` та `checksums.sha256`.

---

## 5. Конфігурація Release

Штатна Release-збірка використовує:

```text
MSP430FR5949
-vmspx
--data_model=restricted
-O2
--heap_size=160
--stack_size=160
```

Linker scripts:

```text
Slot A: lnk_msp430fr5949_slot_a.cmd
Slot B: lnk_msp430fr5949_slot_b.cmd
```

---

## 6. Перевірка результату Release-збірки

Після успішного завершення має бути:

```text
RELEASE READY
```

Перевірка логу:

```bash
tail -n 120 Release/build_release_<VERSION>.log
```

Перевірка каталогу:

```bash
ls -lh dist/SKM8-<VERSION>
```

Очікувані файли:

```text
slot_a.ota
slot_b.ota
slot_a_flasher.txt
slot_b_flasher.txt
slot_a_commands.txt
slot_b_commands.txt
manifest.json
checksums.sha256
RELEASE_NOTES.md
```

Перевірка SHA256:

```bash
cd ~/workspace_ccstheia/skm-8.7.x_GRP/dist/SKM8-<VERSION>
sha256sum -c checksums.sha256
```

Для кожного файла має бути `Гаразд` / `OK`.

---

## 7. Перевірка OTA START-команд

```bash
head -n 2 slot_a_commands.txt
head -n 2 slot_b_commands.txt
```

Формат START:

```text
ou=1,30440,<TRANSPORT_CRC32>;
```

Формат chunk:

```text
oc=0,<CRC16>,<128 HEX characters>;
```

Для поточної архітектури:

```text
OTA image size = 30440 bytes
chunk size     = 64 bytes
```

---

## 8. Перевірка розмірів

```bash
wc -c \
  slot_a.ota \
  slot_b.ota \
  slot_a_commands.txt \
  slot_b_commands.txt \
  slot_a_flasher.txt \
  slot_b_flasher.txt
```

Для перевіреної конфігурації:

```text
slot_a.ota       30440
slot_b.ota       30440
commands         ~68354 bytes
flasher          ~91341 bytes
```

---

## 9. MSP430Flasher — підготовка

```bash
FLASHER="/home/skydom/ti/MSPFlasher_1.3.20/MSP430Flasher"
export LD_LIBRARY_PATH="/home/skydom/ti/MSPFlasher_1.3.20:${LD_LIBRARY_PATH:-}"
```

Перевірка:

```bash
"$FLASHER" --help
```

Для MSP‑FET на `ttyACM0`:

```text
-i ttyACM0
-n MSP430FR5949
-a
```

---

## 10. Резервна копія перед прошивкою

```bash
cd ~/workspace_ccstheia/skm-8.7.x_GRP
mkdir -p controller-backup/<CONTROLLER>-before-<VERSION>
```

Повний dump FRAM:

```bash
"$FLASHER" \
  -i ttyACM0 \
  -n MSP430FR5949 \
  -a \
  -r [controller-backup/<CONTROLLER>-before-<VERSION>/full_fram.txt,0x4400-0x13FFF]
```

Shared area + OTA journal:

```bash
"$FLASHER" \
  -i ttyACM0 \
  -n MSP430FR5949 \
  -a \
  -r [controller-backup/<CONTROLLER>-before-<VERSION>/shared_and_journal.txt,0x4400-0x46FF]
```

Boot metadata:

```bash
"$FLASHER" \
  -i ttyACM0 \
  -n MSP430FR5949 \
  -a \
  -r [controller-backup/<CONTROLLER>-before-<VERSION>/boot_meta.txt,0x4700-0x471F]
```

---

## 11. Перевірка адрес у flasher-файлі

Slot A:

```bash
grep -n '^@' dist/SKM8-<VERSION>/slot_a_flasher.txt
```

Slot B:

```bash
grep -n '^@' dist/SKM8-<VERSION>/slot_b_flasher.txt
```

Для перевіреного Slot A application image очікується:

```text
@5000
@5100
@10000
```

Якщо присутні службові області, наприклад `@4400`, `@46C0`, `@4700`, `@4800`, не прошивати файл до з'ясування його призначення.

---

## 12. Прошивка application Slot A без стирання службових областей

Якщо `slot_a_flasher.txt` містить тільки application-регіони:

```bash
cd ~/workspace_ccstheia/skm-8.7.x_GRP

"$FLASHER" \
  -i ttyACM0 \
  -n MSP430FR5949 \
  -a \
  -e NO_ERASE \
  -w dist/SKM8-<VERSION>/slot_a_flasher.txt \
  -v
```

`NO_ERASE` потрібен, щоб не стерти shared configuration, OTA journal, boot metadata та інші службові дані FRAM.

---

## 13. Контроль після прошивки

Boot metadata після прошивки:

```bash
"$FLASHER" \
  -i ttyACM0 \
  -n MSP430FR5949 \
  -a \
  -r [controller-backup/<CONTROLLER>-before-<VERSION>/boot_meta_after.txt,0x4700-0x471F]
```

OTA journal після прошивки:

```bash
"$FLASHER" \
  -i ttyACM0 \
  -n MSP430FR5949 \
  -a \
  -r [controller-backup/<CONTROLLER>-before-<VERSION>/journal_after.txt,0x46C0-0x46FF]
```

Порівняти boot metadata:

```bash
diff -u \
  controller-backup/<CONTROLLER>-before-<VERSION>/boot_meta.txt \
  controller-backup/<CONTROLLER>-before-<VERSION>/boot_meta_after.txt || true
```

Для application-only flashing boot metadata не повинна змінитися.

---

## 14. Читання окремих областей

Boot metadata:

```bash
"$FLASHER" -i ttyACM0 -n MSP430FR5949 -a \
  -r [boot_meta.txt,0x4700-0x471F]
```

OTA journal:

```bash
"$FLASHER" -i ttyACM0 -n MSP430FR5949 -a \
  -r [ota_journal.txt,0x46C0-0x46FF]
```

Application header Slot A:

```bash
"$FLASHER" -i ttyACM0 -n MSP430FR5949 -a \
  -r [slot_a_header.txt,0x5000-0x502B]
```

---

## 15. Повна factory/system прошивка

Factory/system image відрізняється від application-only flashing.

Для вже підготовленого factory/system TI-TXT:

```bash
"$FLASHER" \
  -i ttyACM0 \
  -n MSP430FR5949 \
  -a \
  -e NO_ERASE \
  -w Release/SKM8-<VERSION>_factory_system_A.txt \
  -v
```

Перед записом обов'язково:

```bash
grep '^@' Release/SKM8-<VERSION>_factory_system_A.txt
```

Factory/system image може включати boot metadata, bootloader та інші системні секції. Не плутати його зі звичайним `slot_a_flasher.txt`.

---

## 16. Формування raw Slot A TI-TXT

```bash
HEX430="/home/skydom/ti/ccs2100/ccs/tools/compiler/ti-cgt-msp430_21.6.2.LTS/bin/hex430"

"$HEX430" \
  --ti_txt \
  --memwidth=8 \
  --romwidth=8 \
  --outfile=Release/SKM8-<VERSION>_raw_slot_a.txt \
  --exclude=.TI.persistent \
  Release/skm-8.7.x_GRP.out
```

Factory/system image слід формувати лише за перевіреним шаблоном конкретної версії boot metadata.

---

## 17. Діагностика збірки

Toolchain:

```bash
grep -n 'CG_TOOL_ROOT' Release/makefile
```

Slot A linker:

```bash
grep -n 'lnk_msp430fr5949_slot_a.cmd' Release/makefile Release/subdir_vars.mk
```

Slot B linker:

```bash
grep -n 'lnk_msp430fr5949_slot_b.cmd' Release/makefile.slot_b
```

Версія у `.out`:

```bash
strings -a Release/skm-8.7.x_GRP.out | grep '8\.'
```

Warnings/errors:

```bash
grep -Ei 'error|warning' Release/build_release_<VERSION>.log
```

---

## 18. Типові linker warnings

У перевірених Release-збірках зустрічалося:

```text
warning #10068-D: no matching section
```

Такі warnings були й у робочих збірках Slot A/Slot B. Критичними є linker errors, відсутність `.out`, невідповідність версії, неправильний OTA size, AppHeader/CRC mismatch або помилки `sha256sum -c`.

---

## 19. Мінімальний чек-лист Release

```text
[ ] firmware_version.h оновлено
[ ] нова версія не перезаписує старий production dist
[ ] build_release.sh завершився RELEASE READY
[ ] Slot A version verified
[ ] Slot B version verified
[ ] slot_a.ota = 30440 bytes
[ ] slot_b.ota = 30440 bytes
[ ] manifest.json коректний
[ ] sha256sum -c checksums.sha256 = OK
[ ] ou CRC32 у commands відповідає manifest
[ ] flasher address records перевірені
[ ] зроблено backup контролера
[ ] після flashing перевірено boot_meta / OTA journal
```

---

## 20. Коротка штатна процедура

### Release build

```bash
cd ~/workspace_ccstheia/skm-8.7.x_GRP

bash tools/ota/build_release.sh \
  2>&1 | tee Release/build_release_<VERSION>.log

cd dist/SKM8-<VERSION>
sha256sum -c checksums.sha256
```

### Backup controller

```bash
FLASHER="/home/skydom/ti/MSPFlasher_1.3.20/MSP430Flasher"
export LD_LIBRARY_PATH="/home/skydom/ti/MSPFlasher_1.3.20:${LD_LIBRARY_PATH:-}"

"$FLASHER" \
  -i ttyACM0 \
  -n MSP430FR5949 \
  -a \
  -r [backup.txt,0x4400-0x13FFF]
```

### Application Slot A flashing

```bash
"$FLASHER" \
  -i ttyACM0 \
  -n MSP430FR5949 \
  -a \
  -e NO_ERASE \
  -w dist/SKM8-<VERSION>/slot_a_flasher.txt \
  -v
```

### Post-flash control

```bash
"$FLASHER" \
  -i ttyACM0 \
  -n MSP430FR5949 \
  -a \
  -r [boot_meta_after.txt,0x4700-0x471F]
```

---

## Примітка

Процедура сформована за реально перевіреним процесом збірки SKM‑8 8.78.1/8.78.2 та роботою з MSP430FR5949 через MSP‑FET / MSP430Flasher 1.3.20.
