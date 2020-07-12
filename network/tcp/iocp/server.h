#pragma once
#include <cstdint>
#include <WinSock2.h>
#include <MSWSock.h>
#include <unordered_map>
#include "network/tcp/public/server_base.h"
#include "network/tcp/iocp/server/client.h"
#include "pool/fixed_memory_pool.h"
#include "pool/thread_pool.h"
#include "pool/socket_pool.h"
#include "timer/time_heap.h"

namespace network::tcp::iocp {
	// IOCP_TCP�����
	class server : public server_base, std::enable_shared_from_this<server> {
		// ������Դ�ٽ���
		enum class shared_cri {
			common,
			accept_context,
			clients,
		};
		// ��Ҫ�ڴ��
		struct need_memory_pool {
			fixed_memory_pool<server_use::io_context, server_use::context_load>	io_context;
			fixed_memory_pool<server_use::client, server_use::data_load>		client;
		};
		// �¼��ص�����
		struct event_process {
			server_use::event_func accept = nullptr;
			server_use::event_func recv = nullptr;
			server_use::event_func disconnect = nullptr;
		};
	private:
	protected:
		CRITICAL_SECTION									_cris[server_use::shared_num];	//�ٽ���
		server_use::client_ptr								_listen;						//����
		HANDLE												_exit_notice;					//�˳��¼����
		HANDLE												_complete_port;					//��ɶ˿ھ��
		uint8_t												_max_work_num;					//������߳���
		std::atomic<uint8_t>								_work_num;						//�����߳���
		time_heap&											_timer;							//��ʱ��
		thread_pool*										_thread_pool;					//�̳߳�
		socket_pool<server_use::socket_load>				_socket_pool;					//socket�����
		need_memory_pool									_memory_pool;					//��Ҫ�ڴ��
		std::deque<server_use::client_ptr>					_accept_context;				//�����������б�
		std::unordered_map<SOCKET, server_use::client_ptr>	_clients;						//�ͻ�����Ϣ
		LPFN_ACCEPTEX										_acceptex_ptr;					//AcceptEx����ָ��
		LPFN_GETACCEPTEXSOCKADDRS							_acceptex_addr_ptr;				//GetAcceptExSockaddrs����ָ��
		LPFN_DISCONNECTEX									_disconnectex_ptr;				//DisconnectEx����ָ��
		event_process										_event_process;					//�¼�����������
		std::function<void(server_use::client*)>			_client_del;					//client���ٺ���

		// ��ʼ��socket
		void init_socket(uint16_t port_);
		// ���������߳�
		void creator_work_thread();
		// �����ͻ���
		server_use::client_ptr creator_client(SOCKET sock_, server_use::client_init_type type_ = server_use::client_init_type::null);
		void creator_client(server_use::client_ptr& client_, SOCKET sock_, server_use::client_init_type type_ = server_use::client_init_type::null);
		// ��ʼ���ͻ���
		void init_client(server_use::client_ptr& client_, server_use::client_init_type type_, ...);
		// ��ӿͻ�������
		void add_client(server_use::client_ptr& client_);
		// ɾ���ͻ�������
		void del_client(server_use::client_ptr& client_, bool dec_sock);
		// Ͷ����������
		void post_accept(server_use::client_ptr& client_);
		// ����������
		void accept_process(server_use::client_ptr& client_);

		// Ͷ��д����
		bool post_write(server_use::client_ptr& client_);
		// д������
		void write_process(server_use::client_ptr& client_, DWORD byte_);

		// Ͷ�����������
		bool post_request_read(server_use::client_ptr& client_);
		// Ͷ�ݶ�����
		bool post_read(server_use::client_ptr& client_);
		// ��������
		void read_process(server_use::client_ptr& client_, DWORD byte_);

		// Ͷ��disconnect(���ŶϿ�<����>, �ͻ��˲�ִ��closesocket������ô������TIME_WAIT)
		void post_disconnect(server_use::client_ptr& client_);
		// disconnect����
		void disconnect_process(server_use::client_ptr& client_);
		// �Ͽ��ͻ�������(ǿ�ƶϿ�<����>)
		void forced_disconnect(server_use::client_ptr& client_);

		// ������
		bool error_process(server_use::client_ptr& client_, server_use::io_type type_);

		// ����
		void unicast(server_use::client_ptr& client_, std::string& str_);
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
		void unicast(SOCKET sock_, std::string& str_);
		void unicast(server_use::event_info* info_, std::string& str_);
		// �鲥
		template <uint32_t N>
		void multicast(SOCKET(&sock_)[N], std::string& str_);
		template <uint32_t N>
		void multicast(server_use::event_info(&info_)[N], std::string& str_);
		// �㲥
		void broadcast(std::string& str_);
	};

#include "server/source/server.icc"
}