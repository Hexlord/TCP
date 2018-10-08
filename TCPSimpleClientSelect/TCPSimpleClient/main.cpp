/*
@author(Alexander Knorre)
*/

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#pragma warning(disable:4996)

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORTION 9
#define DEFAULT_PORT "27015"

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
			printf("recv failed with error: %d\nEXITING\n", WSAGetLastError());
			return SOCKET_ERROR;
		}
	}
	return n;
}

int __cdecl main(int argc, char **argv)
{
	WSADATA wsaData;
	SOCKET ConnectSocket = INVALID_SOCKET;
	char sendbuf[DEFAULT_BUFLEN] = { 0 };
	char recvbuf[DEFAULT_BUFLEN] = { 0 };
	int iResult;
	int recvbuflen = DEFAULT_BUFLEN;

	const char* server_name = "localhost";

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		getchar();
		return 1;
	}

	printf("connecting to %s:%s...\n", server_name, DEFAULT_PORT);
	// Attempt to connect to an address until one succeeds
	ConnectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ConnectSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		getchar();
		return 1;
	}

	struct sockaddr_in server;
	struct hostent    *host = nullptr;

	server.sin_family = AF_INET;
	server.sin_port = htons(27015);
	host = gethostbyname("localhost");
	if (host == nullptr)
	{
		printf("Unable to resolve server\n");
		return 1;
	}
	CopyMemory(&server.sin_addr, host->h_addr_list[0], host->h_length);

	// Connect to server.
	iResult = connect(ConnectSocket, (struct sockaddr *)&server, sizeof(server));
	if (iResult == SOCKET_ERROR) {
		closesocket(ConnectSocket);
		ConnectSocket = INVALID_SOCKET;
		printf("socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		getchar();
		return 1;
	}

	if (ConnectSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		getchar();
		return 1;
	}

	int n_total = 0;
	int n_expected = 0;

	printf("connected to %s:%s...\n", server_name, DEFAULT_PORT);
	while (true)
	{
		gets_s(sendbuf, DEFAULT_BUFLEN);
		if (strlen(sendbuf) != 0)
		{
			iResult = send(ConnectSocket, sendbuf, (int)strlen(sendbuf), 0);
			if (iResult == SOCKET_ERROR) {
				printf("send failed with error: %d\n", WSAGetLastError());
				closesocket(ConnectSocket);
				WSACleanup();
				getchar();
				return 1;
			}

			printf("Bytes Sent: %ld\n", iResult);

			ZeroMemory(recvbuf, sizeof(recvbuf));
			
			n_total += iResult;
			while (n_total >= DEFAULT_PORTION)
			{
				n_total -= DEFAULT_PORTION;
				n_expected += DEFAULT_PORTION;
			}

			while (n_expected > 0)
			{
				iResult = readn(ConnectSocket, recvbuf, DEFAULT_BUFLEN, DEFAULT_PORTION);
				if (iResult == SOCKET_ERROR) {
					printf("readn failed with error: %d\nEXITING\n", WSAGetLastError());
					closesocket(ConnectSocket);
					WSACleanup();
					getchar();
					return 1;
				}
				printf("Received:'%s' (%d bytes)\n", recvbuf, iResult);
				n_expected -= 9;
			}
			
		}
		else break;
	}
	

	// shutdown the connection since no more data will be sent
	iResult = shutdown(ConnectSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		getchar();
		return 1;
	}

	// Receive until the peer closes the connection
	do {
		ZeroMemory(recvbuf, sizeof(recvbuf));
		iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0)
			printf("Bytes received: %d\n", iResult);
		else if (iResult == 0)
			printf("connection closed\n");
		else
			printf("recv failed with error: %d\n", WSAGetLastError());

	} while (iResult > 0);

	// cleanup
	closesocket(ConnectSocket);
	WSACleanup();

	getchar();
	return 0;
}