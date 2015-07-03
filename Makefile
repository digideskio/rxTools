PYTHON = python

CFLAGS = -std=c11 -O2 -Wall -Wextra
CAKEFLAGS = dir_out=$(CURDIR) name=rxTools.dat

tools = tools/addxor_tool tools/cfwtool tools/pack_tool tools/xor
DATA_FILES := $(wildcard data/*.*) rxmode/nat_patch.bin rxmode/agb_patch.bin rxmode/twl_patch.bin

.PHONY: all
all: rxTools.dat

.PHONY: distclean
distclean: clean
	@rm -rf release/rxTools.dat release/ninjhax release/mset

.PHONY: clean
clean:
	@make -C rxtools clean
	@make -C rxmode clean
	@make -C brahma clean
	@make -C rxinstaller clean
	@make $(CAKEFLAGS) -C CakeHax clean
	@rm -f $(tools) payload.bin data.bin rxTools.dat

.PHONY: release
release: rxTools.dat brahma/brahma.3dsx brahma/brahma.smdh
	@mkdir -p release/mset release/ninjhax
	@cp rxTools.dat release
	@cp brahma/brahma.3dsx release/ninjhax/rxtools.3dsx
	@cp brahma/brahma.smdh release/ninjhax/rxtools.smdh
	@cp rxinstaller.nds release/mset/rxinstaller.nds

.PHONY: rxTools.dat
rxTools.dat: rxtools/rxtools.bin rxmode/*.bin data.bin
	@make $(CAKEFLAGS) -C CakeHax bigpayload
	@dd if=rxtools/rxtools.bin of=$@ seek=272 conv=notrunc
	@dd if=data.bin of=$@ seek=2K conv=notrunc

.PHONY: rxinstaller.nds
rxinstaller.nds:
	@make -C rxinstaller all

.PHONY: brahma/brahma.3dsx brahma/brahma.smdh
brahma/brahma.3dsx brahma/brahma.smdh:
	make -C $(dir $@) all

data.bin: tools/pack_tool tools/xor
	@tools/pack_tool $(DATA_FILES) $@
	@tools/xor $@ tools/xorpad/data.xor
	@rm $@
	@mv $@.out $@

.PHONY: rxmode/*.bin
rxmode/*.bin: tools/cfwtool
	@cd rxmode && make

.PHONY: rxtools/rxtools.bin
rxtools/rxtools.bin: tools/addxor_tool
	@make -C $(dir $@) all
	@dd if=$@ of=$@ bs=896K count=1 conv=sync,notrunc

$(tools): tools/%: tools/toolsrc/%/main.c
	$(LINK.c) $(OUTPUT_OPTION) $^
