#pragma once
#ifndef CLIENT_H
#define CLIENT_H

#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_

#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <combaseapi.h>
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <locale>
#include <codecvt>
#include "Resource.h"

class Client {
private:
	SOCKET serverConnection;
	addrinfo servInfo;
	std::string clientName;
	HWND hwndChatDisplay = NULL;

	void ConnectToServer(const char* hostname, const char* port)
	{
		int status;
		struct addrinfo hints;
		struct addrinfo* servInfo;
		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = 0;

		if ((status = getaddrinfo(hostname, port, &hints, &servInfo)) != 0)
		{
			// getaddrinfo failed
			std::cout << "Failed to get address info.\n";
			return;
		}

		serverConnection = socket(servInfo->ai_family, servInfo->ai_socktype, servInfo->ai_protocol);
		if (serverConnection < 0)
		{
			// socket creation failed
			freeaddrinfo(servInfo);
			std::cout << "Failed to create socket.\n";
			return;
		}

		int connectResult = connect(serverConnection, servInfo->ai_addr, servInfo->ai_addrlen);
		if (connectResult != 0)
		{
			std::cout << "Failed to connect to server.\n";
		}
		else
		{
			std::cout << "Connected to server successfully.\n";
		}

		freeaddrinfo(servInfo);
	}

	void AppendTextToDisplay(const wchar_t* text)
	{
		int len = GetWindowTextLength(hwndChatDisplay);
		SendMessage(hwndChatDisplay, EM_SETSEL, (WPARAM)len, (LPARAM)len);
		SendMessage(hwndChatDisplay, EM_REPLACESEL, 0, (LPARAM)text);
	}
public:
	bool connectedToServer = false;

	void SetMessageTarget(HWND hwnd)
	{
		hwndChatDisplay = hwnd;
	}

	void PollReceivingMessages(int timeoutMs)
	{
		pollfd serverFd;
		serverFd.fd = serverConnection;
		serverFd.events = POLLIN;
		int numOfEvents = WSAPoll(&serverFd, 1, timeoutMs);

		if (serverFd.revents & POLLIN)
		{
			char buffer[1024] = {};
			int recvResult = recv(serverFd.fd, buffer, 1024, 0);
			if (recvResult == 0)
			{
				std::cout << "Server closed the connection." << std::endl;
			}
			else if (recvResult > 0)
				if (hwndChatDisplay != NULL)
				{
					std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
					std::wstring wmsg = converter.from_bytes(buffer);
					AppendTextToDisplay(wmsg.c_str());
					AppendTextToDisplay(L"\r\n");
				}
				else
				{
					std::cout << buffer << std::endl;
				}
			else
			{
				// error occurred?
			}
		}
	}

	void SendMessageToServer(std::string msg)
	{
		std::string fullMsg = clientName + ": " + msg;
		send(serverConnection, fullMsg.c_str(), fullMsg.size() + 1, 0);
	}

	void SendMessageToServer(const std::wstring wmsg)
	{
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		std::string msg = converter.to_bytes(wmsg);

		SendMessageToServer(msg);
	}

	void CloseConnectionToServer()
	{
		send(serverConnection, "SHUTDOWN", 9, 0);
		shutdown(serverConnection, SD_SEND);
		closesocket(serverConnection);

		connectedToServer = false;
	}

	Client(std::string clientName, const char* hostname, const char* port)
	{
		ConnectToServer(hostname, port);

		int bindResult = bind(serverConnection, servInfo.ai_addr, servInfo.ai_addrlen);
		int listenResult = listen(serverConnection, SOMAXCONN);

		this->clientName = clientName;
		connectedToServer = true;
	}

	~Client()
	{
		closesocket(serverConnection);
	}
};

#endif
