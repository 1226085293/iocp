#include <stdio.h>
#include <cassert>
#include <WS2tcpip.h>
#include "../../server.h"
#include "../client.h"
#include "../io_context.h"
#include "other/raii/critical.h"
#include "tool/time.h"
#include "tool/system.h"
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "ws2_32.lib")

namespace network::tcp::iocp {
	using namespace server_use;
	using namespace io_data_use;

	// server
	server::server() :
		_exit_notice(CreateEvent(NULL, TRUE, FALSE, NULL)),		//退出通知事件
		_complete_port(NULL),									//完成端口句柄
		_max_work_num(2 * std::thread::hardware_concurrency()), //最大工作线程数(连接io请求数), 建议为: 2 * std::thread::hardware_concurrency()
		_work_num(0),											//工作线程数
		_timer(time_heap::instance()),							//定时器
		_thread_pool(&thread_pool::instance()),					//线程池
		_socket_pool(IPPROTO_TCP)								//socket池
	{
		// 客户端销毁函数
		_client_del = [&](client* ptr)->void {
			_memory_pool.client.del_obj(ptr);
		};
	}

	server::~server() {
		// 关闭工作线程
		close();
		status = server_status::destroy;
		// 清理客户端数据
		for (auto& i : _clients) {
			i.second.reset();
		}
		_clients.clear();
		for (auto& i : _accept_context) {
			i.reset();
		}
		_accept_context.clear();
		// 清理监听
		del_client(_listen, false);
		// 释放句柄
		tool::system::close_handle(_exit_notice);
		tool::system::close_handle(_complete_port);
		// 释放临界区
		for (uint8_t i = 0; i < server_use::shared_num; DeleteCriticalSection(&_cris[i++]));
	}

	void server::init_socket(uint16_t port_) {
		try {
			WSADATA wsa;
			memset(&wsa, 0, sizeof(WSADATA));
			if (WSAStartup(MAKEWORD(2, 2), &wsa)) {
				throw "WSAStartup失败";
			}
			if (LOBYTE(wsa.wVersion) != 2 || HIBYTE(wsa.wVersion) != 2) {
				throw "版本号错误";
			}
			// 构造监听Socket
			char sock_option = 0;
			tool::byte::setbit(sock_option, static_cast<uint32_t>(socket_option::port_multiplex));	//端口复用
			creator_client(_listen, _socket_pool.get(sock_option));
			init_client(_listen, client_init_type::listen, port_);
		}
		catch (const char* err) {
			printf("%s(error: %d)\n", err, WSAGetLastError());
			WSACleanup();
		}
	}

	void server::creator_work_thread() {
		// 建立工作线程
		uint8_t thread_num = _max_work_num - _work_num;
		for (int i = 0; i < thread_num; ++i, ++_work_num) {
			_thread_pool->add(std::move([&]() {
				auto thread_id = GetCurrentThreadId();
				printf("工作线程(%d)进入\n", thread_id);
				// 准备参数
				bool is_restart = false;					//是否重启线程
				BOOL io_status;								//完成端口状态
				io_context* context;						//投递的上下文
				client_ptr current_client;					//投递的client
				OVERLAPPED_ENTRY entry[entry_count];		//重叠结构数组
				OVERLAPPED_ENTRY* current_entry = nullptr;	//当前重叠结构
				DWORD re_num;								//接收数量
				bool no_error = true;						//无错误标识
				while (WaitForSingleObject(_exit_notice, 0) != WAIT_OBJECT_0) {
					try {
						io_status = GetQueuedCompletionStatusEx(_complete_port, entry, entry_count, &re_num, INFINITE, FALSE);
						if (status == server_status::destroy) {
							throw run_error::server_close;
						}
						for (DWORD i = 0; i < re_num; ++i) {
							current_entry = entry + i;
							printf("get\n");
							// 线程退出
							if (!current_entry->lpOverlapped) {
								break;
							}
							try {
								context = CONTAINING_RECORD(current_entry->lpOverlapped, io_context, io_context::overlapped);
								current_client = context->parent;
							}
							catch (...) {
								try {
									auto server = shared_from_this();
								}
								catch (...) {
									throw run_error::server_close;
								}
								throw run_error::obj_invalid;
							}
							if (current_client) {
								if (!io_status ||
									(!current_entry->dwNumberOfBytesTransferred && (
										context->type == io_type::read ||
										context->type == io_type::write))) {
									no_error = !error_process(current_client, context->type);
								}
								// 错误处理函数若未处理则继续
								if (no_error) {
									// I/O操作
									switch (context->type) {
									case io_type::accept: {
										accept_process(current_client);
									} break;
									case io_type::request_read: {
										post_read(current_client);
									} break;
									case io_type::read: {
										read_process(current_client, current_entry->dwNumberOfBytesTransferred);
									} break;
									case io_type::write: {
										write_process(current_client, current_entry->dwNumberOfBytesTransferred);
									} break;
									case io_type::disconnect: {
										disconnect_process(current_client);
									} break;
									case io_type::null: {
										printf("%s\n", "默认io类型(忽略)");

									} break;
									default: {
										printf("I/O操作类型异常\n");
									}
									}
								}
								else {
									no_error = true;
								}
							}
							current_client.reset();
						}
					}
					catch (run_error type_) {
						// 错误处理
						if (type_ == run_error::server_close) {
							break;
						}
						switch (type_) {
						case run_error::obj_invalid: {
							// 重启线程
							printf("重启线程(%d)\n", thread_id);
							is_restart = true;
						} break;
						default: {
						} break;
						}
					}
					catch (std::exception e) {
						// 其他错误直接退出线程
						printf(e.what());
						FatalAppExit(-1, "其他错误, 捕获崩溃!");
						break;
					}
				}
				printf("工作线程(%d)退出\n", thread_id);
				--_work_num;
				if (is_restart) {
					creator_work_thread();
				}
				// 非正常退出直接重启
				if (!_work_num && status == server_status::run) {
					restart();
				}
				})
			);
		}
	}

	void server::init_client(client_ptr& client_, client_init_type type_, ...) {
		switch (type_) {
		case client_init_type::listen: {
			uint16_t port_;
			try {
				// 获取不定参参数
				va_list ap;
				va_start(ap, type_);
				port_ = va_arg(ap, uint16_t);
				va_end(ap);
			}
			catch (...) {
				printf("client_init_type::listen(error: 不定参错误)\n");
			}
			try {
				if (_listen->socket == INVALID_SOCKET) {
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
				sockaddr.sin_port = htons(port_);			//(端口号)
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
		} break;
		case client_init_type::connect: {
			// 加入连接上下文列表
			_accept_context.push_back(client_);
			// 更改通用上下文类型
			client_->context.common.type = io_type::accept;
		} break;
		case client_init_type::accept: {
			// 准备参数
			SOCKADDR_IN* addr_in, * local_addr = nullptr;
			int addr_len = sizeof(SOCKADDR_IN);
			char ip[16] = {};
			// 更新客户端地址信息
			_acceptex_addr_ptr(client_->io_data.io_buf.read.buf, 0, addr_len + 16, addr_len + 16, (LPSOCKADDR*)&local_addr, &addr_len, (LPSOCKADDR*)&addr_in, &addr_len);	//通过GetAcceptExSockAddrs获取客户端/本地SOCKADDR_IN(非零缓冲区大小将获得客户端第一组数据)
			memcpy_s(&client_->addr_in, sizeof(SOCKADDR_IN), addr_in, sizeof(SOCKADDR_IN));
			inet_ntop(AF_INET, &client_->addr_in.sin_addr, client_->info.ip, sizeof(client_->info.ip));
			client_->info.port = ntohs(client_->addr_in.sin_port);
			client_->info.socket = client_->socket;
			// 更改通用上下文类型
			client_->context.common.type = io_type::disconnect;
			// 绑定客户端socket到完成端口
			if (CreateIoCompletionPort(HANDLE(client_->socket), _complete_port, NULL, 0) == 0) {
				auto err_id = WSAGetLastError();
				switch (err_id) {
				case ERROR_INVALID_PARAMETER:
				case NO_ERROR: {
				} break;
				default: {
					printf("%s(error: %d)\n", "绑定客户端socket到完成端口失败", err_id);
					del_client(client_, false);
					return;
				}
				}
			}
		} break;
		default: {
		} break;
		}
	}

	void server::del_client(client_ptr& client_, bool dec_sock) {
		if (!client_) {
			return;
		}
		auto result = _clients.find(client_->socket);
		if (result != _clients.end()) {
			raii::critical r1(&_cris[static_cast<uint32_t>(shared_cri::clients)]);
			client_->context.common.parent.reset();
			client_->context.recv.parent.reset();
			client_->context.send.parent.reset();
			_clients.erase(result);
		}
		printf("已删除客户端 %I64d\n", client_->socket);
		// 回收socket
		if (dec_sock) {
			_socket_pool.rec(client_->socket);
		}
		else {
			_socket_pool.del(client_->socket);
		}
		// 智能指针销毁函数为_client_del, 其内部将执行回收操作
		// 打印剩余客户端
		/*raii::critical r1(&_cris[static_cast<uint32_t>(shared_cri::clients)]);
		std::string remaine;
		char socks[16] = {};
		if (_clients.size() < 20) {
			for (auto& i : _clients) {
				sprintf_s(socks, "%I64d,", i.second->socket);
				remaine.append(socks);
			}
		}
		printf("close_clients num: %zd, 剩余: %s\n", _clients.size(), remaine.c_str());*/
	}

	void server::post_accept(client_ptr& client_) {
		try {
			// 准备参数
			auto& context = client_->context.common;
			DWORD bytes = 0, addr_len = sizeof(SOCKADDR_IN) + 16;
			WSABUF* buf = &client_->io_data.io_buf.read;
			// 设置io数据
			client_->io_data.reset_read_buf();
			if (client_->socket == INVALID_SOCKET) {
				throw "创建socket失败";
			}
			printf("%s %I64d\n", "accpet socket", client_->socket);
			// 投递AcceptEx请求(接收缓冲区大小为0可以防止拒绝服务攻击占用服务器资源, 正常情况下为 wsabuf->len - (addr_len * 2) ,_acceptex_addr_ptr缓冲区大小应与当前一致)
			if (!_acceptex_ptr(_listen->socket, client_->socket, client_->io_data.io_buf.read.buf, 0, addr_len, addr_len, &bytes, &context.overlapped) && WSAGetLastError() != WSA_IO_PENDING) {
				throw "投递accept请求失败";
			}
		}
		catch (const char* err) {
			printf("%s(error: %d)\n", err, WSAGetLastError());
			auto result = std::find(_accept_context.begin(), _accept_context.end(), client_);
			if (result != _accept_context.end()) {
				forced_disconnect(client_);
				_accept_context.erase(result);
			}
		}
	}

	void server::accept_process(client_ptr& client_) {
		// 构造新的客户端数据
		client_ptr new_client(creator_client(client_->socket, client_init_type::accept));
		// 添加客户端信息
		add_client(new_client);
		// 重新投递accept请求
		client_->socket = _socket_pool.get();
		post_accept(client_);
		// 投递recv的I/O请求
		post_request_read(new_client);
		// 事件处理
		if (_event_process.accept) {
			_event_process.accept(&new_client->info);
		}
	}

	bool server::post_write(client_ptr& client_) {
		raii::critical r1(&client_->cri);
		// 投递检查
		if (client_->get_mark(io_mark_type::write)) {
			printf("post_write_exit1: sock: %I64d\n", client_->socket);
			return false;
		}
		// 检查待处理事件
		if (client_->io_data.empty()) {
			printf("post_write_exit2: sock: %I64d\n", client_->socket);
			// 是否等待关闭
			if (client_->get_status(client_status::wait_close)) {
				post_disconnect(client_);
			}
			return false;
		}
		// 合包发送
		if (client_->write_wait_ms && !client_->get_status(client_status::write_wait)) {
			if (!client_->get_status(client_status::write_timer)) {
				client_->set_status(client_status::write_timer);
				_timer.add(tool::time::ms_to_s(client_->write_wait_ms), 1, [this](client_ptr& client__) {
					client__->io_data.merge_io_data();
					client__->set_status(client_status::write_wait);
					post_write(client__);
					client__->del_status(client_status::write_wait);
					client__->del_status(client_status::write_timer);
					}
				, client_);
			}
			return true;
		}
		client_->set_mark(io_mark_type::write);
		// 准备参数
		DWORD flags = NULL, bytes = NULL;
		WSABUF* buf = &client_->io_data.io_buf.write;
		// 设置io数据
		auto& context = client_->context.send;
		//context.type = io_type::write;
		// 设置发送数据
		client_->io_data.update_current_data();
		// 更新缓冲区
		buf->buf = client_->io_data.current_data();
		buf->len = client_->io_data.current_len();
		/*context.wsabuf.buf = client_->io_data.current_data();
		context.wsabuf.len = client_->io_data.current_len();*/
		memset(&context.overlapped, 0, sizeof(OVERLAPPED));
		printf("post_write: sock: %I64d, len: %ld\n", client_->socket, buf->len);
		// 投递请求写(真实大小缓冲区)
		if (SOCKET_ERROR == WSASend(client_->socket, buf, 1, &bytes, flags, &context.overlapped, NULL)) {
			auto err_id = WSAGetLastError();
			switch (err_id)
			{
			case WSA_IO_PENDING:
			case NO_ERROR: {
				return true;
			} break;
			case WSAECONNABORTED:
			case WSAECONNRESET: {	//对端已经关闭连接
				printf("io关闭 sock: %I64d\n", client_->socket);
				client_->io_data.set_io_switch(network::io_type::write, false);
				client_->io_data.del_all_write();
				post_disconnect(client_);
			} break;
			case WSAENOBUFS: {
				printf("sock: %I64d, type: post_write(error: %d)\n", client_->socket, err_id);
			} break;
			default: {
				printf("sock: %I64d, type: post_write(error: %d)\n", client_->socket, err_id);
				post_disconnect(client_);
			} break;
			}
			// 请求失败则删除标记
			client_->del_mark(io_mark_type::write);
			return false;
		}
		return true;
	}

	void server::write_process(client_ptr& client_, DWORD byte_) {
		raii::critical r1(&client_->cri);
		printf("write_process  len: %d  sock: %I64d\n", byte_, client_->socket);
		if (client_->io_data.empty() || byte_ == 0) {
			return;
		}
		// 更新写入数据
		client_->io_data.write_len(static_cast<slen_t>(byte_));
		// 重新投递写入
		client_->del_mark(io_mark_type::write);
		post_write(client_);
	}

	bool server::post_request_read(client_ptr& client_) {
		// 投递检查
		raii::critical r1(&client_->cri);
		if (client_->get_mark(io_mark_type::read)) {
			return false;
		}
		client_->set_mark(io_mark_type::read);
		// 准备参数
		DWORD flags = NULL, bytes = NULL;
		WSABUF* buf = &client_->io_data.io_buf.read;
		// 设置io数据
		auto& context = client_->context.recv;
		context.type = io_type::request_read;
		buf->len = 0;
		//context.wsabuf.len = 0;
		memset(&context.overlapped, 0, sizeof(OVERLAPPED));
		printf("post_request_read, sock: %I64d\n", client_->socket);
		// 投递请求读(0字节的缓冲区将在得到socket可读状态时返回)
		if (SOCKET_ERROR == WSARecv(client_->socket, buf, 1, &bytes, &flags, &context.overlapped, NULL)) {
			auto err_id = WSAGetLastError();
			switch (err_id)
			{
			case WSA_IO_PENDING:
			case NO_ERROR: {
				return true;
			} break;
			case WSAECONNABORTED:
			case WSAECONNRESET: {	//对端已经关闭连接
				printf("io关闭 sock: %I64d\n", client_->socket);
				client_->io_data.set_io_switch(network::io_type::write, false);
				client_->io_data.del_all_write();
				post_disconnect(client_);
			} break;
			case WSAENOBUFS: {
				printf("type: post_request_read(error: %d)\n", err_id);
			} break;
			default: {
				printf("type: post_request_read(error: %d)\n", err_id);
				post_disconnect(client_);
			} break;
			}
			// 请求失败则删除标记
			client_->del_mark(io_mark_type::read);
			return false;
		}
		return true;
	}
	
	bool server::post_read(client_ptr& client_) {
		raii::critical r1(&client_->cri);
		// 准备参数
		DWORD flags = NULL, bytes = NULL;
		WSABUF* buf = &client_->io_data.io_buf.read;
		// 设置io数据
		auto& context = client_->context.recv;
		context.type = io_type::read;
		client_->io_data.reset_read_buf();
		buf->len = io_data_use::read_buf_size;
		memset(&context.overlapped, 0, sizeof(OVERLAPPED));
		printf("post_read, len: %d, sock: %I64d\n", buf->len, client_->socket);
		// 投递请求读(真实大小缓冲区)
		if (SOCKET_ERROR == WSARecv(client_->socket, buf, 1, &bytes, &flags, &context.overlapped, NULL)) {
			auto err_id = WSAGetLastError();
			switch (err_id)
			{
			case WSA_IO_PENDING:
			case NO_ERROR: {
				return true;
			} break;
			case WSAECONNABORTED:
			case WSAECONNRESET: {	//对端已经关闭连接
				printf("io关闭 sock: %I64d\n", client_->socket);
				client_->io_data.set_io_switch(network::io_type::write, false);
				client_->io_data.del_all_write();
				post_disconnect(client_);
			} break;
			case WSAENOBUFS: {
				printf("type: post_read(error: %d)\n", err_id);
			} break;
			default: {
				printf("type: post_read(error: %d)\n", err_id);
				post_disconnect(client_);
			} break;
			}
			// 请求失败则删除标记
			client_->del_mark(io_mark_type::read);
			return false;
		}
		return true;
	}

	void server::read_process(client_ptr& client_, DWORD byte_) {
		// 重置标记
		client_->del_mark(io_mark_type::read);
		// 事件处理
		client_->io_data.read(static_cast<slen_t>(byte_));
		// 投递recv
		post_request_read(client_);
		this->forced_disconnect(client_);
	}

	void server::post_disconnect(client_ptr& client_) {
		//printf("enter_disconnectex  sock: %I64d\n", client_->socket);
		// 投递检查
		{
			raii::critical r1(&client_->cri);
			if (client_->get_mark(io_mark_type::disconnect)) {
				return;
			}
			client_->set_mark(io_mark_type::disconnect);
			// 检测未完成io
			if (!client_->io_data.empty()) {
				client_->del_mark(io_mark_type::disconnect);
				client_->set_status(client_status::wait_close);
				client_->io_data.set_io_switch(network::io_type::write, false);
				printf("disconnectex_未完成,存在未发送数据，延迟断开  sock: %I64d\n", client_->socket);
				return;
			}
		}
		// 准备参数
		DWORD flags = TF_REUSE_SOCKET;
		// 投递disconnectex请求
		if (!_disconnectex_ptr(client_->socket, &client_->context.common.overlapped, flags, NULL)) {
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
				forced_disconnect(client_);
			} break;
			}
			return;
		}
		printf("disconnectex  sock: %I64d\n", client_->socket);
	}

	void server::disconnect_process(client_ptr& client_) {
		printf("disconnect_process  sock: %I64d\n", client_->socket);
		if (!client_) {
			return;
		}
		if (_event_process.disconnect) {
			_event_process.disconnect(&client_->info);
		}
		// 清除引用计数
		del_client(client_, true);
	}

	void server::forced_disconnect(client_ptr& client_) {
		{
			// 投递检查
			raii::critical r1(&client_->cri);
			if (client_->get_mark(io_mark_type::disconnect)) {
				return;
			}
			client_->set_mark(io_mark_type::disconnect);
			// 取消已投递的IO
			CancelIoEx((HANDLE)client_->socket, &client_->context.common.overlapped);
			// 禁止收/发
			shutdown(client_->socket, SD_BOTH);
		}
		// 删除用户
		del_client(client_, false);
	}

	bool server::error_process(server_use::client_ptr& client_, io_type type_) {
		DWORD err_id = WSAGetLastError();
		printf("error_process sock: %I64d, type: %d, error: %d\n", client_->socket, type_, err_id);
		switch (err_id) {
		case NO_ERROR:
		case WSA_IO_PENDING: {
			switch (type_) {
			case io_type::write: {	//备注: 未出现错误
				return false;
			} break;
			case io_type::read: {	//备注：客户端主动断开
				printf("%s(error: %d)\n", "客户端主动断开", err_id);
				post_disconnect(client_);
			} break;
			}
		} break;
		case WAIT_TIMEOUT: {		//备注：客户端异常断开
			if (send(client_->socket, "", 0, 0) == SOCKET_ERROR) {
				printf("%s(error: %d)\n", "客户端异常断开", err_id);
				post_disconnect(client_);
			}
			else {
				printf("%s(error: %d)\n", "等待的操作过时", err_id);
				return false;
			}
		} break;
		default: {
			printf("%s(error: %d)\n", "其他错误", err_id);
			post_disconnect(client_);
		}
		}
		return true;
	}

	void server::start(uint16_t port_) {
		if (status != server_status::null) {
			return;
		}
		// 初始化临界区
		for (uint8_t i = 0; i < server_use::shared_num; InitializeCriticalSection(&_cris[i++]));
		// 建立完成端口
		_complete_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
		assert(_complete_port != NULL);
		// 初始化socket
		init_socket(port_);
		try {
			if (!_listen) {
				throw "监听套接字初始化失败";
			}
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
			for (int i = 0; i < _max_work_num; ++i) {
				// 创建连接client
				client_ptr client(creator_client(_socket_pool.get(), client_init_type::connect));
				post_accept(client);
			}
		}
		catch (const char* err) {
			printf("%s(error: %d)\n", err, WSAGetLastError());
			close();
			return;
		}
		creator_work_thread();
		status = server_status::run;
	}

	void server::close() {
		{
			raii::critical r1(&_cris[static_cast<uint32_t>(shared_cri::common)]);
			if (status != server_status::run) {
				return;
			}
			status = server_status::close;
		}
		// 通知退出操作
		SetEvent(_exit_notice);
		//for (int i = 0; i < _work_num; ++i, QueueUserAPC(_complete_port, (DWORD)NULL, NULL));
		for (uint8_t i = 0; i < _work_num; ++i, PostQueuedCompletionStatus(_complete_port, -1, (DWORD)NULL, NULL));
	}

	void server::restart() {
		printf("重启服务端\n");
		close();
		// 保证其他线程退出
		while (_work_num) {}
		ResetEvent(_exit_notice);
		creator_work_thread();
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

	void server::unicast(server_use::client_ptr& client_, std::string& str_) {
		auto& context = client_->context.send;
		// 安全检测
		if (client_->get_status(client_status::wait_close)) {
			return;
		}
		// 存储到写入数据
		client_->io_data.write(str_);
		// 防止锁竞争
		if (client_->get_mark(io_mark_type::write)) {
			//printf("send_to_individual阻止锁竞争: sock: %I64d\n", client->socket);
			return;
		}
		post_write(client_);
	}

	void server::broadcast(std::string& str_) {
		raii::critical r1(&_cris[static_cast<uint32_t>(shared_cri::clients)]);
		for (auto& i : _clients) {
			unicast(i.second, str_);
		}
	}
}