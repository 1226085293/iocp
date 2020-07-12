#pragma once
#include <cstdint>
#include <WinSock2.h>
#include <MSWSock.h>
#include <unordered_map>
#include "network/tcp/public/client_base.h"
#include "network/tcp/iocp/client/server.h"
#include "pool/fixed_memory_pool.h"
#include "pool/thread_pool.h"
#include "pool/socket_pool.h"
#include "timer/time_heap.h"

namespace network::tcp::iocp {
	// IOCP_TCP�����
	class client : public client_base, std::enable_shared_from_this<client> {
		// ������Դ�ٽ���
		enum class shared_cri {
			common,
			accept_context,
			servers,
		};
		// ��Ҫ�ڴ��
		struct need_memory_pool {
			fixed_memory_pool<client_use::io_context, client_use::context_load>	io_context;
			fixed_memory_pool<client_use::server, client_use::data_load>	server;
		};
		// �¼��ص�����
		struct event_process {
			client_use::event_func accept = nullptr;
			client_use::event_func recv = nullptr;
			client_use::event_func disconnect = nullptr;
		};
	private:
		CRITICAL_SECTION									_cris[client_use::shared_num];	//�ٽ���
		client_use::server_ptr								_listen;						//����
		HANDLE												_exit_notice;					//�˳��¼����
		HANDLE												_complete_port;					//��ɶ˿ھ��
		uint8_t												_max_work_num;					//������߳���
		std::atomic<uint8_t>								_work_num;						//�����߳���
		time_heap& _timer;							//��ʱ��
		thread_pool* _thread_pool;					//�̳߳�
		socket_pool<client_use::socket_load>				_socket_pool;					//socket�����
		need_memory_pool									_memory_pool;					//��Ҫ�ڴ��
		std::deque<client_use::server_ptr>					_accept_context;				//�����������б�
		std::unordered_map<SOCKET, client_use::server_ptr>	_servers;						//�ͻ�����Ϣ
		LPFN_ACCEPTEX										_acceptex_ptr;					//AcceptEx����ָ��
		LPFN_GETACCEPTEXSOCKADDRS							_acceptex_addr_ptr;				//GetAcceptExSockaddrs����ָ��
		LPFN_DISCONNECTEX									_disconnectex_ptr;				//DisconnectEx����ָ��
		event_process										_event_process;					//�¼�����������
		std::function<void(client_use::server*)>			_server_del;					//server���ٺ���

		// ��ʼ��socket
		void init_socket(uint16_t port_);
		// ���������߳�
		void creator_work_thread();
		// �����ͻ���
		client_use::server_ptr creator_server(SOCKET sock_, client_use::server_init_type type_ = client_use::server_init_type::null);
		void creator_server(client_use::server_ptr& server_, SOCKET sock_, client_use::server_init_type type_ = client_use::server_init_type::null);
		// ��ʼ���ͻ���
		void init_server(client_use::server_ptr& server_, client_use::server_init_type type_, ...);
		// ��ӿͻ�������
		void add_server(client_use::server_ptr& server_);
		// ɾ���ͻ�������
		void del_server(client_use::server_ptr& server_, bool dec_sock);
		// Ͷ����������
		void post_accept(client_use::server_ptr& server_);
		// ����������
		void accept_process(client_use::server_ptr& server_);

		// Ͷ��д����
		bool post_write(client_use::server_ptr& server_);
		// д������
		void write_process(client_use::server_ptr& server_, DWORD byte_);

		// Ͷ�����������
		bool post_request_read(client_use::server_ptr& server_);
		// Ͷ�ݶ�����
		bool post_read(client_use::server_ptr& server_);
		// ��������
		void read_process(client_use::server_ptr& server_, DWORD byte_);

		// Ͷ��disconnect(���ŶϿ�<����>, �ͻ��˲�ִ��closesocket������ô������TIME_WAIT)
		void post_disconnect(client_use::server_ptr& server_);
		// disconnect����
		void disconnect_process(client_use::server_ptr& server_);
		// �Ͽ��ͻ�������(ǿ�ƶϿ�<����>)
		void forced_disconnect(client_use::server_ptr& server_);

		// ������
		bool error_process(client_use::server_ptr& server_, client_use::io_type type_);

		// ����
		void unicast(client_use::server_ptr& server_, std::string& str_);
	protected:
	public:
		client();
		client(const client&) = default;
		client& operator =(const client&) = default;
		~client();
		// ����
		void start(uint16_t port_);
		// �ر�
		void close();
		// ����
		void restart();
		// �����¼�������
		void set_event_process(client_use::event_type type_, client_use::event_func func_);
		// ����
		void unicast(SOCKET sock_, std::string& str_);
		void unicast(client_use::event_info* info_, std::string& str_);
		// �鲥
		template <uint32_t N>
		void multicast(SOCKET(&sock_)[N], std::string& str_);
		template <uint32_t N>
		void multicast(client_use::event_info(&info_)[N], std::string& str_);
		// �㲥
		void broadcast(std::string& str_);
	};

#include "client/source/client.icc"
}