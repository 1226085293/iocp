#pragma once
#include <deque>
#include "package.h"
#include "pool/fixed_memory_pool.h"
#include "pool/variable_memory_pool.h"

namespace network::tcp::protocol {
	template <uint16_t min_memory, uint32_t block_contain>
	class protocol_base {
		// 必要内存池
		struct need_memory_pool {
			fixed_memory_pool<std::pair<char*, uint32_t>, 16>	read;
			variable_memory_pool<min_memory, block_contain>&	write;

			need_memory_pool(variable_memory_pool<min_memory, block_contain>& write_);
			need_memory_pool(const need_memory_pool&) = delete;
			need_memory_pool& operator =(const need_memory_pool&) = delete;
			~need_memory_pool() = default;
		};
	private:
	protected:
	public:
		need_memory_pool						_memory;	//内存池
		std::deque<std::pair<char*, uint32_t>>	_read_data;	//读取数据

		protocol_base(variable_memory_pool<min_memory, block_contain>& memory_);
		protocol_base(const protocol_base&) = delete;
		protocol_base& operator =(const protocol_base&) = delete;
		virtual ~protocol_base() = 0;

		// 编码
		virtual pack_info* encode(const char* str_, uint32_t len_);
		// 解码
		virtual bool decode(const char* str_, uint32_t len_);
	};
}