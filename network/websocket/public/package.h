/*	Remarks: 
报文结构: 包头结构 + 包体 + 包尾结构
存储结构：包体信息 + 报文结构
*/
#pragma once
#include "network/websocket/public/define.h"

namespace network::websocket {
#pragma pack(push)
#pragma pack(1)
	/*
	包体信息
	存储结构：包体信息 + 报文结构
	报文结构: 包头结构 + 包体 + 包尾结构
	*/
	struct pack_info {
		slen_t	slen;	//发送长度
		slen_t	len;	//包长
	};

	/*
	请求结构
	*/
	class request {
	private:
		bool update_child(std::string& info_, std::string& data_, size_t& result_index_, size_t& find_index_, const char* mark);
	protected:
	public:
		bool		init;						//初始化状态
		std::string user_agent;					//用户代理信息
		std::string origin;						//来源
		std::string accept_encoding;			//支持的压缩方式
		std::string sec_websocket_key;			//websocket秘钥

		request();
		request(const request&) = delete;
		request& operator =(const request&) = delete;
		~request() = default;

		// 更新信息
		bool update(std::string& info_);
	};

	/*
	应答结构
	*/
	struct handshake {
		std::string protocol_info;			//协议信息
		std::string connection;				//连接类型
		std::string upgrade;				//升级协议
		std::string content_encoding;		//选择的压缩方式
		std::string content_language;		//选择的客户端语言
		std::string sec_websocket_accept;	//websocket秘钥(base64(sha1(sec_websocket_key + 258EAFA5-E914-47DA-95CA-C5AB0DC85B11)))

		handshake(std::string key_);

		// 设置编码协议
		bool set_encode(request& request_, const char* type_);
		// 合并
		std::string merge();
	};
#pragma pack(pop)

#include "source/package.icc"
}