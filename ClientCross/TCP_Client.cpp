#include "TCP_Client.h"


void TCP_Client::schedule_send(std::basic_string<uint8_t> message)
{
	int sent = 0;
	int flags = 0;

	while (sent < message.length())
	{
		// Отправка очередного блока данных 
		int res = send(s, (char *)message.c_str() + sent, message.length() - sent, flags);
#ifndef __linux__
		if (res == SOCKET_ERROR) {
			std::cerr << "Error of sending" << std::endl;
			return;
		}
#else
		if (res == -1) {
			perror("schedule_send : send");
			return;
		}
#endif
		sent += res;
	}

	return;
}

void TCP_Client::schedule_read()
{
	//Объявление и очистка буфера для принятия сообщения
	char buffer[512];
	memset(buffer, 0, 512);
	int curlen = 0;
	int rcv, rcvd = 0;
	//Пока данные приходят, принимать
	do {
		rcv = recv(s, buffer, 512, 0);
		if (rcv > 0)
			rcvd += rcv;
	} while (rcv > 0);
	//Если подучены данные, вывести их в консоль и в файл
	converter.writeToBuffer((uint8_t*)buffer, rcvd);
}

void TCP_Client::client_loop()
{
#ifndef __linux__
	WSAEVENT ev = WSACreateEvent();
	WSAEventSelect(s, ev, FD_READ | FD_CLOSE | FD_WRITE);
#else
	struct pollfd sockEvent;
	memset(&sockEvent, 0, sizeof(struct pollfd));
	sockEvent.fd = s;
	sockEvent.events = POLLHUP || POLLIN || POLLERR;
#endif

	while (1)
	{
#ifndef __linux__
		WSANETWORKEVENTS ne;
		//Ожидание событий в течение 0.1 сек
		DWORD ev_code = WSAWaitForMultipleEvents(1, &ev, false, 100, false);

		if (ev_code > 0) {			   //За время ожидания произошло событие
														//Определить, какое событие пришло на сокет
			WSAEnumNetworkEvents(s, ev, &ne);
			if (ne.lNetworkEvents & FD_READ) {	//Пришли данные на сокет. Можно принимать.
				schedule_read();
			}
			if (ne.lNetworkEvents & FD_WRITE) { //Сокет готов для записи, можно отправлять данные.
				bizy_for_send = false;
			}
			if (ne.lNetworkEvents & FD_CLOSE) { // Удаленная сторона закрыла соединение
				//Завершение исходящих отправлений и отправление сигнала о завершении.
				shutdown(s, SD_SEND);
				break;
			}
		}
#else
		int ev_ctr = poll(&sockEvent, 1, 200);
		if (ev_ctr == -1) {
			perror("client_loop : poll");
			return;
		}
		if (ev_ctr > 0) {			   //За время ожидания произошло событие
			//Определить, какое событие пришло на сокет
			if (sockEvent.revents & POLLIN) {	//Пришли данные на сокет. Можно принимать.
				schedule_read();
			}
			if (sockEvent.revents & POLLHUP) { // Удаленная сторона закрыла соединение
				//Завершение исходящих отправлений и отправление сигнала о завершении.
				shutdown(s, SHUT_RDWR);
				break;
			}
			if (sockEvent.revents & POLLERR) { // Удаленная сторона закрыла соединение
				perror("client_loop : POLLERR ");
				break;
			}
		}
#endif

		std::string toPrint = converter.readMsg();
		while (!toPrint.empty()) {
			std::cout << toPrint << std::endl;
			toPrint = converter.readMsg();
		}

#ifndef __linux__
		//Пришёл сигнал о завершении работы
		if (ctrl_c) {
			//Завершение исходящих отправлений и завершение соединения
			shutdown(s, SD_SEND);

			//Цикл завершения приёма
			while (1) {
				WSANETWORKEVENTS ne;
				//Ожидание событий в течение 0.1 сек
				DWORD ev_code = WSAWaitForMultipleEvents(1, &ev, false, 100, false);

				if (ev_code != WSA_WAIT_TIMEOUT) {			   //За время ожидания произошло событие
															   //Определить, какое событие пришло на сокет
					WSAEnumNetworkEvents(s, ev, &ne);
					if (ne.lNetworkEvents & FD_READ) {	//Пришли данные на сокет. Можно принимать.

					}
					if (ne.lNetworkEvents & FD_CLOSE)  // Удаленная сторона закрыла соединение, можно закрыть сокет.
						break;
				}
			}
			break;
		}
#else
		//Пришёл сигнал о завершении работы
		if (ctrl_c) {
			//Завершение исходящих отправлений и завершение соединения
			shutdown(s, SHUT_RDWR);

			break;
		}
#endif
	}

#ifndef __linux__
	closesocket(s);
	WSACloseEvent(ev);
	WSACleanup();
#else
	close(s);
#endif
	return;
}

void TCP_Client::sending_thread(void)
{
	while (1) {
		std::string message;
		std::getline(std::cin, message);
		
		schedule_send(converter.getTlv(message));

		if (ctrl_c)
			return;
	}
}

void TCP_Client::exit_except()
{
	ctrl_c = true;
}

TCP_Client::TCP_Client() : converter(), ctrl_c(false)
{
	struct sockaddr_in addr;
#ifndef __linux__
	WSADATA wsa_data;
	if (WSAStartup(MAKEWORD(2, 2), &wsa_data)) {
		std::cerr << "WSAStartup error." << std::endl;
		ctrl_c = true;
		return;
	}
#endif

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s < 0)
		std::cerr << "Socket creation error." << std::endl;

	std::cout << "Socket created successful." << std::endl;
#ifdef __linux__
	fcntl(s, F_SETFL, O_NONBLOCK);
#endif
	std::cout << "Enter IP address of your server." << std::endl;
	std::cin >> ip;

	std::cin.clear();
	std::cin.ignore(100, '\n');
	// Заполнение структуры с адресом удаленного узла 
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(9000);
#ifndef __linux__
	addr.sin_addr.s_addr = inet_addr(ip.c_str());
#else
	inet_aton(ip.c_str(), &addr.sin_addr);
#endif

	// Установка соединения с удаленным хостом 
	int i = 0;
#ifndef __linux__
	for (i = 0; i < 10; i++)
	{
		if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == 0)
			break;
	}
	if (i == 10)
	{
		closesocket(s);
		ctrl_c = true;
		return;
	}
#else
	for (i; i < 10; i++)
	{
		int result = connect(s, (struct sockaddr*)&addr, sizeof(addr));
		if (result < 0 && errno != EAGAIN && errno != EINPROGRESS) { 
			if (errno == EISCONN)
				break;
			perror("connect");
			return;
		}
		if (result > 0) 
			break;

	}
	if (i == 10)
	{
		close(s);
		ctrl_c = true;
		return;
	}
#endif
	printf("Connect.\n");

#ifdef __linux__
	fcntl(s, F_SETFL, O_NONBLOCK);
#endif
}

TCP_Client::~TCP_Client()
{
}
