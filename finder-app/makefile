TARGET := writer
SRCS   := writer.c

# Object files for each arch (flat directory: just foo.o, not build/x86/foo.o)
OBJS_AARCH64 := $(SRCS:.c=.aarch64.o)
OBJS_X86     := $(SRCS:.c=.x86.o)

CC_aarch64 := aarch64-none-linux-gnu-gcc
CC_x86     := gcc
CFLAGS     := -Wall

.PHONY: all clean build aarch64 x86

all: build

ifeq ($(CROSS_COMPILE),aarch64-none-linux-gnu-)
build: aarch64
else
build: x86
endif

# Arch targets build the final executable
aarch64: $(TARGET).aarch64
x86:     $(TARGET).x86

# ===== Linking step =====
$(TARGET).aarch64: $(OBJS_AARCH64)
	$(CC_aarch64) $^ -o $(TARGET)

$(TARGET).x86: $(OBJS_X86)
	$(CC_x86) $^ -o $(TARGET)

# ===== Compile step =====
%.aarch64.o: %.c
	@echo "CC (aarch64) $<"
	$(CC_aarch64) $(CFLAGS) -c $< -o $@

%.x86.o: %.c
	@echo "CC (x86) $<"
	$(CC_x86) $(CFLAGS) -c $< -o $@

# ===== Clean =====
clean:
	@echo "Cleaning objects and executables"
	@rm -f *.o $(TARGET).aarch64 $(TARGET).x86 $(TARGET)

# BUILD_DIR := build
# TARGET := executable

# SRCS := writer.c

# # expand all source files to create the object files for them
# # for both build versions
# OBJS_AARCH64 := $(SRCS:%.c=$(BUILD_DIR)/aarch64/%.o)
# OBJS_X86 := $(SRCS:%.c=$(BUILD_DIR)/x86/%.o)

# # Specify build commands
# CCC_aarch64 := aarch64-none-linux-gnu-gcc
# CC_x86 := gcc

# # specify compiler flags
# CC_FLAGS := -Wall

# .PHONY: all clean build aarch64 x86

# all: build

# # check if the cross compile flag is set and make the target for the correct one
# ifeq ($(CROSS_COMPILE),aarch64-none-linux-gnu-)
# build: aarch64
# else
# build: x86
# endif

# # run the targets with all the object files
# aarch64: $(BUILD_DIR)/aarch64/$(TARGET)
# x86:     $(BUILD_DIR)/x86/$(TARGET)

# # ============== Linking Step ==============

# $(BUILD_DIR)/aarch64/$(TARGET): $(OBJS_AARCH64)
# 	$(CCC_aarch64) $^ -o $@

# $(BUILD_DIR)/x86/$(TARGET): $(OBJS_X86)
# 	$(CC_x86) $^ -o $@

# # ============== Compile Step ==============

# # for any of the .o files passed in get the % and make the object file and format the .c file again
# # also make sure that the build directory exists
# $(BUILD_DIR)/aarch64/%.o: %.c | $(BUILD_DIR)/aarch64
# 	@echo "CC (arch64) $<"
# 	$(CCC_aarch64) $(CC_FLAGS) -c $< -o $@

# # for any of the .o files passed in get the % and make the object file and format the .c file again
# # also make sure that the build directory exists
# $(BUILD_DIR)/x86/%.o: %.c | $(BUILD_DIR)/x86
# 	$(CC_x86) $(CC_FLAGS) -c $< -o $@

# # make the build directory
# $(BUILD_DIR)/aarch64 $(BUILD_DIR)/x86:
# 	@mkdir -p $@

# clean:
# 	@echo Cleaning Build Directory
# 	@rm -rf $(BUILD_DIR)/

