#include "../io_data.h"
#include "tool/encode.h"
#include "tool/convert.h"

namespace network {
	namespace websocket {
		using namespace io_data_use;

		io_data::io_data(io_data_use::recv_func&& func_) :
			_current_write(nullptr),
			_recv_func(func_)
		{
			for (uint8_t i = 0; i < shared_num; InitializeCriticalSection(&_cris[i++]));
			for (int i = 0; i < io_type_tnum; _io_switch[i++] = true);
			io_buf.read.buf = _read_buf;
			io_buf.read.len = read_buf_size;
		}

		io_data::~io_data() {
			del_all_write();
			// 释放临界区
			for (uint8_t i = 0; i < shared_num; DeleteCriticalSection(&_cris[i++]));
		}

		void io_data::encode(pack_info* pack_, char* str_, uint32_t str_len_, uint32_t pack_len_, bool fin_, bool opcode_) {
			// 设置包信息
			pack_->slen = 0;
			pack_->len = pack_len_;
			/* 设置包体数据
			1.单独包: fin为1表示最后的包, opcode为类型
			2.分包	: (1). fin为0表示有后续包, opcode为类型
					  (2). fin为0表示有后续包, opcode为0表示延续帧(和上帧拼接)
					  (3). fin为1表示最后的包, opcode为0表示延续帧(和上帧拼接)
			*/
			char* write_head = reinterpret_cast<char*>(pack_ + 1);
			memset(write_head, 0, pack_len_);
			*write_head++ |= (fin_ && opcode_) ? 0x81 : (!fin_ && !opcode_) ? 0x0 : (!fin_ && opcode_) ? 0x1 : 0x80;
			switch (pack_len_ - str_len_) {
			case 2: {	//0-125, 后7位真实长度
				*write_head++ |= str_len_;
			} break;
			case 4: {	//126, 长度后2个字节
				uint16_t len = static_cast<uint16_t>(str_len_);
				*write_head++ |= 126;
				for (int i = 8; i >= 0; i -= 8) {
					*write_head++ = len >> i;
				}
			} break;
			case 10: {	//127, 长度后8个字节
				uint64_t len = str_len_;
				*write_head++ |= 127;
				for (int i = 56; i >= 0; i -= 8) {
					*write_head++ = static_cast<char>(len >> i);
				}
			} break;
			default: {
				FatalAppExit(-1, "严重错误, websocket encode编码错误!");
			}
			}
			memcpy_s(write_head, str_len_, str_, str_len_);
		}

		void io_data::decode() {
			if (!_requset.init) {
				if (!_requset.update(_read_data)) {
					_error_mag.add_error(io_err_type::request);
					return;
				}
				_requset.init = true;
				// 返回握手信息
				handshake();
				printf("握手\n");
				return;
			}
			while (_read_data.length()) {
				// 检测是否包含必要信息(FIN(结束标识), Payload_len(包长))
				if (_read_data.length() < 2) {
					return;
				}
				// 客户端掩码位必须为1
				if (!(_read_data[1] & 0x80)) {
					printf("客户端掩码错误\n");
					_error_mag.add_error(io_err_type::mask);
					return;
				}
				_error_mag.no_error(io_err_type::mask);
				// 检查opcode类型
				opcode type = static_cast<opcode>(_read_data[0] & 0xf);
				printf("opcode: %d\n", type);
				if (type == opcode::close) {
					printf("客户端主动断开\n");
					_error_mag.add_error(io_err_type::close);
					return;
				}
				if ((type >= opcode::unretain_sta && type <= opcode::unretain_end) ||
					(type >= opcode::retain_sta)) {
					printf("opcode类型错误(error: %0x8)", type);
					_error_mag.add_error(io_err_type::opcode);
					return;
				}
				// 数据长度
				uint64_t pack_len;
				uint32_t payload_len = _read_data[1] & 0x7F;
				byte masks[4];
				std::string read_data;
				if (payload_len < 126) {
					if (!payload_len) {
						printf("包大小为0, 错误!\n");
						_error_mag.add_error(io_err_type::close);
						return;
					}
					memcpy_s(masks, 4, &_read_data[2], 4);
					// 拷贝数据
					read_data.append(&_read_data[6], payload_len);
					pack_len = 6 + payload_len;
				}
				else if (payload_len == 126) {
					memcpy_s(masks, 4, &_read_data[4], 4);
					// 拷贝数据
					payload_len = *reinterpret_cast<uint16_t*>(&_read_data[3]);
					read_data.append(&_read_data[8], payload_len);
					pack_len = 8 + payload_len;
				}
				else {
					memcpy_s(masks, 4, &_read_data[10], 4);
					// 拷贝数据
					uint64_t len = *reinterpret_cast<uint64_t*>(&_read_data[3]);
					read_data.append(&_read_data[14], len);
					pack_len = 14 + len;
				}
				for (int i = 0; i < read_data.length(); i++) {
					read_data[i] ^= masks[i % 4];
				}
				// 消息处理
				tool::convert::utf8_to_multibyte(read_data);
				// 非最后帧
				if (!(_read_data[0] & 0x80)) {
					_read_splice.append(read_data);
				}
				else {
					// 非拼接数据
					if (!_read_splice.length()) {
						_recv_func(std::string(read_data));
					}
					// 拼接数据
					else {
						_read_splice.append(read_data);
						_recv_func(std::string(_read_splice));
					}
					_read_data = _read_data.substr(pack_len, _read_data.length());
				}
			}
		}

		void io_data::clear_backup() {
			if (_backup_data.empty()) {
				return;
			}
			std::deque<pack_info*>* deque;
			for (auto& i : _backup_data) {
				// 录入数据
				deque = _write_data.find(i.size);
				if (!deque) {
					deque = _memory_pool.deque.new_obj();
					_write_data.push(i.size, deque);
				}
				deque->push_back(i.data);
			}
			_backup_data.clear();
		}

		void io_data::package_len(uint32_t total_len_, uint32_t& data_len_, uint32_t& pack_len_) {
			// 对于超过write_buf_size的数据进行分包发送, 防止ip分片, 包长(1(fin + rsv1-3 + opcode) + mask_payload_len + data_len_)
			uint8_t mask_payload_len = total_len_ < 126 ? 2 : (total_len_ <= UINT16_MAX ? 4 : 10);
			uint32_t len = total_len_ + mask_payload_len;
			if (len <= io_data_use::write_buf_size) {
				data_len_ = total_len_;
				pack_len_ = len;
			}
			// 计算分片大小
			else {
				mask_payload_len = (io_data_use::write_buf_size - 2) < 126 ? 2 : ((io_data_use::write_buf_size - 4) <= UINT16_MAX ? 4 : 10);
				data_len_ = io_data_use::write_buf_size - mask_payload_len;
				pack_len_ = io_data_use::write_buf_size;
			}
		}

		void io_data::handshake() {
			websocket::handshake pack_data(_requset.sec_websocket_key);
			auto&& key = pack_data.merge();
			// 清除读取缓存
			reset_read_buf();
			_read_data.clear();
			//write(key.c_str(), static_cast<slen_t>(key.length()));
			// 录入数据
			auto write_date = reinterpret_cast<pack_info*>(_memory_pool.common.allocate(sizeof(pack_info) + static_cast<uint32_t>(key.length())));
			write_date->slen = 0;
			write_date->len = static_cast<slen_t>(key.length());
			memcpy_s(write_date + 1, key.length(), &key[0], key.length());
			auto deque = _memory_pool.deque.new_obj();
			_write_data.push(0, deque);
			deque->push_back(write_date);
		}

		void io_data::write(std::string str_, uint8_t priority_) {
			raii::critical r1(&_cris[static_cast<uint32_t>(shared_cri::write)]);
			// 安全检查
			if (!_io_switch[static_cast<uint32_t>(io_type::write)]) {
				return;
			}
			// 转utf-8
			tool::convert::multibyte_to_utf8(str_);
			// 编码
			uint32_t index = 0, data_len, pack_len;
			pack_info* pack;
			while (index < str_.length()) {
				package_len(static_cast<uint32_t>(str_.length() - index), data_len, pack_len);
				pack = reinterpret_cast<pack_info*>(_memory_pool.common.allocate(sizeof(pack_info) + pack_len));
				encode(pack, &str_[index], data_len, pack_len, (index + data_len) == str_.length(), !index);
				index += data_len;
				// 存在未发送完的数据并且写入数据优先级 > 未完成数据那么将加入备用队列
				if (!_write_data.empty()) {
					auto front_pack = reinterpret_cast<pack_info*>(_write_data.front()->data->front());
					if (priority_ > _write_data.front()->size && (front_pack->slen > 0)) {
						_backup_data.push(priority_, pack);
						return;
					}
				}
				// 录入数据
				auto deque = _write_data.find(priority_);
				if (!deque) {
					deque = _memory_pool.deque.new_obj();
					_write_data.push(priority_, deque);
				}
				deque->push_back(pack);
			}
		}

		void io_data::update_current_data() {
			if (_current_write || _write_data.empty()) {
				return;
			}
			// 设置当前写入数据
			auto deque = _write_data.front()->data;
			_current_write = deque->front();
			// 清除备用列表数据
			if (_current_write->slen > 0) {
				clear_backup();
			}
			// 删除队列数据
			deque->pop_front();
			if (deque->empty()) {
				_memory_pool.deque.del_obj(deque);
				_write_data.pop();
			}
		}

		void io_data::del_all_write() {
			raii::critical r1(&_cris[static_cast<uint32_t>(shared_cri::write)]);
			for (auto& i : _write_data) {
				for (auto j : *i.data) {
					_memory_pool.common.deallocate(reinterpret_cast<char*>(j));
				}
				_memory_pool.deque.del_obj(i.data);
			}
			while (!_write_data.empty()) {
				_write_data.pop();
			}
			for (auto& i : _backup_data) {
				_memory_pool.common.deallocate(reinterpret_cast<char*>(i.data));
			}
			while (!_backup_data.empty()) {
				_backup_data.pop();
			}
			del_current_data();
		}

		void io_data::write_len(uint32_t len_) {
			if (!_current_write) {
				return;
			}
			auto pack = reinterpret_cast<pack_info*>(_current_write);
			pack->slen += len_;
			if (pack->slen == pack->len) {
				del_current_data();
			}
		}

		void io_data::merge_io_data() {
			raii::critical r1(&_cris[static_cast<uint32_t>(shared_cri::write)]);
			// 存在当前写入数据或者待写入数据为空退出
			if (_current_write || _write_data.empty()) {
				return;
			}
			// 若只有一个消息列表一条数据则退出操作
			if (_write_data.size() == 1 && _write_data.front()->data->size() == 1 && _write_data.front()->data->front()->len < write_buf_size) {
				update_current_data();
				return;
			}
			// 准备参数
			pack_info* write_data;
			slen_t total_size = 0, pack_size;
			auto deque = _write_data.front()->data;
			// 初始化数据
			_current_write = reinterpret_cast<pack_info*>(_memory_pool.common.allocate(write_buf_size + sizeof(pack_info)));
			_current_write->slen = 0;		//已发送长度
			// 读取写入数据
			while (!_write_data.empty() && total_size < write_buf_size) {
				write_data = deque->front();
				pack_size = write_data->len - write_data->slen;
				if ((total_size + pack_size) > write_buf_size) {
					// 修改包大小(保证占用空间一致性)
					pack_size = write_buf_size - total_size;
				}
				memcpy_s(reinterpret_cast<char*>(_current_write + 1) + total_size, write_buf_size - total_size, reinterpret_cast<char*>(write_data + 1) + write_data->slen, pack_size);
				total_size += pack_size;
				// 修改已发送长度(保证写入数据连续性)
				write_data->slen += pack_size;
				// 删除队列数据
				if (write_data->slen == write_data->len) {
					deque->pop_front();
					if (deque->empty()) {
						_memory_pool.deque.del_obj(deque);
						_write_data.pop();
					}
					// 清除备用列表数据
					clear_backup();
					// 更新当前消息队列
					if (!_write_data.empty()) {
						deque = _write_data.front()->data;
					}
				}
			}
			_current_write->len = total_size;	//大包总长度
		}

		void io_data::set_io_switch(io_type type_, bool status) {
			switch (type_) {
			case io_type::read: {
				raii::critical r1(&_cris[static_cast<uint32_t>(shared_cri::read)]);
				_io_switch[static_cast<uint32_t>(type_)] = status;
			} break;
			case io_type::write: {
				raii::critical r1(&_cris[static_cast<uint32_t>(shared_cri::write)]);
				_io_switch[static_cast<uint32_t>(type_)] = status;
			} break;
			}
		}
	}
}