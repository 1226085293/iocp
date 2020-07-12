#include "../encode.h"
#include <tchar.h>
#include <wincrypt.h>
#include <memory>

namespace tool::encode {
	DWORD hash(ALG_ID hash_type_, std::string& key_, std::string& result_)
	{
		HCRYPTPROV hcry_prov;
		if (!CryptAcquireContext(&hcry_prov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
			return GetLastError();
		}
		HCRYPTHASH hcry_hash;
		if (!CryptCreateHash(hcry_prov, hash_type_, 0, 0, &hcry_hash)) {
			CryptReleaseContext(hcry_prov, 0);
			return GetLastError();
		}
		if (!CryptHashData(hcry_hash, reinterpret_cast<BYTE*>(&key_.front()), static_cast<DWORD>(key_.length()), 0)) {
			CryptDestroyHash(hcry_hash);
			CryptReleaseContext(hcry_prov, 0);
			return GetLastError();
		}
		DWORD size, len = sizeof(DWORD);
		CryptGetHashParam(hcry_hash, HP_HASHSIZE, (BYTE*)(&size), &len, 0);
		// 获取hash结果
		BYTE* hash_result = new BYTE[size];
		CryptGetHashParam(hcry_hash, HP_HASHVAL, hash_result, &size, 0);
		result_.append(reinterpret_cast<char*>(hash_result), size);
		delete[] hash_result;
		CryptDestroyHash(hcry_hash);
		CryptReleaseContext(hcry_prov, 0);
		return 0;
	}

	template<class T, class... Args>
	T _sha1(Args&&... args_) {
		// 准备参数
		T data(std::forward<Args>(args_)...);
		static std::string output_space;
		uint32_t size, i, j, index1, temp1, buf[5], w[80];
		uint32_t k[] = { 0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6 };
		uint32_t h[] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
		// 计算占用空间
		uint32_t len = static_cast<uint32_t>(data.length());
		uint32_t rem = len % 64;
		size = (rem < 56 ? (len - rem + 64)
			: rem == 56 ? (len + 72) : (len - rem + 128)) / 4;
		output_space.resize(size * sizeof(uint32_t), 0);
		auto output = reinterpret_cast<uint32_t*>(&output_space[0]);
		// 补位
		for (i = 0; i < len; i++) {
			output[i / 4] |= data[i] << (24 - ((i & 0x03) << 3));
		}
		output[i / 4] |= 0x80 << (24 - ((i & 0x03) << 3));
		// 补长
		output[size - 1] = len * 8;
		// 计算消息摘要
		for (i = 0; i < size; i += 16) {
			for (j = 0; j < 16; ++j) {
				w[j] = output[j + i];
			}
			for (j = 16; j < 80; ++j) {
				w[j] = w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16];
				w[j] = (w[j] << 1) | (w[j] >> 31);
			}
			for (j = 0; j < 5; ++j) {
				buf[j] = h[j];
			}
			for (j = 0; j < 80; ++j) {
				index1 = j / 20;
				temp1 = index1 == 0 ? ((buf[1] & buf[2]) | (~buf[1] & buf[3]))
					: index1 == 2 ? ((buf[1] & buf[2]) | (buf[1] & buf[3]) | (buf[2] & buf[3])) : (buf[1] ^ buf[2] ^ buf[3]);
				temp1 += ((buf[0] << 5) | (buf[0] >> 27)) + buf[4] + w[j] + k[index1];
				buf[4] = buf[3];
				buf[3] = buf[2];
				buf[2] = (buf[1] << 30) | (buf[1] >> 2);
				buf[1] = buf[0];
				buf[0] = temp1;
			}
			for (j = 0; j < 5; ++j) {
				h[j] += buf[j];
			}
		}
		data.resize(20);
		for (i = 0; i < 20; ++i) {
			data[i] = reinterpret_cast<char*>(h + i / 4)[(19 - i) & 0x03];
		}
		printf("SHA1 = %08X%08X%08X%08X%08X\n",h[0],h[1],h[2],h[3],h[4]);
		return static_cast<T>(data);
	}

	std::string& sha1(std::string& data_) {
		return _sha1<std::string&>(data_);
	}

	std::string sha1(std::string&& data_) {
		return _sha1<std::string&&>(std::move(data_));
	}

	std::string& base64(std::string& str_) {
		static int8_t char_lib[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string result;
		uint32_t i, j;
		uint8_t* str = reinterpret_cast<uint8_t*>(&str_.front());
		uint32_t loop_num = static_cast<uint32_t>(str_.length() & 0x03 ? str_.length() - 3 : str_.length());
		result.resize(loop_num / 3 * 4);
		for (i = 0, j = 0; i + 3 <= loop_num; i += 3) {
			result[j++] = char_lib[str[i] >> 2];									//取出第一个字符的前6位并找出对应的结果字符
			result[j++] = char_lib[((str[i] << 4) & 0x30) | (str[i + 1] >> 4)];		//将第一个字符的后2位与第二个字符的前4位进行组合并找到对应的结果字符
			result[j++] = char_lib[((str[i + 1] << 2) & 0x3c) | (str[i + 2] >> 6)]; //将第二个字符的后4位与第三个字符的前2位组合并找出对应的结果字符
			result[j++] = char_lib[str[i + 2] & 0x3f];								//取出第三个字符的后6位并找出结果字符
		}
		if (i < str_.length()) {
			result.resize(result.size() + 4);
			if (str_.length() - i == 1) {
				result[j++] = char_lib[str[i] >> 2];
				result[j++] = char_lib[(str[i] << 4) & 0x30];
				result[j++] = '=';
				result[j++] = '=';
			}
			else {
				result[j++] = char_lib[str[i] >> 2];
				result[j++] = char_lib[((str[i] << 4) & 0x30) | (str[i + 1] >> 4)];
				result[j++] = char_lib[(str[i + 1] << 2) & 0x3c];
				result[j++] = '=';
			}
		}
		return str_ = result;
	}
}