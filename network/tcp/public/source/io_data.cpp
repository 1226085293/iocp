#include "../io_data.h"

namespace network {
	namespace tcp {
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
			// �ͷ��ٽ���
			for (uint8_t i = 0; i < shared_num; DeleteCriticalSection(&_cris[i++]));
		}

		void io_data::encode(pack_info* pack_, const char* data_, slen_t data_len_) {
			pack_->slen = 0;
			pack_->len = pack_len(data_len_);
			auto head = reinterpret_cast<head_pack*>(reinterpret_cast<char*>(pack_ + 1));
			head->head = head_mark;
			head->len = pack_->len;
			memcpy_s(head + 1, data_len_, data_, data_len_);
			auto tail = reinterpret_cast<tail_pack*>(reinterpret_cast<char*>(head + 1) + data_len_);
			tail->len = pack_->len;
			tail->tail = tail_mark;
		}

		void io_data::decode() {
			while (true) {
				// ����Ƿ������ͷ��Ϣ(��ͷ��Ϣ������)
				if (_read_data.length() < sizeof(head_pack)) {
					return;
				}
				auto head = reinterpret_cast<head_pack*>(&_read_data[0]);
				// ��ȡ���� < ��ͷ��Ϣ(����δ��������)
				if (_read_data.length() < head->len) {
					return;
				}
				// ��ȡ���� >= ��ͷ��Ϣ
				else {
					// ��ȫ����֤
					auto tail = reinterpret_cast<tail_pack*>(&_read_data[head->len - sizeof(tail_pack)]);
					// ��֤��β��ʶ��
					if (tail->tail != tail_mark) {
						_error_mag.add_error(io_err_type::discard);
						auto pos = _read_data.find_first_of(head_mark);
						// �����¸���ͷ��ʶ��
						if (!pos) {
							pos = _read_data.find_first_of(head_mark, 1);
						}
						// ������ȱ����
						if (pos != std::string::npos) {
							_read_data = _read_data.substr(pos, _read_data.length());
							continue;
						}
						_read_data.clear();
						break;
					}
					else {
						// ��ȱ��
						if (tail->len != head->len) {
							_error_mag.add_error(io_err_type::discard);
							auto pos = _read_data.find_first_of(head_mark, 1);
							// ������ȱ����
							if (pos != std::string::npos) {
								_read_data = _read_data.substr(pos, _read_data.length());
								continue;
							}
							_read_data.clear();
							break;
						}
						// ���ջص�
						_error_mag.no_error(io_err_type::discard);
						_recv_func(_read_data.substr(sizeof(head_pack), head->len - pack_len(0)));
						_read_data = _read_data.substr(head->len, _read_data.length());
					}
				}
			}
		}

		void io_data::clear_backup() {
			if (_backup_data.empty()) {
				return;
			}
			std::deque<pack_info*>* deque;
			for (auto& i : _backup_data) {
				// ¼������
				deque = _write_data.find(i.size);
				if (!deque) {
					deque = _memory_pool.deque.new_obj();
					_write_data.push(i.size, deque);
				}
				deque->push_back(i.data);
			}
			_backup_data.clear();
		}

		void io_data::write(const char* data_, slen_t data_len_, uint8_t priority_) {
			raii::critical r1(&_cris[static_cast<uint32_t>(shared_cri::write)]);
			// ��ȫ���
			if (!_io_switch[static_cast<uint32_t>(io_type::write)]) {
				return;
			}
			if (pack_len(data_len_) > UINT16_MAX) {
				printf("��Ϣ�����ȳ���slen_t��С");
				return;
			}
			// ����
			auto pack = reinterpret_cast<pack_info*>(_memory_pool.common.allocate(total_len(data_len_)));
			encode(pack, data_, data_len_);
			// ����δ����������ݲ���д���������ȼ� > δ���������ô�����뱸�ö���
			if (!_write_data.empty()) {
				auto front_pack = reinterpret_cast<pack_info*>(_write_data.front()->data->front());
				if (priority_ > _write_data.front()->size && (front_pack->slen > 0)) {
					_backup_data.push(priority_, pack);
					return;
				}
			}
			// ¼������
			auto deque = _write_data.find(priority_);
			if (!deque) {
				deque = _memory_pool.deque.new_obj();
				_write_data.push(priority_, deque);
			}
			deque->push_back(pack);
		}

		void io_data::update_current_data() {
			if (_current_write || _write_data.empty()) {
				return;
			}
			// ���õ�ǰд������
			auto deque = _write_data.front()->data;
			_current_write = deque->front();
			// ��������б�����
			if (_current_write->slen > 0) {
				clear_backup();
			}
			// ɾ����������
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

		void io_data::write_len(slen_t len_) {
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
			// ���ڵ�ǰд�����ݻ��ߴ�д������Ϊ���˳�
			if (_current_write || _write_data.empty()) {
				return;
			}
			// ��ֻ��һ����Ϣ�б�һ���������˳�����
			if (_write_data.size() == 1 && _write_data.front()->data->size() == 1) {
				update_current_data();
				return;
			}
			// ׼������
			pack_info* write_data;
			slen_t total_size = 0, pack_size;
			auto deque = _write_data.front()->data;
			// ��ʼ������
			_current_write = reinterpret_cast<pack_info*>(_memory_pool.common.allocate(write_buf_size + sizeof(pack_info)));
			_current_write->slen = 0;		//�ѷ��ͳ���
			// ��ȡд������
			while (!_write_data.empty() && total_size < write_buf_size) {
				write_data = deque->front();
				pack_size = write_data->len - write_data->slen;
				if ((total_size + pack_size) > write_buf_size) {
					// �޸İ���С(��֤ռ�ÿռ�һ����)
					pack_size = write_buf_size - total_size;
				}
				memcpy_s(reinterpret_cast<char*>(_current_write + 1) + total_size, write_buf_size - total_size, reinterpret_cast<char*>(write_data + 1) + write_data->slen, pack_size);
				total_size += pack_size;
				// �޸��ѷ��ͳ���(��֤д������������)
				write_data->slen += pack_size;
				// ɾ����������
				if (write_data->slen == write_data->len) {
					deque->pop_front();
					if (deque->empty()) {
						_memory_pool.deque.del_obj(deque);
						_write_data.pop();
					}
					// ��������б�����
					clear_backup();
					// ���µ�ǰ��Ϣ����
					if (!_write_data.empty()) {
						deque = _write_data.front()->data;
					}
				}
			}
			_current_write->len = total_size;	//����ܳ���
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