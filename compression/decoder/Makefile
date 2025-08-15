# Makefile for Polygondwanaland Audio Decoder
# Builds both test program and GBA-ready object files

CC = gcc
GBA_CC = arm-none-eabi-gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
GBA_CFLAGS = -Wall -Wextra -O2 -std=c99 -mcpu=arm7tdmi -mthumb-interwork -fomit-frame-pointer

# Source files
DECODER_SRC = polygond_decoder.c
DECODER_HDR = polygond_decoder.h
TEST_SRC = test_decoder.c

# Output files
TEST_BIN = test_decoder
DECODER_OBJ = polygond_decoder.o
GBA_DECODER_OBJ = polygond_decoder_gba.o

.PHONY: all test clean gba

# Default target - build and run tests
all: test

# Build and run test program
test: $(TEST_BIN)
	@echo "Running decoder tests..."
	./$(TEST_BIN)

# Build test executable
$(TEST_BIN): $(TEST_SRC) $(DECODER_SRC) $(DECODER_HDR)
	@echo "Building test program..."
	$(CC) $(CFLAGS) -o $@ $(TEST_SRC) $(DECODER_SRC)

# Build standard object file
$(DECODER_OBJ): $(DECODER_SRC) $(DECODER_HDR)
	@echo "Building decoder object file..."
	$(CC) $(CFLAGS) -c -o $@ $(DECODER_SRC)

# Build GBA-optimized object file
$(GBA_DECODER_OBJ): $(DECODER_SRC) $(DECODER_HDR)
	@echo "Building GBA-optimized decoder..."
	$(GBA_CC) $(GBA_CFLAGS) -c -o $@ $(DECODER_SRC)

# GBA target (requires arm-none-eabi-gcc toolchain)
gba: $(GBA_DECODER_OBJ)
	@echo "GBA decoder built successfully"
	@echo "Link this object file with your GBA project"

# Check if GBA toolchain is available
check-gba-toolchain:
	@which arm-none-eabi-gcc > /dev/null || \
		(echo "Warning: GBA toolchain (arm-none-eabi-gcc) not found"; \
		 echo "Install devkitPro for GBA compilation"; \
		 exit 1)

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(TEST_BIN) $(DECODER_OBJ) $(GBA_DECODER_OBJ)

# Print build information
info:
	@echo "Polygondwanaland Audio Decoder Build System"
	@echo "============================================"
	@echo "Targets:"
	@echo "  all     - Build and run tests (default)"
	@echo "  test    - Build and run test program"
	@echo "  gba     - Build GBA-optimized object file"
	@echo "  clean   - Remove build artifacts"
	@echo "  info    - Show this information"
	@echo ""
	@echo "Requirements:"
	@echo "  gcc     - For test program compilation"
	@echo "  arm-none-eabi-gcc - For GBA compilation (optional)"