DOS_CC ?= ia16-elf-gcc
DOS_CFLAGS = -march=i8086 -mcmodel=tiny -mmsdos -Os -I dos/include
DOS_LDFLAGS = -nostartfiles -nodefaultlibs -Wl,-T,/usr/ia16-elf/lib/dos-com.ld
PC98_BUILD = build/pc98
PC98_DEBUG_CFLAGS = -DPC98IMEBRIDGE_PC98_DEBUG_SERIAL=1
PC98_CLIENT_SOURCES = dos/common/protocol.c dos/pc98/serial_pc98.c dos/pc98/inject_bios_pc98.c dos/pc98/ime98_control.c dos/pc98/main.c
PC98_TSR_SOURCES = dos/pc98/tsr_resident_start.S dos/common/protocol.c dos/pc98/serial_pc98.c dos/pc98/inject_bios_pc98.c dos/pc98/serial_irq_buffer_pc98.c dos/pc98/tsr_hook.S dos/pc98/tsr_main.c dos/pc98/keyboard_bios.c dos/pc98/foreground.c dos/pc98/tsr_resident_end.S

.PHONY: pc98 pc98-debug pc98-tsr pc98-tsr-debug pc98-keyprobe pc98-sys pc98-device-test pc98-com-probe pc98-int14-probe pc98-com-status-probe test clean

pc98: $(PC98_BUILD)/IME98.COM

pc98-debug: $(PC98_BUILD)/IME98DBG.COM

pc98-tsr: $(PC98_BUILD)/IME98TSR.COM

pc98-tsr-debug: $(PC98_BUILD)/IME98TSD.COM

pc98-keyprobe: $(PC98_BUILD)/KEY98.COM

pc98-sys: $(PC98_BUILD)/IME98.SYS

pc98-device-test: $(PC98_BUILD)/IME98DEV.COM

pc98-com-probe: $(PC98_BUILD)/COMPROBE.COM

pc98-int14-probe: $(PC98_BUILD)/INT14.COM

pc98-com-status-probe: $(PC98_BUILD)/COMSTATUS.COM

test:
	PYTHONPATH=tools python3 -m unittest discover -s tools -p 'test_*.py' -v

$(PC98_BUILD):
	mkdir -p $(PC98_BUILD)

$(PC98_BUILD)/IME98.COM: $(PC98_BUILD) $(PC98_CLIENT_SOURCES) dos/pc98/debug_serial_pc98.h dos/include/protocol.h
	$(DOS_CC) $(DOS_CFLAGS) $(DOS_LDFLAGS) -Wl,-Map,$@.map -o $@ $(PC98_CLIENT_SOURCES)
	python3 tools/check_pc98_release_map.py $@.map

$(PC98_BUILD)/IME98DBG.COM: $(PC98_BUILD) $(PC98_CLIENT_SOURCES) dos/pc98/debug_serial_pc98.c dos/pc98/debug_serial_pc98.h dos/include/protocol.h
	$(DOS_CC) $(DOS_CFLAGS) $(PC98_DEBUG_CFLAGS) $(DOS_LDFLAGS) -o $@ $(PC98_CLIENT_SOURCES) dos/pc98/debug_serial_pc98.c

$(PC98_BUILD)/IME98TSR.COM: $(PC98_BUILD) $(PC98_TSR_SOURCES) dos/pc98/debug_serial_pc98.h
	$(DOS_CC) $(DOS_CFLAGS) -fno-common $(DOS_LDFLAGS) -Wl,-Map,$@.map -o $@ $(PC98_TSR_SOURCES)
	python3 tools/check_pc98_tsr_map.py $@.map
	python3 tools/check_pc98_release_map.py $@.map

$(PC98_BUILD)/IME98TSD.COM: $(PC98_BUILD) $(PC98_TSR_SOURCES) dos/pc98/debug_serial_pc98.c dos/pc98/debug_serial_pc98.h
	$(DOS_CC) $(DOS_CFLAGS) $(PC98_DEBUG_CFLAGS) -fno-common $(DOS_LDFLAGS) -Wl,-Map,$@.map -o $@ $(PC98_TSR_SOURCES) dos/pc98/debug_serial_pc98.c
	python3 tools/check_pc98_tsr_map.py $@.map

$(PC98_BUILD)/KEY98.COM: $(PC98_BUILD) dos/pc98/key_probe.c
	$(DOS_CC) $(DOS_CFLAGS) $(DOS_LDFLAGS) -o $@ dos/pc98/key_probe.c

$(PC98_BUILD)/IME98.SYS: $(PC98_BUILD) dos/pc98/device_driver.S
	ia16-elf-as -o $(PC98_BUILD)/device_driver.o dos/pc98/device_driver.S
	ia16-elf-ld -Ttext 0 -e device_header --oformat binary -o $@ $(PC98_BUILD)/device_driver.o

$(PC98_BUILD)/IME98DEV.COM: $(PC98_BUILD) dos/pc98/device_test.c
	$(DOS_CC) $(DOS_CFLAGS) $(DOS_LDFLAGS) -o $@ dos/pc98/device_test.c

$(PC98_BUILD)/COMPROBE.COM: $(PC98_BUILD) dos/pc98/com_probe.c
	$(DOS_CC) $(DOS_CFLAGS) $(DOS_LDFLAGS) -o $@ dos/pc98/com_probe.c

$(PC98_BUILD)/INT14.COM: $(PC98_BUILD) dos/pc98/int14_probe.c
	$(DOS_CC) $(DOS_CFLAGS) $(DOS_LDFLAGS) -o $@ dos/pc98/int14_probe.c

$(PC98_BUILD)/COMSTATUS.COM: $(PC98_BUILD) dos/common/protocol.c dos/pc98/serial_pc98.c dos/pc98/com_status_probe.c dos/include/protocol.h
	$(DOS_CC) $(DOS_CFLAGS) $(DOS_LDFLAGS) -o $@ dos/common/protocol.c dos/pc98/serial_pc98.c dos/pc98/com_status_probe.c

clean:
	rm -rf build
