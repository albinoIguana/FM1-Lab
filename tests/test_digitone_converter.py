import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "digitone"))

from convert_syx import (  # noqa: E402
    TOPOLOGIES,
    convert_voice,
    decoded_index_for_original_offset,
    dx7_operator_offset,
    dx7_rate,
    ratio_to_dx7,
)


class DigitoneConverterTests(unittest.TestCase):
    def test_legacy_parameter_offsets_map_through_8in7(self):
        self.assertEqual(decoded_index_for_original_offset(0x51), 62)  # algorithm
        self.assertEqual(decoded_index_for_original_offset(0x54), 64)  # C ratio
        self.assertEqual(decoded_index_for_original_offset(0x56), 66)  # A ratio

    def test_dx7_ratio_conversion_and_subharmonic_limit(self):
        self.assertEqual(ratio_to_dx7(0.50), (0, 0))
        self.assertEqual(ratio_to_dx7(0.25), (0, 0))
        self.assertEqual(ratio_to_dx7(1.0), (1, 0))
        self.assertEqual(ratio_to_dx7(1.5), (1, 50))
        self.assertEqual(ratio_to_dx7(2.0), (2, 0))

    def test_expanded_operator_blocks_are_reversed(self):
        self.assertEqual(dx7_operator_offset(1), 105)
        self.assertEqual(dx7_operator_offset(6), 0)

    def test_all_digitone_algorithms_have_explicit_role_maps(self):
        self.assertEqual(set(TOPOLOGIES), set(range(8)))
        for topology in TOPOLOGIES.values():
            self.assertEqual(set(topology["ops"]), {"C", "A", "B1", "B2"})
            self.assertTrue(all(1 <= op <= 6 for op in topology["ops"].values()))
            self.assertIn(topology["second"], {"A", "B1"})

    def test_zeroed_synthetic_payload_still_produces_valid_voice(self):
        voice, params = convert_voice(bytes(302), "TEST VOICE")
        self.assertEqual(len(voice), 156)
        self.assertTrue(all(0 <= value <= 127 for value in voice))
        self.assertEqual(voice[134], TOPOLOGIES[params["algorithm"]]["dx7"])
        self.assertEqual(voice[145:155], [ord(c) for c in "TEST VOICE"])
        self.assertEqual(voice[155], 0)

    def test_digitone_time_is_inverted_for_dx7_rate(self):
        self.assertEqual(dx7_rate(0), 99)
        self.assertEqual(dx7_rate(127), 0)


if __name__ == "__main__":
    unittest.main()
