#include "../test.h"
#include "data_struct/max_heap.h"
#include "data_struct/mini_heap.h"
#include "pool/thread_pool.h"
#include "pool/fixed_memory_pool.h"
#include "pool/variable_memory_pool.h"
#include "other/raii/critical.h"
#include "tool/time.h"
#include "timer/time_heap.h"
#include "../timer1/timer.h"

//template <class T>
//void sdgsfdgs<T>::A(T) {
//}

template <int N>
void asdfasdf::A(int aa[N]) {
}

void test_mini_heap1() {
	//sta_time = tool::time::get_ms();
	//// ◊Ó–°∂—≤‚ ‘
	//mini_heap<int> temp3(64);
	//for (int i = 100000; i > 0; --i) {
	//	temp3.push(i, &i);
	//}
	//printf("◊Ó–°∂—∫ƒ ±: %I64d\n", tool::time::get_ms() - sta_time);

	//sta_time = tool::time::get_ms();
	//// map≤Â»Î≤‚ ‘
	//std::map<int, int, std::less<int>> temp4;
	//for (int i = 100000; i > 0; --i) {
	//	temp4[i] = i;
	//}
	//printf("map∫ƒ ±: %I64d\n", tool::time::get_ms() - sta_time);
}

void test_mini_heap2() {
	//mini_heap<int> t1(64);
	//t1.push(1, nullptr);
	//t1.push(10, nullptr);
	//t1.push(2, nullptr);
	//t1.push(11, nullptr);
	//t1.push(12, nullptr);
	//t1.push(3, nullptr);
	//t1.push(4, nullptr);
	//t1.push(13, nullptr);
	//t1.push(14, nullptr);
	//t1.push(15, nullptr);
	//t1.push(16, nullptr);
	//t1.push(5, nullptr);
	//for (auto& i : t1) {
	//	printf("%d\n", i.size);
	//}
	//t1.del(11);
	//t1.del(4);
}

void test_max_heap1() {
	//auto sta_time = tool::time::get_ms();
	//// ◊Ó¥Û∂—≤‚ ‘
	//max_heap<int> temp2(64);
	///*for (int i = 0; i < 100000; ++i) {
	//	temp2.push(i, &i);
	//}*/
	//printf("◊Ó¥Û∂—∫ƒ ±: %I64d\n", tool::time::get_ms() - sta_time);

	//sta_time = tool::time::get_ms();
	//// map≤Â»Î≤‚ ‘
	//std::map<int, int, std::greater<int>> temp1;
	//for (int i = 0; i < 100000; ++i) {
	//	temp1[i] = i;
	//}
	//printf("map∫ƒ ±: %I64d\n", tool::time::get_ms() - sta_time);
}

void test_max_heap2() {
	//max_heap<int> t2(64);//{79,66,43,83,30,87,38,55,91,72,49,9}
	//t2.push(79, nullptr);
	//t2.push(66, nullptr);
	//t2.push(43, nullptr);
	//t2.push(83, nullptr);
	//t2.push(30, nullptr);
	//t2.push(87, nullptr);
	//t2.push(38, nullptr);
	//t2.push(55, nullptr);
	//t2.push(91, nullptr);
	//t2.push(72, nullptr);
	//t2.push(49, nullptr);
	//t2.push(9, nullptr);
	//for (auto& i : t2) {
	//	printf("%d\n", i.size);
	//}
	//t2.del(87);
	//t2.del(4);
}

void test_timer1() {
	auto& timer = time_heap::instance();
	double second = 5;
	uint64_t count = 1000000;
	uint64_t sta_time = tool::time::get_ms();
	for (int i = 1; i <= count; ++i) {
		timer.add(second, 1, [&](int index) {
			if (index == count) {
				printf("ŒÛ≤Ó: %I64d\n", tool::time::get_ms() - sta_time - static_cast<uint64_t>(second) * 1000);
			}
			//printf("index: %d\n", index);
			}
		, i);
	}
}

void test_timer2() {
	auto timer1 = new zsummer::network::Timer;
	uint64_t second = 5000;
	uint64_t count = 1000000;
	uint64_t sta_time = tool::time::get_ms();
	for (int i = 1; i <= count; ++i) {
		auto func1 = [&](int index) {
			if (index == count) {
				printf("ŒÛ≤Ó: %I64d\n", tool::time::get_ms() - sta_time - second);
			}
			//printf("index: %d\n", index);
		};
		auto func2 = std::bind(func1, i);
		timer1->createTimer(static_cast<uint32_t>(second), [=] {
			func2();
			}
		);
	}
	while (true) {
		timer1->checkTimer();
	}
}

void test_timer3() {
	auto v = 0;
	auto& timer = time_heap::instance();
	auto re1 = timer.add(5, 2, [&] {
		printf("5√Î%d\n", ++v);
		return v;
		}
	);
	auto re2 = timer.add(2, 1, [=,&v] {
		printf("2√Î%d\n", ++v);
		/*if (v == 1)*/
		re1->del();
		return v;
		}
	);
	while (true) {
		if (!re2->valid) {
			return;
		}
		else {
			printf("%d\n", re2->get());
		}
		//printf("%d\n", re2->get());
		//printf("%d\n", re1->get());
		//printf("%d\n", re1->get());
		/*printf("%d\n", re1->get());*/
	}


	/*auto timer2 = timer.add(2, 1, [=](int index) {
		printf("index: %d\n", index);
		}
	, 2);
	auto timer5 = timer.add(5, 1, [=](int index) {
		printf("index: %d\n", index);
		}
	, 5);
	auto timer3 = timer.add(3, 1, [=](int index) {
		printf("index: %d\n", index);
		timer5->del();
		}
	, 3);
	auto timer1 = timer.add(1, 1, [=](int index) {
		printf("index: %d\n", index);
		if (timer2->valid) {
			timer2->del();
		}
		}
	, 1);
	auto timer4 = timer.add(4, 1, [=](int index) {
		printf("index: %d\n", index);
		}
	, 4);*/
}

test1&& get_test() {
	test1 temp1;
	return std::move(temp1);
}

void test2() {
	test1 temp(get_test());
}