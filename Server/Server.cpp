// ChatServer.cpp : Defines the entry point for the console application.

#include "stdafx.h"

#pragma comment (lib, "Ws2_32.lib")

#define CHAT_BUF_SIZE 1024
#define CHAT_PORT "21688"

using namespace std;

typedef struct
{
	SOCKET clientSocket;
	SOCKADDR_IN clientAddr;
} PER_CLIENT_DATA, *LPPER_CLIENT_DATA;

typedef struct
{
	WSAOVERLAPPED overlapped;
	WSABUF wsaBuf;
	char buf[CHAT_BUF_SIZE];
} PER_IO_DATA, *LPPER_IO_DATA;


unsigned int WINAPI WorkerThread(LPVOID lpParam);
void ErrorHandling(char *message);

int main()
{
	struct addrinfo addr;
	struct addrinfo *result_addr = nullptr;

	HANDLE hComPort;

	SYSTEM_INFO sysInfo;

	WSADATA wsaData;

	SOCKET listenSocket = INVALID_SOCKET;
	SOCKET clientSocket = INVALID_SOCKET;
	SOCKADDR_IN clientAddr;
	LPPER_CLIENT_DATA clientData;

	DWORD recvBytes, flag = 0;
	int result, send_result, i, clientAddrLen = sizeof(clientAddr);
	int recvbuflen = CHAT_BUF_SIZE;
	char recvbuf[CHAT_BUF_SIZE];


	GetSystemInfo(&sysInfo);

	//CreateIoCompletionPort thread count is the same as process count
	hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, sysInfo.dwNumberOfProcessors);

	//WorkerThread
	for (i = 0; i < sysInfo.dwNumberOfProcessors; i++)
		(HANDLE)_beginthreadex(NULL, 0, WorkerThread, (LPVOID)hComPort, 0, NULL);

	//WSA set up
	result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0)
		ErrorHandling("WSAStartup() error");

	ZeroMemory(&addr, sizeof(addr));
	addr.ai_family = AF_INET;
	addr.ai_socktype = SOCK_STREAM;
	addr.ai_protocol = IPPROTO_TCP;
	addr.ai_flags = AI_PASSIVE;

	result = getaddrinfo(NULL, CHAT_PORT, &addr, &result_addr);
	if (result != 0)
		ErrorHandling("getaddrinfo() error");

	//listenSocket = socket(result_addr->ai_family, result_addr->ai_socktype, result_addr->ai_protocol);
	listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (listenSocket == INVALID_SOCKET)
		ErrorHandling("socket() error");

	result = bind(listenSocket, result_addr->ai_addr, (int)result_addr->ai_addrlen);
	if (result == SOCKET_ERROR)
		ErrorHandling("bind() error");

	freeaddrinfo(result_addr);

	result = listen(listenSocket, SOMAXCONN);
	if (result == SOCKET_ERROR)
		ErrorHandling("listen() error");

	cout << "Waiting for incoming connection ..." << endl;

	while (1)
	{
		clientSocket = accept(listenSocket, (SOCKADDR*)&clientAddr, &clientAddrLen);
		if (clientSocket == INVALID_SOCKET)
		{
			cerr << "accept() error" << endl;
			continue;
		}

		cout << "client " << inet_ntoa(clientAddr.sin_addr) << " : " << clientAddr.sin_port << endl;

		//save client data 
		clientData = new PER_CLIENT_DATA;
		clientData->clientSocket = clientSocket;
		memcpy(&(clientData->clientAddr), &clientAddr, clientAddrLen);

		CreateIoCompletionPort((HANDLE)clientSocket, hComPort, (ULONG_PTR)clientData, sysInfo.dwNumberOfProcessors);

		//save io data and recieve
		LPPER_IO_DATA ioData = new PER_IO_DATA;
		ZeroMemory(&(ioData->overlapped), sizeof(ioData->overlapped));
		ZeroMemory(&(ioData->buf), sizeof(ioData->buf));
		//ioData->overlapped.hEvent = WSACreateEvent();
		ioData->wsaBuf.len = CHAT_BUF_SIZE;
		ioData->wsaBuf.buf = ioData->buf;

		WSARecv(clientData->clientSocket,
			&(ioData->wsaBuf),
			1,
			&recvBytes,
			&flag,
			&(ioData->overlapped),
			NULL);
	}

	result = shutdown(clientSocket, SD_BOTH);
	if (result == SOCKET_ERROR)
		ErrorHandling("shutdown() error");

	closesocket(listenSocket);
	closesocket(clientSocket);
	WSACleanup();

	return 0;
}

unsigned int WINAPI WorkerThread(LPVOID lpParam)
{
	HANDLE hComPort = (HANDLE)lpParam;
	SOCKET clientSocket;
	DWORD recvBytes;
	LPPER_CLIENT_DATA clientData;
	LPPER_IO_DATA ioData;

	while (1)
	{
		GetQueuedCompletionStatus(
			hComPort,
			&recvBytes,
			(ULONG_PTR*)&clientData,
			(LPOVERLAPPED*)&ioData,
			INFINITE
			);
		clientSocket = clientData->clientSocket;

		cout << ioData->buf << endl;
	}

	return 0;
}

void ErrorHandling(char *message)
{
	cerr << message << endl;
	exit(1);
}