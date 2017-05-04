all:
	g++ -std=c++1y -o server server.cpp
	g++ -std=c++1y -o client client.cpp
clean:
	rm -r Client* Server
	rm *.out server client
