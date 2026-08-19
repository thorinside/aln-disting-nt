SHELL := /bin/sh

API_DIR ?= distingNT_API
API_INCLUDE := $(API_DIR)/include
API_HEADER := $(API_INCLUDE)/distingnt/api.h

HOST_CXX ?= c++
ARM_CXX ?= arm-none-eabi-g++
ARM_NM ?= arm-none-eabi-nm
ARM_READELF ?= arm-none-eabi-readelf
ARM_SIZE ?= arm-none-eabi-size

BUILD_DIR := build
RELEASE_DIR := release
FOLD_DIR := plugins/aln_fold_wavefolder
DISTORTION_DIR := plugins/aln_distortion_bank

HOST_FLAGS := -std=c++11 -O2 -Wall -Wextra -Werror -fno-exceptions -fno-rtti
ARM_ARCH := -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb
ARM_FLAGS := -std=c++11 $(ARM_ARCH) -Os -fPIC -ffunction-sections \
	-fdata-sections -fno-exceptions -fno-rtti -fno-unwind-tables \
	-fno-asynchronous-unwind-tables -Wall -Wextra -Werror

FOLD_TEST := $(BUILD_DIR)/test_aln_fold_wavefolder
DISTORTION_TEST := $(BUILD_DIR)/test_aln_distortion_bank
FOLD_COMPILED := $(BUILD_DIR)/aln_fold_wavefolder_compiled.o
DISTORTION_COMPILED := $(BUILD_DIR)/aln_distortion_bank_compiled.o
FOLD_OBJECT := $(BUILD_DIR)/aln_fold_wavefolder.o
DISTORTION_OBJECT := $(BUILD_DIR)/aln_distortion_bank.o

.PHONY: all check-api test hardware inspect verify package clean

all: verify

check-api:
	@test -f "$(API_HEADER)" || { \
		echo "Missing $(API_HEADER). Clone with --recurse-submodules or run git submodule update --init --recursive." >&2; \
		exit 1; \
	}

$(BUILD_DIR):
	mkdir -p "$@"

$(FOLD_TEST): $(FOLD_DIR)/test.cpp $(FOLD_DIR)/aln_fold_wavefolder.cpp \
		$(FOLD_DIR)/aln_fold_core.h $(FOLD_DIR)/generated/aln_fold_models.h | check-api $(BUILD_DIR)
	$(HOST_CXX) $(HOST_FLAGS) -I"$(API_INCLUDE)" -I"$(FOLD_DIR)" \
		$(FOLD_DIR)/test.cpp $(FOLD_DIR)/aln_fold_wavefolder.cpp -o "$@"

$(DISTORTION_TEST): $(DISTORTION_DIR)/test.cpp $(DISTORTION_DIR)/aln_distortion_bank.cpp \
		$(DISTORTION_DIR)/aln_distortion_core.h $(DISTORTION_DIR)/generated/aln_distortion_models.h | check-api $(BUILD_DIR)
	$(HOST_CXX) $(HOST_FLAGS) -I"$(API_INCLUDE)" -I"$(DISTORTION_DIR)" \
		$(DISTORTION_DIR)/test.cpp $(DISTORTION_DIR)/aln_distortion_bank.cpp -o "$@"

test: $(FOLD_TEST) $(DISTORTION_TEST)
	./$(FOLD_TEST)
	./$(DISTORTION_TEST)

$(FOLD_COMPILED): $(FOLD_DIR)/aln_fold_wavefolder.cpp $(FOLD_DIR)/aln_fold_core.h \
		$(FOLD_DIR)/generated/aln_fold_models.h | check-api $(BUILD_DIR)
	$(ARM_CXX) $(ARM_FLAGS) -I"$(API_INCLUDE)" -I"$(FOLD_DIR)" -c "$<" -o "$@"

$(DISTORTION_COMPILED): $(DISTORTION_DIR)/aln_distortion_bank.cpp \
		$(DISTORTION_DIR)/aln_distortion_core.h $(DISTORTION_DIR)/generated/aln_distortion_models.h | check-api $(BUILD_DIR)
	$(ARM_CXX) $(ARM_FLAGS) -I"$(API_INCLUDE)" -I"$(DISTORTION_DIR)" -c "$<" -o "$@"

$(FOLD_OBJECT): $(FOLD_COMPILED)
	$(ARM_CXX) $(ARM_ARCH) -nostdlib -Wl,--relocatable "$<" -o "$@"

$(DISTORTION_OBJECT): $(DISTORTION_COMPILED)
	$(ARM_CXX) $(ARM_ARCH) -nostdlib -Wl,--relocatable "$<" -o "$@"

hardware: $(FOLD_OBJECT) $(DISTORTION_OBJECT)

inspect: hardware
	ARM_NM="$(ARM_NM)" ARM_READELF="$(ARM_READELF)" ARM_SIZE="$(ARM_SIZE)" \
		./scripts/inspect_object.sh $(FOLD_OBJECT) $(DISTORTION_OBJECT)

verify: test inspect

package: verify
	./scripts/package_release.sh

clean:
	rm -rf -- "$(BUILD_DIR)" "$(RELEASE_DIR)"
