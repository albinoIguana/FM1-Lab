#include "fm1lab_voice_editor.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    uint8_t voice[FM1LAB_PATCH_BYTES] = {0};
    uint8_t value = 0;
    /* OP1 is the last 21-byte block; OP6 is the first. */
    assert(fm1lab_voice_operator_set(voice, 1, FM1LAB_OP_OUTPUT, 87) == 0);
    assert(voice[5U * 21U + 16U] == 87);
    assert(fm1lab_voice_operator_get(voice, 1, FM1LAB_OP_OUTPUT, &value) == 0 && value == 87);
    assert(fm1lab_voice_operator_set(voice, 6, FM1LAB_OP_COARSE, 31) == 0);
    assert(voice[18] == 31);
    assert(fm1lab_voice_operator_set(voice, 3, FM1LAB_OP_MODE, 2) == -1);
    assert(fm1lab_voice_operator_set(voice, 0, FM1LAB_OP_OUTPUT, 10) == -1);
    assert(fm1lab_voice_operator_get(voice, 7, FM1LAB_OP_OUTPUT, &value) == -1);

    assert(fm1lab_voice_global_set(voice, FM1LAB_GLOBAL_ALGORITHM, 32) == 0);
    assert(voice[134] == 31);
    assert(fm1lab_voice_global_get(voice, FM1LAB_GLOBAL_ALGORITHM, &value) == 0 && value == 32);
    assert(fm1lab_voice_global_set(voice, FM1LAB_GLOBAL_ALGORITHM, 0) == -1);
    assert(fm1lab_voice_global_set(voice, FM1LAB_GLOBAL_FEEDBACK, 8) == -1);

    assert(fm1lab_voice_operator_is_carrier(voice, 1) == 1);
    assert(fm1lab_voice_operator_depth(voice, 1) == fm1lab_operator_depth(31, 1));
    assert(fm1lab_voice_operator_is_carrier(voice, 7) == -1);

    assert(fm1lab_voice_name_set(voice, "deep bass") == 0);
    char name[11];
    assert(fm1lab_voice_name_get(voice, name) == 0);
    assert(strcmp(name, "DEEP BASS") == 0);
    uint8_t before[FM1LAB_PATCH_BYTES];
    memcpy(before, voice, sizeof before);
    assert(fm1lab_voice_name_set(voice, "bad\nname") == -1);
    assert(memcmp(before, voice, sizeof before) == 0);
    return 0;
}
