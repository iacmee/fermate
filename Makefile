NAME = main
CC = cc
CPPFLAGS += -Iinclude -D_GNU_SOURCE
CFLAGS = -Wall -Wextra -Werror -g
LDLIBS += -lxkbcommon
SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,obj/%.o,$(SRC))
TEST_OBJ := $(patsubst src/%.c,obj/testing/%.o,$(SRC))
DEP := $(OBJ:.o=.d) $(TEST_OBJ:.o=.d)
all: $(NAME)
$(NAME): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) $(LDLIBS) -o $@
testing: main-test
main-test: $(TEST_OBJ)
	$(CC) $(LDFLAGS) $(TEST_OBJ) $(LDLIBS) -o $@
obj/testing/%.o: src/%.c
	@mkdir -p obj/testing
	$(CC) $(CPPFLAGS) -DENABLE_EMERGENCY_EXIT $(CFLAGS) -MMD -MP -c $< -o $@
obj/%.o: src/%.c
	@mkdir -p obj
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
clean:
	rm -rf obj
fclean: clean
	rm -f $(NAME) main-test
re: fclean
	$(MAKE) all
run r: all
	sudo ./$(NAME)
rerun: re
	$(MAKE) run
-include $(DEP)
.PHONY: all testing clean fclean re run r rerun
