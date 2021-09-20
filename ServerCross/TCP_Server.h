#pragma once
#include "TlvRwClass.h"
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1

#define WIN32_LEAN_AND_MEAN
// Директива линковщику: использовать библиотеку сокетов 

#include <list>
#include <iostream> 
#include <string>
#include <cstring>
#include <fstream>
#include <ctime>
#include <map>
#include <vector>
#include <thread>
#include <mutex>

#ifdef __linux__
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#else
#include <winsock2.h> 
#include <mswsock.h>
#include <windows.h> 
#pragma comment(lib, "ws2_32.lib") 
#pragma comment(lib, "mswsock.lib")
#endif

class TCP_Server
{
private:
	std::vector<std::thread*> threads;

	struct client_ctx
	{
		bool bizy_for_send;
		int socket;
		std::string ip;

		//client_ctx(const client_ctx& a) : bizy_for_send(a.bizy_for_send), ip(a.ip), socket(a.socket) {
		//}
		//client_ctx() : bizy_for_send(false), ip(), socket(-1) {
		//}
	};

	typedef std::map<int, client_ctx> thr_connect_map;
	typedef std::pair<int, client_ctx> thr_connect_elem;

	struct server_thr_data {
		std::mutex vectorMutex;
		std::vector<struct client_ctx> newConnections;
		thr_connect_map activeConnections;

		TlvRwClass* tlvConverter;
		server_thr_data(const server_thr_data& a) : vectorMutex() {
			this->activeConnections = a.activeConnections;
			this->newConnections = a.newConnections;
		}
		server_thr_data() : vectorMutex(), activeConnections(), newConnections() {
		}
	};

	std::vector<server_thr_data> threadsData;


	std::mutex logMutex;
	int gAcceptedSocket;
	int listenSocket;
	sockaddr_in newClient;
	int thrCount, indOfThrToAcceptNextConnection;
	int addrlen = 16;
	bool ctrl_c;

	void PrintLog(std::string message);
#ifndef __linux__
	int SockErr(const std::string &function, int s);
#endif
	void AddAcceptedConnection();
	int ScheduleRead(TCP_Server::client_ctx &client, TlvRwClass* converter);
	void ScheduleWrite(std::string& message, client_ctx &client);

#ifdef __linux__
	void ScheduleDisconnect(int sInd, int thrIndex, struct pollfd*& events, int*& sockets);
#else
	void ScheduleDisconnect(int sInd, int thrIndex, WSAEVENT*& events, SOCKET*& sockets);
#endif
	int ScheduleAccept();
	static void thrStart(TCP_Server* exmpl, int thrInd);
	void thrLoop(int thrInd);
public:
	void ServerLoop(int thrCount);
	void ExitExcept();
	TCP_Server();
	~TCP_Server();
};

