#pragma once
#include <Windows.h>
#include <DbgHelp.h>

namespace debug {
	// ����ת���ļ�
	LONG WINAPI dump_file(struct _EXCEPTION_POINTERS* info_);
	// ��ʼ��ת��
	void init_dump();

#include "source/dump.icc"
}