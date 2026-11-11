import ast
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
RESCUE = ROOT / "base" / "bsp" / "tools" / "fm1_rescue.py"
LOADER = ROOT / "base" / "bsp" / "loader" / "loader.c"


class RescueFlashIdTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not RESCUE.exists() or not LOADER.exists():
            raise unittest.SkipTest("nested felucca-bsp submodule is not initialized")
        cls.source = RESCUE.read_text()
        cls.tree = ast.parse(cls.source)

    def test_rescue_accepts_exactly_both_documented_ids(self):
        values = {}
        for node in self.tree.body:
            if isinstance(node, ast.Assign):
                for target in node.targets:
                    if isinstance(target, ast.Name) and target.id == "FLASH_IDS":
                        values[target.id] = ast.literal_eval(node.value)
        self.assertEqual(values.get("FLASH_IDS"), (0x856014, 0xB36014))

    def test_rescue_validation_uses_flash_id_allowlist(self):
        self.assertIn("fid not in FLASH_IDS", self.source)
        self.assertIn("0x856014", self.source)
        self.assertIn("0xB36014", self.source)

    def test_ota_loader_uses_the_same_patched_flash_driver(self):
        loader = LOADER.read_text()
        self.assertIn('#include "fm1_flash.h"', loader)
        self.assertIn("return fl_erase4k_ram(off, &took);", loader)
        self.assertIn("return fl_prog_ram(off, p, n, &took);", loader)


if __name__ == "__main__":
    unittest.main()
