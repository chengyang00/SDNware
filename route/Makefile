.PHONY : all clean

all : test test2

test : HTTPRequest.hpp test.cpp
	g++ test.cpp -o test

test2 : test2.cpp
	g++ test2.cpp -o test2 -pthread -lcpprest -lcrypto -lssl -lboost_thread -lboost_chrono -lboost_system
clean:
	rm test test2