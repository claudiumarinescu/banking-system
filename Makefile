CC=gcc
LIBSOCKET=-lnsl
CCFLAGS=-Wall -g
SRV=server
CLT=client
USERS=users_data_file

PORT = 8000
IP_SERVER = 127.0.0.1

all: $(SRV) $(CLT)

$(SRV):$(SRV).c
	$(CC) -o $(SRV) $(LIBSOCKET) $(SRV).c

$(CLT):	$(CLT).c
	$(CC) -o $(CLT) $(LIBSOCKET) $(CLT).c

# Ruleaza serverul
run_server:
	./${SRV} ${PORT} $(USERS)

# Ruleaza clientul 	
run_client:
	./${CLT} ${IP_SERVER} ${PORT}

clean:
	rm -f *.o *~
	rm -f $(SRV) $(CLT)


