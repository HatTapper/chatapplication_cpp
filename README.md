# C++ Chat Application

A server-client chat application written in C++ that utilizes the Windows library and Winsock to create a TCP connection between multiple connected clients through a server medium.

The files for the server and client can be found in their respective folder. Compiling the program requires version 2.2 of the Windows Subsystem for Android in order to run Winsock as intended.

For running the program, the server executable must be executed before any clients are to connect. It will automatically bind to localhost on port 25565. To connect a client, run the client executable. Upon successful connection to the server, the client can freely send messages which will be replicated to all other connected clients.
