Reed Villanueva
EE367L Lab2 

To compile:
gcc ee367client.c -o ee367client.o;
gcc ee367server.c -o ee367server.o;

Running the program:
1. Start the server: 
	In the directory containing ee367server.o, enter
	./ee367server.o

2. Start the client:
	In the directory containing ee367client.o, enter
	./ee367client.o hostname
	replacing hostaname with the hostname of the machine.