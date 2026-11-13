import importlib.util
from pathlib import Path
import unittest

MODULE_PATH = Path(__file__).resolve().parents[1] / "tools" / "fit-polyphony.py"
SPEC = importlib.util.spec_from_file_location("fit_polyphony", MODULE_PATH)
FIT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FIT)


class PolyphonyFitTests(unittest.TestCase):
    def test_linear_fit_keeps_fixed_callback_intercept(self):
        intercept, slope = FIT.fit([(1, 125), (3, 185), (5, 245)])
        self.assertAlmostEqual(intercept, 95.0)
        self.assertAlmostEqual(slope, 30.0)

    def test_fit_rejects_constant_voice_count(self):
        with self.assertRaises(ValueError):
            FIT.fit([(4, 100), (4, 110)])


if __name__ == "__main__":
    unittest.main()
