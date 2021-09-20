#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <iostream> 
#include <string>
#include <cstring>
#include <ctime>
#include <thread>
#include <limits>
#include <fstream>
#include "TlvRwClass.h"
// Директива линковщику: использовать библиотеку сокетов


#ifdef __linux__
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#else
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif


class TCP_Client
{
private:
	int s;
	bool bizy_for_send, ctrl_c;

	TlvRwClass converter;

	void schedule_send(std::basic_string<uint8_t> message);
	void schedule_read();
	std::string ip;
public:
	void sending_thread(void);
	void client_loop();
	void exit_except();
	TCP_Client();
	~TCP_Client();
};

