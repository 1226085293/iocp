#pragma once

namespace tool::memory {
	// �ͷŵ����ڴ�
	void del_ptr(void* ptr);
	// �ͷŶ���ڴ�
	void del_ptrs(void* ptr);

#include "source/memory.icc"
}