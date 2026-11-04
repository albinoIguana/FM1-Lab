#include "fm1lab_physical_controls.h"
#include <assert.h>

int main(void)
{
    /* Lock the printed panel-to-source mapping so future UI work cannot
     * silently swap physical controls by assuming array order. */
    assert(FM1LAB_ENCODER_SELECT == 0);
    assert(FM1LAB_ENCODER_ALGORITHM == 1);
    assert(FM1LAB_ENCODER_KNOB_1 == 2);
    assert(FM1LAB_ENCODER_KNOB_2 == 3);
    assert(FM1LAB_ENCODER_KNOB_3 == 4);
    assert(FM1LAB_ENCODER_KNOB_4 == 5);
    assert(FM1LAB_ENCODER_PRESETS == 6);
    assert(FM1LAB_ENCODER_COUNT == 7);
    assert(FM1LAB_ADC_MASTER == 4u);
    return 0;
}
