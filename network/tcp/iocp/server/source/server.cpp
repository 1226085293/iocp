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
		_exit_notice(CreateEvent(NULL, TRUE, FALSE, NULL)),		//�˳�֪ͨ�¼�
		_complete_port(NULL),									//��ɶ˿ھ��
		_max_work_num(2 * std::thread::hardware_concurrency()), //������߳���(����io������), ����Ϊ: 2 * std::thread::hardware_concurrency()
		_work_num(0),											//�����߳���
		_timer(time_heap::instance()),							//��ʱ��
		_thread_pool(&thread_pool::instance()),					//�̳߳�
		_socket_pool(IPPROTO_TCP)								//socket��
	{
		// �ͻ������ٺ���
		_client_del = [&](client* ptr)->void {
			_memory_pool.client.del_obj(ptr);
		};
	}

	server::~server() {
		// �رչ����߳�
		close();
		status = server_status::destroy;
		// ����ͻ�������
		for (auto& i : _clients) {
			i.second.reset();
		}
		_clients.clear();
		for (auto& i : _accept_context) {
			i.reset();
		}
		_accept_context.clear();
		// �������
		del_client(_listen, false);
		// �ͷž��
		tool::system::close_handle(_exit_notice);
		tool::system::close_handle(_complete_port);
		// �ͷ��ٽ���
		for (uint8_t i = 0; i < server_use::shared_num; DeleteCriticalSection(&_cris[i++]));
	}

	void server::init_socket(uint16_t port_) {
		try {
			WSADATA wsa;
			memset(&wsa, 0, sizeof(WSADATA));
			if (WSAStartup(MAKEWORD(2, 2), &wsa)) {
				throw "WSAStartupʧ��";
			}
			if (LOBYTE(wsa.wVersion) != 2 || HIBYTE(wsa.wVersion) != 2) {
				throw "�汾�Ŵ���";
			}
			// �������Socket
			char sock_option = 0;
			tool::byte::setbit(sock_option, static_cast<uint32_t>(socket_option::port_multiplex));	//�˿ڸ���
			creator_client(_listen, _socket_pool.get(sock_option));
			init_client(_listen, client_init_type::listen, port_);
		}
		catch (const char* err) {
			printf("%s(error: %d)\n", err, WSAGetLastError());
			WSACleanup();
		}
	}

	void server::creator_work_thread() {
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
				client_ptr current_client;					//Ͷ�ݵ�client
				OVERLAPPED_ENTRY entry[entry_count];		//�ص��ṹ����
				OVERLAPPED_ENTRY* current_entry = nullptr;	//��ǰ�ص��ṹ
				DWORD re_num;								//��������
				bool no_error = true;						//�޴����ʶ
				while (WaitForSingleObject(_exit_notice, 0) != WAIT_OBJECT_0) {
					try {
						io_status = GetQueuedCompletionStatusEx(_complete_port, entry, entry_count, &re_num, INFINITE, FALSE);
						if (status == server_status::destroy) {
							throw run_error::server_close;
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
								// ����������δ���������
								if (no_error) {
									// I/O����
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
							current_client.reset();
						}
					}
					catch (run_error type_) {
						// ������
						if (type_ == run_error::server_close) {
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
						FatalAppExit(-1, "��������, �������!");
						break;
					}
				}
				printf("�����߳�(%d)�˳�\n", thread_id);
				--_work_num;
				if (is_restart) {
					creator_work_thread();
				}
				// �������˳�ֱ������
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
				// ��ȡ�����β���
				va_list ap;
				va_start(ap, type_);
				port_ = va_arg(ap, uint16_t);
				va_end(ap);
			}
			catch (...) {
				printf("client_init_type::listen(error: �����δ���)\n");
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
		case client_init_type::connect: {
			// ���������������б�
			_accept_context.push_back(client_);
			// ����ͨ������������
			client_->context.common.type = io_type::accept;
		} break;
		case client_init_type::accept: {
			// ׼������
			SOCKADDR_IN* addr_in, * local_addr = nullptr;
			int addr_len = sizeof(SOCKADDR_IN);
			char ip[16] = {};
			// ���¿ͻ��˵�ַ��Ϣ
			_acceptex_addr_ptr(client_->io_data.io_buf.read.buf, 0, addr_len + 16, addr_len + 16, (LPSOCKADDR*)&local_addr, &addr_len, (LPSOCKADDR*)&addr_in, &addr_len);	//ͨ��GetAcceptExSockAddrs��ȡ�ͻ���/����SOCKADDR_IN(���㻺������С����ÿͻ��˵�һ������)
			memcpy_s(&client_->addr_in, sizeof(SOCKADDR_IN), addr_in, sizeof(SOCKADDR_IN));
			inet_ntop(AF_INET, &client_->addr_in.sin_addr, client_->info.ip, sizeof(client_->info.ip));
			client_->info.port = ntohs(client_->addr_in.sin_port);
			client_->info.socket = client_->socket;
			// ����ͨ������������
			client_->context.common.type = io_type::disconnect;
			// �󶨿ͻ���socket����ɶ˿�
			if (CreateIoCompletionPort(HANDLE(client_->socket), _complete_port, NULL, 0) == 0) {
				auto err_id = WSAGetLastError();
				switch (err_id) {
				case ERROR_INVALID_PARAMETER:
				case NO_ERROR: {
				} break;
				default: {
					printf("%s(error: %d)\n", "�󶨿ͻ���socket����ɶ˿�ʧ��", err_id);
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
		printf("��ɾ���ͻ��� %I64d\n", client_->socket);
		// ����socket
		if (dec_sock) {
			_socket_pool.rec(client_->socket);
		}
		else {
			_socket_pool.del(client_->socket);
		}
		// ����ָ�����ٺ���Ϊ_client_del, ���ڲ���ִ�л��ղ���
		// ��ӡʣ��ͻ���
		/*raii::critical r1(&_cris[static_cast<uint32_t>(shared_cri::clients)]);
		std::string remaine;
		char socks[16] = {};
		if (_clients.size() < 20) {
			for (auto& i : _clients) {
				sprintf_s(socks, "%I64d,", i.second->socket);
				remaine.append(socks);
			}
		}
		printf("close_clients num: %zd, ʣ��: %s\n", _clients.size(), remaine.c_str());*/
	}

	void server::post_accept(client_ptr& client_) {
		try {
			// ׼������
			auto& context = client_->context.common;
			DWORD bytes = 0, addr_len = sizeof(SOCKADDR_IN) + 16;
			WSABUF* buf = &client_->io_data.io_buf.read;
			// ����io����
			client_->io_data.reset_read_buf();
			if (client_->socket == INVALID_SOCKET) {
				throw "����socketʧ��";
			}
			printf("%s %I64d\n", "accpet socket", client_->socket);
			// Ͷ��AcceptEx����(���ջ�������СΪ0���Է�ֹ�ܾ����񹥻�ռ�÷�������Դ, ���������Ϊ wsabuf->len - (addr_len * 2) ,_acceptex_addr_ptr��������СӦ�뵱ǰһ��)
			if (!_acceptex_ptr(_listen->socket, client_->socket, client_->io_data.io_buf.read.buf, 0, addr_len, addr_len, &bytes, &context.overlapped) && WSAGetLastError() != WSA_IO_PENDING) {
				throw "Ͷ��accept����ʧ��";
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
		// �����µĿͻ�������
		client_ptr new_client(creator_client(client_->socket, client_init_type::accept));
		// ��ӿͻ�����Ϣ
		add_client(new_client);
		// ����Ͷ��accept����
		client_->socket = _socket_pool.get();
		post_accept(client_);
		// Ͷ��recv��I/O����
		post_request_read(new_client);
		// �¼�����
		if (_event_process.accept) {
			_event_process.accept(&new_client->info);
		}
	}

	bool server::post_write(client_ptr& client_) {
		raii::critical r1(&client_->cri);
		// Ͷ�ݼ��
		if (client_->get_mark(io_mark_type::write)) {
			printf("post_write_exit1: sock: %I64d\n", client_->socket);
			return false;
		}
		// ���������¼�
		if (client_->io_data.empty()) {
			printf("post_write_exit2: sock: %I64d\n", client_->socket);
			// �Ƿ�ȴ��ر�
			if (client_->get_status(client_status::wait_close)) {
				post_disconnect(client_);
			}
			return false;
		}
		// �ϰ�����
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
		// ׼������
		DWORD flags = NULL, bytes = NULL;
		WSABUF* buf = &client_->io_data.io_buf.write;
		// ����io����
		auto& context = client_->context.send;
		//context.type = io_type::write;
		// ���÷�������
		client_->io_data.update_current_data();
		// ���»�����
		buf->buf = client_->io_data.current_data();
		buf->len = client_->io_data.current_len();
		/*context.wsabuf.buf = client_->io_data.current_data();
		context.wsabuf.len = client_->io_data.current_len();*/
		memset(&context.overlapped, 0, sizeof(OVERLAPPED));
		printf("post_write: sock: %I64d, len: %ld\n", client_->socket, buf->len);
		// Ͷ������д(��ʵ��С������)
		if (SOCKET_ERROR == WSASend(client_->socket, buf, 1, &bytes, flags, &context.overlapped, NULL)) {
			auto err_id = WSAGetLastError();
			switch (err_id)
			{
			case WSA_IO_PENDING:
			case NO_ERROR: {
				return true;
			} break;
			case WSAECONNABORTED:
			case WSAECONNRESET: {	//�Զ��Ѿ��ر�����
				printf("io�ر� sock: %I64d\n", client_->socket);
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
			// ����ʧ����ɾ�����
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
		// ����д������
		client_->io_data.write_len(static_cast<slen_t>(byte_));
		// ����Ͷ��д��
		client_->del_mark(io_mark_type::write);
		post_write(client_);
	}

	bool server::post_request_read(client_ptr& client_) {
		// Ͷ�ݼ��
		raii::critical r1(&client_->cri);
		if (client_->get_mark(io_mark_type::read)) {
			return false;
		}
		client_->set_mark(io_mark_type::read);
		// ׼������
		DWORD flags = NULL, bytes = NULL;
		WSABUF* buf = &client_->io_data.io_buf.read;
		// ����io����
		auto& context = client_->context.recv;
		context.type = io_type::request_read;
		buf->len = 0;
		//context.wsabuf.len = 0;
		memset(&context.overlapped, 0, sizeof(OVERLAPPED));
		printf("post_request_read, sock: %I64d\n", client_->socket);
		// Ͷ�������(0�ֽڵĻ��������ڵõ�socket�ɶ�״̬ʱ����)
		if (SOCKET_ERROR == WSARecv(client_->socket, buf, 1, &bytes, &flags, &context.overlapped, NULL)) {
			auto err_id = WSAGetLastError();
			switch (err_id)
			{
			case WSA_IO_PENDING:
			case NO_ERROR: {
				return true;
			} break;
			case WSAECONNABORTED:
			case WSAECONNRESET: {	//�Զ��Ѿ��ر�����
				printf("io�ر� sock: %I64d\n", client_->socket);
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
			// ����ʧ����ɾ�����
			client_->del_mark(io_mark_type::read);
			return false;
		}
		return true;
	}
	
	bool server::post_read(client_ptr& client_) {
		raii::critical r1(&client_->cri);
		// ׼������
		DWORD flags = NULL, bytes = NULL;
		WSABUF* buf = &client_->io_data.io_buf.read;
		// ����io����
		auto& context = client_->context.recv;
		context.type = io_type::read;
		client_->io_data.reset_read_buf();
		buf->len = io_data_use::read_buf_size;
		memset(&context.overlapped, 0, sizeof(OVERLAPPED));
		printf("post_read, len: %d, sock: %I64d\n", buf->len, client_->socket);
		// Ͷ�������(��ʵ��С������)
		if (SOCKET_ERROR == WSARecv(client_->socket, buf, 1, &bytes, &flags, &context.overlapped, NULL)) {
			auto err_id = WSAGetLastError();
			switch (err_id)
			{
			case WSA_IO_PENDING:
			case NO_ERROR: {
				return true;
			} break;
			case WSAECONNABORTED:
			case WSAECONNRESET: {	//�Զ��Ѿ��ر�����
				printf("io�ر� sock: %I64d\n", client_->socket);
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
			// ����ʧ����ɾ�����
			client_->del_mark(io_mark_type::read);
			return false;
		}
		return true;
	}

	void server::read_process(client_ptr& client_, DWORD byte_) {
		// ���ñ��
		client_->del_mark(io_mark_type::read);
		// �¼�����
		client_->io_data.read(static_cast<slen_t>(byte_));
		// Ͷ��recv
		post_request_read(client_);
		this->forced_disconnect(client_);
	}

	void server::post_disconnect(client_ptr& client_) {
		//printf("enter_disconnectex  sock: %I64d\n", client_->socket);
		// Ͷ�ݼ��
		{
			raii::critical r1(&client_->cri);
			if (client_->get_mark(io_mark_type::disconnect)) {
				return;
			}
			client_->set_mark(io_mark_type::disconnect);
			// ���δ���io
			if (!client_->io_data.empty()) {
				client_->del_mark(io_mark_type::disconnect);
				client_->set_status(client_status::wait_close);
				client_->io_data.set_io_switch(network::io_type::write, false);
				printf("disconnectex_δ���,����δ�������ݣ��ӳٶϿ�  sock: %I64d\n", client_->socket);
				return;
			}
		}
		// ׼������
		DWORD flags = TF_REUSE_SOCKET;
		// Ͷ��disconnectex����
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
				printf("Ͷ��disconnectex����ʧ��(error: %d)\n", err_id);
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
		// ������ü���
		del_client(client_, true);
	}

	void server::forced_disconnect(client_ptr& client_) {
		{
			// Ͷ�ݼ��
			raii::critical r1(&client_->cri);
			if (client_->get_mark(io_mark_type::disconnect)) {
				return;
			}
			client_->set_mark(io_mark_type::disconnect);
			// ȡ����Ͷ�ݵ�IO
			CancelIoEx((HANDLE)client_->socket, &client_->context.common.overlapped);
			// ��ֹ��/��
			shutdown(client_->socket, SD_BOTH);
		}
		// ɾ���û�
		del_client(client_, false);
	}

	bool server::error_process(server_use::client_ptr& client_, io_type type_) {
		DWORD err_id = WSAGetLastError();
		printf("error_process sock: %I64d, type: %d, error: %d\n", client_->socket, type_, err_id);
		switch (err_id) {
		case NO_ERROR:
		case WSA_IO_PENDING: {
			switch (type_) {
			case io_type::write: {	//��ע: δ���ִ���
				return false;
			} break;
			case io_type::read: {	//��ע���ͻ��������Ͽ�
				printf("%s(error: %d)\n", "�ͻ��������Ͽ�", err_id);
				post_disconnect(client_);
			} break;
			}
		} break;
		case WAIT_TIMEOUT: {		//��ע���ͻ����쳣�Ͽ�
			if (send(client_->socket, "", 0, 0) == SOCKET_ERROR) {
				printf("%s(error: %d)\n", "�ͻ����쳣�Ͽ�", err_id);
				post_disconnect(client_);
			}
			else {
				printf("%s(error: %d)\n", "�ȴ��Ĳ�����ʱ", err_id);
				return false;
			}
		} break;
		default: {
			printf("%s(error: %d)\n", "��������", err_id);
			post_disconnect(client_);
		}
		}
		return true;
	}

	void server::start(uint16_t port_) {
		if (status != server_status::null) {
			return;
		}
		// ��ʼ���ٽ���
		for (uint8_t i = 0; i < server_use::shared_num; InitializeCriticalSection(&_cris[i++]));
		// ������ɶ˿�
		_complete_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
		assert(_complete_port != NULL);
		// ��ʼ��socket
		init_socket(port_);
		try {
			if (!_listen) {
				throw "�����׽��ֳ�ʼ��ʧ��";
			}
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
				// ��������client
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
		// ֪ͨ�˳�����
		SetEvent(_exit_notice);
		//for (int i = 0; i < _work_num; ++i, QueueUserAPC(_complete_port, (DWORD)NULL, NULL));
		for (uint8_t i = 0; i < _work_num; ++i, PostQueuedCompletionStatus(_complete_port, -1, (DWORD)NULL, NULL));
	}

	void server::restart() {
		printf("���������\n");
		close();
		// ��֤�����߳��˳�
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
		// ��ȫ���
		if (client_->get_status(client_status::wait_close)) {
			return;
		}
		// �洢��д������
		client_->io_data.write(str_);
		// ��ֹ������
		if (client_->get_mark(io_mark_type::write)) {
			//printf("send_to_individual��ֹ������: sock: %I64d\n", client->socket);
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