# KauriTestSolution
TestSolution

Реализованы клиент и многопоточный эхо-сервер, использующие TCP.

Актуальные версии кода находятся в папках Client и Server.

Компиляция:
Под Windows копмилировать через Visual Studio. Дополнительных настроек проекта не требется.
Под Linux компилировать через make. Требуется GCC

Запуск:
Сервер: при запуске указать в качестве параметра количество потоков сервера. Сервер сядет слушать порт 9000
Клиент: запускать без параметров. После запуска попросит ввести IP сервера. Ввести IP в формате ХХХ.ХХХ.ХХХ.ХХХ без порта (в коде зашит порт 9000)

Отключение:
И клиент и сервер завершают работу по сигналу Ctrl-C. В клиент после отправки сигнала может понадобиться ввести любую строку для окончательного завершения.