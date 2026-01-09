CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -O2
INCLUDES = -Iinclude
SRCDIR = src
INCDIR = include
TARGET = build/main.exe

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:.c=.o)

$(TARGET): build $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(INCLUDES) $(CFLAGS)
	@echo "Build completed"
	# Remove object files after build
	@rm -f $(OBJECTS)

%.o: %.c
	$(CC) -c $< -o $@ $(INCLUDES) $(CFLAGS)

build:
	mkdir -p build

clean:
	rm -rf build/
	rm -rf $(OBJECTS)

INSTALL_DIR = $(USERPROFILE)\bin

install: $(TARGET)
	@mkdir -p "$(INSTALL_DIR)"
	cp $(TARGET) "$(INSTALL_DIR)\wcrontab.exe"
	@echo "Installed to $(INSTALL_DIR)\wcrontab.exe"

uninstall:
	rm -f "$(INSTALL_DIR)\wcrontab.exe"

run: $(TARGET)
	./$(TARGET)

.PHONY: clean install uninstall run
