#include "../dump.h"
#include <tchar.h>
#include "tool/time.h"

namespace debug {
	LONG WINAPI dump_file(struct _EXCEPTION_POINTERS* info_)
	{
		std::string path;
		path.append(tool::time::get_time_str('_', '.')).append(".dmp");
		HANDLE file = CreateFile(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file == INVALID_HANDLE_VALUE) {
			return EXCEPTION_EXECUTE_HANDLER;
		}
		MINIDUMP_EXCEPTION_INFORMATION mdei;
		mdei.ThreadId = GetCurrentThreadId();
		mdei.ExceptionPointers = info_;
		mdei.ClientPointers = NULL;
		MINIDUMP_CALLBACK_INFORMATION mci;
		mci.CallbackRoutine = NULL;
		mci.CallbackParam = 0;
		// 写入转储文件
		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpNormal, &mdei, NULL, &mci);
		CloseHandle(file);
		FatalAppExit(-1, _T("已成功创建崩溃转储!"));
		return EXCEPTION_EXECUTE_HANDLER;
	}
}