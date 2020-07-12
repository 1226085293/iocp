#include "../system.h"
#include <DbgHelp.h>
#pragma comment(lib, "dbghelp.lib")

namespace tool::system {
    std::shared_ptr<HANDLE> get_real_process_handle();

    std::shared_ptr<HANDLE> get_real_thread_handle();
}