#pragma once
#include <stdio.h>
#include "network/tcp/iocp/server.h"

class test1 {
public:
	test1() {
		printf("����()\n");
	}
	test1(const test1&) {
		printf("��ֵ��������()\n");
	}
	test1(test1&&) {
		printf("��ֵ��������()\n");
	}
	test1& operator =(const test1&) {
		printf("��ֵͬ�ำֵ()\n");
	}
	test1& operator =(test1&&) {
		printf("��ֵͬ�ำֵ()\n");
	}
	~test1() {
		printf("����()\n");
	}
};

template <class T>
class sdgsfdgs {
public:

	void A(T);
};

template <class T>
void sdgsfdgs<T>::A(T) {
}

class asdfasdf {
public:
	template <int N>
	void A(int aa[N]);
};

// ��С��
void test_mini_heap1();
void test_mini_heap2();
// ����
void test_max_heap1();
void test_max_heap2();
// ��ʱ��(ʱ���, �¼�)
void test_timer1();
// ��ʱ��(ʱ���, ѭ��)
void test_timer2();
// ��ʱ�����ɾ��
void test_timer3();
// 
test1&& get_test();
// 
void test2();
