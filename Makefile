# Build and flash sketches for the STM32F103C8 "Blue Pill" via ST-Link.
#
# Each sketch is a directory holding a like-named .ino, per Arduino convention:
#
#     blink/blink.ino
#     serial-echo/serial-echo.ino
#
# New directories are picked up automatically -- no edits here.
#
#     make                  build every sketch
#     make blink            build one
#     make flash-blink      build, verify and flash one
#     make flash            flash $(SKETCH) (default: $(SKETCH))
#     make list             show discovered sketches
#     make probe            identify the attached ST-Link and chip
#     make clean            remove all build output

FQBN       := STMicroelectronics:stm32:GenF1:pnum=BLUEPILL_F103C8
BUILD_DIR  := build
FLASH_ADDR := 0x8000000

# Initial stack pointer expected in the vector table: top of the F103C8's
# 20 KB SRAM. Guards against flashing an image built for another MCU.
EXPECTED_SP := 20005000

# A directory is a sketch only if it contains <dir>/<dir>.ino.
dirname  = $(notdir $(patsubst %/,%,$1))
SKETCHES := $(sort $(foreach d,$(wildcard */), \
              $(if $(wildcard $(d)$(call dirname,$(d)).ino),$(call dirname,$(d)))))

# Sketch used by the bare `flash` and `check` targets.
SKETCH ?= $(firstword $(SKETCHES))

.PHONY: all list probe clean flash check

all: $(SKETCHES)

# Per-sketch build, check and flash targets.
define SKETCH_RULES
$(BUILD_DIR)/$(1)/$(1).ino.bin: $(wildcard $(1)/*.ino $(1)/*.h $(1)/*.cpp $(1)/*.c)
	arduino-cli compile -b "$(FQBN)" --output-dir $(BUILD_DIR)/$(1) $(1)

.PHONY: $(1) check-$(1) flash-$(1)

$(1): $(BUILD_DIR)/$(1)/$(1).ino.bin

check-$(1): $(BUILD_DIR)/$(1)/$(1).ino.bin
	@./tools/check-sp.sh $$< $(EXPECTED_SP)

flash-$(1): check-$(1)
	st-flash --reset write $(BUILD_DIR)/$(1)/$(1).ino.bin $(FLASH_ADDR)
endef

$(foreach s,$(SKETCHES),$(eval $(call SKETCH_RULES,$(s))))

# Convenience aliases for the default sketch.
check: check-$(SKETCH)
flash: flash-$(SKETCH)

list:
	@printf '%s\n' $(SKETCHES) | sed 's/^/  /'
	@echo "default (SKETCH): $(SKETCH)"

probe:
	st-info --probe

clean:
	rm -rf $(BUILD_DIR)
