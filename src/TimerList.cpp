#include <iostream>
#include "TimerList.h"
using namespace std;

SortTimerList::SortTimerList()
{
	m_head = nullptr;
	m_tail = nullptr;
}

SortTimerList::~SortTimerList()
{
	UtilTimer* tmp = m_head;
	while (tmp) {
		m_head = tmp->next;
		delete tmp;
		tmp = m_head;
	}
}

void SortTimerList::addTimer(UtilTimer* timer)
{
	if (timer == nullptr) {
		return;
	}
	/* If the current linked list is empty, the head and tail pointers point to this node */
	if (m_head == nullptr) {
		m_head = m_tail = timer;
		return;
	}
	/* If the current timer timeout is less than the head node of the linked list, insert it into the head of the linked list */
	if (timer->expire < m_head->expire) {
		timer->next = m_head;
		m_head->prev = timer;
		m_head = timer;
		return;
	}
	/* In other cases, call the auxiliary insertion function */
	addTimer(timer, m_head);
}

void SortTimerList::adjustTimer(UtilTimer* timer)
{
	if (timer == nullptr) {
		return;
	}
	UtilTimer* tmp = timer->next;
	if (tmp == nullptr || (timer->expire < tmp->expire)) {
		return;
	}
	/* Remove the node from the linked list and reinsert it */
	if (timer == m_head) {
		m_head = m_head->next;
		m_head->prev = nullptr;
		timer->next = nullptr;
		addTimer(timer, m_head);
	}
	else {
		timer->prev->next = timer->next;
		timer->next->prev = timer->prev;
		addTimer(timer, timer->next);
	}
}

void SortTimerList::deleteTimer(UtilTimer* timer)
{
	if (timer == nullptr) {
		return;
	}
	/* If there is only one timer in the current linked list */
	if ((timer == m_head) && (timer == m_tail)) {
		delete timer;
		m_head = nullptr;
		m_tail = nullptr;
		return;
	}
	/* If the deletion is the head node */
	if (timer == m_head) {
		m_head = m_head->next;
		m_head->prev = nullptr;
		delete timer;
		return;
	}
	/* If the deletion is the tail node */
	if (timer == m_tail) {
		m_tail = m_tail->prev;
		m_tail->next = nullptr;
		delete timer;
		return;
	}
	/* If the deletion is an intermediate node */
	timer->prev->next = timer->next;
	timer->next->prev = timer->prev;
	delete timer;
}

void SortTimerList::tick()
{
	if (m_head == nullptr) {
		return;
	}
	/* Get the current time and compare it with each node in the timer linked list */
	time_t cur = time(nullptr);
	UtilTimer* tmp = m_head;
	while (tmp) {
		/* Exit if the timeout of the current node has not yet expired */
		if (cur < tmp->expire) {
			break;
		}
		/* otherwise call the task callback function of the current node */
		tmp->cbFunc(tmp->userData);
		m_head = tmp->next;
		/* Remove the timer that has reached the timeout from the linked list */
		if (m_head) {
			m_head->prev = nullptr;
		}
		delete tmp;
		tmp = m_head;
	}
}

void SortTimerList::addTimer(UtilTimer* timer, UtilTimer* listHead)
{
	UtilTimer* prev = listHead;
	UtilTimer* tmp =  prev->next;
	while (tmp) {
		if (timer->expire < tmp->expire) {
			prev->next = timer;
			timer->next = tmp;
			tmp->prev = timer;
			timer->prev = prev;
			break;
		}
		prev = tmp;
		tmp = tmp->next;
	}
	/* insert at the end of the linked list */
	if (tmp == nullptr) {
		prev->next = timer;
		timer->prev = prev;
		timer->next = nullptr;
		m_tail = timer;
	}
}