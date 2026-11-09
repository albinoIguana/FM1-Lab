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
    /* UI encoder bounds must match the packed DX7 field ranges. */
    assert(fm1lab_voice_operator_set(voice, 2, FM1LAB_OP_COARSE, 32) == -1);
    assert(fm1lab_voice_operator_set(voice, 2, FM1LAB_OP_DETUNE, 14) == 0);
    assert(fm1lab_voice_operator_set(voice, 2, FM1LAB_OP_DETUNE, 15) == -1);
    assert(fm1lab_voice_operator_set(voice, 2, FM1LAB_OP_FINE, 99) == 0);
    assert(fm1lab_voice_operator_set(voice, 2, FM1LAB_OP_FINE, 100) == -1);
    assert(fm1lab_voice_operator_set(voice, 2, FM1LAB_OP_RATE_4, 99) == 0);
    assert(fm1lab_voice_operator_set(voice, 2, FM1LAB_OP_RATE_4, 100) == -1);

    assert(fm1lab_voice_global_set(voice, FM1LAB_GLOBAL_ALGORITHM, 32) == 0);
    assert(voice[134] == 31);
    assert(fm1lab_voice_global_get(voice, FM1LAB_GLOBAL_ALGORITHM, &value) == 0 && value == 32);
    assert(fm1lab_voice_global_set(voice, FM1LAB_GLOBAL_ALGORITHM, 0) == -1);
    assert(fm1lab_voice_global_set(voice, FM1LAB_GLOBAL_FEEDBACK, 8) == -1);

    assert(fm1lab_voice_operator_is_carrier(voice, 1) == 1);
    /* Algorithm 0 has carrier OP1/OP3; patch ADSR updates both but leaves
     * modulator envelopes untouched. */
    assert(fm1lab_voice_global_set(voice, FM1LAB_GLOBAL_ALGORITHM, 1u) == 0);
    uint8_t adsr[4] = {12u, 34u, 56u, 78u};
    assert(fm1lab_voice_adsr_set(voice, adsr) == 0);
    uint8_t read_adsr[4] = {0};
    assert(fm1lab_voice_adsr_get(voice, read_adsr) == 0);
    assert(memcmp(adsr, read_adsr, sizeof adsr) == 0);
    for (unsigned op = 1; op <= 6; ++op) {
        uint8_t r1, r2, r3, r4, l1, l2, l3, l4;
        fm1lab_voice_operator_get(voice, op, FM1LAB_OP_RATE_1, &r1);
        fm1lab_voice_operator_get(voice, op, FM1LAB_OP_RATE_2, &r2);
        fm1lab_voice_operator_get(voice, op, FM1LAB_OP_RATE_3, &r3);
        fm1lab_voice_operator_get(voice, op, FM1LAB_OP_RATE_4, &r4);
        fm1lab_voice_operator_get(voice, op, FM1LAB_OP_LEVEL_1, &l1);
        fm1lab_voice_operator_get(voice, op, FM1LAB_OP_LEVEL_2, &l2);
        fm1lab_voice_operator_get(voice, op, FM1LAB_OP_LEVEL_3, &l3);
        fm1lab_voice_operator_get(voice, op, FM1LAB_OP_LEVEL_4, &l4);
        if (op == 1u || op == 3u) {
            assert(r1 == 12u && r2 == 99u && r3 == 34u && r4 == 78u);
            assert(l1 == 99u && l2 == 99u && l3 == 56u && l4 == 0u);
        } else {
            assert(r1 == 0u && r2 == 0u && r3 == 0u);
            assert(r4 == (op == 2u ? 99u : 0u)); /* OP2 was pre-edited above. */
            assert(l1 == 0u && l2 == 0u && l3 == 0u && l4 == 0u);
        }
    }
    adsr[2] = 100u;
    assert(fm1lab_voice_adsr_set(voice, adsr) == -1);
    assert(fm1lab_voice_operator_depth(voice, 1) == fm1lab_operator_depth(31, 1));
    assert(fm1lab_voice_operator_modulates(voice, 1, 2) ==
           fm1lab_operator_modulates(31, 1, 2));
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
