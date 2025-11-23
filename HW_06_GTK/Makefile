CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11
GTK_CFLAGS = `pkg-config --cflags gtk4`
GTK_LIBS = `pkg-config --libs gtk4`

all:
	@mkdir -p ./out
	$(CC) $(GTK_CFLAGS) -o ./out/treeview-v3 treeview-v3.c $(GTK_LIBS)
