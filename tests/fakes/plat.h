#ifndef FM1LAB_TEST_PLAT_H
#define FM1LAB_TEST_PLAT_H
#include <stdint.h>
#define RGB(r,g,b) ((uint32_t)0)
int plat_store_load(uint32_t obj, void *dst, uint32_t max);
int plat_store_size(uint32_t obj);
int plat_store_save(uint32_t obj, const void *src, uint32_t len);
#endif
