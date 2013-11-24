/*
 * Bag.cpp
 *
 *  Created on: 03 нояб. 2013 г.
 *      Author: andrej
 */

#include "Bag.h"
#include <signal.h>
#include <unistd.h>

namespace mad_n {

unsigned int const Bag::TIME_COMMAND_TO_BEG = 3;
unsigned int const Bag::TIME_SESSION_GASIC = 3600;
unsigned Bag::size_buf_ = 0;
int Bag::sock_ = 0;
sockaddr_in Bag::addrBag_;
Bag::mode Bag::mode_ = STOP_GASIK;
int Bag::buf_[SIZE_BUF];

Bag::Bag(const int& sock, const sockaddr_in& addrBag) :
		period_(0) {
	sock_ = sock;
	addrBag_ = addrBag;
}

void Bag::startSessionGasik(void) {
	if (mode_ == START_GASIK)
		return;
	int buf = TO_BEG_START_GASIC;
	unsigned size = 1;
	instruct(&buf, size);
	mode_ = START_GASIK;
	return;
}
;

void Bag::stopSessionGasik(void) {
	if (mode_ == STOP_GASIK)
		return;
	int buf = TO_BEG_STOP_GASIC;
	unsigned size = 1;
	instruct(&buf, size);
	mode_ = Bag::STOP_GASIK;
	return;
}

void Bag::instruct(const int* buf, unsigned const& size) {
	struct sigaction act;
	if (buf != nullptr) {
		size_buf_ = size;
		for (unsigned i = 0; i < size_buf_; i++)
			buf_[i] = buf[i];
	}
	sendto(sock_, reinterpret_cast<void*>(buf_), size_buf_ * sizeof(int), 0,
			reinterpret_cast<sockaddr*>(&addrBag_), sizeof(addrBag_));
	act.sa_handler = handRetransmit;
	act.sa_flags = SA_RESTART;
	sigaction(SIGALRM, &act, nullptr);
	alarm(TIME_COMMAND_TO_BEG);
	return;
}

void Bag::handRetransmit(int) {
	instruct();
	return;
}

void Bag::passAnswerFromBag(const int* buf, unsigned size) {
	if (size == 0)
		return;
	switch (*buf) {
	case FROM_BEG_STOP_GASIC:
		if (size == 1)
			alarm(0);
		break;
	case FROM_BEG_START_GASIC:
		if (size == 1) {
			struct sigaction act;
			act.sa_handler = handExceedTimeGasik;
			act.sa_flags = SA_RESTART;
			sigaction(SIGALRM, &act, nullptr);
			alarm(TIME_SESSION_GASIC);
		}
		break;
	}
}

void Bag::handExceedTimeGasik(int) {
	stopSessionGasik();
}

Bag::~Bag() {
}

} /* namespace mad_n */
