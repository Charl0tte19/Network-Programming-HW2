.PHONY: clean
all: server client
server: server.c server.h
	gcc -o server server.c
client: client.c client.h
	gcc -o client client.c
clean:
	rm server client
