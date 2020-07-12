#pragma once
#include <Windows.h>
#include <functional>

namespace tool::system {
    // 关闭句柄
    void close_handle(HANDLE& handle);
	// 获取真实进程句柄
    std::shared_ptr<HANDLE> get_real_process_handle();
    // 获取真实线程句柄
    std::shared_ptr<HANDLE> get_real_thread_handle();
    // 传递句柄

#include "source/system.icc"
}