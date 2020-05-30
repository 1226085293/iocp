#pragma once
#include <deque>
#include <string>
#include <functional>
#include <WinSock2.h>
#include "data_struct/max_heap.h"
#include "network/public/io_error_manage.h"
#include "pool/fixed_memory_pool.h"
#include "pool/variable_memory_pool.h"
#include "package.h"
#include "../public/define.h"

namespace network {
	namespace tcp {
		// 数据包管理
		class io_data {
			// 共享资源临界区
			enum class shared_cri {
				write,
				read,
			};
			// 内存池
			struct need_memory_pool {
				variable_memory_pool<8, 32>									common;
				fixed_memory_pool<std::deque<pack_info*>, 5>	deque;
			};
			// 缓冲区
			struct io_data_buf {
				WSABUF	read;	//读取
				WSABUF	write;	//写入
			};
		private:
			CRITICAL_SECTION								_cris[io_data_use::shared_num];			//临界区
			need_memory_pool								_memory_pool;							//内存池
			pack_info*							_current_write;							//当前写入数据
			max_heap<std::deque<pack_info*>>	_write_data;							//写入数据
			max_heap<pack_info>				_backup_data;							//备用写入数据
			char											_read_buf[io_data_use::read_buf_size];	//读取缓冲区
			std::string										_read_data;								//读取数据
			io_data_use::recv_func							_recv_func;								//接收函数回调
			bool											_io_switch[io_type_tnum];				//io开关

			// 编码
			void encode(pack_info* pack_, const char* data_, slen_t data_len_);
			// 解码
			void decode();
			// 清除备用写入数据
			void clear_backup();
			// 获取包长度
			uint32_t pack_len(slen_t data_len_);
			// 获取包总长度
			uint32_t total_len(slen_t data_len_);
		protected:
		public:
			io_data_buf			io_buf;
			io_error_manage	_error_mag;	//错误信息

			io_data(io_data_use::recv_func&& func_);
			~io_data();

			// 写入数据
			void write(const char* data_, slen_t data_len_, uint8_t priority_ = 0);
			// 设置当前操作数据
			void update_current_data();
			// 获取当前写入数据
			char* current_data();
			// 获取当前写入长度
			slen_t current_len();
			// 删除当前写入数据
			void del_current_data();
			// 删除所有写入数据
			void del_all_write();
			// 重置读取缓冲区
			void reset_read_buf();
			// 写入长度
			void write_len(slen_t len_);
			// 是否存在写入数据
			bool empty();
			// 添加接收数据
			void read(slen_t data_len_);
			// 合包
			void merge_io_data();
			// 设置io开关
			void set_io_switch(io_type type_, bool status);
		};

#include "source/io_data.icc"
	}
}