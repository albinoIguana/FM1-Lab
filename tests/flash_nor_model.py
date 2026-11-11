"""Protocol-level NOR model for the two documented FM1 flash populations.

This is a contract test, not a silicon emulator: both parts must expose the
same 1 MiB, 256-byte-page, 4 KiB-sector SPI NOR behavior used by fm1_flash.h.
"""
import unittest

PUYA = 0x856014
UCUN = 0xB36014
FLASH_BYTES = 0x100000
PAGE_BYTES = 256
SECTOR_BYTES = 4096


class FlashNorModel:
    def __init__(self, jedec_id):
        self.jedec_id = jedec_id
        self.memory = bytearray([0xFF]) * FLASH_BYTES
        self.write_enabled = False

    def read_jedec(self):
        return self.jedec_id

    def write_enable(self):
        self.write_enabled = True

    def read(self, offset, length):
        if offset < 0 or length < 0 or offset + length > FLASH_BYTES:
            raise ValueError("read outside 1 MiB device")
        return bytes(self.memory[offset:offset + length])

    def page_program(self, offset, payload):
        if not self.write_enabled:
            raise PermissionError("WEL is clear")
        if not payload or len(payload) > PAGE_BYTES:
            raise ValueError("invalid page-program length")
        if offset < 0 or offset + len(payload) > FLASH_BYTES:
            raise ValueError("program outside device")
        if offset // PAGE_BYTES != (offset + len(payload) - 1) // PAGE_BYTES:
            raise ValueError("page program may not wrap")
        for index, value in enumerate(payload):
            old = self.memory[offset + index]
            if value | old != old:
                raise ValueError("NOR programming cannot change 0 bits back to 1")
            self.memory[offset + index] = old & value
        self.write_enabled = False

    def erase_4k(self, offset):
        if not self.write_enabled:
            raise PermissionError("WEL is clear")
        if offset < 0 or offset % SECTOR_BYTES or offset + SECTOR_BYTES > FLASH_BYTES:
            raise ValueError("invalid 4 KiB erase range")
        self.memory[offset:offset + SECTOR_BYTES] = b"\xFF" * SECTOR_BYTES
        self.write_enabled = False


class DualFlashProtocolTests(unittest.TestCase):
    def make_parts(self):
        return [FlashNorModel(PUYA), FlashNorModel(UCUN)]

    def test_both_ids_are_supported_and_distinct(self):
        self.assertEqual([p.read_jedec() for p in self.make_parts()], [PUYA, UCUN])
        self.assertNotEqual(PUYA, UCUN)

    def test_same_read_program_erase_sequence_on_both_parts(self):
        for part in self.make_parts():
            with self.subTest(jedec=hex(part.read_jedec())):
                offset = 0x97000
                payload = bytes(range(256))
                part.write_enable()
                part.page_program(offset, payload)
                self.assertEqual(part.read(offset, 256), payload)
                part.write_enable()
                part.erase_4k(offset)
                self.assertEqual(part.read(offset, 256), b"\xFF" * 256)

    def test_write_enable_is_required_and_consumed(self):
        for part in self.make_parts():
            with self.subTest(jedec=hex(part.read_jedec())):
                with self.assertRaises(PermissionError):
                    part.page_program(0x97000, b"\x00")
                part.write_enable()
                part.page_program(0x97000, b"\x00")
                with self.assertRaises(PermissionError):
                    part.erase_4k(0x97000)

    def test_page_wrap_and_invalid_sector_alignment_are_rejected(self):
        for part in self.make_parts():
            with self.subTest(jedec=hex(part.read_jedec())):
                part.write_enable()
                with self.assertRaises(ValueError):
                    part.page_program(0x00FF, b"\x00\x00")
                part.write_enable()
                with self.assertRaises(ValueError):
                    part.erase_4k(0x97001)

    def test_nor_bits_only_transition_one_to_zero_until_erase(self):
        for part in self.make_parts():
            with self.subTest(jedec=hex(part.read_jedec())):
                part.write_enable()
                part.page_program(0x97000, b"\x0F")
                part.write_enable()
                with self.assertRaises(ValueError):
                    part.page_program(0x97000, b"\xF0")
                part.write_enable()
                part.erase_4k(0x97000)
                part.write_enable()
                part.page_program(0x97000, b"\xF0")
                self.assertEqual(part.read(0x97000, 1), b"\xF0")

    def test_capacity_boundary_and_overflow_are_rejected(self):
        for part in self.make_parts():
            with self.subTest(jedec=hex(part.read_jedec())):
                self.assertEqual(len(part.read(FLASH_BYTES - 1, 1)), 1)
                with self.assertRaises(ValueError):
                    part.read(FLASH_BYTES, 1)
                part.write_enable()
                with self.assertRaises(ValueError):
                    part.page_program(FLASH_BYTES - 1, b"\x00\x00")


if __name__ == "__main__":
    unittest.main()
