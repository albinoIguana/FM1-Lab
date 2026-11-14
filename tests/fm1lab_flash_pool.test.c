#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../app/app.h"
#include "../app/fm1lab_store_library.h"

static uint8_t nor_flash[0x100000];
static int fail_after = -1;
static unsigned operations;

#include "../base/device/storage.c"

static int st_read(uint32_t off, void *dst, uint32_t n)
{
    if (off > sizeof nor_flash || n > sizeof nor_flash - off) return -1;
    memcpy(dst, nor_flash + off, n);
    return 0;
}
static int cut(void)
{
    ++operations;
    if (fail_after == 0) return 1;
    if (fail_after > 0) --fail_after;
    return 0;
}
static int st_erase(uint32_t off)
{
    if (cut()) return -9;
    if (off > sizeof nor_flash - 4096u || (off & 4095u)) return -1;
    memset(nor_flash + off, 0xFF, 4096u);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *p = (const uint8_t *)src;
    if (cut()) return -9;
    if (off > sizeof nor_flash || n > sizeof nor_flash - off ||
        ((off & 255u) + n > 256u)) return -1;
    for (uint32_t i = 0; i < n; ++i) nor_flash[off + i] &= p[i];
    return 0;
}

int plat_store_size(uint32_t obj) { return st_size(obj); }
int plat_store_load(uint32_t obj, void *dst, uint32_t max) { return st_load(obj, dst, max); }
int plat_store_save(uint32_t obj, const void *src, uint32_t len) { return st_save(obj, src, len); }
static void fill(uint8_t *p, uint32_t n, uint8_t seed)
{
    for (uint32_t i = 0; i < n; ++i) p[i] = (uint8_t)(seed + i * 13u);
}
int main(void)
{
    uint8_t meta[1742], bank[3752], got[3752], head[16];
    memset(nor_flash, 0xFF, sizeof nor_flash);
    fill(meta, sizeof meta, 0x11u);
    fill(bank, sizeof bank, 0x37u);

    assert(st_pool_scan == APP_STORE_POOL_N);
    assert(st_size(OBJ_INSERTS) < 0);
    assert(st_load(OBJ_INSERTS, got, sizeof got) < 0);
    assert(st_save(OBJ_INSERTS, meta, sizeof meta) == 0);
    assert(st_size(OBJ_INSERTS) == (int)sizeof meta);
    assert(st_load(OBJ_INSERTS, got, sizeof got) == (int)sizeof meta);
    assert(memcmp(got, meta, sizeof meta) == 0);
    assert(st_load_head(OBJ_INSERTS, head, sizeof head) == (int)sizeof head);
    assert(memcmp(head, meta, sizeof head) == 0);

    /* Actual FM1 object IDs share the 12-sector pool. Bank 0 and metadata
     * remain independently addressable even though they share flash sectors. */
    assert(st_save(OBJ_INSTRUMENTS0, bank, sizeof bank) == 0);
    assert(st_load(OBJ_INSTRUMENTS0, got, sizeof got) == (int)sizeof bank);
    assert(memcmp(got, bank, sizeof bank) == 0);
    uint8_t newer[3752];
    fill(newer, sizeof newer, 0xA3u);
    assert(st_save(OBJ_INSTRUMENTS0, newer, sizeof newer) == 0);
    assert(st_load(OBJ_INSTRUMENTS0, got, sizeof got) == (int)sizeof newer);
    assert(memcmp(got, newer, sizeof newer) == 0);
    assert(st_load_prev(OBJ_INSTRUMENTS0, got, sizeof got) == (int)sizeof bank);
    assert(memcmp(got, bank, sizeof bank) == 0);

    /* A corrupt newest payload must fall back to the older CRC-valid copy. */
    st_hdr_t newest;
    int current = st_current(OBJ_INSTRUMENTS0, &newest);
    assert(current >= 0);
    nor_flash[APP_STORE_POOL_BASE + (uint32_t)current * ST_SECTOR + ST_PAYLOAD_OFF + 17u] ^= 1u;
    assert(st_load(OBJ_INSTRUMENTS0, got, sizeof got) == (int)sizeof bank);
    assert(memcmp(got, bank, sizeof bank) == 0);

    /* Missing object and corrupted-only record are distinguishable from
     * successful reads; no silent default object is fabricated by the pool. */
    assert(st_load(OBJ_INSTRUMENTS1, got, sizeof got) < 0);
    assert(st_save(OBJ_INSTRUMENTS2, bank, sizeof bank) == 0);
    st_hdr_t h;
    int slot = st_current(OBJ_INSTRUMENTS2, &h);
    assert(slot >= 0);
    nor_flash[APP_STORE_POOL_BASE + (uint32_t)slot * ST_SECTOR] = 0u;
    assert(st_load(OBJ_INSTRUMENTS2, got, sizeof got) < 0);

    /* Save power-cut injection: previous committed record survives a torn
     * write, and a later save can recover the same object. */
    assert(st_save(OBJ_INSERTS, meta, sizeof meta) == 0);
    fill(meta, sizeof meta, 0x62u);
    fail_after = 1;
    assert(st_save(OBJ_INSERTS, meta, sizeof meta) != 0);
    fail_after = -1;
    assert(st_load(OBJ_INSERTS, got, sizeof got) == (int)sizeof meta);
    assert(st_save(OBJ_INSERTS, meta, sizeof meta) == 0);
    assert(st_load(OBJ_INSERTS, got, sizeof got) == (int)sizeof meta);
    assert(memcmp(got, meta, sizeof meta) == 0);

    assert(st_pool_room() >= 0);
    puts("FM1-Lab actual 12-sector flash-pool tests passed.");

    /* Full startup-format/reload integration: use the real production
     * storage pool and the real Instrument library together, not a fake
     * plat_store implementation. This is the first-boot path used by app_init. */
    memset(nor_flash, 0xFF, sizeof nor_flash);
    fm1lab_instrument_t factory, loaded;
    fm1lab_insert_state_t inserts;
    fm1lab_instrument_init(&factory, "FACTORY");
    assert(fm1lab_instrument_valid(&factory));
    for (uint32_t bank_index = 0u; bank_index < FM1LAB_STORE_BANK_COUNT; ++bank_index)
        assert(fm1lab_store_library_format_bank(bank_index, &factory) == 0);
    assert(st_size(OBJ_INSERTS) == (int)FM1LAB_STORE_METADATA_BYTES);
    for (uint32_t index = 0u; index < FM1LAB_STORE_INSTRUMENT_COUNT; ++index) {
        assert(fm1lab_store_library_load_full(index, &loaded, &inserts) == 0);
        assert(fm1lab_instrument_valid(&loaded));
        assert(fm1lab_store_library_last_load_stage() == FM1LAB_STORE_LOAD_OK);
    }

    /* Factory seeding rewrites many Instruments across the shared five-object
     * pool. Metadata must remain discoverable after pool sector recycling. */
    for (uint32_t index = 0u; index < FM1LAB_STORE_INSTRUMENT_COUNT; ++index) {
        assert(fm1lab_store_library_load_full(index, &loaded, &inserts) == 0);
        loaded.patches[0].macros[0] = (uint8_t)(index % 101u);
        assert(fm1lab_store_library_save_full(index, &loaded, &inserts) == 0);
    }
    assert(st_pool_boot() == 0);
    for (uint32_t index = 0u; index < FM1LAB_STORE_INSTRUMENT_COUNT; ++index) {
        assert(fm1lab_store_library_load_full(index, &loaded, &inserts) == 0);
        assert(loaded.patches[0].macros[0] == (uint8_t)(index % 101u));
    }
    puts("FM1-Lab production Instrument library + real flash-pool startup/reload tests passed.");
    return 0;
}
