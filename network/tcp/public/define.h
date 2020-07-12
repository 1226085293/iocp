#pragma once
#include <functional>
#include "../../public/define.h"

namespace network::tcp {
	// --------------------------------常量定义
	constexpr uint32_t io_err_tnum = 3;		//io错误类型数
	// --------------------------------枚举
	// 数据包错误类型
	enum class io_err_type {
		discard,	//丢包数(残缺包)
		invalid,	//无校包(解码后存在剩余空间/空间不够解码完成)
		repeat,		//重复数(时间戳相同/消息类型相同并且不在重复消息列表内)
	};
	// --------------------------------类/结构体声明
	class server;
	class client;
	// --------------------------------类型别名
	using slen_t = uint16_t;	//发送长度单位
	// --------------------------------函数别名
	// --------------------------------指针别名
	using server_ptr = std::shared_ptr<server>;
	using client_ptr = std::shared_ptr<client>;

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