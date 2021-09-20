#include "TCP_Server.h"
#include <csignal>

#ifdef __linux__

#else
#include <windows.h>
#pragma comment(lib, "Gdi32.lib")
#endif

TCP_Server Server;

//LONG WINAPI WndProc(HWND, UINT, WPARAM, LPARAM); // функция обработки сообщений окна


void INTHandler(int sig)
{
	Server.ExitExcept();
	return;
}

int main(int argc, char** argv) {

	if (argc != 2) {
		std::cerr << "Need a count of threads as argument" << std::endl;
		return 1;
	}
	int thrCount = std::atoi(argv[1]);

	if (thrCount < 1) {
		std::cerr << "Invalid argument" << std::endl;
		return 1;
	}


#ifndef __linux__
	signal(SIGINT, INTHandler);
#else
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_handler = INTHandler;
	sigset_t   set;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	act.sa_mask = set;
	sigaction(SIGINT, &act, 0);
#endif

	Server.ServerLoop(thrCount);

	return 0;
}
