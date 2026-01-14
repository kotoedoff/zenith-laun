CC = gcc
CFLAGS = `pkg-config --cflags gtk+-3.0 vte-2.91` -O2 -Wall
LIBS = `pkg-config --libs gtk+-3.0 vte-2.91`

all: zenith-installer

zenith-installer: zenith_installer.c
	$(CC) -o zenith-installer zenith_installer.c $(CFLAGS) $(LIBS)

clean:
	rm -f zenith-installer

install: zenith-installer
	sudo cp zenith-installer /usr/local/bin/
	sudo chmod +x /usr/local/bin/zenith-installer

uninstall:
	sudo rm -f /usr/local/bin/zenith-installer

run: zenith-installer
	./zenith-installer

.PHONY: all clean install uninstall run
