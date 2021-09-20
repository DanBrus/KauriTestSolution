#include "TCP_Server.h"

void TCP_Server::ServerLoop(int thrCount)
{
	this->thrCount = thrCount;
	threads.resize(thrCount);
	threadsData.resize(thrCount);

	for (int i = 0; i < thrCount; i++) {
		threads[i] = new std::thread(TCP_Server::thrStart, this, i);
	}

	//Асоциация события подключения с сокетом подключений
#ifdef __linux__

	struct pollfd acceptEvent;
	memset(&acceptEvent, 0, sizeof(struct pollfd));

	acceptEvent.fd = listenSocket;
	acceptEvent.events = POLLIN;
#else
	WSAEVENT connectEvent = WSACreateEvent();
	WSAEventSelect(listenSocket, connectEvent, FD_ACCEPT);
#endif

	while (1)
	{
		//Ожидание событий в течение 0.1 сек
#ifdef __linux__
		if (ctrl_c) {
			shutdown(listenSocket, SHUT_RDWR);
			close(listenSocket);
			break;
		}

		int res = poll(&acceptEvent, 1, 100);
		if (res == -1) {
			perror("ServerLoop : poll ");

			ctrl_c = true;

			for (int i = 0; i < thrCount; i++) {
				threads[i]->join();
				delete threads[i];
			}

			return;
		}

		if (res == 0)
			continue;

		ScheduleAccept();
		AddAcceptedConnection();


#else
		WSANETWORKEVENTS ne;
		DWORD ev_code = WSAWaitForMultipleEvents(1, &connectEvent, false, 200, false);

		if (ev_code != WSA_WAIT_TIMEOUT) {			   //За время ожидания произошло событие
				//Определить, какое событие пришло на сокет
			WSAEnumNetworkEvents(listenSocket, connectEvent, &ne);
			if (ne.lNetworkEvents & FD_ACCEPT) {
				// Принятие подключения
				ScheduleAccept();
				AddAcceptedConnection();
			}
		}
		if (ctrl_c) {
			shutdown(listenSocket, SD_BOTH);

			closesocket(listenSocket);
			WSACloseEvent(connectEvent);
			break;
		}
#endif
	}

	for (int i = 0; i < thrCount; i++) {
		threads[i]->join();
		delete threads[i];
	}
}

void TCP_Server::ExitExcept()
{
	ctrl_c = true;
}

void TCP_Server::PrintLog(std::string message)
{
	logMutex.lock();
	time_t seconds = time(NULL);
	tm* timeinfo = localtime(&seconds);

	//Вывод даты и времени
	std::cout << (1900 + timeinfo->tm_year) << '.' << timeinfo->tm_mon << '.' << timeinfo->tm_mday << ' ' << timeinfo->tm_hour << ':' << timeinfo->tm_min << " | ";
	//Вывод сообщения
	std::cout << message << std::endl;

	logMutex.unlock();
}

#ifndef __linux__
int TCP_Server::SockErr(const std::string & function, int s)
{

	std::cerr << function << " : socket error." << std::endl;
	return -1;
}
#endif

void TCP_Server::AddAcceptedConnection()
{
	//Создание структуры с информацией о подключении
	client_ctx cur;
	memset(&cur, 0, sizeof(cur));

	threadsData[indOfThrToAcceptNextConnection].vectorMutex.lock();

	threadsData[indOfThrToAcceptNextConnection].newConnections.push_back(cur);
	client_ctx &i_client = threadsData[indOfThrToAcceptNextConnection].newConnections.back();

#ifndef __linux__
	i_client.ip = std::string(inet_ntoa(newClient.sin_addr));
#else
	std::string ip = inet_ntoa(newClient.sin_addr);
	i_client.ip = ip;
#endif
	i_client.socket = gAcceptedSocket;
	i_client.bizy_for_send = false;
	//Вывод информации о подключении в файл
	PrintLog(i_client.ip + std::string(" | connected"));

	threadsData[indOfThrToAcceptNextConnection].vectorMutex.unlock();

	indOfThrToAcceptNextConnection += 1;
	indOfThrToAcceptNextConnection %= thrCount;

	return;
}

int TCP_Server::ScheduleRead(TCP_Server::client_ctx & client, TlvRwClass* converter)
{
	//Объявление и очистка буфера для принятия сообщения
	char buffer[512];
	memset(buffer, 0, 512);
	int curlen = 0;
	int rcv, rcvd = 0;
	//Пока данные приходят, принимать
	do {
		rcv = recv(client.socket, buffer, 512, 0);
#ifndef __linux__
		if (rcv < 0) {
#else
		if (rcv < 0 && errno != EAGAIN) {
			perror("ScheduleRead : recv");
#endif
			return -1;
		}
		if(rcv > 0)
			rcvd += rcv;
	} while (rcv > 0);
	//Если получены данные, вывести их в консоль и в файл
	if (rcvd > 0) {
		std::string toSend(buffer, rcvd);
		//PrintLog(client.ip, std::string(buffer, rcvd));
		converter->writeToBuffer((uint8_t*)buffer, rcvd);

		ScheduleWrite(toSend, client);
	}
	return 0;
}

void TCP_Server::ScheduleWrite(std::string &message, client_ctx &client)
{
	int sent = 0;
	while (sent < message.length())
	{
		// Отправка очередного блока данных 
		int res = send(client.socket, &message[sent], message.length() - sent, 0);
#ifndef __linux__
		if (res == SOCKET_ERROR) {
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
				client.bizy_for_send = true;
			}
			else {
				//std::cerr << "Error of sending" << std::endl;
				break;
			}
		}
#else
		if (res == -1) {
			perror("ScheduleWrite : send");
			break;
		}
#endif
		sent += res;
	}
}

#ifndef __linux__
void TCP_Server::ScheduleDisconnect(int sInd, int thrInd, WSAEVENT*& events, SOCKET*& sockets)
#else
void TCP_Server::ScheduleDisconnect(int sInd, int thrInd, struct pollfd*& events, int*& sockets)
#endif
{
	PrintLog(threadsData[thrInd].activeConnections[sockets[sInd]].ip + " | thread " + std::to_string(thrInd) + " | disconnected");

#ifndef __linux__
	//Завершение всех исходящих отправлений и отправка сообщения о закрытии соединения
	shutdown(sockets[sInd], SD_SEND);
	//Закрытие сокета
	closesocket(sockets[sInd]);
	//Удаление сокетаи связаного с ним события из массива.
	WSACloseEvent(events[sInd]);
#else
	//Завершение всех исходящих отправлений и отправка сообщения о закрытии соединения
	shutdown(sockets[sInd], SHUT_WR);
	//Закрытие сокета
	close(sockets[sInd]);
#endif

	int activeConnCtr = threadsData[thrInd].activeConnections.size();
	threadsData[thrInd].activeConnections.erase(sockets[sInd]);
	if (activeConnCtr >= 2) {
		events[sInd] = events[activeConnCtr - 1];
		sockets[sInd] = sockets[activeConnCtr - 1];
	}
#ifndef __linux__
	events = (WSAEVENT*)realloc(events, sizeof(WSAEVENT) * activeConnCtr - 1);
	sockets = (SOCKET*)realloc(sockets, sizeof(SOCKET) * activeConnCtr - 1);
#else
	events = (struct pollfd*)realloc(events, sizeof(struct pollfd) * activeConnCtr - 1);
	sockets = (int*)realloc(sockets, sizeof(int) * activeConnCtr - 1);

#endif
}

int TCP_Server::ScheduleAccept()
{
#ifndef __linux__
	gAcceptedSocket = accept(listenSocket, (sockaddr*)&newClient, &addrlen);
	if (gAcceptedSocket == INVALID_SOCKET) {
		std::cerr << WSAGetLastError();
		return -1;
	}
#else
	socklen_t len = sizeof(sockaddr);
	gAcceptedSocket = accept(listenSocket, (sockaddr*)&newClient, &len);
	if (gAcceptedSocket == -1) {
		perror("ScheduleAccept : accept");
		return -1;
	}
#ifdef __linux__
	fcntl(gAcceptedSocket, F_SETFL, O_NONBLOCK);
#endif

#endif
}

void TCP_Server::thrStart(TCP_Server* exmpl, int thrInd)
{
	exmpl->thrLoop(thrInd);
}

void TCP_Server::thrLoop(int thrInd)
{
#ifndef __linux__
	WSAEVENT *events = (WSAEVENT*)malloc(1);
	SOCKET* sockets = (SOCKET*) malloc(1);
#else
	struct pollfd* events = (struct pollfd*)malloc(1);
	int* sockets = (int*)malloc(1);
#endif
	server_thr_data& curThrData = threadsData[thrInd];
	curThrData.tlvConverter = new TlvRwClass();
	std::string threadLogHead;
	threadLogHead = "thread";
	threadLogHead += thrInd;
	PrintLog(threadLogHead + "| started");
	while (1) {

		if (ctrl_c) {
			while (curThrData.activeConnections.size() > 0) {
				ScheduleDisconnect(0, thrInd, events, sockets);
			}
			break;
		}
		//Проверка, не поступило ли новых подключений. Если поступило, то обработать и добавить в массив событий.
		curThrData.vectorMutex.lock();
		while (curThrData.newConnections.size() > 0) {
			curThrData.activeConnections.insert(thr_connect_elem(curThrData.newConnections.back().socket, curThrData.newConnections.back()));
			auto i_client = curThrData.activeConnections.at(curThrData.newConnections.back().socket);
			
			curThrData.newConnections.pop_back();

			//Создание события для работы с соединением и заполнение массивов сокетов и событий.
#ifndef __linux__
			events = (WSAEVENT*)realloc(events, curThrData.activeConnections.size() * sizeof(WSAEVENT));
			sockets = (SOCKET*)realloc(sockets, curThrData.activeConnections.size() * sizeof(SOCKET));
#else
			events = (struct pollfd*)realloc(events, curThrData.activeConnections.size() * sizeof(struct pollfd));
			sockets = (int*)realloc(sockets, curThrData.activeConnections.size() * sizeof(int));
#endif

#ifndef __linux__
			events[curThrData.activeConnections.size() - 1] = WSACreateEvent();
#else
			memset(&events[curThrData.activeConnections.size() - 1], 0, sizeof(struct pollfd));
#endif
			sockets[curThrData.activeConnections.size() - 1] = i_client.socket;

			PrintLog(i_client.ip + " | thread " + std::to_string(thrInd) + " | connected");

			//Ассоциация события и сокета
#ifndef __linux__
			WSAEventSelect(i_client.socket, events[curThrData.activeConnections.size() - 1], FD_READ | FD_CLOSE);
#else
			events[curThrData.activeConnections.size() - 1].fd = i_client.socket;
			events[curThrData.activeConnections.size() - 1].events = POLLIN || POLLHUP;
#endif
		}
		curThrData.vectorMutex.unlock();

		if (curThrData.activeConnections.size() == 0) {
#ifndef __linux__
			Sleep(200);
#else
			usleep(200);
#endif
			continue;
		}

#ifndef __linux__
		WSANETWORKEVENTS ne;
		//Ожидание событий в течение 0.1 сек
		DWORD ev_code = WSAWaitForMultipleEvents(curThrData.activeConnections.size(), events, false, 200, false);
		if (ev_code != WSA_WAIT_TIMEOUT) {			   //За время ожидания произошло событие
				//Определить, какое событие пришло на сокет
			WSAEnumNetworkEvents(sockets[ev_code - WSA_WAIT_EVENT_0], events[ev_code - WSA_WAIT_EVENT_0], &ne);
			if (ne.lNetworkEvents & FD_READ) {	//Пришли данные на сокет. Можно принимать.
				ScheduleRead(curThrData.activeConnections[sockets[ev_code - WSA_WAIT_EVENT_0]], curThrData.tlvConverter);
			}
			if (ne.lNetworkEvents & FD_CLOSE) { // Удаленная сторона закрыла соединение, можно закрыть сокет.
				ScheduleDisconnect(ev_code - WSA_WAIT_EVENT_0, thrInd, events, sockets);
			}
		}
#else
		//Ожидание событий в течение 0.1 сек
		int ev_ctr = poll(events, curThrData.activeConnections.size(), 200);
		if (ev_ctr == -1) {
			perror("thrLoop : poll ");
			ctrl_c = true;
		}

		if (ev_ctr > 0) {			   //За время ожидания произошло событие
			for (int i = 0; i < curThrData.activeConnections.size(); i++) {
				//Определить, какое событие пришло на сокет
				if (events[i].revents & POLLIN) {	//Пришли данные на сокет. Можно принимать.
					if (ScheduleRead(curThrData.activeConnections[sockets[i]], curThrData.tlvConverter) < 0)
						ScheduleDisconnect(i, thrInd, events, sockets);
				}
				if (events[i].revents & POLLHUP) { // Удаленная сторона закрыла соединение, можно закрыть сокет.
					ScheduleDisconnect(i, thrInd, events, sockets);
				}
			}
		}
#endif
		std::string toPrint = curThrData.tlvConverter->readMsg();
		while (!toPrint.empty()) {
			PrintLog(toPrint);
			toPrint = curThrData.tlvConverter->readMsg();
		}

	}

	PrintLog(threadLogHead + " | ends");

	delete curThrData.tlvConverter;
	free(events);
	free(sockets);
}

TCP_Server::TCP_Server() : ctrl_c(false), indOfThrToAcceptNextConnection(0)
{
#ifndef __linux__
	WSADATA wsa_data;
	//Инициализация
	if (WSAStartup(MAKEWORD(2, 2), &wsa_data)) {
		std::cerr << "WSAStartup error." << std::endl;
		ctrl_c = true;
		return;
	}
#endif

	//Создание сокета прослушивания
#ifndef __linux__
	listenSocket = WSASocketA(AF_INET, SOCK_STREAM, 0, NULL, 0, 0);
#else
	listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif
	if (listenSocket < 0) {
		std::cerr << "Socket creation error." << std::endl;
		//free(listenSocket);
		ctrl_c = true;
		return;
	}
	
	//Перевод сокета в неблокирующий режим

#ifndef __linux__
	unsigned long mode = 1;
	ioctlsocket(listenSocket, FIONBIO, &mode);
#endif

	
	//Заполнение структуры для прослушивания
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(9000);					// Ввод в структура прослушиваемого порта 
	addr.sin_addr.s_addr = htonl(INADDR_ANY);		// Приём сообщений со всех адресов

													//Связывание сокета и адреса прослушивания 
	if (bind(listenSocket, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
#ifndef __linux__
		SockErr(std::string("bind"), listenSocket);
		closesocket(listenSocket);
#else
		perror("TCP_Server constructor : bind ");
		close(listenSocket);
#endif
		//free(sockets);
		ctrl_c = true;
		return;
	}
	std::cout << "Bind is OK." << std::endl;

	//Открыть сокет для прослушиания
	if (listen(listenSocket, SOMAXCONN) < 0)
	{
#ifndef __linux__
		SockErr(std::string("listen"), listenSocket);
		closesocket(listenSocket);
#else

		perror("TCP_Server constructor : listen ");
		close(listenSocket);
#endif
		ctrl_c = true;
		return;
	}
	else
		std::cout << "Listening TCP port: " << ntohs(addr.sin_port) << std::endl;
	
	//connectCtr = 1;
}

TCP_Server::~TCP_Server()
{
#ifndef __linux__
	WSACleanup();
#endif
}

