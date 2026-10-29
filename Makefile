files=helper.o main.o types.h workers.o socket.o request.o
CC=gcc


server.exe : $(files)
	$(CC) $^ -o $@ -Wall -Wextra

%.o : %c %h
	$(CC) $< -o $@