FLAGS = -O -Wall -pthread
CC = gcc
SODIUM = -lsodium
all:
	${CC} ${FLAGS} Functions/functions.c Functions/functions.h -c
	${CC} ${FLAGS} Server/server.c Server/server.h functions.o ${SODIUM} -o server
	${CC} ${FLAGS} Client/client.c Client/client.h functions.o ${SODIUM} -o client
	${CC} ${FLAGS} Proxy/ircproxy.c Proxy/ircproxy.h functions.o ${SODIUM} -o ircproxy
clean:
	rm server client ircproxy functions.o
