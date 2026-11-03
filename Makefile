# STM8L101F3P6 build + SWIM flash
#   make          -> build/main.ihx (flash image, Intel HEX)
#   make bin      -> build/main.bin (raw 64 KB address-space image, code at 0x8000)
#   make flash    -> program over SWIM with OpenOCD + ST-LINK/V2
#   make flash-stm8flash -> same, using stm8flash if you have it on PATH

SDCC     ?= "C:/Program Files/SDCC/bin/sdcc"
MAKEBIN  ?= "C:/Program Files/SDCC/bin/makebin"
OPENOCD  ?= openocd
TARGET   := main
BUILD    := build
SRCS     := $(wildcard src/*.c)
RELS     := $(patsubst src/%.c,$(BUILD)/%.rel,$(SRCS))

CFLAGS   := -mstm8 --std-c11 --opt-code-size -Isrc
LDFLAGS  := -mstm8 --out-fmt-ihx

all: $(BUILD)/$(TARGET).ihx

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.rel: src/%.c | $(BUILD)
	$(SDCC) $(CFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).ihx: $(RELS)
	$(SDCC) $(LDFLAGS) $^ -o $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).ihx
	$(MAKEBIN) -s 65536 -p $< $@

flash: $(BUILD)/$(TARGET).ihx
	$(OPENOCD) -f openocd/stm8l101f3.cfg -c "program $< verify reset exit"

flash-stm8flash: $(BUILD)/$(TARGET).ihx
	stm8flash -c stlinkv2 -p stm8l101f3 -w $<

clean:
	rm -rf $(BUILD)

.PHONY: all bin flash flash-stm8flash clean

bin: $(BUILD)/$(TARGET).bin
