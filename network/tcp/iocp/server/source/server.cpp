#include <stdio.h>
#include <cassert>
#include <WS2tcpip.h>
#include "../../server.h"
#include "../user.h"
#include "../io_context.h"
#include "raii/critical.h"
#include "tool/time.h"
#include "tool/public.h"
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "ws2_32.lib")

namespace network::tcp::iocp {
	using namespace server_use;
	using namespace io_data_use;

	// server
	server::server() :
		_exit(false),											//退出状态
		_exit_notice(NULL),										//退出通知事件
		_complete_port(NULL),									//完成端口句柄
		_max_work_num(2 * std::thread::hardware_concurrency()), //最大工作线程数(连接io请求数), 建议为: 2 * std::thread::hardware_concurrency()
		_work_num(_max_work_num),								//工作线程数
		_timer(time_heap::instance()),							//定时器
		_thread_pool(&thread_pool::instance()),					//线程池
		_socket_pool(IPPROTO_TCP)								//socket池
	{
		// user销毁函数
		_user_del = [&](user* ptr)->void {
			_memory_pool.user.del_obj(ptr);
		};
	}

	server::~server() {
		_exit = true;
		close();
	}

	void server::init_socket(uint16_t port) {
		try {
			WSADATA wsa;
			memset(&wsa, 0, sizeof(WSADATA));
			assert(WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
			if (LOBYTE(wsa.wVersion) != 2 || HIBYTE(wsa.wVersion) != 2) {
				throw "版本号错误";
			}
			// 构造监听Socket
			creator_user(_listen);
			if (!init_user(user_init_type::monitor, _listen)) {
				throw "初始化监听套接字失败";
			}
			// 绑定监听Socket到完成端口
			if (CreateIoCompletionPort(HANDLE(_listen->socket), _complete_port, NULL, 0) == NULL) {
				_socket_pool.del(_listen->socket);
				throw "绑定监听套接字到完成端口失败";
			}
			// 配置监听信息
			SOCKADDR_IN sockaddr;
			memset(&sockaddr, 0, sizeof(sockaddr));
			sockaddr.sin_family = AF_INET;				//协议簇(使用的网络协议)
			sockaddr.sin_addr.S_un.S_addr = ADDR_ANY;	//(任何地址)
			sockaddr.sin_port = htons(port);			//(端口号)
			// 设置端口复用
			BOOL optval = TRUE;
			if (SOCKET_ERROR == setsockopt(_listen->socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval))) {
				throw "设置端口复用失败";
			}
			// 绑定端口
			if (SOCKET_ERROR == bind(_listen->socket, (SOCKADDR*)&sockaddr, sizeof(SOCKADDR))) {
				throw "绑定端口失败";
			}
			// 开启监听
			if (SOCKET_ERROR == listen(_listen->socket, SOMAXCONN)) {
				throw "开启监听失败";
			}
		}
		catch (const char* err) {
			printf("%s(error: %d)\n", err, WSAGetLastError());
			WSACleanup();
		}
	}

	void server::clear_all() {
		printf("clear_all\n");
		// 清理客户端数据
		for (auto& i : _users) {
			i.second.reset();
		}
		_users.clear();
		for (auto& i : _accept_context) {
			i.reset();
		}
		_accept_context.clear();
		// 释放临界区
		for (uint8_t i = 0; i < server_use::shared_num; DeleteCriticalSection(&_cris[i++]));
		// 释放句柄
		tool::common::close_handle(_exit_notice);
		tool::common::close_handle(_complete_port);
		// 清理监听
		del_user(_listen, false);
	}

	bool server::init_user(user_init_type type_, user_ptr& user_, SOCKET sock_, SOCKADDR_IN* addr_) {
		switch (type_) {
		case user_init_type::monitor: {
			user_->socket = _socket_pool.get();
			return user_->socket != INVALID_SOCKET;
		} break;
		case user_init_type::accept: {
			// 初始化上下文
			user_->context.accept = _memory_pool.io_context.new_obj(user_, &_memory_pool.io_context);
			user_->context.accept->type = io_type::accept;
			return true;
		} break;
		case user_init_type::normal: {
			user_->socket = user_->info.socket = sock_;
			// 更新地址信息
			user_->update_addr_in(addr_);
			// 初始化上下文
			user_->context.send = _memory_pool.io_context.new_obj(user_, &_memory_pool.io_context);
			user_->context.recv = _memory_pool.io_context.new_obj(user_, &_memory_pool.io_context);
			return true;
		} break;
		case user_init_type::disconnect: {
			if (!user_->context.disconnect) {
				user_->context.disconnect = _memory_pool.io_context.new_obj(user_, &_memory_pool.io_context);
				user_->context.disconnect->type = io_type::disconnect;;
			}
			return true;
		} break;
		default: {
		} break;
		}
		return false;
	}

	void server::del_user(user_ptr& user_, bool dec_sock) {
		if (!user_) {
			return;
		}
		if (user_->context.accept) {
			user_->context.accept->~io_context();
		}
		if (user_->context.send) {
			user_->context.send->~io_context();
		}
		if (user_->context.recv) {
			user_->context.recv->~io_context();
		}
		if (user_->context.disconnect) {
			user_->context.disconnect->~io_context();
		}
		auto result = _users.find(user_->socket);
		if (result != _users.end()) {
			_users.erase(result);
		}
		// 回收socket
		if (dec_sock) {
			_socket_pool.rec(user_->socket);
		}
		else {
			_socket_pool.del(user_->socket);
		}
		// 智能指针销毁函数为_user_del, 其内部将执行回收操作
	}

	void server::post_accept(user_ptr& user_) {
		try {
			// 准备参数
			auto context = user_->context.accept;
			DWORD bytes = 0, addr_len = sizeof(SOCKADDR_IN) + 16;
			WSABUF* buf = &user_->io_data.io_buf.read;
			// 设置io数据
			user_->io_data.reset_read_buf();
			// 准备客户端socket
			user_->socket = _socket_pool.get();
			// 非阻塞套接字
			if (user_->socket == INVALID_SOCKET) {
				throw "创建socket失败";
			}
			printf("%s %I64d\n", "accpet socket", user_->socket);
			// 投递AcceptEx请求(接收缓冲区大小为0可以防止拒绝服务攻击占用服务器资源, 正常情况下为 wsabuf->len - (addr_len * 2) ,_acceptex_addr_ptr缓冲区大小应与当前一致)
			if (!_acceptex_ptr(_listen->socket, user_->socket, user_->io_data.io_buf.read.buf, 0, addr_len, addr_len, &bytes, &context->overlapped) && WSAGetLastError() != WSA_IO_PENDING) {
				throw "投递accept请求失败";
			}
		}
		catch (const char* err) {
			printf("%s(error: %d)\n", err, WSAGetLastError());
			auto result = std::find(_accept_context.begin(), _accept_context.end(), user_);
			if (result != _accept_context.end()) {
				forced_disconnect(user_);
				_accept_context.erase(result);
			}
		}
	}

	void server::accept_process(user_ptr& user_) {
		// 准备参数
		auto context = user_->context.accept;
		SOCKADDR_IN* user_addr = nullptr, * local_addr = nullptr;
		int addr_len = sizeof(SOCKADDR_IN);
		char ip[16] = {};
		// 通过GetAcceptExSockAddrs获取客户端/本地SOCKADDR_IN(非零缓冲区大小将获得客户端第一组数据)
		_acceptex_addr_ptr(user_->io_data.io_buf.read.buf, 0, addr_len + 16, addr_len + 16, (LPSOCKADDR*)&local_addr, &addr_len, (LPSOCKADDR*)&user_addr, &addr_len);
		// 构造新的客户端数据
		user_ptr new_user(creator_user());
		init_user(user_init_type::normal, new_user, user_->socket, user_addr);
		// 重新投递accept请求
		post_accept(user_);
		// 绑定客户端socket到完成端口
		if (CreateIoCompletionPort(HANDLE(new_user->socket), _complete_port, NULL, 0) == 0) {
			auto err_id = WSAGetLastError();
			switch (err_id) {
			case ERROR_INVALID_PARAMETER:
			case NO_ERROR: {
			} break;
			default: {
				printf("%s(error: %d)\n", "绑定客户端socket到完成端口失败", err_id);
				del_user(new_user, false);
				return;
			}
			}
		}
		// 添加客户端信息
		add_user(new_user);
		// 投递recv的I/O请求
		post_request_read(new_user);
		// 事件处理
		if (_event_process.accept) {
			_event_process.accept(&new_user->info);
		}
	}

	bool server::post_write(user_ptr& user_) {
		raii::critical r1(&user_->cri);
		// 投递检查
		if (user_->get_mark(io_mark_type::write)) {
			printf("post_write_exit1: sock: %I64d\n", user_->socket);
			return false;
		}
		// 检查待处理事件
		if (user_->io_data.empty()) {
			printf("post_write_exit2: sock: %I64d\n", user_->socket);
			// 是否等待关闭
			if (user_->get_status(user_status::wait_close)) {
				post_disconnect(user_);
			}
			return false;
		}
		// 合包发送
		if (user_->write_wait_ms && !user_->get_status(user_status::write_wait)) {
			if (!user_->get_status(user_status::write_timer)) {
				user_->set_status(user_status::write_timer);
				_timer.add(tool::time::ms_to_s(user_->write_wait_ms), 1, [this](user_ptr& user__) {
					user__->io_data.merge_io_data();
					user__->set_status(user_status::write_wait);
					post_write(user__);
					user__->del_status(user_status::write_wait);
					user__->del_status(user_status::write_timer);
					}
				, user_);
			}
			return true;
		}
		user_->set_mark(io_mark_type::write);
		// 准备参数
		DWORD flags = NULL, bytes = NULL;
		WSABUF* buf = &user_->io_data.io_buf.write;
		// 设置io数据
		auto context = user_->context.send;
		context->type = io_type::write;
		// 设置发送数据
		user_->io_data.update_current_data();
		// 更新缓冲区
		buf->buf = user_->io_data.current_data();
		buf->len = user_->io_data.current_len();
		/*context->wsabuf.buf = user_->io_data.current_data();
		context->wsabuf.len = user_->io_data.current_len();*/
		memset(&context->overlapped, 0, sizeof(OVERLAPPED));
		printf("post_write: sock: %I64d, len: %ld\n", user_->socket, buf->len);
		// 投递请求写(真实大小缓冲区)
		if (SOCKET_ERROR == WSASend(user_->socket, buf, 1, &bytes, flags, &context->overlapped, NULL)) {
			auto err_id = WSAGetLastError();
			switch (err_id)
			{
			case WSA_IO_PENDING:
			case NO_ERROR: {
				return true;
			} break;
			case WSAECONNABORTED:
			case WSAECONNRESET: {	//对端已经关闭连接
				printf("io关闭 sock: %I64d\n", user_->socket);
				user_->io_data.set_io_switch(network::io_type::write, false);
				user_->io_data.del_all_write();
				post_disconnect(user_);
			} break;
			case WSAENOBUFS: {
				printf("sock: %I64d, type: post_write(error: %d)\n", user_->socket, err_id);
			} break;
			default: {
				printf("sock: %I64d, type: post_write(error: %d)\n", user_->socket, err_id);
				post_disconnect(user_);
			} break;
			}
			// 请求失败则删除标记
			user_->del_mark(io_mark_type::write);
			return false;
		}
		return true;
	}

	void server::write_process(user_ptr& user_, DWORD byte_) {
		raii::critical r1(&user_->cri);
		printf("write_process  len: %d  sock: %I64d\n", byte_, user_->socket);
		if (user_->io_data.empty() || byte_ == 0) {
			return;
		}
		// 更新写入数据
		user_->io_data.write_len(static_cast<slen_t>(byte_));
		// 重新投递写入
		user_->del_mark(io_mark_type::write);
		post_write(user_);
	}

	bool server::post_request_read(user_ptr& user_) {
		// 投递检查
		raii::critical r1(&user_->cri);
		if (user_->get_mark(io_mark_type::read)) {
			return false;
		}
		user_->set_mark(io_mark_type::read);
		// 准备参数
		DWORD flags = NULL, bytes = NULL;
		WSABUF* buf = &user_->io_data.io_buf.read;
		// 设置io数据
		auto context = user_->context.recv;
		context->type = io_type::request_read;
		buf->len = 0;
		//context->wsabuf.len = 0;
		memset(&context->overlapped, 0, sizeof(OVERLAPPED));
		printf("post_request_read, sock: %I64d\n", user_->socket);
		// 投递请求读(0字节的缓冲区将在得到socket可读状态时返回)
		if (SOCKET_ERROR == WSARecv(user_->socket, buf, 1, &bytes, &flags, &context->overlapped, NULL)) {
			auto err_id = WSAGetLastError();
			switch (err_id)
			{
			case WSA_IO_PENDING:
			case NO_ERROR: {
				return true;
			} break;
			case WSAECONNABORTED:
			case WSAECONNRESET: {	//对端已经关闭连接
				printf("io关闭 sock: %I64d\n", user_->socket);
				user_->io_data.set_io_switch(network::io_type::write, false);
				user_->io_data.del_all_write();
				post_disconnect(user_);
			} break;
			case WSAENOBUFS: {
				printf("type: post_request_read(error: %d)\n", err_id);
			} break;
			default: {
				printf("type: post_request_read(error: %d)\n", err_id);
				post_disconnect(user_);
			} break;
			}
			// 请求失败则删除标记
			user_->del_mark(io_mark_type::read);
			return false;
		}
		return true;
	}
	
	bool server::post_read(user_ptr& user_) {
		raii::critical r1(&user_->cri);
		// 准备参数
		DWORD flags = NULL, bytes = NULL;
		WSABUF* buf = &user_->io_data.io_buf.read;
		// 设置io数据
		auto context = user_->context.recv;
		context->type = io_type::read;
		user_->io_data.reset_read_buf();
		buf->len = io_data_use::read_buf_size;
		memset(&context->overlapped, 0, sizeof(OVERLAPPED));
		printf("post_read, len: %d, sock: %I64d\n", buf->len, user_->socket);
		// 投递请求读(真实大小缓冲区)
		if (SOCKET_ERROR == WSARecv(user_->socket, buf, 1, &bytes, &flags, &context->overlapped, NULL)) {
			auto err_id = WSAGetLastError();
			switch (err_id)
			{
			case WSA_IO_PENDING:
			case NO_ERROR: {
				return true;
			} break;
			case WSAECONNABORTED:
			case WSAECONNRESET: {	//对端已经关闭连接
				printf("io关闭 sock: %I64d\n", user_->socket);
				user_->io_data.set_io_switch(network::io_type::write, false);
				user_->io_data.del_all_write();
				post_disconnect(user_);
			} break;
			case WSAENOBUFS: {
				printf("type: post_read(error: %d)\n", err_id);
			} break;
			default: {
				printf("type: post_read(error: %d)\n", err_id);
				post_disconnect(user_);
			} break;
			}
			// 请求失败则删除标记
			user_->del_mark(io_mark_type::read);
			return false;
		}
		return true;
	}

	void server::read_process(user_ptr& user_, DWORD byte_) {
		// 重置标记
		user_->del_mark(io_mark_type::read);
		// 事件处理
		user_->io_data.read(static_cast<slen_t>(byte_));
		// 投递recv
		post_request_read(user_);
		forced_disconnect(user_);
	}

	void server::post_disconnect(user_ptr& user_) {
		//printf("enter_disconnectex  sock: %I64d\n", user_->socket);
		// 投递检查
		raii::critical r1(&user_->cri);
		if (user_->get_mark(io_mark_type::disconnect)) {
			return;
		}
		user_->set_mark(io_mark_type::disconnect);
		// 检测未完成io
		if (!user_->io_data.empty()) {
			user_->del_mark(io_mark_type::disconnect);
			user_->set_status(user_status::wait_close);
			user_->io_data.set_io_switch(network::io_type::write, false);
			printf("disconnectex_未完成,存在未发送数据，延迟断开  sock: %I64d\n", user_->socket);
			return;
		}
		// 参数更新
		init_user(user_init_type::disconnect, user_);
		// 准备参数
		DWORD flags = TF_REUSE_SOCKET;
		// 投递disconnectex请求
		if (!_disconnectex_ptr(user_->socket, &user_->context.disconnect->overlapped, flags, NULL)) {
			auto err_id = WSAGetLastError();
			switch (err_id)
			{
			case WSAENOTCONN:
			case WSA_IO_PENDING:
			case NO_ERROR: {
				return;
			} break;
			default: {
				printf("投递disconnectex请求失败(error: %d)\n", err_id);
				forced_disconnect(user_);
			} break;
			}
			return;
		}
		printf("disconnectex  sock: %I64d\n", user_->socket);
	}

	void server::disconnect_process(user_ptr& user_) {
		printf("disconnect_process  sock: %I64d\n", user_->socket);
		if (!user_) {
			return;
		}
		if (_event_process.disconnect) {
			_event_process.disconnect(&user_->info);
		}
		raii::critical r1(&_cris[static_cast<uint32_t>(shared_cri::common)]);;
		// 清除引用计数
		del_user(user_, true);
		// 打印剩余客户端
		std::string remaine;
		char socks[16] = {};
		if (_users.size() < 20) {
			for (auto& i : _users) {
				sprintf_s(socks, "%I64d,", i.second->socket);
				remaine.append(socks);
			}
		}
		printf("close_users num: %zd, 剩余: %s\n", _users.size(), remaine.c_str());
	}

	void server::forced_disconnect(user_ptr& user_) {
		// 投递检查
		raii::critical r1(&user_->cri);
		if (user_->get_mark(io_mark_type::disconnect)) {
			return;
		}
		user_->set_mark(io_mark_type::disconnect);
		// 参数更新
		init_user(user_init_type::disconnect, user_);
		// 取消已投递的IO
		CancelIoEx((HANDLE)user_->socket, &user_->context.disconnect->overlapped);
		// 禁止收/发
		shutdown(user_->socket, SD_BOTH);
		// 删除用户
		del_user(user_, false);
	}

	bool server::error_process(io_context* context_) {
		DWORD err_id = WSAGetLastError();
		auto user = context_->parent;
		printf("error_process sock: %I64d, type: %d, error: %d\n", user->socket, context_->type, err_id);
		switch (err_id) {
		case NO_ERROR:
		case WSA_IO_PENDING: {
			switch (context_->type) {
			case io_type::write: {	//备注: 未出现错误
				return false;
			} break;
			case io_type::read: {	//备注：客户端主动断开
				printf("%s(error: %d)\n", "客户端主动断开", err_id);
				post_disconnect(user);
			} break;
			}
		} break;
		case WAIT_TIMEOUT: {		//备注：客户端异常断开
			if (send(user->socket, "", 0, 0) == SOCKET_ERROR) {
				printf("%s(error: %d)\n", "客户端异常断开", err_id);
				post_disconnect(user);
			}
			else {
				printf("%s(error: %d)\n", "等待的操作过时", err_id);
				return false;
			}
		} break;
		default: {
			printf("%s(error: %d)\n", "其他错误", err_id);
			post_disconnect(user);
		}
		}
		return true;
	}

	void server::start(uint16_t port_) {
		if (_port == 0) {
			_port = port_;
		}
		// 初始化临界区
		for (uint8_t i = 0; i < server_use::shared_num; InitializeCriticalSection(&_cris[i++]));
		// 建立完成端口
		_complete_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
		assert(_complete_port != NULL);
		// 初始化socket
		init_socket(port_);
		try {
			GUID guid_acceptex = WSAID_ACCEPTEX;
			GUID guid_acceptex_addr = WSAID_GETACCEPTEXSOCKADDRS;
			GUID guid_disconnectex_addr = WSAID_DISCONNECTEX;
			DWORD bytes = 0;
			// 获取AcceptEx函数指针
			if (WSAIoctl(_listen->socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid_acceptex, sizeof(GUID), &_acceptex_ptr, sizeof(LPFN_ACCEPTEX), &bytes, NULL, NULL) == SOCKET_ERROR) {
				throw "获取AcceptEx函数指针失败";
			}
			// 获取GetAcceptExSockAddrs函数指针
			if (WSAIoctl(_listen->socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid_acceptex_addr, sizeof(GUID), &_acceptex_addr_ptr, sizeof(LPFN_GETACCEPTEXSOCKADDRS), &bytes, NULL, NULL) == SOCKET_ERROR) {
				throw "获取GetAcceptExSockAddrs函数指针失败";
			}
			if (WSAIoctl(_listen->socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid_disconnectex_addr, sizeof(GUID), &_disconnectex_ptr, sizeof(LPFN_DISCONNECTEX), &bytes, NULL, NULL) == SOCKET_ERROR) {
				throw "获取DisconnectEx函数指针失败";
			}
			_accept_context.reserve(_max_work_num);
			for (int i = 0; i < _max_work_num; ++i) {
				// 创建连接user
				user_ptr user(creator_user());
				init_user(user_init_type::accept, user);
				// 加入连接上下文列表
				_accept_context.push_back(user);
				// 投递accept请求
				post_accept(user);
			}
		}
		catch (const char* err) {
			printf("%s(error: %d)\n", err, WSAGetLastError());
			clear_all();
			return;
		}
		// 建立工作线程
		for (int i = 0; i < _work_num; ++i) {
			_thread_pool->add(std::move([&]() {
				uint32_t thread_id = (*(uint32_t*)&std::this_thread::get_id());
				printf("工作线程(%d)进入\n", thread_id);
				// 准备参数
				BOOL status;								//完成端口状态
				io_context* context;						//投递的上下文
				user_ptr user;								//投递的client
				OVERLAPPED_ENTRY entry[entry_count];		//重叠结构数组
				OVERLAPPED_ENTRY* current_entry = nullptr;	//当前重叠结构
				DWORD re_num;								//接收数量
				bool no_error = true;						//无错误标识
				while (WaitForSingleObject(_exit_notice, 0) != WAIT_OBJECT_0) {
					status = GetQueuedCompletionStatusEx(_complete_port, entry, entry_count, &re_num, INFINITE, FALSE);
					for (DWORD i = 0; i < re_num; ++i) {
						current_entry = entry + i;
						context = CONTAINING_RECORD(current_entry->lpOverlapped, io_context, io_context::overlapped);
						user = context->parent;
						//user_ptr user(context->parent);
						if (user) {
							/*if (!current_entry->dwNumberOfBytesTransferred && context->type != io_type::accept && context->type != io_type::disconnect) {
								printf("sock: %I64d, type: %d, error: %d\n", user->socket, context->type, WSAGetLastError());
							}*/
							if (!status || 
								(!current_entry->dwNumberOfBytesTransferred && (
									context->type == io_type::read ||
									context->type == io_type::write))) {
								no_error = !error_process(context);
							}
							// 错误处理函数若未处理则继续
							if (no_error) {
								try {
									// I/O操作
									switch (context->type) {
									case io_type::accept: {
										accept_process(user);
									} break;
									case io_type::request_read: {
										post_read(user);
									} break;
									case io_type::read: {
										read_process(user, current_entry->dwNumberOfBytesTransferred);
									} break;
									case io_type::write: {
										write_process(user, current_entry->dwNumberOfBytesTransferred);
									} break;
									case io_type::disconnect: {
										disconnect_process(user);
									} break;
									case io_type::null: {
										printf("%s\n", "默认io类型(忽略)");

									} break;
									default: {
										printf("I/O操作类型异常\n");
									}
									}
								}
								catch (std::exception e) {
									printf(e.what());
								}
							}
							else {
								no_error = true;
							}
						}
						user.reset();
					}
				}
				printf("工作线程(%d)结束\n", thread_id);
				if (--_work_num == 0) {
					restart();
				}
				}));
		}
	}

	void server::close() {
		if (_listen && _listen->socket != INVALID_SOCKET) {
			_socket_pool.del(_listen->socket);
			// 通知线程退出
			SetEvent(_exit_notice);
			// 通知完成端口操作退出
			//for (int i = 0; i < _work_num; ++i, QueueUserAPC(_complete_port, (DWORD)NULL, NULL));
			for (int i = 0; i < _work_num; ++i, PostQueuedCompletionStatus(_complete_port, -1, (DWORD)NULL, NULL));
		}
	}

	void server::restart() {
		if (_work_num != 0) {
			close();
			return;
		}
		clear_all();
		if (!_exit) {
			_work_num = _max_work_num;
			start(_port);
		}
	}

	void server::set_event_process(event_type type_, event_func func_) {
		switch (type_) {
		case event_type::accept: {
			_event_process.accept = func_;
		} break;
		case event_type::recv: {
			_event_process.recv = func_;
		} break;
		case event_type::disconnect: {
			_event_process.disconnect = func_;
		} break;
		}
	}

	void server::unicast(SOCKET sock_, const char* data_, uint32_t len_) {
		auto result = _users.find(sock_);
		if (result != _users.end()) {
			unicast(result->second, data_, len_);
		}
	}

	void server::unicast(server_use::user_ptr& user_, const char* data_, uint32_t len_) {
		auto context = user_->context.send;
		// 安全检测
		if (user_->get_status(user_status::wait_close)) {
			return;
		}
		// 存储到写入数据
		user_->io_data.write(data_, len_);
		// 防止锁竞争
		if (user_->get_mark(io_mark_type::write)) {
			//printf("send_to_individual阻止锁竞争: sock: %I64d\n", user->socket);
			return;
		}
		post_write(user_);
	}

	void server::broadcast(const char* data_, uint32_t len_) {
		for (auto& i : _users) {
			unicast(i.second, data_, len_);
		}
	}
}