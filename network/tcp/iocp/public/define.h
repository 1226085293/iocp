#pragma once
#include <WinSock2.h>
#include <functional>
#include "../../public/define.h"
#include "../server/event_info.h"

// server使用(备注: 1.防止公共组件命名冲突; 2.此处只放对外部公开数据)
namespace network::tcp::iocp::server_use {
	// --------------------------------常量定义
	constexpr uint16_t	socket_load = 512;				//套接字预加载数
	constexpr uint16_t	data_load = 512;				//用户预加载数
	constexpr uint16_t	context_load = data_load * 2;	//上下文预加载数
	constexpr uint16_t	info_load = data_load * 2;		//事件预加载数
	constexpr uint8_t	entry_count = 64;				//重叠结构数组大小
	constexpr uint8_t	shared_num = 5;					//共享资源数
	constexpr uint16_t	write_wait_ms = 50;				//发送等待时间默认值
	// --------------------------------枚举
	// 事件类型
	enum class event_type {
		null,
		accept = 0x01,	//连接
		recv = 0x02,	//接收
		send = 0x04,	//发送
		disconnect = 0x08,	//断开
	};
	// io类型
	enum class io_type {
		null,
		accept = 0x01,	//连接
		request_read = 0x02,	//请求读
		read = 0x04,	//读
		write = 0x08,	//写
		disconnect = 0x10,	//断开
		forced_disconnect = 0x20,	//强制断开
	};
	// io标志类型
	enum class io_mark_type {
		null,
		read = 0x01,	//读取
		write = 0x02,	//写入
		disconnect = 0x04,	//断开
	};
	// user构造类型
	enum class user_init_type {
		null,
		monitor,	//监听
		accept,		//连接
		normal,		//正常
		disconnect,	//断开
	};
	// user状态
	enum class user_status {
		null,
		wait_close = 0x01,	//等待关闭
		write_wait = 0x02,	//写入等待
		write_timer = 0x04,	//写入倒计时
	};
	// --------------------------------类/结构体声明
	class user;
	class io_context;
	// --------------------------------类型别名
	// --------------------------------函数别名
	using event_func = typename std::function<void(event_info*)>;
	// --------------------------------指针别名
	using server_ptr = std::shared_ptr<server>;
	using user_ptr = std::shared_ptr<user>;
}