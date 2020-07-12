#include "../package.h"
#include "tool/encode.h"

namespace network::websocket {
	bool request::update(std::string& info_) {
		size_t result_index;
		size_t find_index = 0;
		// 用户代理信息
		if (!update_child(info_, user_agent, result_index, find_index, "User-Agent: ")) {
			return false;
		}
		// 来源
		if (!update_child(info_, origin, result_index, find_index, "Origin: ")) {
			return false;
		}
		// 支持的压缩方式
		if (!update_child(info_, accept_encoding, result_index, find_index, "Accept-Encoding: ")) {
			return false;
		}
		// websocket秘钥
		if (!update_child(info_, sec_websocket_key, result_index, find_index, "Sec-WebSocket-Key: ")) {
			return false;
		}
		init = true;
		return true;
	}

	bool request::update_child(std::string& info_, std::string& data_, size_t& result_index_, size_t& find_index_, const char* mark) {
		result_index_ = info_.find(mark, find_index_);
		if (std::string::npos == result_index_) {
			return false;
		}
		result_index_ += strlen(mark);
		find_index_ = result_index_;
		result_index_ = info_.find('\r', find_index_);
		if (std::string::npos == result_index_) {
			return false;
		}
		data_ = info_.substr(find_index_, result_index_ - find_index_);
		find_index_ = result_index_;
		return true;
	}

	handshake::handshake(std::string key_) :
		protocol_info("HTTP/1.1 101 Switching Protocols\r\n"),
		upgrade("Upgrade: websocket\r\n"),
		connection("Connection: Upgrade\r\n"),
		content_encoding("Content-Encoding: gzip\r\n"),
		content_language("Content-Language: zh-CN\r\n"),
		sec_websocket_accept("Sec-WebSocket-Accept: ")
	{
		key_.append("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
		tool::encode::sha1(key_);
		tool::encode::base64(key_);
		sec_websocket_accept.append(key_).append("\r\n\r\n");
	}

	// 设置编码协议
	bool handshake::set_encode(request& request_, const char* type_) {
		// 安全检查
		if (std::string::npos == request_.accept_encoding.find(type_)) {
			return false;
		}
		content_encoding = "Content-Encoding: ";
		content_encoding.append(type_);
		return true;
	}
	// 合并
	std::string handshake::merge() {//.append(content_encoding).append(content_language)
		return std::string(protocol_info).
			append(upgrade).
			append(connection).
			append(sec_websocket_accept);
	}
}