.PHONY : all clean

all : client server

client : common.hpp client.cpp
	g++ client.cpp -o client -pthread

server : common.hpp server.cpp
	g++ server.cpp -o server -pthread -lcpprest -lcrypto -lssl -lboost_thread -lboost_chrono -lboost_system

clean:
	rm client server