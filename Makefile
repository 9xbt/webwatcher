CC = gcc
CFLAGS = -Wall -O2 $(shell pkg-config --cflags gtk4 libadwaita-1 webkitgtk-6.0)
LDFLAGS = $(shell pkg-config --libs gtk4 libadwaita-1 webkitgtk-6.0)
OUT = webwatcher
CFILES = $(shell find . -type f -name '*.c')
OBJECTS = $(patsubst %.c,bin/%.o,$(CFILES))

all: $(OUT)

run: all
	./webwatcher

bin/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $(OUT)

clean:
	rm -rf bin/ $(OUT)