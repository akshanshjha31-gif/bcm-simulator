# ==========================================================================
#  Automotive Body Control Module (BCM) Simulator - SIL harness
#  Target : native (MinGW-w64 / g++), NOT the STM32
#  Purpose: run the shipped control logic on a PC against scripted vehicle
#           scenarios, per the architecture document's SIL strategy.
#
#  The sources under test are compiled straight from firmware/services - the
#  harness links the same translation units as the firmware, so a scenario
#  that passes here exercises real code rather than a model of it.
# ==========================================================================

# The xPack `make` ships a built-in CXX default pointing at its own build
# machine, so `?=` is not enough - replace it whenever CXX was not set
# explicitly by the environment or the command line.
ifneq ($(filter default undefined,$(origin CXX)),)
CXX := x86_64-w64-mingw32-g++
endif

BUILD_DIR := build
TARGET    := $(BUILD_DIR)/bcm_sil.exe

DRIVERS   := ../firmware/drivers
SERVICES  := ../firmware/services

CXXFLAGS := -std=c++14 -O1 -g -MMD -MP \
            -Wall -Wextra -Wpedantic -Wshadow \
            -I$(DRIVERS) -I$(SERVICES) -I.

# Statically linked so the binary runs without MinGW's DLLs on PATH.
LDFLAGS := -static -static-libgcc -static-libstdc++

HARNESS_SRCS := sil_main.cpp \
                sil_runner.cpp \
                scenarios.cpp

# Production code under test.
UUT_SRCS := $(SERVICES)/bcm_core.cpp \
            $(SERVICES)/lighting_mgr.cpp \
            $(SERVICES)/door_mgr.cpp \
            $(SERVICES)/power_mgr.cpp \
            $(SERVICES)/lamp_arbiter.cpp \
            $(SERVICES)/fault_mgr.cpp \
            $(SERVICES)/logger.cpp \
            $(SERVICES)/diag_mgr.cpp

OBJS := $(addprefix $(BUILD_DIR)/,$(HARNESS_SRCS:.cpp=.o))
OBJS += $(addprefix $(BUILD_DIR)/,$(notdir $(UUT_SRCS:.cpp=.o)))

vpath %.cpp $(sort $(dir $(UUT_SRCS)))

.PHONY: all run list clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(BUILD_DIR):
	mkdir -p $@

## Run every scenario. Non-zero exit on any failure, so CI can gate on it.
run: $(TARGET)
	./$(TARGET)

list: $(TARGET)
	./$(TARGET) --list

clean:
	rm -rf $(BUILD_DIR)

-include $(wildcard $(BUILD_DIR)/*.d)
