CC = gcc

main.o: main.c
	$(CC) -c -Wall -Werror $< -o $@