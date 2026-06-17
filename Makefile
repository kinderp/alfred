# =============================================================================
# Makefile
# Build system for alfred
# =============================================================================

# -----------------------------------------------------------------------------
# PROJECT
# -----------------------------------------------------------------------------

TARGET      := alfred
MODULES     ?= inotify

# -----------------------------------------------------------------------------
# DIRECTORIES
# -----------------------------------------------------------------------------

APP_DIR     := app
CORE_DIR    := core
MODULE_DIR  := modules
BUILD_DIR   := build

OBJ_DIR     := $(BUILD_DIR)/obj/core

APP_INC_DIR := $(APP_DIR)/include
CORE_INC_DIR := $(CORE_DIR)/include
CORE_PRIVATE_INC_DIR := $(CORE_DIR)/src

# -----------------------------------------------------------------------------
# COMPILER
# -----------------------------------------------------------------------------

CC          := gcc

# -----------------------------------------------------------------------------
# FLAGS
# -----------------------------------------------------------------------------

C_STANDARD  := -std=gnu99

WARNINGS    := \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Wshadow \
	-Wconversion \
	-Wsign-conversion \
	-Wformat=2 \
	-Wundef \
	-Wnull-dereference \
	-Wdouble-promotion \
	-Wimplicit-fallthrough

DEBUG_FLAGS := \
	-g3 \
	-O0 \
	-DDEBUG

RELEASE_FLAGS := \
	-O2 \
	-DNDEBUG

SANITIZERS  := \
	-fsanitize=address \
	-fsanitize=undefined

DEPFLAGS    := -MMD -MP
DEFINES     :=

INCLUDES    := \
	-I$(APP_INC_DIR) \
	-I$(CORE_INC_DIR) \
	-I$(CORE_PRIVATE_INC_DIR)

CFLAGS      = \
	$(C_STANDARD) \
	$(WARNINGS) \
	$(DEBUG_FLAGS) \
	$(SANITIZERS) \
	$(DEPFLAGS) \
	$(DEFINES) \
	$(INCLUDES)

LDFLAGS     := \
	-fsanitize=address \
	-fsanitize=undefined

# -----------------------------------------------------------------------------
# SOURCES
# -----------------------------------------------------------------------------

APP_SRCS := \
	$(APP_DIR)/src/main.c \
	$(APP_DIR)/src/app.c \
	$(APP_DIR)/src/core_logger.c \
	$(APP_DIR)/src/config.c \
	$(APP_DIR)/src/fs_scanner.c \
	$(APP_DIR)/src/logger.c \
	$(APP_DIR)/src/utils.c

CORE_SRCS := \
	$(CORE_DIR)/src/alfred_correlator.c \
	$(CORE_DIR)/src/alfred_record_adapter.c \
	$(CORE_DIR)/src/alfred_record_diagnostic.c \
	$(CORE_DIR)/src/alfred_tables.c \
	$(CORE_DIR)/src/alfred_utils.c

MODULE_SRCS :=

ifneq ($(filter inotify,$(MODULES)),)
INCLUDES += -I$(MODULE_DIR)/inotify/include
DEFINES += -DALFRED_ENABLE_INOTIFY
MODULE_SRCS += \
	$(MODULE_DIR)/inotify/src/inotify_adapter.c \
	$(MODULE_DIR)/inotify/src/inotify_backend.c \
	$(MODULE_DIR)/inotify/src/inotify_config.c \
	$(MODULE_DIR)/inotify/src/watch_manager.c \
	$(MODULE_DIR)/inotify/src/watcher.c
endif

SRCS := $(APP_SRCS) $(CORE_SRCS) $(MODULE_SRCS)

# -----------------------------------------------------------------------------
# OBJECTS
# -----------------------------------------------------------------------------

OBJS := $(SRCS:%.c=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)
OBJ_DIRS := $(sort $(dir $(OBJS)))

# -----------------------------------------------------------------------------
# COLORS
# -----------------------------------------------------------------------------

GREEN  := \033[0;32m
YELLOW := \033[1;33m
RED    := \033[0;31m
BLUE   := \033[0;34m
RESET  := \033[0m

# -----------------------------------------------------------------------------
# DEFAULT TARGET
# -----------------------------------------------------------------------------

all: banner directories $(TARGET)

# -----------------------------------------------------------------------------
# LINK
# -----------------------------------------------------------------------------

$(TARGET): FORCE $(OBJS)
	@printf "$(BLUE)[LINK]$(RESET) %s\n" "$(TARGET)"
	@$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@printf "$(GREEN)[OK]$(RESET) build completed\n"

FORCE:

# -----------------------------------------------------------------------------
# COMPILE
# -----------------------------------------------------------------------------

$(OBJ_DIR)/%.o: %.c
	@printf "$(YELLOW)[CC]$(RESET) %s\n" "$<"
	@$(CC) $(CFLAGS) -c $< -o $@

# -----------------------------------------------------------------------------
# DIRECTORIES
# -----------------------------------------------------------------------------

directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(OBJ_DIRS)

# -----------------------------------------------------------------------------
# CLEAN
# -----------------------------------------------------------------------------

clean:
	@printf "$(RED)[CLEAN]$(RESET) removing objects\n"
	@rm -rf $(BUILD_DIR)

fclean: clean
	@printf "$(RED)[FCLEAN]$(RESET) removing binary\n"
	@rm -f $(TARGET)

re: fclean all

# -----------------------------------------------------------------------------
# RELEASE BUILD
# -----------------------------------------------------------------------------

release: CFLAGS = \
	$(C_STANDARD) \
	$(WARNINGS) \
	$(RELEASE_FLAGS) \
	$(DEPFLAGS) \
	$(DEFINES) \
	$(INCLUDES)

release: LDFLAGS :=
release: re

# -----------------------------------------------------------------------------
# RUN
# -----------------------------------------------------------------------------

run: all
	@printf "$(BLUE)[RUN]$(RESET)\n"
	@./$(TARGET)

# -----------------------------------------------------------------------------
# TEST
# test-core is the official core end-to-end suite.
# test-backend-diagnostics checks backend health logs that are not semantics.
# test is the official core alias.
# -----------------------------------------------------------------------------

test: test-core

test-core:
	$(MAKE) all
	cd tests/core && bash run_all.sh

test-backend-diagnostics:
	$(MAKE) all
	cd tests/backend && bash run_all.sh

test-scanner:
	cd tests/scanner && bash run_all.sh

test-watcher:
	cd tests/watcher && bash run_all.sh

perf-lost-scope:
	cd tests/perf && bash run_lost_scope_recovery.sh

# -----------------------------------------------------------------------------
# VALGRIND
# -----------------------------------------------------------------------------

valgrind: all
	valgrind \
		--leak-check=full \
		--show-leak-kinds=all \
		--track-origins=yes \
		./$(TARGET)

# -----------------------------------------------------------------------------
# GDB
# -----------------------------------------------------------------------------

gdb: all
	gdb ./$(TARGET)

# -----------------------------------------------------------------------------
# FORMAT
# -----------------------------------------------------------------------------

format:
	clang-format -i \
		$(APP_DIR)/src/*.c \
		$(APP_DIR)/include/*.h \
		$(CORE_DIR)/src/*.c \
		$(CORE_DIR)/include/*.h \
		$(MODULE_DIR)/inotify/src/*.c \
		$(MODULE_DIR)/inotify/include/*.h

# -----------------------------------------------------------------------------
# STATIC ANALYSIS
# -----------------------------------------------------------------------------

scan:
	cppcheck \
		--enable=all \
		--inconclusive \
		--std=gnu99 \
		$(INCLUDES) \
		$(APP_DIR)/src \
		$(CORE_DIR)/src \
		$(MODULE_DIR)/inotify/src

# -----------------------------------------------------------------------------
# CLANG TIDY
# -----------------------------------------------------------------------------

tidy:
	clang-tidy \
		$(APP_DIR)/src/*.c \
		$(CORE_DIR)/src/*.c \
		$(MODULE_DIR)/inotify/src/*.c \
		-- $(INCLUDES)

# -----------------------------------------------------------------------------
# BANNER
# -----------------------------------------------------------------------------

banner:
	@printf "$(GREEN)"
	@printf "=========================================\n"
	@printf " Alfred - Filesystem Event Engine\n"
	@printf " Professional Linux inotify Engine\n"
	@printf "=========================================\n"
	@printf "$(RESET)"

# -----------------------------------------------------------------------------
# PHONY
# -----------------------------------------------------------------------------

.PHONY: \
	all \
	clean \
	fclean \
	re \
	release \
	run \
	test \
	test-core \
	test-scanner \
	test-watcher \
	test-backend-diagnostics \
	perf-lost-scope \
	valgrind \
	gdb \
	format \
	scan \
	tidy \
	banner \
	directories \
	FORCE

-include $(DEPS)
