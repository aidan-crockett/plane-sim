# Project Configurations
PROJECT_NAME = raylib_cross_platform
SRC_DIR = src
OBJ_DIR = obj
ASSETS_DIR = assets

# Sources
SRCS = $(SRC_DIR)/main.c
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Default Platform (Desktop)
PLATFORM ?= DESKTOP

# -----------------------------------------------------------------------------
# Configuration for NATIVE DESKTOP
# -----------------------------------------------------------------------------
ifeq ($(PLATFORM),DESKTOP)
    CC = gcc
    CFLAGS = -Wall -std=c99 -O2 -DPLATFORM_DESKTOP
    
    # Auto-detect OS for proper desktop linking
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
    endif
    ifeq ($(UNAME_S),Darwin) # macOS
        LIBS = -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
    endif
    ifeq ($(OS),Windows_NT) # Windows (MinGW)
        LIBS = -lraylib -lopengl32 -lgdi32 -lwinmm
    endif
    
    TARGET = $(PROJECT_NAME)
    BUILD_DIR = build/desktop
endif

# -----------------------------------------------------------------------------
# Configuration for HTML5 (EMSCRIPTEN)
# -----------------------------------------------------------------------------
ifeq ($(PLATFORM),WEB)
    CC = emcc
    CFLAGS = -Wall -std=c99 -Os -DPLATFORM_WEB -sUSE_GLFW=3 -sASYNCIFY
    
    # Bundle the assets folder if it exists
    ifneq ($(wildcard $(ASSETS_DIR)/.*),)
        CFLAGS += --preload-file $(ASSETS_DIR)@/$(ASSETS_DIR)
    endif
    
    # Raylib web library and forced html shell output
    LIBS = -lraylib
    TARGET = index.html
    BUILD_DIR = build/web
endif

# -----------------------------------------------------------------------------
# Build Rules
# -----------------------------------------------------------------------------
.PHONY: all clean setup

all: setup $(BUILD_DIR)/$(TARGET)

# Create compilation directories
setup:
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(BUILD_DIR)
	@# For native builds, copy assets alongside the binary so paths match up
	@ifeq ($(PLATFORM),DESKTOP)
		@if [ -d "$(ASSETS_DIR)" ]; then cp -r $(ASSETS_DIR) $(BUILD_DIR)/; fi
	@endif

# Link the executable/web app
$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(CFLAGS) $(LIBS)
	@echo "Build successful: $@ [PLATFORM=$(PLATFORM)]"

# Compile source files into object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -rf $(OBJ_DIR) build
