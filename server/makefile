TARGET ?= aesdsocket
SRCS   := aesdsocket.c

# Object files for each arch
OBJS_AARCH64 := $(SRCS:.c=.aarch64.o)
OBJS_X86     := $(SRCS:.c=.x86.o)

CC      	?= $(CROSS_COMPILE)gcc
CC_x86  	?= gcc
CFLAGS      ?= -Wall

LDFLAGS ?= -lpthread -lrt

.PHONY: all clean build aarch64 x86

all: build

ifeq ($(findstring aarch64,$(CC)),aarch64)
build: aarch64
else
build: x86
endif

# Arch targets build the final executable
aarch64: $(TARGET).aarch64
x86:     $(TARGET).x86

# ============== Linking Step ==============
$(TARGET).aarch64: $(OBJS_AARCH64)
	$(CC) $^ -o $(TARGET) $(LDFLAGS)

$(TARGET).x86: $(OBJS_X86)
	$(CC_x86) $^ -o $(TARGET) $(LDFLAGS)

# ============== Compile Step ==============
%.aarch64.o: %.c
	@echo "CC (aarch64) $<"
	$(CC) $(CFLAGS) -c $< -o $@ 

%.x86.o: %.c
	@echo "CC (x86) $<"
	$(CC_x86) $(CFLAGS) -c $< -o $@

clean:
	@echo "Cleaning objects and executables"
	@rm -f *.o $(TARGET).aarch64 $(TARGET).x86 $(TARGET)
