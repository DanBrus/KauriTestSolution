
#include <signal.h>

#include "TCP_Client.h"
#include <thread>

TCP_Client client[1];

void INTHandler(int sig)
{
	std::cerr << "GET SIGINT" << std::endl;
	client->exit_except();
	return;
}

int main(int argc, char *argv[])
{
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

	std::thread sender([=]() {client->sending_thread(); });
	client->client_loop();

	sender.join();
	return 0;
}