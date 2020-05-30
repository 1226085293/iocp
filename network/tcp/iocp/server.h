#pragma once
#include <cstdint>
#include <WinSock2.h>
#include <MSWSock.h>
#include <unordered_map>
#include "network/tcp/public/server_base.h"
#include "network/tcp/iocp/server/user.h"
#include "pool/fixed_memory_pool.h"
#include "pool/thread_pool.h"
#include "pool/socket_pool.h"
#include "timer/time_heap.h"

namespace network::tcp::iocp {
	// �����
	class server : public server_base, std::enable_shared_from_this<server> {
		// ������Դ�ٽ���
		enum class shared_cri {
			common,
			accept_context,
			users,
		};
		// ��Ҫ�ڴ��
		struct need_memory_pool {
			fixed_memory_pool<server_use::io_context, server_use::context_load>	io_context;
			fixed_memory_pool<server_use::user, server_use::data_load>			user;
		};
		// �¼��ص�����
		struct event_process {
			server_use::event_func accept = nullptr;
			server_use::event_func recv = nullptr;
			server_use::event_func disconnect = nullptr;
		};
	private:
		CRITICAL_SECTION									_cris[server_use::shared_num];	//�ٽ���
		std::atomic<bool>									_exit;							//����
		char												_status;						//�����״̬
		server_use::user_ptr								_listen;						//����
		uint16_t											_port;							//�˿ں�
		HANDLE												_exit_notice;					//�˳��¼����
		HANDLE												_complete_port;					//��ɶ˿ھ��
		uint8_t												_max_work_num;					//������߳���
		std::atomic<uint8_t>								_work_num;						//�����߳���
		time_heap&											_timer;							//��ʱ��
		thread_pool*										_thread_pool;					//�̳߳�
		socket_pool<server_use::socket_load>				_socket_pool;					//socket�����
		need_memory_pool									_memory_pool;					//��Ҫ�ڴ��
		std::vector<server_use::user_ptr>					_accept_context;				//�����������б�
		std::unordered_map<SOCKET, server_use::user_ptr>	_users;							//�ͻ�����Ϣ
		LPFN_ACCEPTEX										_acceptex_ptr;					//AcceptEx����ָ��
		LPFN_GETACCEPTEXSOCKADDRS							_acceptex_addr_ptr;				//GetAcceptExSockaddrs����ָ��
		LPFN_DISCONNECTEX									_disconnectex_ptr;				//DisconnectEx����ָ��
		event_process										_event_process;					//�¼�����������
		std::function<void(server_use::user*)>				_user_del;						//user���ٺ���

		// ��ʼ��socket
		void init_socket(uint16_t port);
		// ������Դ
		void clear_all();
		// �����ͻ���
		server_use::user_ptr creator_user();
		void creator_user(server_use::user_ptr& user_);
		// ��ʼ���ͻ���
		bool init_user(server_use::user_init_type type_, server_use::user_ptr& user_, SOCKET sock_ = INVALID_SOCKET, SOCKADDR_IN* addr_ = nullptr);
		// ��ӿͻ�������
		void add_user(server_use::user_ptr& user_);
		// ɾ���ͻ�������
		void del_user(server_use::user_ptr& user_, bool dec_sock);
		// Ͷ����������
		void post_accept(server_use::user_ptr& user_);
		// ����������
		void accept_process(server_use::user_ptr& user_);

		// Ͷ��д����
		bool post_write(server_use::user_ptr& user_);
		// д������
		void write_process(server_use::user_ptr& user_, DWORD byte_);

		// Ͷ�����������
		bool post_request_read(server_use::user_ptr& user_);
		// Ͷ�ݶ�����
		bool post_read(server_use::user_ptr& user_);
		// ��������
		void read_process(server_use::user_ptr& user_, DWORD byte_);

		// Ͷ��disconnect(���ŶϿ�<����>, �ͻ��˲�ִ��closesocket������ô������TIME_WAIT)
		void post_disconnect(server_use::user_ptr& user_);
		// disconnect����
		void disconnect_process(server_use::user_ptr& user_);
		// �Ͽ��ͻ�������(ǿ�ƶϿ�<����>)
		void forced_disconnect(server_use::user_ptr& user_);

		// ������
		bool error_process(server_use::io_context* context_);
	protected:
	public:
		server();
		server(const server&) = default;
		server& operator =(const server&) = default;
		~server();
		// ����
		void start(uint16_t port_);
		// �ر�
		void close();
		// ����
		void restart();
		// �����¼�������
		void set_event_process(server_use::event_type type_, server_use::event_func func_);
		// ����
		void unicast(SOCKET sock_, const char* data_, uint32_t len_);
		void unicast(server_use::user_ptr& user_, const char* data_, uint32_t len_);
		// �鲥
		template <uint32_t N>
		void multicast(SOCKET(&sock_)[N], const char* data_, uint32_t len_);
		template <uint32_t N>
		void multicast(server_use::user_ptr(&users_)[N], const char* data_, uint32_t len_);
		// �㲥
		void broadcast(const char* data_, uint32_t len_);
	};

#include "server/source/server.icc"
}