import unittest

from check_pc98_tsr_map import validate_map


def map_text(overrides=None):
    symbols = {
        "pc98_uninstall_prepare_entry": 0x0800,
        "pc98_prepare_uninstall": 0x0900,
        "pc98_tsr_signature": 0x0A00,
        "pc98_tsr_bss_start": 0x1000,
        "pc98_old_input_vector": 0x1010,
        "pc98_old_serial_irq_vector": 0x1020,
        "pc98_old_timer_vector": 0x1030,
        "pc98_old_idle_vector": 0x1040,
        "pc98_hotkey_latched": 0x1050,
        "pc98_serial_irq_capture": 0x1060,
        "pc98_serial_irq_buffer": 0x1070,
        "pc98_worker_stack_top": 0x1100,
        "pc98_tsr_resident_end": 0x1200,
    }
    symbols.update(overrides or {})
    return "".join(f"  0x{address:04x} {name}\n" for name, address in symbols.items())


class Pc98TsrMapTests(unittest.TestCase):
    def test_accepts_resident_state_and_uninstall_entry(self):
        self.assertEqual(validate_map(map_text()), [])

    def test_rejects_uninstall_entry_past_resident_end(self):
        self.assertEqual(
            validate_map(map_text({"pc98_uninstall_prepare_entry": 0x1200})),
            ["pc98_uninstall_prepare_entry=0x1200"],
        )

    def test_rejects_state_before_resident_bss(self):
        self.assertEqual(
            validate_map(map_text({"pc98_hotkey_latched": 0x0FFF})),
            ["pc98_hotkey_latched=0x0FFF"],
        )


if __name__ == "__main__":
    unittest.main()
