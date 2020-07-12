#pragma once
#include <Windows.h>
#include <functional>

namespace tool::system {
    // �رվ��
    void close_handle(HANDLE& handle);
	// ��ȡ��ʵ���̾��
    std::shared_ptr<HANDLE> get_real_process_handle();
    // ��ȡ��ʵ�߳̾��
    std::shared_ptr<HANDLE> get_real_thread_handle();
    // ���ݾ��

#include "source/system.icc"
}