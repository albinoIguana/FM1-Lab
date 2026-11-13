#include <stdint.h>
#include "../base/device/fm1_flash_ok.h"

int main(void)
{
    if (!FM1B_FLASH_JEDEC_OK(0x856014u)) return 1; /* Puya P25Q80H */
    if (!FM1B_FLASH_JEDEC_OK(0xB36014u)) return 2; /* UCUN UC25HQ80 */
    if (FM1B_FLASH_JEDEC_OK(0x000000u)) return 3;
    if (FM1B_FLASH_JEDEC_OK(0xEF4014u)) return 4; /* unrelated 1 MiB NOR */
    if (FM1B_FLASH_JEDEC_OK(0xB36015u)) return 5; /* wrong capacity/density byte */
    return 0;
}
