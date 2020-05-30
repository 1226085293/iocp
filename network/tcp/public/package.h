/*	Remarks: 
报文结构: 包头结构 + 包体 + 包尾结构
存储结构：包体信息 + 报文结构
*/
#pragma once
#include "network/tcp/public/define.h"

#pragma pack(push)
#pragma pack(1)

// 包体信息


namespace network::tcp {
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
	包头结构
	报文结构: 包头结构 + 包体 + 包尾结构
	*/
	struct head_pack {
		char	head;	//包头标识符
		slen_t	len;	//包长
	};

	/*
	包尾结构
	报文结构: 包头结构 + 包体 + 包尾结构
	*/
	struct tail_pack {
		slen_t	len;	//包长
		char	tail;	//包尾标识符
	};
#pragma pack(pop)
}