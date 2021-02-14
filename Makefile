CFLAGS := -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function
LDFLAGS := -lreadline

NAME := meon
BUILD_DIR := build
SOURCE_DIR := src
HEADER_DIR := includes

ifeq ($(MODE),debug)
	CFLAGS += -O0 -DDEBUG -g
else
	CFLAGS += -O3 -flto
endif

HEADERS := $(wildcard $(HEADER_DIR)/*.h)
SOURCES := $(wildcard $(SOURCE_DIR)/*.c)
OBJECTS := $(addprefix $(BUILD_DIR)/objects/, $(notdir $(SOURCES:.c=.o)))

default: clean $(BUILD_DIR)/$(NAME)

$(BUILD_DIR)/$(NAME): $(OBJECTS)
	@ printf "%8s %-16s %s\n" $(CC) $@ "-I $(HEADER_DIR) $(CFLAGS) $(LDFLAGS)"
	@ mkdir -p $(BUILD_DIR)
	@ $(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@
	@ rm -rf $(BUILD_DIR)/objects

$(BUILD_DIR)/objects/%.o: $(SOURCE_DIR)/%.c $(HEADERS)
	@ printf "%8s %-16s %s\n" $(CC) $< "-I $(HEADER_DIR) $(CFLAGS)"
	@ mkdir -p $(BUILD_DIR)/objects
	@ $(CC) -c $(CFLAGS) -I $(HEADER_DIR) -o $@ $<

clean:
	@ rm -rf $(BUILD_DIR)

.PHONY: default clean