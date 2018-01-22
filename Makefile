NAME      = ledscron

PKGS      = gio-unix-2.0 x11

INCS     := $(shell pkg-config --cflags $(PKGS)) -I./
CFLAGS   := -std=c11 -ggdb -W -Wall -Wextra -Wno-unused-parameter $(INCS) $(CFLAGS) -DNAME='"$(NAME)"'

LIBS     := $(shell pkg-config --libs $(PKGS))
LDFLAGS  := $(LIBS) $(LDFLAGS) -Wl,--export-dynamic

SRCS  = $(wildcard *.c)
HEADS = $(wildcard *.h)
OBJS  = $(foreach obj,$(SRCS:.c=.o),$(obj))

$(NAME): $(OBJS)
	@echo $(CC) -o $@ $(OBJS)
	@$(CC) -o $@ $(OBJS) $(LDFLAGS)

$(OBJS): $(HEADS)

.c.o:
	@echo $(CC) -c $< -o $@
	@$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

clean:
	rm -f $(NAME) $(OBJS)

all: $(NAME)

