#pragma once
#include <Windows.h>
#include <string>

// ±àÂë
namespace tool::encode {
	// hash
	DWORD hash(ALG_ID hash_type_, std::string& key_, std::string& result_);
	// sha1
	std::string& sha1(std::string& data_);
	std::string sha1(std::string&& data_);
	// base64
	std::string& base64(std::string& str_);
}