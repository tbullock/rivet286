# Rivet/286 builds two relocation-free 80286 code regions with Microsoft C.
LEGACYCC_HOME ?= ../legacycc
LEGACYCC ?= $(LEGACYCC_HOME)/.venv/bin/legacycc
NASM ?= nasm

BUILD := build
KERNEL_WORK := $(BUILD)/kernel
PATH_WORK := $(BUILD)/paths
MSC := $(LEGACYCC_HOME)/tools/msc600
CFLAGS_286 := /c /AS /G2 /Gs /Zl /W3 /Osler
KERNEL_SOURCES := src/pcat/kernel.c src/pcat/mailbox.h src/pcat/heapall.c \
                  src/abi/request.c src/abi/request.h src/abi/dispatch.c \
                  src/heap/heap.c src/heap/heap.h src/heap/ranges.c src/heap/ranges.h \
                  src/heap/descr.c src/heap/descr.h src/heap/segments.c src/heap/segments.h \
                  src/heap/commands.c src/heap/commands.h src/heap/heapkern.c \
                  src/heap/heapkern.h src/heap/heapgdt.h src/xms/xms.c src/xms/xms.h \
                  src/startup/startup.c src/startup/startup.h src/startup/startnat.c \
                  src/startup/startnat.h src/startup/startmon.c src/startup/startmon.h \
                  src/path/pathkern.c src/path/pathnat.h
PATH_SOURCES := src/path/pathext.c src/path/path.c src/path/path.h \
                src/path/search.c src/path/search.h src/path/execute.c \
                src/path/execute.h src/path/pathnat.c src/path/pathnat.h src/abi/request.h
MSC_HEADERS := $(MSC)/INCLUDE/LIMITS.H $(MSC)/INCLUDE/STDDEF.H
MSC_LINK := $(MSC)/BIN/LINK.EXE $(MSC)/LIB/LIBH.LIB

KERNEL_LINK_ARGS := /NOLOGO /NOD /MAP 'ENTRY.OBJ+KERNEL.OBJ,KERNEL.EXE,KERNEL.MAP,T:\LIB\LIBH.LIB;'

PATH_LINK_ARGS := /NOLOGO /NOD /MAP 'ENTRY.OBJ+PATH.OBJ,PATH.EXE,PATH.MAP,T:\LIB\LIBH.LIB;'

.PHONY: all demo clean
.DELETE_ON_ERROR:
all: $(BUILD)/RIVET.COM
demo: $(BUILD)/DEMO.P16

$(BUILD) $(KERNEL_WORK) $(PATH_WORK):
	mkdir -p "$@"

# Each legacycc invocation gets disposable guest state. Its DOS working files
# use distinct 8.3 names so the same source builds on case-sensitive hosts.
define legacy_run
	@set -eu; \
	state=$$(mktemp -d "$(CURDIR)/$(BUILD)/legacycc.XXXXXX"); \
	trap 'rm -rf "$$state"' 0; \
	trap 'exit 1' HUP INT TERM; \
	"$(LEGACYCC)" --home "$(LEGACYCC_HOME)" run msc600 --state-dir "$$state" \
	    --cwd "$(1)" --tool "$(2)" --empty-stdin --timeout 120 -- $(3)
endef

$(KERNEL_WORK)/KERNEL.OBJ: $(KERNEL_SOURCES) $(MSC_HEADERS) $(MSC)/BIN/CL.EXE Makefile \
                         | $(KERNEL_WORK) $(BUILD)/path-extension.bin
	cp $(KERNEL_SOURCES) "$(KERNEL_WORK)/"
	rm -f "$@"
	$(call legacy_run,$(KERNEL_WORK),CL.EXE,$(CFLAGS_286) /FoKERNEL.OBJ KERNEL.C)
	test -s "$@"
	touch "$@"

$(PATH_WORK)/PATH.OBJ: $(PATH_SOURCES) $(MSC)/BIN/CL.EXE Makefile | $(PATH_WORK)
	cp $(PATH_SOURCES) "$(PATH_WORK)/"
	rm -f "$@"
	$(call legacy_run,$(PATH_WORK),CL.EXE,$(CFLAGS_286) /FoPATH.OBJ PATHEXT.C)
	test -s "$@"
	touch "$@"

$(KERNEL_WORK)/ENTRY.OBJ: src/pcat/entry.asm Makefile | $(KERNEL_WORK)
	$(NASM) -f obj -MD $(BUILD)/kernel-entry.d -MP "$<" -o "$@"

$(PATH_WORK)/ENTRY.OBJ: src/path/entry.asm Makefile | $(PATH_WORK)
	$(NASM) -f obj "$<" -o "$@"

# LINK emits MZ. Check its header and segment map, then extract the code that
# follows the zero padding at the region's fixed entry address.
define project_mz
	@set -eu; \
	set -- $$(od -An -v -N28 -tu1 "$(1)"); \
	test "$$#" -eq 28; \
	last=$$(($$3 + $$4 * 256)); \
	pages=$$(($$5 + $$6 * 256)); \
	header=$$((($$9 + $${10} * 256) * 16)); \
	entry=$$(($${21} + $${22} * 256)); \
	size=$$(wc -c < "$(1)"); \
	text_length=$$(awk '{ sub(/\r$$/, "") } $$1 ~ /^[0-9A-F]+H$$/ && $$3 !~ /^0+H$$/ { \
	    if ($$4 == "_TEXT" && $$5 == "CODE" && $$1 ~ /^0+H$$/) { \
	        sub(/H$$/, "", $$3); print $$3; \
	    } else if ($$4 != "STACK") exit 1; \
	}' "$(2)"); \
	test -n "$$text_length"; \
	if [ "$$1" -ne 77 ] || [ "$$2" -ne 90 ] || [ "$$pages" -eq 0 ] || \
	   [ "$$last" -ge 512 ] || [ "$$7" -ne 0 ] || [ "$$8" -ne 0 ] || \
	   [ "$${23}" -ne 0 ] || [ "$${24}" -ne 0 ] || [ "$${27}" -ne 0 ] || [ "$${28}" -ne 0 ] || \
	   [ "$$size" -ne "$$(((pages - 1) * 512 + (last ? last : 512)))" ] || \
	   [ "$$header" -lt 28 ] || [ "$$entry" -ne "$$(($(3)))" ] || \
	   [ "$$((size - header))" -ne "$$((0x$$text_length))" ] || \
	   [ "$$((size - header))" -le "$$entry" ] || [ "$$((size - header))" -gt "$$(($(4)))" ]; then \
	    printf 'Invalid linked code region: %s\n' "$(1)" >&2; exit 1; \
	fi; \
	test "$$(dd if="$(1)" bs=1 skip="$$header" count="$$entry" 2>/dev/null \
	    | LC_ALL=C tr -d '\000' | wc -c)" -eq 0; \
	dd if="$(1)" of="$@" bs=1 skip="$$((header + entry))" 2>/dev/null; \
	test "$$(wc -c < "$@")" -eq "$$((size - header - entry))"
endef

$(BUILD)/kernel.bin: $(KERNEL_WORK)/ENTRY.OBJ $(KERNEL_WORK)/KERNEL.OBJ $(MSC_LINK) Makefile
	rm -f "$(KERNEL_WORK)/KERNEL.EXE" "$(KERNEL_WORK)/KERNEL.MAP"
	$(call legacy_run,$(KERNEL_WORK),LINK.EXE,$(KERNEL_LINK_ARGS))
	$(call project_mz,$(KERNEL_WORK)/KERNEL.EXE,$(KERNEL_WORK)/KERNEL.MAP,0x2000,0x5000)

$(BUILD)/path-extension.bin: $(PATH_WORK)/ENTRY.OBJ $(PATH_WORK)/PATH.OBJ $(MSC_LINK) Makefile
	rm -f "$(PATH_WORK)/PATH.EXE" "$(PATH_WORK)/PATH.MAP"
	$(call legacy_run,$(PATH_WORK),LINK.EXE,$(PATH_LINK_ARGS))
	$(call project_mz,$(PATH_WORK)/PATH.EXE,$(PATH_WORK)/PATH.MAP,0xb000,0xc000)

$(BUILD)/RIVET.COM: src/pcat/boot.asm $(BUILD)/kernel.bin $(BUILD)/path-extension.bin Makefile
	$(NASM) -f bin -MD $(BUILD)/rivet.d -MP \
	    -D 'KERNEL_FILE="$(BUILD)/kernel.bin"' \
	    -D 'PATHS_EXTENSION="$(BUILD)/path-extension.bin"' "$<" -o "$@"

$(BUILD)/DEMO.P16: examples/demo.asm Makefile | $(BUILD)
	$(NASM) -f bin "$<" -o "$@"

clean:
	rm -rf "$(BUILD)"

-include $(BUILD)/kernel-entry.d $(BUILD)/rivet.d
