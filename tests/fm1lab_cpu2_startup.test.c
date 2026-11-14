#include <assert.h>
#include <stdint.h>
#include "../app/fm1lab_cpu2_startup.h"

int main(void)
{
    const uint32_t expected=0x91a7c35du;
    assert(!fm1lab_cpu2_selftest_success(0u,0u,-1,0u,expected));
    assert(!fm1lab_cpu2_selftest_success(1u,0u,-1,0u,expected));
    assert(!fm1lab_cpu2_selftest_success(1u,1u,-1,expected,expected));
    assert(!fm1lab_cpu2_selftest_success(1u,1u,0,expected^1u,expected));
    assert(fm1lab_cpu2_selftest_success(1u,1u,0,expected,expected));
    return 0;
}
