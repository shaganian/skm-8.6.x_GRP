#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* GENERATED_HEADERS */

_Static_assert(sizeof(AppHeader) == 44, "Target AppHeader ABI");
_Static_assert(sizeof(BootMeta) == 14, "Target BootMeta ABI");

static union { uint32_t alignment; uint8_t bytes[0x14010]; } memory;
#define fram memory.bytes
static uint32_t test_header_address;
static uint32_t fail_address = UINT32_MAX;
static unsigned writes, image_writes, resets;
static unsigned drop_from_write = UINT32_MAX;
static uint16_t sr = 8;
static uint16_t MPUSAM = 0x1553;
static union { uint16_t word; struct { uint8_t low, high; } byte; } mpu;
#define MPUCTL0 mpu.word
#define MPUCTL0_L mpu.byte.low
#define MPUCTL0_H mpu.byte.high
#define MPULOCK 2u
#define MPUSEG3WE 0x0200u
#define MPUPW 0xA500u
#define GIE 8u
#define OTA_FORMAT_VERSION 1u
static uint8_t encrdata[160];
static uint16_t pack_time;

static BootMeta *mock_meta(void) { return (BootMeta *)(fram + BOOT_META_ADDR); }
static AppHeader *mock_header(uint32_t address)
{
    assert(address + sizeof(AppHeader) <= sizeof(fram));
    return (AppHeader *)(fram + address);
}
#define g_app_header (*mock_header(test_header_address))
static uint8_t __data20_read_char(uint32_t address)
{
    assert(address < sizeof(fram));
    return fram[address];
}
static void __data20_write_char(uint32_t address, uint8_t value)
{
    assert(address < sizeof(fram));
    writes++;
    assert((address >= BOOT_META_ADDR && address < BOOT_META_ADDR + sizeof(BootMeta)) ||
           (address >= 0x46C0 && address < 0x4700) ||
           (test_header_address == 0x5000 &&
            ((address >= 0xA7C0 && address < 0xA7EC) ||
             (address >= 0xA8C0 && address < 0xFF80) ||
             (address >= 0x11FFC && address < 0x13FF8))) ||
           (test_header_address == 0xA7C0 &&
            ((address >= 0x5000 && address < 0x502C) ||
             (address >= 0x5100 && address < 0xA7C0) ||
             (address >= 0x10000 && address < 0x11FFC))));
    if (address >= 0x5000) {
        assert((MPUSAM & MPUSEG3WE) && !(sr & GIE));
        image_writes++;
    }
    if (address != fail_address && writes < drop_from_write) fram[address] = value;
}
static uint16_t __get_SR_register(void) { return sr; }
static void __disable_interrupt(void) { sr &= ~GIE; }
static void __enable_interrupt(void) { sr |= GIE; }
static void ota_software_reset(void) { resets++; }

/* GENERATED_PRODUCTION_CODE */

static uint8_t image[30440];
static char commands[478][162];

static void fresh(uint8_t slot)
{
    memset(fram, 0xFF, sizeof(fram));
    test_header_address = slot == BOOT_SLOT_A ? 0x5000 : 0xA7C0;
    g_app_header.magic = APP_HEADER_MAGIC;
    g_app_header.format_version = APP_HEADER_FORMAT_VERSION_V2;
    g_app_header.image_size = APP_IMAGE_PAYLOAD_SIZE;
    g_app_header.entry = slot == BOOT_SLOT_A ? 0x5100 : 0xA8C0;
    g_app_header.isr_port1 = g_app_header.entry;
    g_app_header.isr_port2 = g_app_header.entry;
    g_app_header.isr_port3 = g_app_header.entry;
    g_app_header.isr_wdt = g_app_header.entry;
    g_app_header.isr_usci_a0 = g_app_header.entry;
    g_app_header.isr_usci_b0 = g_app_header.entry;
    g_app_header.isr_adc12 = g_app_header.entry;
    uint32_t crc = ota_crc32_region20(0xFFFFFFFFu,
        slot == BOOT_SLOT_A ? 0x5100 : 0xA8C0, 0x56C0);
    g_app_header.image_crc32 = ota_crc32_region20(crc,
        slot == BOOT_SLOT_A ? 0x10000 : 0x11FFC, 0x1FFC) ^ 0xFFFFFFFFu;
    memset(&ota_rx, 0, sizeof(ota_rx));
    ota_resume_checked = 0;
    writes = image_writes = resets = 0;
    fail_address = UINT32_MAX;
    drop_from_write = UINT32_MAX;
    sr = GIE;
    MPUSAM = 0x1553;
    MPUCTL0 = 0;
}

static char request_result(const char *command)
{
    size_t size = strlen(command);
    assert(size <= sizeof(encrdata));
    memset(encrdata, ' ', sizeof(encrdata));
    memcpy(encrdata, command, size);
    return ota_remote_command();
}

static void request(const char *command)
{
    assert(request_result(command) == 1);
}

static void init_cases(void)
{
    for (uint8_t slot = 0; slot < 2; slot++) {
        fresh(slot);
        assert(boot_prepare_ota());
        assert(mock_meta()->active_slot == slot);
        assert(mock_meta()->pending_slot == BOOT_SLOT_NONE);
        assert(mock_meta()->magic == BOOT_META_MAGIC);
        assert(mock_meta()->sequence == 0);
        BootMeta before = *mock_meta();
        unsigned count = writes;
        assert(boot_prepare_ota());
        assert(writes == count && !memcmp(&before, mock_meta(), sizeof(before)));
        assert(boot_set_pending(slot ^ 1));
        assert(mock_meta()->pending_slot == (slot ^ 1));
        before = *mock_meta(); count = writes;
        assert(boot_set_pending(slot ^ 1));
        assert(writes == count && !memcmp(&before, mock_meta(), sizeof(before)));
        assert(!boot_prepare_ota());
        assert(writes == count);
    }
    puts("PASS: erased A/B initialization; valid metadata unchanged; pending idempotent");

    for (int fault = 0; fault < 9; fault++) {
        fresh(BOOT_SLOT_A);
        assert(boot_prepare_ota());
        switch (fault) {
        case 0: mock_meta()->magic ^= 1; break;
        case 1: mock_meta()->format_version++; break;
        case 2: mock_meta()->active_slot = BOOT_SLOT_B; break;
        case 3: mock_meta()->pending_slot = BOOT_SLOT_A; break;
        case 4: mock_meta()->flags = BOOT_FLAG_TRIAL_STARTED; break;
        case 5: mock_meta()->reserved0 = 1; break;
        case 6: mock_meta()->pending_slot = 7; break;
        case 7: memset(mock_meta(), 0, sizeof(BootMeta)); break;
        case 8: memset(mock_meta(), 0xFF, sizeof(BootMeta)); mock_meta()->sequence = 0; break;
        }
        BootMeta before = *mock_meta();
        unsigned count = writes;
        assert(!boot_prepare_ota());
        assert(!boot_set_pending(BOOT_SLOT_B));
        request(commands[0]);
        assert(ota_rx.status == OTA_STATUS_ERROR && !ota_rx.active);
        assert(writes == count && image_writes == 0);
        assert(!memcmp(&before, mock_meta(), sizeof(before)));
    }
    puts("PASS: corrupt/inconsistent/trial/zero/torn BootMeta rejected before START without writes");

    for (int fault = 0; fault < 9; fault++) {
        fresh(BOOT_SLOT_A);
        switch (fault) {
        case 0: test_header_address = 0x5100; break;
        case 1: g_app_header.magic = 0; break;
        case 2: g_app_header.format_version = 1; break;
        case 3: g_app_header.entry = 0xA8C0; break;
        case 4: g_app_header.image_size = 0; break;
        case 5: fram[0x5100] ^= 1; break;
        case 6: g_app_header.image_crc32 ^= 1; break;
        case 7: g_app_header.isr_port1 = 0xA8C0; break;
        case 8: g_app_header.entry |= 1; break;
        }
        assert(!boot_prepare_ota() && writes == 0);
    }
    /* Every non-erased byte prevents initialization; no partial record repair. */
    for (unsigned byte = 0; byte < sizeof(BootMeta); byte++) {
        fresh(BOOT_SLOT_A);
        fram[BOOT_META_ADDR + byte] = 0;
        assert(!boot_prepare_ota() && writes == 0);
    }
    /* Interrupt each initialization write, including both magic bytes. */
    for (unsigned cut = 1; cut <= 16; cut++) {
        fresh(BOOT_SLOT_A);
        drop_from_write = cut;
        assert(!boot_prepare_ota());
        assert(mock_meta()->magic != BOOT_META_MAGIC);
    }
    puts("PASS: exact-erased bytes; torn writes/magic-last; current image CRC/vector validation");
    fresh(BOOT_SLOT_A);
    fail_address = BOOT_META_ADDR + 2;
    assert(!boot_prepare_ota());
    assert(mock_meta()->magic != BOOT_META_MAGIC);
    unsigned count = writes;
    fail_address = UINT32_MAX;
    assert(!boot_prepare_ota() && writes == count);
    fresh(BOOT_SLOT_A);
    assert(!boot_set_pending(BOOT_SLOT_B) && writes == 0);
    assert(boot_prepare_ota());
    fail_address = BOOT_META_ADDR + offsetof(BootMeta, pending_slot);
    assert(!boot_set_pending(BOOT_SLOT_B));
    assert(mock_meta()->pending_slot == BOOT_SLOT_NONE);
    puts("PASS: ambiguous header; initialization/pending read-back failure; FINISH cannot initialize");
}

static void transfer(void)
{
    fresh(BOOT_SLOT_A);
    request(commands[0]);
    assert(ota_rx.status == OTA_STATUS_STARTED && ota_rx.received == 0);
    for (unsigned i = 1; i <= 476; i++) {
        request(commands[i]);
        assert(ota_rx.status == OTA_STATUS_CHUNK_OK);
        assert(ota_rx.received == (i < 476 ? i * 64 : 30440));
    }
    assert(image_writes == 30440);
    assert(ota_rx.running_crc32 == (0xBD6F8B6Eu ^ 0xFFFFFFFFu));
    assert(!memcmp(fram + 0xA7C0, image, 44));
    assert(!memcmp(fram + 0xA8C0, image + 44, 0x56C0));
    assert(!memcmp(fram + 0x11FFC, image + 44 + 0x56C0, 0x1FFC));
    for (unsigned i = 0x13FF8; i < sizeof(fram); i++) assert(fram[i] == 0xFF);
    assert(MPUSAM == 0x1553 && sr == GIE);
}

static void finalization_cases(void)
{
    transfer();
    unsigned count = writes;
    uint32_t crc = ota_rx.running_crc32;
    request(commands[476]); /* Last 40 bytes repeated after a lost ACK. */
    assert(writes == count && ota_rx.running_crc32 == crc);
    assert(ota_rx.received == 30440 && ota_rx.status == OTA_STATUS_CHUNK_OK);

    /* Power loss after final checkpoint, before FINISH; retain FRAM only. */
    memset(&ota_rx, 0, sizeof(ota_rx));
    ota_resume_checked = 0;
    assert(ota_get_status() == OTA_STATUS_CHUNK_OK);
    assert(ota_get_received() == 30440 && ota_rx.running_crc32 == crc);
    request(commands[0]); /* Idempotent START after restore. */
    assert(ota_rx.received == 30440 && writes == count);
    assert(ota_rx.running_crc32 == crc);

    /* Request carried CHUNK_OK; its response contains FINISH. */
    unsigned char sent_status = ota_get_status();
    request(commands[477]);
    assert(ota_rx.status == OTA_STATUS_READY && !ota_rx.active);
    assert(mock_meta()->pending_slot == BOOT_SLOT_B);
    assert(mock_meta()->active_slot == BOOT_SLOT_A);
    assert(pack_time == 3);
    ota_reset_after_response(sent_status, 1);
    assert(resets == 0);
    BootMeta before = *mock_meta(); count = writes;
    request(commands[477]);
    assert(ota_rx.status == OTA_STATUS_READY);
    assert(writes == count && !memcmp(&before, mock_meta(), sizeof(before)));
    ota_reset_after_response(OTA_STATUS_CHUNK_OK, 1);
    ota_reset_after_response(OTA_STATUS_READY, 0); /* Lost/malformed response. */
    assert(resets == 0);
    sent_status = ota_get_status(); /* Next request actually carries READY. */
    request(commands[477]); /* Server may repeat FINISH. */
    ota_reset_after_response(sent_status, 1);
    assert(resets == 1 && writes == count);
    puts("PASS: full 8.74 flow 475x64+40; exact FRAM/CRC; duplicate chunk; resume at 30440");
    puts("PASS: READY precedes reset; retries preserve READY; duplicate FINISH does not write");

    /* Real parser failures preserve READY and cannot authorize reset. */
    const char *bad[] = {"of=BD6F8B6F;", "of=BD6F8B6E", "of=nothex;",
                         "ou=1,30440,BD6F8B6E;", "oc=30400,garbage;"};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        char result = request_result(bad[i]);
        assert(result == 2 && ota_rx.status == OTA_STATUS_READY);
        ota_reset_after_response(OTA_STATUS_READY, result);
        assert(resets == 1 && writes == count);
        assert(!memcmp(&before, mock_meta(), sizeof(before)));
    }
    request(commands[477]);
    ota_reset_after_response(OTA_STATUS_READY, 1);
    assert(resets == 2 && writes == count);
    puts("PASS: malformed FINISH / unrelated OTA retry preserves READY and cannot reset");

    transfer();
    mock_meta()->magic = 0;
    before = *mock_meta(); count = writes;
    request(commands[477]);
    assert(ota_rx.status == OTA_STATUS_ERROR && ota_rx.received == 30440);
    assert(writes == count && !memcmp(&before, mock_meta(), sizeof(before)));
    puts("PASS: corruption at FINISH fails closed without metadata repair");

    transfer();
    mock_meta()->active_slot = BOOT_SLOT_B;
    memset(&ota_rx, 0, sizeof(ota_rx));
    ota_resume_checked = 0; count = writes;
    assert(ota_get_status() == OTA_STATUS_ERROR && !ota_rx.active);
    assert(writes == count);
    puts("PASS: resume cannot bypass BootMeta preflight");

    transfer();
    fram[0x13FF7] ^= 1;
    count = writes;
    request(commands[477]);
    assert(ota_rx.status == OTA_STATUS_ERROR && writes == count);
    assert(mock_meta()->pending_slot == BOOT_SLOT_NONE);
    puts("PASS: physical payload corruption rejected before pending");
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    FILE *file = fopen(argv[1], "rb");
    assert(file && fread(image, 1, sizeof(image), file) == sizeof(image));
    assert(fgetc(file) == EOF); fclose(file);
    file = fopen(argv[2], "r"); assert(file);
    for (unsigned i = 0; i < 478; i++) {
        assert(fgets(commands[i], sizeof(commands[i]), file));
        commands[i][strcspn(commands[i], "\r\n")] = 0;
    }
    assert(fgetc(file) == EOF); fclose(file);
    init_cases();
    finalization_cases();
    puts("ALL OFFLINE OTA TESTS PASS");
    return 0;
}
