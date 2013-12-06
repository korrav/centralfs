/*
 * Bag.h
 *
 *  Created on: 03 нояб. 2013 г.
 *      Author: andrej
 */

#ifndef BAG_H_
#define BAG_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace mad_n {
#define SIZE_BUF 100 //размер буфера команд, передаваемых в БЭГ
/*
 *класс для взаимодействия с БЭГ
 */
class Bag {
	static int buf_[SIZE_BUF]; //буфер, содержащий сообщение, передаваемое в БЭГ
	static unsigned size_buf_; //длина сообщения, передаваемого в БЭГ
	enum idCommand {
		TO_BEG_STOP_GASIC, TO_BEG_START_GASIC
	}; //идентификаторы команд БЭГ
	enum idAnswer {
		FROM_BEG_STOP_GASIC, FROM_BEG_START_GASIC
	}; //идентификаторы ответов БЭГ
	enum destination {
		BAG = -1, MADS
	}; //адресаты команды; БЭГ и все отслеживаемые Мад, соответственно
	static int sock_; //дескриптор сокета коммуницирования с БЭГ
	static sockaddr_in addrBag_; //адрес БЭГ
	unsigned int period_; //время текущего периода испускания сигнала SIGALR
	static enum mode {
		START_GASIK, STOP_GASIK
	} mode_;
	static void instruct(enum destination dest = MADS, int const* buf = nullptr,
			unsigned const& size = 0); //передача команды в БЭГ
public:
	static unsigned int const TIME_COMMAND_TO_BEG; //время повторного отправления команды в БЭГ, если не был получен ответ на предыдущую команду
	static unsigned int const TIME_SESSION_GASIC; //время сессии Гасик. Если по истечению времени не получено указание остановить сессию Гасик,
	//то данное действие выполняется принудительно
	void passAnswerFromBag(int const* buf, unsigned size); //передача объекту сообщения от БЭГ
//	friend void startSessionGasik(Bag& bag); //начало сессии Гасик
//	friend void stopSessionGasik(Bag& bag); //окончание сессии Гасик
	static void startSessionGasik(void); //начало сессии Гасик
	static void stopSessionGasik(void); //окончание сессии Гасик
//	friend void set_handler(void(*f)(int), unsigned& time); //установить обработчик для сигнала SIGARM
	static void handRetransmit(int); //обработчик сигнала SIGALRM, повторяющий передачу текущей команды в БЭГ
	static void handExceedTimeGasik(int); //обработчик сигнала SIGALRM, вызываемый при превышении лимита по времени сессии Гасик
	Bag(const int& sock, const sockaddr_in& addrBag);
	virtual ~Bag();
};

} /* namespace mad_n */

#endif /* BAG_H_ */
