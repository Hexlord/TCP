/*
@author(Alexander Knorre)
*/

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#pragma warning(disable:4996)


struct package
{
	char data[1024];
};

struct shared_data
{
	HANDLE mutex;
	std::vector<package> queue;

	shared_data()
	{
		mutex = NULL;
	}
};

shared_data g_shared_data;

#define DEFAULT_BUFLEN 1024
#define DEFAULT_PORTION 1024
#define DEFAULT_PORT "27015"

bool startsWith(const char* str, const char* starts)
{
	if (str == 0 || starts == 0) return false;
	int length = strlen(starts);
	if (strlen(str) < (unsigned int)length) return false;
	for (int i = 0; i < length; ++i)
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

DWORD WINAPI exec_thread(void* args)
{
	SOCKET sock = *((SOCKET*)args);
	char cmd[DEFAULT_PORTION] = { 0 };
	char recvbuf[DEFAULT_BUFLEN] = { 0 };
	int iResult;

	while (true)
	{
		Sleep(10);

		ZeroMemory(cmd, sizeof(cmd));
		WaitForSingleObject(g_shared_data.mutex, INFINITE);
		if (g_shared_data.queue.size() > 0)
		{
			strcpy(cmd, g_shared_data.queue.back().data);
			g_shared_data.queue.pop_back();
		}
		ReleaseMutex(g_shared_data.mutex);

		// wait for command
		if (strlen(cmd) == 0) continue;

		else if (startsWith(cmd, "EORL"))
		{
			char result[DEFAULT_PORTION] = { 0 };
			strcpy_s(result, DEFAULT_PORTION, cmd + 4);
			printf("Roulette stopped on %s. You can look at results now.\n", result);
		}
		else
		{
			// manual command
			iResult = send(sock, cmd, DEFAULT_PORTION, 0);
			if (iResult == SOCKET_ERROR) {
				printf("s: send failed with error: %d\n", WSAGetLastError());
				break;
			}

			printf("s: Sent %ld bytes:%s\n", iResult, cmd);

			ZeroMemory(recvbuf, sizeof(recvbuf));
			readn(sock, recvbuf, DEFAULT_PORTION, DEFAULT_PORTION);
			printf("%s\n", recvbuf);
		}
	}

	iResult = shutdown(sock, SD_BOTH);
	if (iResult == SOCKET_ERROR) {
		printf("s: shutdown failed with error: %d\n", WSAGetLastError());
	}
	closesocket(sock);
	return 1;
}

DWORD WINAPI event_thread(void* args)
{
	SOCKET sock = *((SOCKET*)args);
	char cmd[DEFAULT_PORTION] = { 0 };
	char recvbuf[DEFAULT_BUFLEN] = { 0 };
	int iResult;

	printf("e: Sending role package...\n");
	strcpy_s(cmd, DEFAULT_PORTION, "ROLEE");
	iResult = send(sock, cmd, DEFAULT_PORTION, 0);
	if (iResult == SOCKET_ERROR) {
		printf("e: send failed with error: %d\n", WSAGetLastError());
		goto exit_;
	} 
	readn(sock, recvbuf, DEFAULT_PORTION, DEFAULT_PORTION);
	if (strcmp(recvbuf, "OKK:") != 0)
	{
		printf("e: Failed to change role to event listener\n");
		goto exit_;
	}

	while (true)
	{
		ZeroMemory(recvbuf, DEFAULT_PORTION);
		iResult = readn(sock, recvbuf, DEFAULT_BUFLEN, DEFAULT_PORTION);
		if (iResult > 0) {
			char sPackType[5] = { 0 };
			memcpy(sPackType, recvbuf, sizeof(char) * 4);
			printf("e: Received %s package\n", sPackType);

			ZeroMemory(cmd, sizeof(cmd));
			strcpy_s(cmd, DEFAULT_PORTION, recvbuf);
			WaitForSingleObject(g_shared_data.mutex, INFINITE);
			g_shared_data.queue.push_back(*((package*)&cmd));
			ReleaseMutex(g_shared_data.mutex);

		}
		else
		{
			break;
		}

	}

exit_:

	iResult = shutdown(sock, SD_BOTH);
	if (iResult == SOCKET_ERROR) {
		printf("e: shutdown failed with error: %d\n", WSAGetLastError());
	}
	closesocket(sock);
	return 1;
}

int __cdecl main(int argc, char **argv)
{
	g_shared_data.mutex = CreateMutex(NULL, FALSE, NULL);
	if (g_shared_data.mutex == NULL)
	{
		printf("failed to create mutex\n");
		return 1;
	}

	WSADATA wsaData;
	SOCKET ConnectSocket = INVALID_SOCKET;
	SOCKET EventSocket = INVALID_SOCKET;
	struct addrinfo *result = NULL,
		*ptr = NULL,
		hints;
	char recvbuf[DEFAULT_BUFLEN] = { 0 };
	char cmd[DEFAULT_BUFLEN] = { 0 };
	int iResult;

	const char* server_name = "localhost";

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		getchar();
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo(server_name, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		getchar();
		return 1;
	}

	printf("connecting to %s:%s...\n", server_name, DEFAULT_PORT);
	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			getchar();
			return 1;
		}

		// Connect to server.
		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	printf("connecting event listener to %s:%s...\n", server_name, DEFAULT_PORT);
	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		EventSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (EventSocket == INVALID_SOCKET) {
			printf("event socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			getchar();
			return 1;
		}

		// Connect to server.
		iResult = connect(EventSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(EventSocket);
			EventSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET ||
		EventSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		getchar();
		return 1;
	}

	printf("connected to %s:%s...\n", server_name, DEFAULT_PORT);
	HANDLE hSend, hEvent;

	
	WaitForSingleObject(g_shared_data.mutex, INFINITE);
	hSend = CreateThread(NULL, 0, exec_thread, &ConnectSocket, 0, NULL);
	hEvent = CreateThread(NULL, 0, event_thread, &EventSocket, 0, NULL);
	ReleaseMutex(g_shared_data.mutex);

	while (true)
	{
		gets_s(recvbuf, DEFAULT_BUFLEN);

		ZeroMemory(cmd, sizeof(cmd));
		strcpy_s(cmd, DEFAULT_PORTION, recvbuf);
		WaitForSingleObject(g_shared_data.mutex, INFINITE);
		g_shared_data.queue.push_back(*((package*)&cmd));
		ReleaseMutex(g_shared_data.mutex);

		if (strcmp(recvbuf, "exit") == 0) break;
	}

	iResult = shutdown(ConnectSocket, SD_BOTH);
	if (iResult == SOCKET_ERROR)
	{
		printf("failed to shutdown send socket\n");
	}
	closesocket(ConnectSocket);

	iResult = shutdown(EventSocket, SD_BOTH);
	if (iResult == SOCKET_ERROR)
	{
		printf("failed to shutdown event socket\n");
	}
	closesocket(EventSocket);

	printf("Waiting for threads to finish...\n");
	WaitForSingleObject(hSend, INFINITE);
	WaitForSingleObject(hEvent, INFINITE);

	WSACleanup();

	printf("Successful termination\n");
	getchar();
	return 0;
}