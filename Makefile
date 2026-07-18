# =============================================================================
# Makefile
# Build system for alfred
# =============================================================================

# -----------------------------------------------------------------------------
# PROJECT
# -----------------------------------------------------------------------------

TARGET      := alfred
MODULES     ?= inotify

# Public installation layout. BINDIR and MANDIR are logical paths; DESTDIR is
# prepended only when constructing the final destination.
PREFIX      ?= /usr/local
DESTDIR     ?=
BINDIR      ?= $(PREFIX)/bin
MANDIR      ?= $(PREFIX)/share/man
INSTALL     ?= install

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
	$(CORE_DIR)/src/alfred_backend_capabilities.c \
	$(CORE_DIR)/src/alfred_backend_ops.c \
	$(CORE_DIR)/src/alfred_correlator.c \
	$(CORE_DIR)/src/alfred_record_adapter.c \
	$(CORE_DIR)/src/alfred_record_counter_sink.c \
	$(CORE_DIR)/src/alfred_record_diagnostic.c \
	$(CORE_DIR)/src/alfred_record_dispatcher.c \
	$(CORE_DIR)/src/alfred_record_jsonl.c \
	$(CORE_DIR)/src/alfred_record_jsonl_sink.c \
	$(CORE_DIR)/src/alfred_record_jsonl_writer.c \
	$(CORE_DIR)/src/alfred_record_owned.c \
	$(CORE_DIR)/src/alfred_record_output_pipeline.c \
	$(CORE_DIR)/src/alfred_record_queue.c \
	$(CORE_DIR)/src/alfred_record_runtime.c \
	$(CORE_DIR)/src/alfred_record_sink.c \
	$(CORE_DIR)/src/alfred_record_text.c \
	$(CORE_DIR)/src/alfred_record_text_sink.c \
	$(CORE_DIR)/src/alfred_tables.c \
	$(CORE_DIR)/src/alfred_utils.c

MODULE_SRCS :=

ifneq ($(filter inotify,$(MODULES)),)
INCLUDES += -I$(MODULE_DIR)/inotify/include
DEFINES += -DALFRED_ENABLE_INOTIFY
MODULE_SRCS += \
	$(MODULE_DIR)/inotify/src/inotify_adapter.c \
	$(MODULE_DIR)/inotify/src/inotify_backend.c \
	$(MODULE_DIR)/inotify/src/inotify_backend_capabilities.c \
	$(MODULE_DIR)/inotify/src/inotify_backend_ops.c \
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

# Install sources are overridable so the focused suite can exercise failed
# preconditions without mutating tracked files in the checkout.
ALFRED_INSTALL_BINARY_SOURCE ?= $(TARGET)
ALFRED_MAN1_EN_SOURCE ?= docs/man/man1/alfred.1
ALFRED_MAN5_EN_SOURCE ?= docs/man/man5/alfred.conf.5
ALFRED_MAN7_EN_SOURCE ?= docs/man/man7/alfred-events.7
ALFRED_MAN1_IT_SOURCE ?= docs/man/it/man1/alfred.1
ALFRED_MAN5_IT_SOURCE ?= docs/man/it/man5/alfred.conf.5
ALFRED_MAN7_IT_SOURCE ?= docs/man/it/man7/alfred-events.7

ALFRED_INSTALL_BINARY_DEST = $(DESTDIR)$(BINDIR)/$(TARGET)
ALFRED_MAN1_EN_DEST = $(DESTDIR)$(MANDIR)/man1/alfred.1
ALFRED_MAN5_EN_DEST = $(DESTDIR)$(MANDIR)/man5/alfred.conf.5
ALFRED_MAN7_EN_DEST = $(DESTDIR)$(MANDIR)/man7/alfred-events.7
ALFRED_MAN1_IT_DEST = $(DESTDIR)$(MANDIR)/it/man1/alfred.1
ALFRED_MAN5_IT_DEST = $(DESTDIR)$(MANDIR)/it/man5/alfred.conf.5
ALFRED_MAN7_IT_DEST = $(DESTDIR)$(MANDIR)/it/man7/alfred-events.7

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
# INSTALLATION
# install is deliberately copy-only: callers build the desired profile first.
# -----------------------------------------------------------------------------

validate-install-layout:
	@set -eu; \
	for path in "$(BINDIR)" "$(MANDIR)"; do \
		case "$$path" in \
			/*) ;; \
			*) printf "install layout path must be absolute: %s\n" "$$path" >&2; exit 2 ;; \
		esac; \
		case "/$$path/" in \
			*/../*) printf "install layout path must not contain '..': %s\n" "$$path" >&2; exit 2 ;; \
		esac; \
	done; \
	if [ -n "$(DESTDIR)" ]; then \
		case "$(DESTDIR)" in \
			/*) ;; \
			*) printf "DESTDIR must be empty or absolute: %s\n" "$(DESTDIR)" >&2; exit 2 ;; \
		esac; \
		case "/$(DESTDIR)/" in \
			*/../*) printf "DESTDIR must not contain '..': %s\n" "$(DESTDIR)" >&2; exit 2 ;; \
		esac; \
	fi

install: validate-install-layout
	@set -eu; \
	if [ ! -f "$(ALFRED_INSTALL_BINARY_SOURCE)" ] || \
	   [ ! -r "$(ALFRED_INSTALL_BINARY_SOURCE)" ] || \
	   [ ! -x "$(ALFRED_INSTALL_BINARY_SOURCE)" ]; then \
		printf "install source must be a readable regular executable: %s\n" \
			"$(ALFRED_INSTALL_BINARY_SOURCE)" >&2; \
		exit 1; \
	fi; \
	for source in \
		"$(ALFRED_MAN1_EN_SOURCE)" \
		"$(ALFRED_MAN5_EN_SOURCE)" \
		"$(ALFRED_MAN7_EN_SOURCE)" \
		"$(ALFRED_MAN1_IT_SOURCE)" \
		"$(ALFRED_MAN5_IT_SOURCE)" \
		"$(ALFRED_MAN7_IT_SOURCE)"; do \
		if [ ! -f "$$source" ] || [ ! -r "$$source" ]; then \
			printf "manual source must be a readable regular file: %s\n" \
				"$$source" >&2; \
			exit 1; \
		fi; \
	done
	@$(INSTALL) -d \
		"$(DESTDIR)$(BINDIR)" \
		"$(DESTDIR)$(MANDIR)/man1" \
		"$(DESTDIR)$(MANDIR)/man5" \
		"$(DESTDIR)$(MANDIR)/man7" \
		"$(DESTDIR)$(MANDIR)/it/man1" \
		"$(DESTDIR)$(MANDIR)/it/man5" \
		"$(DESTDIR)$(MANDIR)/it/man7"
	@$(INSTALL) -m 0755 "$(ALFRED_INSTALL_BINARY_SOURCE)" "$(ALFRED_INSTALL_BINARY_DEST)"
	@$(INSTALL) -m 0644 "$(ALFRED_MAN1_EN_SOURCE)" "$(ALFRED_MAN1_EN_DEST)"
	@$(INSTALL) -m 0644 "$(ALFRED_MAN5_EN_SOURCE)" "$(ALFRED_MAN5_EN_DEST)"
	@$(INSTALL) -m 0644 "$(ALFRED_MAN7_EN_SOURCE)" "$(ALFRED_MAN7_EN_DEST)"
	@$(INSTALL) -m 0644 "$(ALFRED_MAN1_IT_SOURCE)" "$(ALFRED_MAN1_IT_DEST)"
	@$(INSTALL) -m 0644 "$(ALFRED_MAN5_IT_SOURCE)" "$(ALFRED_MAN5_IT_DEST)"
	@$(INSTALL) -m 0644 "$(ALFRED_MAN7_IT_SOURCE)" "$(ALFRED_MAN7_IT_DEST)"
	@printf "$(GREEN)[INSTALL]$(RESET) staged seven Alfred files\n"

uninstall: validate-install-layout
	@rm -f -- \
		"$(ALFRED_INSTALL_BINARY_DEST)" \
		"$(ALFRED_MAN1_EN_DEST)" \
		"$(ALFRED_MAN5_EN_DEST)" \
		"$(ALFRED_MAN7_EN_DEST)" \
		"$(ALFRED_MAN1_IT_DEST)" \
		"$(ALFRED_MAN5_IT_DEST)" \
		"$(ALFRED_MAN7_IT_DEST)"
	@printf "$(GREEN)[UNINSTALL]$(RESET) removed seven Alfred files\n"

# -----------------------------------------------------------------------------
# RUN
# -----------------------------------------------------------------------------

run: all
	@printf "$(BLUE)[RUN]$(RESET)\n"
	@./$(TARGET)

# -----------------------------------------------------------------------------
# TEST
# test-core is the official core end-to-end suite.
# test-cli checks user-facing informational commands and exit behavior.
# test-backend-diagnostics checks backend health logs that are not semantics.
# test-jsonl checks the structured external output contract.
# smoke-mvp runs the short user-facing MVP smoke recipe.
# test is the official core alias.
# -----------------------------------------------------------------------------

test: test-core

test-core:
	$(MAKE) all
	cd tests/core && bash run_all.sh

test-cli:
	$(MAKE) all
	cd tests/cli && bash run_all.sh

test-backend-diagnostics:
	$(MAKE) all
	cd tests/backend && bash run_all.sh

test-jsonl:
	$(MAKE) all
	cd tests/jsonl && bash run_all.sh

test-scanner:
	cd tests/scanner && bash run_all.sh

test-watcher:
	cd tests/watcher && bash run_all.sh

test-install:
	$(MAKE) release
	cd tests/install && bash run_all.sh

smoke-mvp: all
	cd tests/smoke && bash mvp_smoke.sh

perf:
	@printf "Manual Alfred performance targets\n"
	@printf "\n"
	@printf "make perf-record-sinks\n"
	@printf "  Measures synthetic alfred_record_t delivery to counter, text,\n"
	@printf "  JSONL sinks, queue/dispatcher paths, and the single-writer\n"
	@printf "  output pipeline. It isolates these costs from inotify,\n"
	@printf "  filesystem I/O, sockets, threads, and real backpressure.\n"
	@printf "\n"
	@printf "make perf-core-input\n"
	@printf "  Measures the raw-first core input baseline:\n"
	@printf "  synthetic alfred_raw_event_t -> alfred_process() ->\n"
	@printf "  semantic counter/no-op callback. It isolates core input\n"
	@printf "  cost from JSONL, text writers, queues, runtime I/O and\n"
	@printf "  the real inotify backend.\n"
	@printf "\n"
	@printf "make perf-runtime-output\n"
	@printf "  Measures the real Alfred runtime with filesystem activity,\n"
	@printf "  inotify, app callbacks, compatibility logs, and optional\n"
	@printf "  JSONL output_enabled=true. It complements the synthetic\n"
	@printf "  queue-dispatcher-jsonl and output-pipeline-jsonl rows.\n"
	@printf "\n"
	@printf "make perf-lost-scope\n"
	@printf "  Measures synthetic lost-scope recovery over generated\n"
	@printf "  directory trees with fake watch operations. It isolates\n"
	@printf "  scanner and recovery control flow from real\n"
	@printf "  inotify_add_watch() cost.\n"

perf-lost-scope:
	cd tests/perf && bash run_lost_scope_recovery.sh

perf-record-sinks:
	cd tests/perf && bash run_record_sinks.sh

perf-core-input:
	cd tests/perf && bash run_core_input.sh

perf-runtime-output: all
	cd tests/perf && bash run_runtime_output.sh

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
	validate-install-layout \
	install \
	uninstall \
	run \
	test \
	test-core \
	test-cli \
	test-jsonl \
	test-scanner \
	test-watcher \
	test-install \
	test-backend-diagnostics \
	smoke-mvp \
	perf \
	perf-lost-scope \
	perf-record-sinks \
	perf-core-input \
	valgrind \
	gdb \
	format \
	scan \
	tidy \
	banner \
	directories \
	FORCE

-include $(DEPS)
