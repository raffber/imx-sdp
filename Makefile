CFLAGS = -Wall -Wextra -Werror -std=gnu11 -DENABLE_UDEV
LDFLAGS = -Wall -Wextra -Werror

BIN = imx-sdp
BUILD_DIR = build
SRC = main.c stages.c steps.c sdp.c udev.c
OBJ = $(SRC:%.c=$(BUILD_DIR)/%.o)
DEP = $(OBJ:%.o=%.d)

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ -lhidapi-hidraw -ludev

-include $(DEP)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN)

.PHONY: clean
