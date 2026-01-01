#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_

#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <thread>
#include <string>
#include <vector>

class Server {
private:
	// list of all connected clients
	std::vector<SOCKET> clientSockets;
	// list of all connected clients relative to the pollfd struct
	std::vector<pollfd> pollFds;
	// the socket the server is attached to
	SOCKET listeningSocket;

	// sends buffer to all clients except the sender
	void ReplicateClientMessage(SOCKET sender, char* buffer, int len)
	{
		for (SOCKET clientFd : clientSockets)
		{
			//if (clientFd == sender) continue;
			send(clientFd, buffer, len + 1, 0);
		}
			
	}
public:
	bool isRunning = true;

	void AddClient(SOCKET clientSocket)
	{
		clientSockets.push_back(clientSocket);
		pollfd pfd{};
		pfd.fd = clientSocket;
		pfd.events = POLLIN;
		pollFds.push_back(pfd);

		std::cout << "Registered new client. Total connected: " << (int)clientSockets.size() << std::endl;
	}

	void DisconnectClient(SOCKET clientSocket)
	{
		closesocket(clientSocket);

		for (int i = clientSockets.size() - 1; i >= 0; i--)
		{
			SOCKET sock = clientSockets[i];
			if (sock != clientSocket) continue;
			clientSockets.erase(clientSockets.begin() + i);
		}

		for (int i = pollFds.size() - 1; i >= 0; i--)
		{
			SOCKET pfd = pollFds[i].fd;
			if (pfd != clientSocket) continue;
			pollFds.erase(pollFds.begin() + i);
		}

		std::cout << "Disconnected client. Remaining clients: " << (int)clientSockets.size() << std::endl;

		// the server shuts off when the last connected client disconnects
		if (clientSockets.empty())
		{
			isRunning = false;
			std::cout << "No clients left. Server stopping." << std::endl;
		}
	}

	void PollClientMessages(int timeoutMs)
	{
		if (!isRunning || clientSockets.empty()) return;

		int numOfEvents = WSAPoll(pollFds.data(), static_cast<ULONG>(pollFds.size()), timeoutMs);

		std::vector<SOCKET> disconnected;

		for (pollfd& pfd : pollFds)
		{
			if (pfd.revents & POLLIN)
			{
				char buffer[1024] = {};
				int recvResult = recv(pfd.fd, buffer, 1024, 0);
				// catches any disconnected clients
				if (recvResult == 0)
					disconnected.push_back(pfd.fd);
				else if (recvResult > 0)
					// check for disconnect flag
					if (strcmp(buffer, "SHUTDOWN") == 0)
					{
						DisconnectClient(pfd.fd);
					}
					else
					{
						std::cout << "Received client message: " << buffer << std::endl;
						ReplicateClientMessage(pfd.fd, buffer, recvResult);
					}
				else
				{
					// error occurred with recv
				}
			}
		}

		for (SOCKET fd : disconnected)
			DisconnectClient(fd);
	}

	void PollAccepts()
	{
		if (!isRunning) return;

		pollfd pfd{};
		pfd.fd = listeningSocket;
		pfd.events = POLLIN;
		int numOfEvents = WSAPoll(&pfd, 1, 0);

		if (numOfEvents > 0)
		{
			sockaddr_storage clientAddr{};
			socklen_t addrSize = sizeof(struct sockaddr_storage);
			SOCKET connectingSocket = accept(listeningSocket, (struct sockaddr*)&clientAddr, &addrSize);
			AddClient(connectingSocket);
		}
	}

	Server()
	{
		int status;
		struct addrinfo hints;
		struct addrinfo* servInfo;
		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;

		if ((status = getaddrinfo(NULL, "25565", &hints, &servInfo)) != 0)
		{
			std::cout << "getaddrinfo failed.\n";
		}
		else
		{
			std::cout << "getaddrinfo succeeded.\n";
		}


		SOCKET serverSocket = socket(servInfo->ai_family, servInfo->ai_socktype, servInfo->ai_protocol);
		if (serverSocket < 0)
		{
			freeaddrinfo(servInfo);
		}
		listeningSocket = serverSocket;

		if (bind(serverSocket, servInfo->ai_addr, servInfo->ai_addrlen) == SOCKET_ERROR)
		{
			std::cout << "An error occurred binding the server to the socket.\n";
		}	
		if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
		{
			std::cout << "An error occurred listening on the server socket.\n";
		}
			

		freeaddrinfo(servInfo);
	}

	~Server()
	{
		for (SOCKET sock : clientSockets)
		{
			closesocket(sock);
		}
		closesocket(listeningSocket);
	}
};

void ServerTask()
{
	Server* ChatServer = new Server();
	std::cout << "Server is live!";

	while (ChatServer->isRunning)
	{
		ChatServer->PollClientMessages(100);
		ChatServer->PollAccepts();
	}

	delete ChatServer;
}

int WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
	WSADATA wsaData;
	FILE *fpOut, *fpIn;
	int freopenRes;

	// creates a console to be used as the logging interface for the server
	if (AllocConsole() == 0)
		return 1;
	if ((freopenRes = freopen_s(&fpOut, "CONOUT$", "w", stdout)) != 0)
		return 1;
	if ((freopenRes = freopen_s(&fpIn, "CONIN$", "r", stdin)) != 0)
		return 1;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cout << "WSAStartup failed.\n";
		return 1;
	}

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		std::cout << "Could not find a usable version of Winsock.dll\n";
		WSACleanup();
		return 1;
	}

	std::cout << "Launching server...\n";

	ServerTask();

	std::cout << "Server is assumed to be stopped.\n";

	WSACleanup();

	return 0;
}
