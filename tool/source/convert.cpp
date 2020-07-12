#include "../convert.h"
#include <Windows.h>

namespace tool::convert {
	void utf8_to_multibyte(std::string& str_) {
		int len = MultiByteToWideChar(CP_UTF8, 0, str_.c_str(), -1, NULL, NULL);
		wchar_t* wbuf = new wchar_t[len + 1];
		memset(wbuf, 0, len * 2 + 2);
		MultiByteToWideChar(CP_UTF8, NULL, str_.c_str(), -1, wbuf, len);
		len = WideCharToMultiByte(CP_ACP, 0, wbuf, -1, NULL, 0, NULL, NULL);
		str_.resize(len - 1);
		WideCharToMultiByte(CP_ACP, 0, wbuf, -1, &str_[0], len, NULL, NULL);
		delete[] wbuf;
	}

	void multibyte_to_utf8(std::string& str_) {
        int len = MultiByteToWideChar(CP_ACP, NULL, str_.c_str(), -1, NULL, NULL);
        wchar_t* wbuf = new wchar_t[len + 1];
        memset(wbuf, 0, len * 2 + 2);
        MultiByteToWideChar(CP_ACP, NULL, str_.c_str(), -1, wbuf, len);
        len = WideCharToMultiByte(CP_UTF8, 0, wbuf, len - 1, NULL, 0, NULL, NULL);
		str_.resize(len);
        WideCharToMultiByte(CP_UTF8, 0, wbuf, len - 1, &str_[0], len, NULL, NULL);
        delete[] wbuf;
	}
}