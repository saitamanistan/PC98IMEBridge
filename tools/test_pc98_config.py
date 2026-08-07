import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class Pc98ConfigTests(unittest.TestCase):
    def test_loader_streams_complete_config_by_line(self):
        source = (ROOT / "dos/pc98/serial_pc98.c").read_text(encoding="utf-8")
        self.assertNotIn("PC98_CONFIG_BUFFER_SIZE", source)
        self.assertIn(
            "dos_read((uint16_t)handle, read_buffer, sizeof(read_buffer))", source
        )
        self.assertIn("for (;;)", source)
        self.assertIn("parse_config_line(line)", source)
        self.assertIn("line_overflow", source)
        self.assertIn("line_comment", source)
        self.assertIn("value == ';' || value == '#'", source)

        sample = (ROOT / "samples/IME98.CFG").read_text(encoding="ascii")
        self.assertIn("HOTKEY=", sample)
        self.assertTrue(any(line.startswith(";") for line in sample.splitlines()))

    def test_sample_autoexec_selects_config_drive(self):
        lines = (ROOT / "samples/AUTOEXEC.PC98.BAT").read_text(
            encoding="ascii"
        ).splitlines()
        select_drive = lines.index("Z:")
        install_tsr = next(
            index for index, line in enumerate(lines) if "IME98TSR.COM" in line
        )
        self.assertLess(select_drive, install_tsr)


if __name__ == "__main__":
    unittest.main()
