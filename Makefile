ifdef DEBUG
    FLAGS += -DDEBUG 
else
    FLAGS += -DNDEBUG
endif

all:client server

server: server.cpp message.h noisy.o
	g++ -std=c++11 server.cpp noisy.o $(FLAGS) -g -o server

client: client.cpp message.h noisy.o
	g++ -std=c++11 client.cpp noisy.o $(FLAGS) -g -o client


clean: 
	rm client server noisy.o
