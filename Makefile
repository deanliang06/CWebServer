files=helper.o main.o server.o workers.o socket.o request.o worker_queue.o http_helper.o routing.o dyna_buffer.o
CC=gcc
CFLAGS=-Wall -Wextra


server.exe : $(files)
	$(CC) $(files) -o $@

%.o : %.c $(wildcard *.h)
	$(CC) $(CFLAGS) -c $< -o $@
