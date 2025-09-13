TARGET := writer
SRCS   := writer.c

# Object files for each arch
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

# ============== Linking Step ==============
$(TARGET).aarch64: $(OBJS_AARCH64)
	$(CC_aarch64) $^ -o $(TARGET)

$(TARGET).x86: $(OBJS_X86)
	$(CC_x86) $^ -o $(TARGET)

# ============== Compile Step ==============
%.aarch64.o: %.c
	@echo "CC (aarch64) $<"
	$(CC_aarch64) $(CFLAGS) -c $< -o $@

%.x86.o: %.c
	@echo "CC (x86) $<"
	$(CC_x86) $(CFLAGS) -c $< -o $@

clean:
	@echo "Cleaning objects and executables"
	@rm -f *.o $(TARGET).aarch64 $(TARGET).x86 $(TARGET)
