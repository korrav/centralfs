/*
 * main.cpp
 *
 *  Created on: 09.05.2013
 *      Author: andrej
 */
/*Параметры командной строки:
 * --bip ip бэга
 * --port порт
 * --catalog каталог, куда будут писаться данные
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <string>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <ctime>
#include "errno.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <iomanip>

#define BAG_IP "192.168.1.165"
#define BAG_PORT 31011
#define CATALOG "/home/x/file_of_andrej/data"
#define LEN_RECEIVE_BUF 4100 //длина приёмного буфера
#define SAMPLING_RATE 187500
using namespace std;

static struct {
	bool isEnableWr; //разрешение записи на диск
	int buf[LEN_RECEIVE_BUF];	//приёмный буфер
	size_t len; //длина блока полезных данных(в байтах)
	unsigned int nummad;	//количество мадов в системе
	string name_month; //название текущего месяца
	int day; //текущий день
} statp;

struct mad {
	string color; //цвет командной строки
	string catalog_d; //каталог, куда будут записываться файлы данных
	string catalog_m_d; //каталог, куда будут записываться файлы мониторограмм(дисперсия)
	string catalog_m_e; //каталог, куда будут записываться файлы мониторограмм(среднее)
	string catalog_t; //каталог, куда будут записываться файлы статистики алгоритмов
	unsigned int id; //идентификатор мада
	ofstream f_d; //поток для файлов данных
	ofstream f_m_d; //поток для файлов дисперсии
	ofstream f_m_e; //поток для файлов средних
	ofstream f_t; //поток для файлов статистики
	int day_for_mon;
	int day_for_stat;
	string month;
};

//СТРУКТУРА MONITOR
#define ID_MONITOR 4	//код, идентифицирующий блок монитор
struct Monitor {
	int time;	//время отправления
	int ident; //идентификатор блока данных
	unsigned int id_MAD; //идентификатор МАДа
	int dispersion[4]; //величина дисперсии для каждого канала
	int math_ex[4]; //величина математического ожидания для каждого канала
	int num_sampl; //количество отсчётов, при котором производится анализ даннных
};

//ШАПКА СТРУКТУРЫ ПЕРЕДАЧИ ДАННЫХ
#define SIGNAL_SAMPL 3	 //код, идентифицирующий блок данных сигнала
struct DataUnit {
	unsigned int time;	//время отправления
	int ident; //идентификатор блока данных
	int mode; //режим сбора данных
	unsigned int numFirstCount; //номер первого отсчёта
	int amountCount; //количество отсчётов (1 отс = 4 x 4 байт)
	unsigned int id_MAD; //идентификатор МАДа
};

#define STAT_ALG 5 //идентификатор пакета статистики алгоритма
//СТРУКТУРА СТАТИСТИКИ
struct StatAlg {
	unsigned time;	//время отправления
	int ident;	//идентификатор пакета
	int num_alg;	//номер аргумента
	unsigned maximum; //максимальное количество пакетов в памяти за период измерения
	unsigned average; //среднее количество пакетов в памяти за период измерения
	unsigned int id_MAD; //идентификатор МАДа
};

static void hand_command(void);
static void hand_socket(int& s, mad* mad, const int& num);
static void hand_monitor(Monitor* buf, size_t size, mad* mad, int idmad); //обработчик мониторограмм
static void hand_stat(StatAlg* buf, size_t size, mad* mad, int idmad); //обработчик статистики алгоритма
static void change_date(mad* mad, const int& num); //функция изменения временных меток
static void hand_data(DataUnit* buf, size_t size, mad* mad, int idmad);	//обработчик пакетов данных
static void message_about_receiv(mad* mad, DataUnit* buf); //выводит на консоль информацию о детектированном событии

int main(int argc, char* argv[]) {
	fd_set fdin; //набор дескрипторов, на которых ожидаются входные данные
	char ipBag[15] = BAG_IP;
	unsigned int port = BAG_PORT;
	string catalog = CATALOG;
	for (int i = 1; i < argc; i += 2) {
		if (!strcmp("--bip", argv[i]))
			strcpy(ipBag, argv[i + 1]);
		else if (!strcmp("--port", argv[i]))
			port = atoi(argv[i + 1]);
		else if (!strcmp("--catalog", argv[i]))
			catalog = argv[i + 1];
		else
			printf("%d параметр не поддерживается программой\n", i);
	}
	//инициализация сокета
	int sock;
	sock = socket(AF_INET, SOCK_DGRAM, 0); //сокет приёма
	if (sock == -1) {
		std::cerr << "socket  not create\n";
		exit(1);
	}
	sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY );
	if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
		std::cerr << "socket  not bind\n";
		exit(1);
	}
	//создание папок
	for (int i = 0; i < 3; i++) {
		mkdir((catalog + "/mad" + to_string(i + 1)).c_str(),
				S_IRWXU | S_IRWXG | S_IRWXO);
		mkdir((catalog + "/mad" + to_string(i + 1) + "/data").c_str(),
				S_IRWXU | S_IRWXG | S_IRWXO);
		mkdir((catalog + "/mad" + to_string(i + 1) + "/dispersion").c_str(),
				S_IRWXU | S_IRWXG | S_IRWXO);
		mkdir((catalog + "/mad" + to_string(i + 1) + "/expectation").c_str(),
				S_IRWXU | S_IRWXG | S_IRWXO);
		mkdir((catalog + "/mad" + to_string(i + 1) + "/test").c_str(),
				S_IRWXU | S_IRWXG | S_IRWXO);
	}
	//инициализация структур
	mad mads[3] = { { "\033[34", catalog + "/mad1" + "/data", catalog + "/mad1"
			+ "/dispersion", catalog + "/mad1" + "/expectation", catalog
			+ "/mad1" + "/test", 1 }, { "\033[33", catalog + "/mad2" + "/data",
			catalog + "/mad2" + "/dispersion", catalog + "/mad2"
					+ "/expectation", catalog + "/mad2" + "/test", 2 }, {
			"\033[36", catalog + "/mad3" + "/data", catalog + "/mad3"
					+ "/dispersion", catalog + "/mad3" + "/expectation", catalog
					+ "/mad3" + "/test", 3 } };
	statp.isEnableWr = true;
	statp.len = 0;
	statp.nummad = 3;
	statp.day = -1;
	//ГЛАВНЫЙ ЦИКЛ ПРОГРАММЫ
	int status = 0;
	printf("hello\n");
	for (;;) {
		//задание набора дескрипторов
		FD_ZERO(&fdin);
		FD_SET(STDIN_FILENO, &fdin);
		FD_SET(sock, &fdin);
		//ожидание событий
		status = select(sock + 1, &fdin, NULL, NULL, NULL);
		if (status == -1) {
			if (errno == EINTR)
				continue;
			else {
				perror("Функция select завершилась крахом\n");
				exit(1);
			}
		}
		if (FD_ISSET(STDIN_FILENO, &fdin))
			hand_command();
		if (FD_ISSET(sock, &fdin)) {
			hand_socket(sock, mads, 3);
			printf("Приняты данные на сокете\n");
		}
	}
	return 0;
}

//обработчик инструкций командной строки
void hand_command(void) {
	string comlin;
	getline(cin, comlin);
	if (comlin == "onWrite")
		statp.isEnableWr = true;
	else if (comlin == "offWrite")
		statp.isEnableWr = false;
	else
		cout << "Неизвестная команда\n";
	return;
}

//обработчик пакетов от сокета
void hand_socket(int& s, mad* mad, const int& num) {
	change_date(mad, num);
	statp.len = recvfrom(s, reinterpret_cast<void *>(statp.buf),
			sizeof(statp.buf), 0, NULL, NULL);
	if (!statp.isEnableWr) {
		statp.len = 0;
		return;
	}
	if (statp.len == sizeof(Monitor)
			&& (reinterpret_cast<Monitor*>(statp.buf)->ident == ID_MONITOR)
			&& (reinterpret_cast<Monitor*>(statp.buf)->id_MAD <= statp.nummad)) {
		hand_monitor(reinterpret_cast<Monitor*>(statp.buf), statp.len, mad,
				reinterpret_cast<Monitor*>(statp.buf)->id_MAD - 1);
		printf("Принята мониторограмма\n");
	} else if (statp.len > sizeof(DataUnit)
			&& (reinterpret_cast<DataUnit*>(statp.buf)->ident == SIGNAL_SAMPL)
			&& (reinterpret_cast<DataUnit*>(statp.buf)->id_MAD <= statp.nummad)) {
		hand_data(reinterpret_cast<DataUnit*>(statp.buf), statp.len, mad,
				reinterpret_cast<DataUnit*>(statp.buf)->id_MAD - 1);
		printf("Принят блок данных\n");
	} else if (statp.len == sizeof(StatAlg)
			&& (reinterpret_cast<StatAlg*>(statp.buf)->ident == STAT_ALG)
			&& (reinterpret_cast<StatAlg*>(statp.buf)->id_MAD <= statp.nummad)) {
		hand_stat(reinterpret_cast<StatAlg*>(statp.buf), statp.len, mad,
				reinterpret_cast<StatAlg*>(statp.buf)->id_MAD - 1);
		printf("Принят пакет статистики\n");
	} else
		printf("Принят неизвестный пакет данных\n");
	statp.len = 0;

	return;
}

void hand_monitor(Monitor* buf, size_t size, mad* mad, int idmad) {
	//проверка изменился ли день
	if (mad[idmad].day_for_mon != statp.day) {
		mad[idmad].day_for_mon = statp.day;
		mad[idmad].f_m_d.close();
		mad[idmad].f_m_e.close();
		mad[idmad].f_m_d.open(
				(mad[idmad].catalog_m_d + "/" + statp.name_month + "/"
						+ to_string(mad[idmad].day_for_mon) + "_"
						+ statp.name_month + "_" + to_string(buf->id_MAD) + "_d").c_str(),
				ios::out | ios::app);
		mad[idmad].f_m_e.open(
				(mad[idmad].catalog_m_e + "/" + statp.name_month + "/"
						+ to_string(mad[idmad].day_for_mon) + "_"
						+ statp.name_month + "_" + to_string(buf->id_MAD) + "_e").c_str(),
				ios::out | ios::app);
	}
	//заполнение файла дисперсий
	mad[idmad].f_m_d << left << setw(12) << buf->time;
	for (int i = 0; i < 4; i++)
		mad[idmad].f_m_d << left << setw(12) << buf->dispersion[i];
	mad[idmad].f_m_d << endl;
	//заполнение файла математического ожидания
	mad[idmad].f_m_e << left << setw(12) << buf->time;
	for (int i = 0; i < 4; i++)
		mad[idmad].f_m_e << left << setw(12) << buf->math_ex[i];
	mad[idmad].f_m_e << endl;
	return;
}

void change_date(mad* mad, const int& num) {
	static int month = -1;
	char m[10];
	time_t tim = time(NULL);
	tm* t = localtime(&tim);
	statp.day = t->tm_mday;
	if (month != t->tm_mon) {
		strftime(m, sizeof(m), "%B", t);
		statp.name_month = m;
		month = t->tm_mon;
		//создание новых папок
		for (int i = 0; i < num; i++) {
			mkdir((mad[i].catalog_m_d + "/" + statp.name_month).c_str(),
					S_IRWXU | S_IRWXG | S_IRWXO);
			mkdir((mad[i].catalog_m_e + "/" + statp.name_month).c_str(),
					S_IRWXU | S_IRWXG | S_IRWXO);
			mkdir((mad[i].catalog_d + "/" + statp.name_month).c_str(),
					S_IRWXU | S_IRWXG | S_IRWXO);
			mkdir((mad[i].catalog_t + "/" + statp.name_month).c_str(),
					S_IRWXU | S_IRWXG | S_IRWXO);
		}

	}
}

void hand_data(DataUnit* buf, size_t size, mad* mad, int idmad) {
	mad[idmad].f_d.open(
			(mad[idmad].catalog_d + "/" + statp.name_month + "/"
					+ to_string(buf->time) + "_" + to_string(buf->numFirstCount)
					+ "_" + to_string(buf->id_MAD)).c_str(),
			ios::out | ios::trunc | ios::binary);
	mad[idmad].f_d.write((char*) ((int*) &(buf->id_MAD) + 1),
			size - sizeof(DataUnit));
	mad[idmad].f_d.close();
	message_about_receiv(&mad[idmad], buf);
	return;
}

void message_about_receiv(mad* mad, DataUnit* buf) {
	char cur_time[26];
	unsigned add_s = buf->numFirstCount / SAMPLING_RATE;
	unsigned num_us =
			static_cast<unsigned>((static_cast<double>(buf->numFirstCount)
					/ SAMPLING_RATE - add_s) * 1000000);
	time_t tim = static_cast<time_t>(buf->time + add_s);
	tm* t = localtime(&tim);
	strftime(cur_time, sizeof(cur_time), "%x %X", t);
	cout << mad->color << "Получено событие от мада № " << buf->id_MAD
			<< ".  Время детектирования: " << cur_time << " :  " << num_us
			<< "us\033[0m\n";
	return;
}

void hand_stat(StatAlg* buf, size_t size, mad* mad, int idmad) {
	//проверка изменился ли день
	if (mad[idmad].day_for_stat != statp.day) {
		mad[idmad].day_for_stat = statp.day;
		mad[idmad].f_t.close();
		mad[idmad].f_t.open(
				(mad[idmad].catalog_t + "/" + statp.name_month + "/"
						+ to_string(mad[idmad].day_for_stat) + "_"
						+ statp.name_month + "_" + to_string(buf->id_MAD) + "_t").c_str(),
				ios::out | ios::app);
	}
	//заполнение файла статистики алгоритма
	mad[idmad].f_t << left << setw(12) << buf->time;
	mad[idmad].f_t << left << setw(12) << buf->average;
	mad[idmad].f_t << left << setw(12) << buf->maximum;
	mad[idmad].f_t << endl;
	return;
}
