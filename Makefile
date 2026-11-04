files=helper.o main.o server.o workers.o socket.o request.o worker_queue.o http_helper.c
CC=gcc
CFLAGS=-Wall -Wextra


server.exe : $(files)
	$(CC) $(files) -o $@

%.o : %.c $(wildcard *.h)
	$(CC) $(CFLAGS) -c $< -o $@
