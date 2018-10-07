#pragma once


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

struct package
{
	char data[1024];
};


#define DEFAULT_BUFLEN 1024
#define DEFAULT_PORTION 1024
#define DEFAULT_CLIENT_COUNT 16
#define DEFAULT_PORT "27015"

class Game;

struct shared_data
{
	HANDLE mutex;
	HANDLE client_threads[DEFAULT_CLIENT_COUNT];
	int client_count;
	SOCKET socket_array[DEFAULT_CLIENT_COUNT];
	Game* gameRef;


	shared_data()
	{
		client_count = 0;
		mutex = NULL;
		for (int i = 0; i < DEFAULT_CLIENT_COUNT; ++i)
		{
			client_threads[i] = NULL;
			socket_array[i] = INVALID_SOCKET;
		}
	}
};


void kill_client(int n);

bool startsWith(const char* str, const char* starts);

int readn(SOCKET sock, char* buf, int buflen, int _n);


DWORD WINAPI client_thread(void* data);

DWORD WINAPI listen_thread(void* data);

int network_main(Game* gameRef);
