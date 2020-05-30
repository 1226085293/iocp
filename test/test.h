#pragma once
#include <stdio.h>
#include "network/tcp/iocp/server.h"

class test1 {
public:
	test1() {
		printf("构造()\n");
	}
	test1(const test1&) {
		printf("左值拷贝构造()\n");
	}
	test1(test1&&) {
		printf("右值拷贝构造()\n");
	}
	test1& operator =(const test1&) {
		printf("左值同类赋值()\n");
	}
	test1& operator =(test1&&) {
		printf("右值同类赋值()\n");
	}
	~test1() {
		printf("析构()\n");
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

// 最小堆
void test_mini_heap1();
void test_mini_heap2();
// 最大堆
void test_max_heap1();
void test_max_heap2();
// 定时器(时间堆, 事件)
void test_timer1();
// 定时器(时间堆, 循环)
void test_timer2();
// 定时器添加删除
void test_timer3();
// 
test1&& get_test();
// 
void test2();
