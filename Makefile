CC     := gcc
CFLAGS := $(shell pkg-config --cflags libadwaita-1) -Wall -Wextra -O2
LDLIBS := $(shell pkg-config --libs libadwaita-1 libportal-gtk4) -lm

BUILD_DIR := build
SRCS := $(wildcard *.c)
OBJS := $(addprefix $(BUILD_DIR)/,$(SRCS:.c=.o))

regardemoica: $(OBJS)
	$(CC) $(LDLIBS) -o $@ $^

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -fr regardemoica $(BUILD_DIR)

.PHONY: clean