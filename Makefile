files=helper.o main.o config.h
CC=gcc


server.exe : $(files)
	$(CC) $^ -o $@

%.o : %c %h
	$(CC) $< -o $@