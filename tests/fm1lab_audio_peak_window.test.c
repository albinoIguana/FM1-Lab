#include <assert.h>
#include <stdint.h>
#include "../app/fm1lab_audio_peak_window.h"

int main(void)
{
    volatile uint32_t pre=0u, reduction=0u;
    fm1lab_audio_peak_window_update(&pre,&reduction,100u,10u);
    fm1lab_audio_peak_window_update(&pre,&reduction,50u,20u);
    fm1lab_audio_peak_window_update(&pre,&reduction,150u,5u);
    assert(pre==150u);
    assert(reduction==20u);
    fm1lab_audio_peak_window_reset(&pre,&reduction);
    assert(pre==0u);
    assert(reduction==0u);
    fm1lab_audio_peak_window_update(&pre,&reduction,75u,12u);
    assert(pre==75u);
    assert(reduction==12u);
    return 0;
}
