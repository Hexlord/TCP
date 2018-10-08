
/*
@author(Alexander Knorre)
*/


#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORTION 9
#define DEFAULT_CLIENT_COUNT 4
#define DEFAULT_PORT "27015"

struct socket_info_t 
{

	CHAR buffer[DEFAULT_BUFLEN]{ 0 };
	WSABUF dataBuffer;
	SOCKET socket{ INVALID_SOCKET };
	OVERLAPPED overlapped;
	DWORD bytesSend{ 0 };
	DWORD bytesReceive{ 0 };

};

struct server_info_t
{
	int clientCount{ 0 };
	socket_info_t* socketArray[DEFAULT_CLIENT_COUNT];
};

server_info_t g_server_info;

void kill_client(int n)
{
	if (n > g_server_info.clientCount ||
		!g_server_info.socketArray[n]) return;
	auto& sock = g_server_info.socketArray[n]->socket;
	if (sock == INVALID_SOCKET ||
		sock == SOCKET_ERROR) return;
	printf("shutting down client socket #%d\n", n);
	int iResult = shutdown(sock, SD_BOTH);
	if (iResult == SOCKET_ERROR) {
		printf("socket #%d still in use\n", n);
	}
	closesocket(sock);
	sock = INVALID_SOCKET;
}

bool startsWith(const char* str, const char* starts)
{
	if (str == 0 || starts == 0) return false;
	auto length = strlen(starts);
	if (strlen(str) < length) return false;
	for (auto i = 0; i < length; ++i)
	{
		if (str[i] != starts[i]) return false;
	}
	return true;
}

int readn(SOCKET sock, char* buf, int buflen, int _n)
{
	ZeroMemory(buf, buflen);
	int iResult;
	int n = 0;
	while (n < _n)
	{
		iResult = recv(sock, buf + n, _n - n, 0);
		if (iResult > 0) {
			n += iResult;
		}
		else if (iResult == 0)
		{
			printf("connection terminated by remote user...\n");
			return SOCKET_ERROR;
		}
		else {
			printf("recv failed with error: %d\n", WSAGetLastError());
			return SOCKET_ERROR;
		}
	}
	return n;
}


bool create_client(SOCKET s)
{
	if (g_server_info.clientCount >= DEFAULT_CLIENT_COUNT)
	{
		printf("too many clients error\n");
		return false;
	}
	auto& info = g_server_info.socketArray[g_server_info.clientCount];
	info = static_cast<socket_info_t*>(GlobalAlloc(GPTR, sizeof(socket_info_t)));
	if (!info)
	{
		printf("could not perform global allocation\n");
		return false;
	}
	info->socket = s;
	info->bytesSend = 0;
	info->bytesReceive = 0;
	++g_server_info.clientCount;
	return true;
}

DWORD WINAPI listen_thread(void* data)
{
	SOCKET server_sock{ *static_cast<SOCKET*>(data) };

	ULONG nonBlock{ 1 };

	auto result = ioctlsocket(server_sock, FIONBIO, &nonBlock);
	if (result == SOCKET_ERROR)
	{
		printf("ioctlsocket failure\n");
		goto terminate;
	}

	while (true)
	{
		FD_SET writeSet;
		FD_SET readSet;
		FD_ZERO(&readSet);
		FD_ZERO(&writeSet);

		FD_SET(server_sock, &readSet);

		for (int i = 0; i < g_server_info.clientCount; ++i)
		{
			auto info = g_server_info.socketArray[i];
			SOCKET sock = info->socket;

			if (info->bytesReceive > info->bytesSend)
			{
				FD_SET(sock, &writeSet);
			}
			else
			{
				FD_SET(sock, &readSet);
			}
		}

		DWORD total = select(0, &readSet, &writeSet, nullptr, nullptr);
		if (total == SOCKET_ERROR)
		{
			printf("socket selection failure\n");
			goto exit;
		}

		if (FD_ISSET(server_sock, &readSet))
		{
			--total;
			auto acceptSocket = accept(server_sock, nullptr, nullptr);
			if (acceptSocket != INVALID_SOCKET)
			{
				nonBlock = 1;

				if (ioctlsocket(acceptSocket, FIONBIO, &nonBlock) == SOCKET_ERROR)
				{
					printf("socket selection accept failure\n");
					goto exit;
				}

				create_client(acceptSocket);
			}
			else
			{
				if (WSAGetLastError() != WSAEWOULDBLOCK)
				{
					printf("socket select accept failure\n");
					goto exit;
				}
			}
		}

		for (int i = 0; total > 0 && i < g_server_info.clientCount; ++i)
		{
			auto info = g_server_info.socketArray[i];
			SOCKET sock = info->socket;

			if (FD_ISSET(sock, &readSet))
			{
				--total;

				info->dataBuffer.buf = info->buffer;
				info->dataBuffer.len = DEFAULT_BUFLEN;

				DWORD flags{ 0 };
				DWORD recvBytes{ 0 };
				if (WSARecv(sock, &info->dataBuffer, 1, &recvBytes, &flags, nullptr, nullptr) == SOCKET_ERROR)
				{
					if (WSAGetLastError() != WSAEWOULDBLOCK)
					{
						printf("socket select receive failure, killing client %d\n", i);
						kill_client(i);
					}
					continue;
				}
				else
				{
					info->bytesReceive = recvBytes;
					if (recvBytes == 0)
					{
						printf("remote user closed connection, killing client %d\n", i);
						kill_client(i);
						continue;
					}
				}
			}

			if (FD_ISSET(sock, &writeSet))
			{
				--total;

				info->dataBuffer.buf = info->buffer + info->bytesSend;
				info->dataBuffer.len = info->bytesReceive - info->bytesSend;

				DWORD sendBytes{ 0 };
				if (WSASend(sock, &info->dataBuffer, 1, &sendBytes, 0, nullptr, nullptr) == SOCKET_ERROR)
				{
					if (WSAGetLastError() != WSAEWOULDBLOCK)
					{
						printf("socket select send failure, killing client %d\n", i);
						kill_client(i);
					}
					continue;
				}
				else
				{
					info->bytesSend += sendBytes;
					if (info->bytesSend == info->bytesReceive)
					{
						info->bytesSend = 0;
						info->bytesReceive = 0;
					}
				}
			}
		}
	}

exit:

	for (int i = 0; i < DEFAULT_CLIENT_COUNT; ++i)
	{
		kill_client(i);
	}

terminate:

	delete ((SOCKET*)data);

	return 0;
}

int main(void)
{
	HANDLE lthread;
	WSADATA wsaData;

	int iResult;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	// Create a SOCKET for connecting to server
	SOCKET server_sock = WSASocketW(AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (server_sock == INVALID_SOCKET) {
		printf("server socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	SOCKADDR_IN InternetAddr;
	InternetAddr.sin_family = AF_INET;
	InternetAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	InternetAddr.sin_port = htons(27015);

	// Setup the TCP listening socket
	printf("listening for port %s...\n", DEFAULT_PORT);
	iResult = bind(server_sock, (PSOCKADDR)&InternetAddr, sizeof(InternetAddr));
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		closesocket(server_sock);
		server_sock = INVALID_SOCKET;
		WSACleanup();
		return 1;
	}

	iResult = listen(server_sock, 5);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(server_sock);
		server_sock = INVALID_SOCKET;
		WSACleanup();
		return 1;
	}


	SOCKET* params = new SOCKET;
	*params = server_sock;

	lthread = CreateThread(NULL, 0, listen_thread, params, 0, NULL);

	char cmdbuf[DEFAULT_BUFLEN] = { 0 };

	while (true)
	{
		gets_s(cmdbuf, DEFAULT_BUFLEN);

		if (strcmp(cmdbuf, "exit") == 0)
		{
			break;
		}
		else if (strlen(cmdbuf) == 0)
		{
			;
		}
		else if (strcmp(cmdbuf, "list") == 0)
		{
			printf("connection list:\n");
			for (int i = 0; i < g_server_info.clientCount; ++i)
			{
				auto sock = g_server_info.socketArray[i]->socket;
				if (sock != INVALID_SOCKET &&
					sock != SOCKET_ERROR)
				{
					printf("%ld: remote user connected on socket %lld\n", i, sock);
				}
			}
		}
		else if (startsWith(cmdbuf, "kill"))
		{
			char num[16] = { 0 };
			strcpy_s(num, 15, cmdbuf + 4);
			if (strlen(num) == 0) return false;
			int n = atoi(num);
			kill_client(n);
		}
		else
		{
			printf("Unrecognized command. Use 'exit' to exit, 'kill 0..%d' to disconnect client, 'list' to list all clients\n", DEFAULT_CLIENT_COUNT - 1);
		}
	}


	printf("shutting down server socket...\n");

	// shutdown the connection since we're done
	iResult = shutdown(server_sock, SD_BOTH);
	if (iResult == SOCKET_ERROR) {
		printf("server socket still in use\n");
	}
	closesocket(server_sock);

	WaitForSingleObject(lthread, INFINITE);

	WSACleanup();

	printf("main thread finished\n");

	getchar();
	return 0;
}
