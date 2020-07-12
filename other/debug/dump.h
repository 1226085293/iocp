#pragma once
#include <Windows.h>
#include <DbgHelp.h>

namespace debug {
	// 创建转储文件
	LONG WINAPI dump_file(struct _EXCEPTION_POINTERS* info_);
	// 初始化转储
	void init_dump();

#include "source/dump.icc"
}