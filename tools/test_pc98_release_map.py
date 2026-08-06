import unittest

from check_pc98_release_map import find_forbidden_markers


class Pc98ReleaseMapTests(unittest.TestCase):
    def test_accepts_map_without_debug_serial(self):
        self.assertEqual(find_forbidden_markers("pc98_serial_open\n"), [])

    def test_rejects_debug_source_and_symbols(self):
        text = "debug_serial_pc98.c\npc98_debug_open\n"
        self.assertEqual(
            find_forbidden_markers(text),
            ["debug_serial_pc98.c", "pc98_debug_"],
        )


if __name__ == "__main__":
    unittest.main()
