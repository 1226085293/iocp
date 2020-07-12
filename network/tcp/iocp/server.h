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
	// IOCP_TCP服务端
	class server : public server_base, std::enable_shared_from_this<server> {
		// 共享资源临界区
		enum class shared_cri {
			common,
			accept_context,
			clients,
		};
		// 必要内存池
		struct need_memory_pool {
			fixed_memory_pool<server_use::io_context, server_use::context_load>	io_context;
			fixed_memory_pool<server_use::client, server_use::data_load>		client;
		};
		// 事件回调函数
		struct event_process {
			server_use::event_func accept = nullptr;
			server_use::event_func recv = nullptr;
			server_use::event_func disconnect = nullptr;
		};
	private:
	protected:
		CRITICAL_SECTION									_cris[server_use::shared_num];	//临界区
		server_use::client_ptr								_listen;						//监听
		HANDLE												_exit_notice;					//退出事件句柄
		HANDLE												_complete_port;					//完成端口句柄
		uint8_t												_max_work_num;					//最大工作线程数
		std::atomic<uint8_t>								_work_num;						//工作线程数
		time_heap&											_timer;							//定时器
		thread_pool*										_thread_pool;					//线程池
		socket_pool<server_use::socket_load>				_socket_pool;					//socket对象池
		need_memory_pool									_memory_pool;					//必要内存池
		std::deque<server_use::client_ptr>					_accept_context;				//连接上下文列表
		std::unordered_map<SOCKET, server_use::client_ptr>	_clients;						//客户端信息
		LPFN_ACCEPTEX										_acceptex_ptr;					//AcceptEx函数指针
		LPFN_GETACCEPTEXSOCKADDRS							_acceptex_addr_ptr;				//GetAcceptExSockaddrs函数指针
		LPFN_DISCONNECTEX									_disconnectex_ptr;				//DisconnectEx函数指针
		event_process										_event_process;					//事件处理函数集合
		std::function<void(server_use::client*)>			_client_del;					//client销毁函数

		// 初始化socket
		void init_socket(uint16_t port_);
		// 创建工作线程
		void creator_work_thread();
		// 创建客户端
		server_use::client_ptr creator_client(SOCKET sock_, server_use::client_init_type type_ = server_use::client_init_type::null);
		void creator_client(server_use::client_ptr& client_, SOCKET sock_, server_use::client_init_type type_ = server_use::client_init_type::null);
		// 初始化客户端
		void init_client(server_use::client_ptr& client_, server_use::client_init_type type_, ...);
		// 添加客户端数据
		void add_client(server_use::client_ptr& client_);
		// 删除客户端数据
		void del_client(server_use::client_ptr& client_, bool dec_sock);
		// 投递连接请求
		void post_accept(server_use::client_ptr& client_);
		// 连接请求处理
		void accept_process(server_use::client_ptr& client_);

		// 投递写请求
		bool post_write(server_use::client_ptr& client_);
		// 写请求处理
		void write_process(server_use::client_ptr& client_, DWORD byte_);

		// 投递请求读请求
		bool post_request_read(server_use::client_ptr& client_);
		// 投递读请求
		bool post_read(server_use::client_ptr& client_);
		// 读请求处理
		void read_process(server_use::client_ptr& client_, DWORD byte_);

		// 投递disconnect(优雅断开<慎用>, 客户端不执行closesocket函数那么将经历TIME_WAIT)
		void post_disconnect(server_use::client_ptr& client_);
		// disconnect处理
		void disconnect_process(server_use::client_ptr& client_);
		// 断开客户端连接(强制断开<常用>)
		void forced_disconnect(server_use::client_ptr& client_);

		// 错误处理
		bool error_process(server_use::client_ptr& client_, server_use::io_type type_);

		// 单播
		void unicast(server_use::client_ptr& client_, std::string& str_);
	public:
		server();
		server(const server&) = default;
		server& operator =(const server&) = default;
		~server();
		// 启动
		void start(uint16_t port_);
		// 关闭
		void close();
		// 重启
		void restart();
		// 设置事件处理函数
		void set_event_process(server_use::event_type type_, server_use::event_func func_);
		// 单播
		void unicast(SOCKET sock_, std::string& str_);
		void unicast(server_use::event_info* info_, std::string& str_);
		// 组播
		template <uint32_t N>
		void multicast(SOCKET(&sock_)[N], std::string& str_);
		template <uint32_t N>
		void multicast(server_use::event_info(&info_)[N], std::string& str_);
		// 广播
		void broadcast(std::string& str_);
	};

#include "server/source/server.icc"
}