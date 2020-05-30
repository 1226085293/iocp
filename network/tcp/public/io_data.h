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
		// ���ݰ�����
		class io_data {
			// ������Դ�ٽ���
			enum class shared_cri {
				write,
				read,
			};
			// �ڴ��
			struct need_memory_pool {
				variable_memory_pool<8, 32>									common;
				fixed_memory_pool<std::deque<pack_info*>, 5>	deque;
			};
			// ������
			struct io_data_buf {
				WSABUF	read;	//��ȡ
				WSABUF	write;	//д��
			};
		private:
			CRITICAL_SECTION								_cris[io_data_use::shared_num];			//�ٽ���
			need_memory_pool								_memory_pool;							//�ڴ��
			pack_info*							_current_write;							//��ǰд������
			max_heap<std::deque<pack_info*>>	_write_data;							//д������
			max_heap<pack_info>				_backup_data;							//����д������
			char											_read_buf[io_data_use::read_buf_size];	//��ȡ������
			std::string										_read_data;								//��ȡ����
			io_data_use::recv_func							_recv_func;								//���պ����ص�
			bool											_io_switch[io_type_tnum];				//io����

			// ����
			void encode(pack_info* pack_, const char* data_, slen_t data_len_);
			// ����
			void decode();
			// �������д������
			void clear_backup();
			// ��ȡ������
			uint32_t pack_len(slen_t data_len_);
			// ��ȡ���ܳ���
			uint32_t total_len(slen_t data_len_);
		protected:
		public:
			io_data_buf			io_buf;
			io_error_manage	_error_mag;	//������Ϣ

			io_data(io_data_use::recv_func&& func_);
			~io_data();

			// д������
			void write(const char* data_, slen_t data_len_, uint8_t priority_ = 0);
			// ���õ�ǰ��������
			void update_current_data();
			// ��ȡ��ǰд������
			char* current_data();
			// ��ȡ��ǰд�볤��
			slen_t current_len();
			// ɾ����ǰд������
			void del_current_data();
			// ɾ������д������
			void del_all_write();
			// ���ö�ȡ������
			void reset_read_buf();
			// д�볤��
			void write_len(slen_t len_);
			// �Ƿ����д������
			bool empty();
			// ��ӽ�������
			void read(slen_t data_len_);
			// �ϰ�
			void merge_io_data();
			// ����io����
			void set_io_switch(io_type type_, bool status);
		};

#include "source/io_data.icc"
	}
}