# ==========================================================================
#  Automotive Body Control Module (BCM) Simulator - host unit tests
#  Target : native (MinGW-w64 / g++), NOT the STM32
#  Purpose: verify the HAL-free logic in the driver layer off-target,
#           per SRS-MNT-002 and the architecture document's dependency rule.
# ==========================================================================

# The xPack `make` ships a built-in CXX default pointing at its own build
# machine, so `?=` is not enough - replace it whenever CXX was not set
# explicitly by the environment or the command line.
ifneq ($(filter default undefined,$(origin CXX)),)
CXX := x86_64-w64-mingw32-g++
endif

BUILD_DIR := build
TARGET    := $(BUILD_DIR)/bcm_tests.exe

DRIVERS   := ../firmware/drivers
SERVICES  := ../firmware/services
VENDOR    := vendor
CATCH_HPP := $(VENDOR)/catch2/catch.hpp
CATCH_URL := https://github.com/catchorg/Catch2/releases/download/v2.13.10/catch.hpp

# Warnings are deliberately strict: this code also compiles for the target,
# where a silent narrowing bug is far more expensive to find.
# -MMD -MP so editing a header under test rebuilds the tests that include it.
CXXFLAGS := -std=c++14 -O1 -g -MMD -MP \
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
            -I$(DRIVERS) -I$(SERVICES) -I$(VENDOR)

# Test translation units.
SRCS := test_main.cpp \
        test_debounce.cpp \
        test_blink.cpp \
        test_ring_buffer.cpp \
        test_filter.cpp \
        test_soft_timer.cpp \
        test_crc8.cpp \
        test_frame_codec.cpp \
        test_dispatcher.cpp \
        test_icd_examples.cpp \
        test_fsm.cpp \
        test_lighting.cpp \
        test_door.cpp \
        test_arbiter.cpp \
        test_power_fault.cpp \
        test_logger_diag.cpp

# Production sources under test that are not header-only.
UUT_SRCS := $(SERVICES)/frame_codec.cpp \
            $(SERVICES)/dispatcher.cpp \
            $(SERVICES)/comm_mgr.cpp \
            $(SERVICES)/lighting_mgr.cpp \
            $(SERVICES)/door_mgr.cpp \
            $(SERVICES)/power_mgr.cpp \
            $(SERVICES)/lamp_arbiter.cpp \
            $(SERVICES)/fault_mgr.cpp \
            $(SERVICES)/logger.cpp \
            $(SERVICES)/diag_mgr.cpp

OBJS := $(addprefix $(BUILD_DIR)/,$(SRCS:.cpp=.o))
OBJS += $(addprefix $(BUILD_DIR)/,$(notdir $(UUT_SRCS:.cpp=.o)))

vpath %.cpp $(sort $(dir $(UUT_SRCS)))

.PHONY: all run deps clean

all: $(TARGET)

# Link statically so the test binary runs without MinGW's runtime DLLs being
# on PATH - otherwise it exits silently with no output.
LDFLAGS := -static -static-libgcc -static-libstdc++

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: %.cpp $(CATCH_HPP) | $(BUILD_DIR)
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(BUILD_DIR):
	mkdir -p $@

## Run the suite. Non-zero exit on any failure, so CI can gate on it.
run: $(TARGET)
	./$(TARGET)

## Catch2 is a single header, fetched not committed (same policy as the
## STM32 HAL - see the architecture document, section 7).
$(CATCH_HPP):
	@echo "Catch2 not found. Run: make deps"
	@false

deps:
	@mkdir -p $(VENDOR)/catch2
	@echo "Fetching Catch2 single-header..."
	@curl -fsSL $(CATCH_URL) -o $(CATCH_HPP) || \
	  powershell -NoProfile -Command \
	    "Invoke-WebRequest -Uri '$(CATCH_URL)' -OutFile '$(CATCH_HPP)' -UseBasicParsing"
	@echo "Catch2 ready."

clean:
	rm -rf $(BUILD_DIR)

-include $(wildcard $(BUILD_DIR)/*.d)
