#pragma once

namespace tool::memory {
	// 释放单个内存
	void del_ptr(void* ptr);
	// 释放多个内存
	void del_ptrs(void* ptr);

#include "source/memory.icc"
}