#pragma once
#include <functional>
#include "../../public/define.h"

namespace network::tcp {
	// --------------------------------常量定义
	// --------------------------------枚举
	// --------------------------------类/结构体声明
	class server;
	class client;
	// --------------------------------类型别名
	using slen_t = uint16_t;	//发送长度单位
	// --------------------------------函数别名
	// --------------------------------指针别名

	namespace io_data_use {
		// --------------------------------常量定义
		constexpr uint16_t	read_buf_size = 8192;	//读缓存大小
		constexpr uint16_t	write_buf_size = 1024;	//写缓存大小(不要超过最大传输单元MTU(1360~1500))
		constexpr uint8_t	shared_num = 2;			//共享资源数
		constexpr char		head_mark = '\r';		//包头标识符
		constexpr char		tail_mark = '\n';		//包尾标识符
		// --------------------------------枚举
		// --------------------------------类/结构体声明
		// --------------------------------类型别名
		// --------------------------------函数别名
		using recv_func = typename std::function<void(std::string&&)>;
		// --------------------------------指针别名
	}
}