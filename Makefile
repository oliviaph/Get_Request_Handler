#Olivia Houghton
#Makefile for Programming Assignment 1, CS371

myserver : Source.o AcceptTCPConnection.o CreateTCPServerSocket.o HandleTCPClient.o DieWithError.o
	g++ -o myserver Source.o AcceptTCPConnection.o CreateTCPServerSocket.o HandleTCPClient.o DieWithError.o
Source.o : Source.cpp TCPEchoServer.h
	g++ -c Source.cpp
AcceptTCPConnection.o : AcceptTCPConnection.c
	g++ -c AcceptTCPConnection.c
CreateTCPServerSocket.o : CreateTCPServerSocket.c
	g++ -c CreateTCPServerSocket.c
HandleTCPClient.o : HandleTCPClient.cpp
	g++ -c HandleTCPClient.cpp
DieWithError.o : DieWithError.c
	g++ -c DieWithError.c
clean:
	rm -f *.o
