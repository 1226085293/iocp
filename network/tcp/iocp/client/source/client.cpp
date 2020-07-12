#include <stdio.h>
#include <cassert>
#include <WS2tcpip.h>
#include "../../client.h"
#include "../server.h"
#include "../io_context.h"
#include "other/raii/critical.h"
#include "tool/time.h"
#include "tool/system.h"
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "ws2_32.lib")

namespace network::tcp::iocp {
	using namespace client_use;
	using namespace io_data_use;

	// client
	client::client() :
		_exit_notice(CreateEvent(NULL, TRUE, FALSE, NULL)),		//�˳�֪ͨ�¼�
		_complete_port(NULL),									//��ɶ˿ھ��
		_max_work_num(2 * std::thread::hardware_concurrency()), //������߳���(����io������), ����Ϊ: 2 * std::thread::hardware_concurrency()
		_work_num(0),											//�����߳���
		_timer(time_heap::instance()),							//��ʱ��
		_thread_pool(&thread_pool::instance()),					//�̳߳�
		_socket_pool(IPPROTO_TCP)								//socket��
	{
		// �ͻ������ٺ���
		_server_del = [&](server* ptr)->void {
			_memory_pool.server.del_obj(ptr);
		};
	}

	client::~client() {
		// �رչ����߳�
		close();
		status = client_status::destroy;
		// ����ͻ�������
		for (auto& i : _servers) {
			i.second.reset();
		}
		_servers.clear();
		for (auto& i : _accept_context) {
			i.reset();
		}
		_accept_context.clear();
		// �������
		del_server(_listen, false);
		// �ͷž��
		tool::system::close_handle(_exit_notice);
		tool::system::close_handle(_complete_port);
		// �ͷ��ٽ���
		for (uint8_t i = 0; i < client_use::shared_num; DeleteCriticalSection(&_cris[i++]));
	}

	void client::init_socket(uint16_t port_) {
		try {
			WSADATA wsa;
			memset(&wsa, 0, sizeof(WSADATA));
			assert(WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
			if (LOBYTE(wsa.wVersion) != 2 || HIBYTE(wsa.wVersion) != 2) {
				throw "�汾�Ŵ���";
			}
			// �������Socket
			char sock_option = 0;
			tool::byte::setbit(sock_option, static_cast<uint32_t>(socket_option::port_multiplex));	//�˿ڸ���
			creator_server(_listen, _socket_pool.get(sock_option));
			init_server(_listen, server_init_type::listen, port_);
		}
		catch (const char* err) {
			printf("%s(error: %d)\n", err, WSAGetLastError());
			WSACleanup();
		}
	}

	void client::creator_work_thread() {
		// ���������߳�
		uint8_t thread_num = _max_work_num - _work_num;
		for (int i = 0; i < thread_num; ++i, ++_work_num) {
			_thread_pool->add(std::move([&]() {
				auto thread_id = GetCurrentThreadId();
				printf("�����߳�(%d)����\n", thread_id);
				// ׼������
				bool is_restart = false;					//�Ƿ������߳�
				BOOL io_status;								//��ɶ˿�״̬
				io_context* context;						//Ͷ�ݵ�������
				server_ptr current_server;						//Ͷ�ݵ�server
				OVERLAPPED_ENTRY entry[entry_count];		//�ص��ṹ����
				OVERLAPPED_ENTRY* current_entry = nullptr;	//��ǰ�ص��ṹ
				DWORD re_num;								//��������
				bool no_error = true;						//�޴����ʶ
				while (WaitForSingleObject(_exit_notice, 0) != WAIT_OBJECT_0) {
					try {
						io_status = GetQueuedCompletionStatusEx(_complete_port, entry, entry_count, &re_num, INFINITE, FALSE);
						if (status == client_status::destroy) {
							throw run_error::client_close;
						}
						for (DWORD i = 0; i < re_num; ++i) {
							current_entry = entry + i;
							printf("get\n");
							// �߳��˳�
							if (!current_entry->lpOverlapped) {
								break;
							}
							try {
								context = CONTAINING_RECORD(current_entry->lpOverlapped, io_context, io_context::overlapped);
								current_server = context->parent;
							}
							catch (...) {
								try {
									auto client = shared_from_this();
								}
								catch (...) {
									throw run_error::client_close;
								}
								throw run_error::obj_invalid;
							}
							if (current_server) {
								if (!io_status ||
									(!current_entry->dwNumberOfBytesTransferred && (
										context->type == io_type::read ||
										context->type == io_type::write))) {
									no_error = !error_process(current_server, context->type);
								}
								// ����������δ���������
								if (no_error) {
									// I/O����
									switch (context->type) {
									case io_type::accept: {
										accept_process(current_server);
									} break;
									case io_type::request_read: {
										post_read(current_server);
									} break;
									case io_type::read: {
										read_process(current_server, current_entry->dwNumberOfBytesTransferred);
									} break;
									case io_type::write: {
										write_process(current_server, current_entry->dwNumberOfBytesTransferred);
									} break;
									case io_type::disconnect: {
										disconnect_process(current_server);
									} break;
									case io_type::null: {
										printf("%s\n", "Ĭ��io����(����)");

									} break;
									default: {
										printf("I/O���������쳣\n");
									}
									}
								}
								else {
									no_error = true;
								}
							}
							current_server.reset();
						}
					}
					catch (run_error type_) {
						// ������
						if (type_ == run_error::client_close) {
							break;
						}
						switch (type_) {
						case run_error::obj_invalid: {
							// �����߳�
							printf("�����߳�(%d)\n", thread_id);
							is_restart = true;
						} break;
						default: {
						} break;
						}
					}
					catch (std::exception e) {
						// ��������ֱ���˳��߳�
						printf(e.what());
						FatalAppExit(-1, "�������3!");
						break;
					}
				}
				printf("�����߳�(%d)�˳�\n", thread_id);
				--_work_num;
				if (is_restart) {
					creator_work_thread();
				}
				// �������˳�ֱ������
				if (_work_num == 0 && status == client_status::run) {
					restart();
				}
				})
			);
		}
	}
	void client::init_server(server_ptr& server_, server_init_type type_, ...) {
		switch (type_) {
		case server_init_type::listen: {
			uint16_t port_;
			try {
				// ��ȡ�����β���
				va_list ap;
				va_start(ap, type_);
				port_ = va_arg(ap, uint16_t);
				va_end(ap);
			}
			catch (...) {
				printf("server_init_type::listen(error: �����δ���)\n");
			}
			try {
				if (_listen->socket == INVALID_SOCKET) {
					throw "��ʼ�������׽���ʧ��";
				}
				// �󶨼���Socket����ɶ˿�
				if (CreateIoCompletionPort(HANDLE(_listen->socket), _complete_port, NULL, 0) == NULL) {
					_socket_pool.del(_listen->socket);
					throw "�󶨼����׽��ֵ���ɶ˿�ʧ��";
				}
				// ���ü�����Ϣ
				SOCKADDR_IN sockaddr;
				memset(&sockaddr, 0, sizeof(sockaddr));
				sockaddr.sin_family = AF_INET;				//Э���(ʹ�õ�����Э��)
				sockaddr.sin_addr.S_un.S_addr = ADDR_ANY;	//(�κε�ַ)
				sockaddr.sin_port = htons(port_);			//(�˿ں�)
				// �󶨶˿�
				if (SOCKET_ERROR == bind(_listen->socket, (SOCKADDR*)&sockaddr, sizeof(SOCKADDR))) {
					throw "�󶨶˿�ʧ��";
				}
				// ��������
				if (SOCKET_ERROR == listen(_listen->socket, SOMAXCONN)) {
					throw "��������ʧ��";
				}
			}
			catch (const char* err) {
				printf("%s(error: %d)\n", err, WSAGetLastError());
				WSACleanup();
			}
		} break;
		case server_init_type::connect: {
			// ���������������б�
			_accept_context.push_back(server_);
			// ����ͨ������������
			server_->context.common.type = io_type::accept;
		} break;
		case server_init_type::accept: {
			// ׼������
			SOCKADDR_IN* addr_in, * local_addr = nullptr;
			int addr_len = sizeof(SOCKADDR_IN);
			char ip[16] = {};
			// ���¿ͻ��˵�ַ��Ϣ
			_acceptex_addr_ptr(server_->io_data.io_buf.read.buf, 0, addr_len + 16, addr_len + 16, (LPSOCKADDR*)&local_addr, &addr_len, (LPSOCKADDR*)&addr_in, &addr_len);	//ͨ��GetAcceptExSockAddrs��ȡ�ͻ���/����SOCKADDR_IN(���㻺������С����ÿͻ��˵�һ������)
			memcpy_s(&server_->addr_in, sizeof(SOCKADDR_IN), addr_in, sizeof(SOCKADDR_IN));
			inet_ntop(AF_INET, &server_->addr_in.sin_addr, server_->info.ip, sizeof(server_->info.ip));
			server_->info.port = ntohs(server_->addr_in.sin_port);
			server_->info.socket = server_->socket;
			// ����ͨ������������
			server_->context.common.type = io_type::disconnect;
			// �󶨿ͻ���socket����ɶ˿�
			if (CreateIoCompletionPort(HANDLE(server_->socket), _complete_port, NULL, 0) == 0) {
				auto err_id = WSAGetLastError();
				switch (err_id) {
				case ERROR_INVALID_PARAMETER:
				case NO_ERROR: {
				} break;
				default: {
					printf("%s(error: %d)\n", "�󶨿ͻ���socket����ɶ˿�ʧ��", err_id);
					del_server(server_, false);
					return;
				}
				}
			}
		} break;
		default: {
		} break;
		}
	}

	void client::del_server(server_ptr& server_, bool dec_sock) {
		if (!server_) {
			return;
		}
		auto result = _servers.find(server_->socket);
		if (result != _servers.end()) {
			raii::critical r1(&_cris[static_cast<uint32_t>(shared_cri::servers)]);
			server_->context.common.parent.reset();
			server_->context.recv.parent.reset();
			server_->context.send.parent.reset();
			_servers.erase(result);
		}
		printf("��ɾ���ͻ��� %I64d\n", server_->socket);
		// ����socket
		if (dec_sock) {
			_socket_pool.rec(server_->socket);
		}
		else {
			_socket_pool.del(server_->socket);
		}
		// ����ָ�����ٺ���Ϊ_server_del, ���ڲ���ִ�л��ղ���
		// ��ӡʣ��ͻ���
		/*raii::critical r1(&_cris[static_cast<uint32_t>(shared_cri::servers)]);
		std::string remaine;
		char socks[16] = {};
		if (_servers.size() < 20) {
			for (auto& i : _servers) {
				sprintf_s(socks, "%I64d,", i.second->socket);
				remaine.append(socks);
			}
		}
		printf("close_servers num: %zd, ʣ��: %s\n", _servers.size(), remaine.c_str());*/
	}

	void client::post_accept(server_ptr& server_) {
		try {
			// ׼������
			auto& context = server_->context.common;
			DWORD bytes = 0, addr_len = sizeof(SOCKADDR_IN) + 16;
			WSABUF* buf = &server_->io_data.io_buf.read;
			// ����io����
			server_->io_data.reset_read_buf();
			// �������׽���
			if (server_->socket == INVALID_SOCKET) {
				throw "����socketʧ��";
			}
			printf("%s %I64d\n", "accpet socket", server_->socket);
			// Ͷ��AcceptEx����(���ջ�������СΪ0���Է�ֹ�ܾ����񹥻�ռ�÷�������Դ, ���������Ϊ wsabuf->len - (addr_len * 2) ,_acceptex_addr_ptr��������СӦ�뵱ǰһ��)
			if (!_acceptex_ptr(_listen->socket, server_->socket, server_->io_data.io_buf.read.buf, 0, addr_len, addr_len, &bytes, &context.overlapped) && WSAGetLastError() != WSA_IO_PENDING) {
				throw "Ͷ��accept����ʧ��";
			}
		}
		catch (const char* err) {
			printf("%s(error: %d)\n", err, WSAGetLastError());
			auto result = std::find(_accept_context.begin(), _accept_context.end(), server_);
			if (result != _accept_context.end()) {
				forced_disconnect(server_);
				_accept_context.erase(result);
			}
		}
	}

	void client::accept_process(server_ptr& server_) {
		// �����µĿͻ�������
		server_ptr new_server(creator_server(server_->socket, server_init_type::accept));
		// ��ӿͻ�����Ϣ
		add_server(new_server);
		// ����Ͷ��accept����
		server_->socket = _socket_pool.get();
		post_accept(server_);
		// Ͷ��recv��I/O����
		post_request_read(new_server);
		// �¼�����
		if (_event_process.accept) {
			_event_process.accept(&new_server->info);
		}
	}

	bool client::post_write(server_ptr& server_) {
		raii::critical r1(&server_->cri);
		// Ͷ�ݼ��
		if (server_->get_mark(io_mark_type::write)) {
			printf("post_write_exit1: sock: %I64d\n", server_->socket);
			return false;
		}
		// ���������¼�
		if (server_->io_data.empty()) {
			printf("post_write_exit2: sock: %I64d\n", server_->socket);
			// �Ƿ�ȴ��ر�
			if (server_->get_status(server_status::wait_close)) {
				post_disconnect(server_);
			}
			return false;
		}
		// �ϰ�����
		if (server_->write_wait_ms && !server_->get_status(server_status::write_wait)) {
			if (!server_->get_status(server_status::write_timer)) {
				server_->set_status(server_status::write_timer);
				_timer.add(tool::time::ms_to_s(server_->write_wait_ms), 1, [this](server_ptr& server__) {
					server__->io_data.merge_io_data();
					server__->set_status(server_status::write_wait);
					post_write(server__);
					server__->del_status(server_status::write_wait);
					server__->del_status(server_status::write_timer);
					}
				, server_);
			}
			return true;
		}
		server_->set_mark(io_mark_type::write);
		// ׼������
		DWORD flags = NULL, bytes = NULL;
		WSABUF* buf = &server_->io_data.io_buf.write;
		// ����io����
		auto& context = server_->context.send;
		//context.type = io_type::write;
		// ���÷�������
		server_->io_data.update_current_data();
		// ���»�����
		buf->buf = server_->io_data.current_data();
		buf->len = server_->io_data.current_len();
		/*context.wsabuf.buf = server_->io_data.current_data();
		context.wsabuf.len = server_->io_data.current_len();*/
		memset(&context.overlapped, 0, sizeof(OVERLAPPED));
		printf("post_write: sock: %I64d, len: %ld\n", server_->socket, buf->len);
		// Ͷ������д(��ʵ��С������)
		if (SOCKET_ERROR == WSASend(server_->socket, buf, 1, &bytes, flags, &context.overlapped, NULL)) {
			auto err_id = WSAGetLastError();
			switch (err_id)
			{
			case WSA_IO_PENDING:
			case NO_ERROR: {
				return true;
			} break;
			case WSAECONNABORTED:
			case WSAECONNRESET: {	//�Զ��Ѿ��ر�����
				printf("io�ر� sock: %I64d\n", server_->socket);
				server_->io_data.set_io_switch(network::io_type::write, false);
				server_->io_data.del_all_write();
				post_disconnect(server_);
			} break;
			case WSAENOBUFS: {
				printf("sock: %I64d, type: post_write(error: %d)\n", server_->socket, err_id);
			} break;
			default: {
				printf("sock: %I64d, type: post_write(error: %d)\n", server_->socket, err_id);
				post_disconnect(server_);
			} break;
			}
			// ����ʧ����ɾ�����
			server_->del_mark(io_mark_type::write);
			return false;
		}
		return true;
	}

	void client::write_process(server_ptr& server_, DWORD byte_) {
		raii::critical r1(&server_->cri);
		printf("write_process  len: %d  sock: %I64d\n", byte_, server_->socket);
		if (server_->io_data.empty() || byte_ == 0) {
			return;
		}
		// ����д������
		server_->io_data.write_len(static_cast<slen_t>(byte_));
		// ����Ͷ��д��
		server_->del_mark(io_mark_type::write);
		post_write(server_);
	}

	bool client::post_request_read(server_ptr& server_) {
		// Ͷ�ݼ��
		raii::critical r1(&server_->cri);
		if (server_->get_mark(io_mark_type::read)) {
			return false;
		}
		server_->set_mark(io_mark_type::read);
		// ׼������
		DWORD flags = NULL, bytes = NULL;
		WSABUF* buf = &server_->io_data.io_buf.read;
		// ����io����
		auto& context = server_->context.recv;
		context.type = io_type::request_read;
		buf->len = 0;
		//context.wsabuf.len = 0;
		memset(&context.overlapped, 0, sizeof(OVERLAPPED));
		printf("post_request_read, sock: %I64d\n", server_->socket);
		// Ͷ�������(0�ֽڵĻ��������ڵõ�socket�ɶ�״̬ʱ����)
		if (SOCKET_ERROR == WSARecv(server_->socket, buf, 1, &bytes, &flags, &context.overlapped, NULL)) {
			auto err_id = WSAGetLastError();
			switch (err_id)
			{
			case WSA_IO_PENDING:
			case NO_ERROR: {
				return true;
			} break;
			case WSAECONNABORTED:
			case WSAECONNRESET: {	//�Զ��Ѿ��ر�����
				printf("io�ر� sock: %I64d\n", server_->socket);
				server_->io_data.set_io_switch(network::io_type::write, false);
				server_->io_data.del_all_write();
				post_disconnect(server_);
			} break;
			case WSAENOBUFS: {
				printf("type: post_request_read(error: %d)\n", err_id);
			} break;
			default: {
				printf("type: post_request_read(error: %d)\n", err_id);
				post_disconnect(server_);
			} break;
			}
			// ����ʧ����ɾ�����
			server_->del_mark(io_mark_type::read);
			return false;
		}
		return true;
	}

	bool client::post_read(server_ptr& server_) {
		raii::critical r1(&server_->cri);
		// ׼������
		DWORD flags = NULL, bytes = NULL;
		WSABUF* buf = &server_->io_data.io_buf.read;
		// ����io����
		auto& context = server_->context.recv;
		context.type = io_type::read;
		server_->io_data.reset_read_buf();
		buf->len = io_data_use::read_buf_size;
		memset(&context.overlapped, 0, sizeof(OVERLAPPED));
		printf("post_read, len: %d, sock: %I64d\n", buf->len, server_->socket);
		// Ͷ�������(��ʵ��С������)
		if (SOCKET_ERROR == WSARecv(server_->socket, buf, 1, &bytes, &flags, &context.overlapped, NULL)) {
			auto err_id = WSAGetLastError();
			switch (err_id)
			{
			case WSA_IO_PENDING:
			case NO_ERROR: {
				return true;
			} break;
			case WSAECONNABORTED:
			case WSAECONNRESET: {	//�Զ��Ѿ��ر�����
				printf("io�ر� sock: %I64d\n", server_->socket);
				server_->io_data.set_io_switch(network::io_type::write, false);
				server_->io_data.del_all_write();
				post_disconnect(server_);
			} break;
			case WSAENOBUFS: {
				printf("type: post_read(error: %d)\n", err_id);
			} break;
			default: {
				printf("type: post_read(error: %d)\n", err_id);
				post_disconnect(server_);
			} break;
			}
			// ����ʧ����ɾ�����
			server_->del_mark(io_mark_type::read);
			return false;
		}
		return true;
	}

	void client::read_process(server_ptr& server_, DWORD byte_) {
		// ���ñ��
		server_->del_mark(io_mark_type::read);
		// �¼�����
		server_->io_data.read(static_cast<slen_t>(byte_));
		// Ͷ��recv
		post_request_read(server_);
		this->forced_disconnect(server_);
	}

	void client::post_disconnect(server_ptr& server_) {
		//printf("enter_disconnectex  sock: %I64d\n", server_->socket);
		// Ͷ�ݼ��
		{
			raii::critical r1(&server_->cri);
			if (server_->get_mark(io_mark_type::disconnect)) {
				return;
			}
			server_->set_mark(io_mark_type::disconnect);
			// ���δ���io
			if (!server_->io_data.empty()) {
				server_->del_mark(io_mark_type::disconnect);
				server_->set_status(server_status::wait_close);
				server_->io_data.set_io_switch(network::io_type::write, false);
				printf("disconnectex_δ���,����δ�������ݣ��ӳٶϿ�  sock: %I64d\n", server_->socket);
				return;
			}
		}
		// ׼������
		DWORD flags = TF_REUSE_SOCKET;
		// Ͷ��disconnectex����
		if (!_disconnectex_ptr(server_->socket, &server_->context.common.overlapped, flags, NULL)) {
			auto err_id = WSAGetLastError();
			switch (err_id)
			{
			case WSAENOTCONN:
			case WSA_IO_PENDING:
			case NO_ERROR: {
				return;
			} break;
			default: {
				printf("Ͷ��disconnectex����ʧ��(error: %d)\n", err_id);
				forced_disconnect(server_);
			} break;
			}
			return;
		}
		printf("disconnectex  sock: %I64d\n", server_->socket);
	}

	void client::disconnect_process(server_ptr& server_) {
		printf("disconnect_process  sock: %I64d\n", server_->socket);
		if (!server_) {
			return;
		}
		if (_event_process.disconnect) {
			_event_process.disconnect(&server_->info);
		}
		// ������ü���
		del_server(server_, true);
	}

	void client::forced_disconnect(server_ptr& server_) {
		{
			// Ͷ�ݼ��
			raii::critical r1(&server_->cri);
			if (server_->get_mark(io_mark_type::disconnect)) {
				return;
			}
			server_->set_mark(io_mark_type::disconnect);
			// ȡ����Ͷ�ݵ�IO
			CancelIoEx((HANDLE)server_->socket, &server_->context.common.overlapped);
			// ��ֹ��/��
			shutdown(server_->socket, SD_BOTH);
		}
		// ɾ���û�
		del_server(server_, false);
	}

	bool client::error_process(client_use::server_ptr& server_, io_type type_) {
		DWORD err_id = WSAGetLastError();
		printf("error_process sock: %I64d, type: %d, error: %d\n", server_->socket, type_, err_id);
		switch (err_id) {
		case NO_ERROR:
		case WSA_IO_PENDING: {
			switch (type_) {
			case io_type::write: {	//��ע: δ���ִ���
				return false;
			} break;
			case io_type::read: {	//��ע���ͻ��������Ͽ�
				printf("%s(error: %d)\n", "�ͻ��������Ͽ�", err_id);
				post_disconnect(server_);
			} break;
			}
		} break;
		case WAIT_TIMEOUT: {		//��ע���ͻ����쳣�Ͽ�
			if (send(server_->socket, "", 0, 0) == SOCKET_ERROR) {
				printf("%s(error: %d)\n", "�ͻ����쳣�Ͽ�", err_id);
				post_disconnect(server_);
			}
			else {
				printf("%s(error: %d)\n", "�ȴ��Ĳ�����ʱ", err_id);
				return false;
			}
		} break;
		default: {
			printf("%s(error: %d)\n", "��������", err_id);
			post_disconnect(server_);
		}
		}
		return true;
	}

	void client::start(uint16_t port_) {
		if (status != client_status::null) {
			return;
		}
		// ��ʼ���ٽ���
		for (uint8_t i = 0; i < client_use::shared_num; InitializeCriticalSection(&_cris[i++]));
		// ������ɶ˿�
		_complete_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
		assert(_complete_port != NULL);
		// ��ʼ��socket
		init_socket(port_);
		try {
			GUID guid_acceptex = WSAID_ACCEPTEX;
			GUID guid_acceptex_addr = WSAID_GETACCEPTEXSOCKADDRS;
			GUID guid_disconnectex_addr = WSAID_DISCONNECTEX;
			DWORD bytes = 0;
			// ��ȡAcceptEx����ָ��
			if (WSAIoctl(_listen->socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid_acceptex, sizeof(GUID), &_acceptex_ptr, sizeof(LPFN_ACCEPTEX), &bytes, NULL, NULL) == SOCKET_ERROR) {
				throw "��ȡAcceptEx����ָ��ʧ��";
			}
			// ��ȡGetAcceptExSockAddrs����ָ��
			if (WSAIoctl(_listen->socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid_acceptex_addr, sizeof(GUID), &_acceptex_addr_ptr, sizeof(LPFN_GETACCEPTEXSOCKADDRS), &bytes, NULL, NULL) == SOCKET_ERROR) {
				throw "��ȡGetAcceptExSockAddrs����ָ��ʧ��";
			}
			if (WSAIoctl(_listen->socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid_disconnectex_addr, sizeof(GUID), &_disconnectex_ptr, sizeof(LPFN_DISCONNECTEX), &bytes, NULL, NULL) == SOCKET_ERROR) {
				throw "��ȡDisconnectEx����ָ��ʧ��";
			}
			for (int i = 0; i < _max_work_num; ++i) {
				// ��������server
				server_ptr server(creator_server(_socket_pool.get(), server_init_type::connect));
				post_accept(server);
			}
		}
		catch (const char* err) {
			printf("%s(error: %d)\n", err, WSAGetLastError());
			close();
			return;
		}
		creator_work_thread();
		status = client_status::run;
	}

	void client::close() {
		{
			raii::critical r1(&_cris[static_cast<uint32_t>(shared_cri::common)]);
			if (status != client_status::run) {
				return;
			}
			status = client_status::close;
		}
		// ֪ͨ�˳�����
		SetEvent(_exit_notice);
		//for (int i = 0; i < _work_num; ++i, QueueUserAPC(_complete_port, (DWORD)NULL, NULL));
		for (uint8_t i = 0; i < _work_num; ++i, PostQueuedCompletionStatus(_complete_port, -1, (DWORD)NULL, NULL));
	}

	void client::restart() {
		printf("���������\n");
		close();
		// ��֤�����߳��˳�
		while (_work_num) {}
		ResetEvent(_exit_notice);
		creator_work_thread();
	}

	void client::set_event_process(event_type type_, event_func func_) {
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

	void client::unicast(client_use::server_ptr& server_, std::string& str_) {
		auto& context = server_->context.send;
		// ��ȫ���
		if (server_->get_status(server_status::wait_close)) {
			return;
		}
		// �洢��д������
		server_->io_data.write(str_);
		// ��ֹ������
		if (server_->get_mark(io_mark_type::write)) {
			//printf("send_to_individual��ֹ������: sock: %I64d\n", server->socket);
			return;
		}
		post_write(server_);
	}

	void client::broadcast(std::string& str_) {
		raii::critical r1(&_cris[static_cast<uint32_t>(shared_cri::servers)]);
		for (auto& i : _servers) {
			unicast(i.second, str_);
		}
	}
}