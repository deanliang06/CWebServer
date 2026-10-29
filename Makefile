files=helper.o main.o types.h
CC=gcc


server.exe : $(files)
	$(CC) $^ -o $@

%.o : %c %h
	$(CC) $< -o $@