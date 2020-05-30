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
		_exit(false),											//�˳�״̬
		_exit_notice(NULL),										//�˳�֪ͨ�¼�
		_complete_port(NULL),									//��ɶ˿ھ��
		_max_work_num(2 * std::thread::hardware_concurrency()), //������߳���(����io������), ����Ϊ: 2 * std::thread::hardware_concurrency()
		_work_num(_max_work_num),								//�����߳���
		_timer(time_heap::instance()),							//��ʱ��
		_thread_pool(&thread_pool::instance()),					//�̳߳�
		_socket_pool(IPPROTO_TCP)								//socket��
	{
		// user���ٺ���
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
				throw "�汾�Ŵ���";
			}
			// �������Socket
			creator_user(_listen);
			if (!init_user(user_init_type::monitor, _listen)) {
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
			sockaddr.sin_port = htons(port);			//(�˿ں�)
			// ���ö˿ڸ���
			BOOL optval = TRUE;
			if (SOCKET_ERROR == setsockopt(_listen->socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval))) {
				throw "���ö˿ڸ���ʧ��";
			}
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
	}

	void server::clear_all() {
		printf("clear_all\n");
		// ����ͻ�������
		for (auto& i : _users) {
			i.second.reset();
		}
		_users.clear();
		for (auto& i : _accept_context) {
			i.reset();
		}
		_accept_context.clear();
		// �ͷ��ٽ���
		for (uint8_t i = 0; i < server_use::shared_num; DeleteCriticalSection(&_cris[i++]));
		// �ͷž��
		tool::common::close_handle(_exit_notice);
		tool::common::close_handle(_complete_port);
		// �������
		del_user(_listen, false);
	}

	bool server::init_user(user_init_type type_, user_ptr& user_, SOCKET sock_, SOCKADDR_IN* addr_) {
		switch (type_) {
		case user_init_type::monitor: {
			user_->socket = _socket_pool.get();
			return user_->socket != INVALID_SOCKET;
		} break;
		case user_init_type::accept: {
			// ��ʼ��������
			user_->context.accept = _memory_pool.io_context.new_obj(user_, &_memory_pool.io_context);
			user_->context.accept->type = io_type::accept;
			return true;
		} break;
		case user_init_type::normal: {
			user_->socket = user_->info.socket = sock_;
			// ���µ�ַ��Ϣ
			user_->update_addr_in(addr_);
			// ��ʼ��������
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
		// ����socket
		if (dec_sock) {
			_socket_pool.rec(user_->socket);
		}
		else {
			_socket_pool.del(user_->socket);
		}
		// ����ָ�����ٺ���Ϊ_user_del, ���ڲ���ִ�л��ղ���
	}

	void server::post_accept(user_ptr& user_) {
		try {
			// ׼������
			auto context = user_->context.accept;
			DWORD bytes = 0, addr_len = sizeof(SOCKADDR_IN) + 16;
			WSABUF* buf = &user_->io_data.io_buf.read;
			// ����io����
			user_->io_data.reset_read_buf();
			// ׼���ͻ���socket
			user_->socket = _socket_pool.get();
			// �������׽���
			if (user_->socket == INVALID_SOCKET) {
				throw "����socketʧ��";
			}
			printf("%s %I64d\n", "accpet socket", user_->socket);
			// Ͷ��AcceptEx����(���ջ�������СΪ0���Է�ֹ�ܾ����񹥻�ռ�÷�������Դ, ���������Ϊ wsabuf->len - (addr_len * 2) ,_acceptex_addr_ptr��������СӦ�뵱ǰһ��)
			if (!_acceptex_ptr(_listen->socket, user_->socket, user_->io_data.io_buf.read.buf, 0, addr_len, addr_len, &bytes, &context->overlapped) && WSAGetLastError() != WSA_IO_PENDING) {
				throw "Ͷ��accept����ʧ��";
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
		// ׼������
		auto context = user_->context.accept;
		SOCKADDR_IN* user_addr = nullptr, * local_addr = nullptr;
		int addr_len = sizeof(SOCKADDR_IN);
		char ip[16] = {};
		// ͨ��GetAcceptExSockAddrs��ȡ�ͻ���/����SOCKADDR_IN(���㻺������С����ÿͻ��˵�һ������)
		_acceptex_addr_ptr(user_->io_data.io_buf.read.buf, 0, addr_len + 16, addr_len + 16, (LPSOCKADDR*)&local_addr, &addr_len, (LPSOCKADDR*)&user_addr, &addr_len);
		// �����µĿͻ�������
		user_ptr new_user(creator_user());
		init_user(user_init_type::normal, new_user, user_->socket, user_addr);
		// ����Ͷ��accept����
		post_accept(user_);
		// �󶨿ͻ���socket����ɶ˿�
		if (CreateIoCompletionPort(HANDLE(new_user->socket), _complete_port, NULL, 0) == 0) {
			auto err_id = WSAGetLastError();
			switch (err_id) {
			case ERROR_INVALID_PARAMETER:
			case NO_ERROR: {
			} break;
			default: {
				printf("%s(error: %d)\n", "�󶨿ͻ���socket����ɶ˿�ʧ��", err_id);
				del_user(new_user, false);
				return;
			}
			}
		}
		// ��ӿͻ�����Ϣ
		add_user(new_user);
		// Ͷ��recv��I/O����
		post_request_read(new_user);
		// �¼�����
		if (_event_process.accept) {
			_event_process.accept(&new_user->info);
		}
	}

	bool server::post_write(user_ptr& user_) {
		raii::critical r1(&user_->cri);
		// Ͷ�ݼ��
		if (user_->get_mark(io_mark_type::write)) {
			printf("post_write_exit1: sock: %I64d\n", user_->socket);
			return false;
		}
		// ���������¼�
		if (user_->io_data.empty()) {
			printf("post_write_exit2: sock: %I64d\n", user_->socket);
			// �Ƿ�ȴ��ر�
			if (user_->get_status(user_status::wait_close)) {
				post_disconnect(user_);
			}
			return false;
		}
		// �ϰ�����
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
		// ׼������
		DWORD flags = NULL, bytes = NULL;
		WSABUF* buf = &user_->io_data.io_buf.write;
		// ����io����
		auto context = user_->context.send;
		context->type = io_type::write;
		// ���÷�������
		user_->io_data.update_current_data();
		// ���»�����
		buf->buf = user_->io_data.current_data();
		buf->len = user_->io_data.current_len();
		/*context->wsabuf.buf = user_->io_data.current_data();
		context->wsabuf.len = user_->io_data.current_len();*/
		memset(&context->overlapped, 0, sizeof(OVERLAPPED));
		printf("post_write: sock: %I64d, len: %ld\n", user_->socket, buf->len);
		// Ͷ������д(��ʵ��С������)
		if (SOCKET_ERROR == WSASend(user_->socket, buf, 1, &bytes, flags, &context->overlapped, NULL)) {
			auto err_id = WSAGetLastError();
			switch (err_id)
			{
			case WSA_IO_PENDING:
			case NO_ERROR: {
				return true;
			} break;
			case WSAECONNABORTED:
			case WSAECONNRESET: {	//�Զ��Ѿ��ر�����
				printf("io�ر� sock: %I64d\n", user_->socket);
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
			// ����ʧ����ɾ�����
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
		// ����д������
		user_->io_data.write_len(static_cast<slen_t>(byte_));
		// ����Ͷ��д��
		user_->del_mark(io_mark_type::write);
		post_write(user_);
	}

	bool server::post_request_read(user_ptr& user_) {
		// Ͷ�ݼ��
		raii::critical r1(&user_->cri);
		if (user_->get_mark(io_mark_type::read)) {
			return false;
		}
		user_->set_mark(io_mark_type::read);
		// ׼������
		DWORD flags = NULL, bytes = NULL;
		WSABUF* buf = &user_->io_data.io_buf.read;
		// ����io����
		auto context = user_->context.recv;
		context->type = io_type::request_read;
		buf->len = 0;
		//context->wsabuf.len = 0;
		memset(&context->overlapped, 0, sizeof(OVERLAPPED));
		printf("post_request_read, sock: %I64d\n", user_->socket);
		// Ͷ�������(0�ֽڵĻ��������ڵõ�socket�ɶ�״̬ʱ����)
		if (SOCKET_ERROR == WSARecv(user_->socket, buf, 1, &bytes, &flags, &context->overlapped, NULL)) {
			auto err_id = WSAGetLastError();
			switch (err_id)
			{
			case WSA_IO_PENDING:
			case NO_ERROR: {
				return true;
			} break;
			case WSAECONNABORTED:
			case WSAECONNRESET: {	//�Զ��Ѿ��ر�����
				printf("io�ر� sock: %I64d\n", user_->socket);
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
			// ����ʧ����ɾ�����
			user_->del_mark(io_mark_type::read);
			return false;
		}
		return true;
	}
	
	bool server::post_read(user_ptr& user_) {
		raii::critical r1(&user_->cri);
		// ׼������
		DWORD flags = NULL, bytes = NULL;
		WSABUF* buf = &user_->io_data.io_buf.read;
		// ����io����
		auto context = user_->context.recv;
		context->type = io_type::read;
		user_->io_data.reset_read_buf();
		buf->len = io_data_use::read_buf_size;
		memset(&context->overlapped, 0, sizeof(OVERLAPPED));
		printf("post_read, len: %d, sock: %I64d\n", buf->len, user_->socket);
		// Ͷ�������(��ʵ��С������)
		if (SOCKET_ERROR == WSARecv(user_->socket, buf, 1, &bytes, &flags, &context->overlapped, NULL)) {
			auto err_id = WSAGetLastError();
			switch (err_id)
			{
			case WSA_IO_PENDING:
			case NO_ERROR: {
				return true;
			} break;
			case WSAECONNABORTED:
			case WSAECONNRESET: {	//�Զ��Ѿ��ر�����
				printf("io�ر� sock: %I64d\n", user_->socket);
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
			// ����ʧ����ɾ�����
			user_->del_mark(io_mark_type::read);
			return false;
		}
		return true;
	}

	void server::read_process(user_ptr& user_, DWORD byte_) {
		// ���ñ��
		user_->del_mark(io_mark_type::read);
		// �¼�����
		user_->io_data.read(static_cast<slen_t>(byte_));
		// Ͷ��recv
		post_request_read(user_);
		forced_disconnect(user_);
	}

	void server::post_disconnect(user_ptr& user_) {
		//printf("enter_disconnectex  sock: %I64d\n", user_->socket);
		// Ͷ�ݼ��
		raii::critical r1(&user_->cri);
		if (user_->get_mark(io_mark_type::disconnect)) {
			return;
		}
		user_->set_mark(io_mark_type::disconnect);
		// ���δ���io
		if (!user_->io_data.empty()) {
			user_->del_mark(io_mark_type::disconnect);
			user_->set_status(user_status::wait_close);
			user_->io_data.set_io_switch(network::io_type::write, false);
			printf("disconnectex_δ���,����δ�������ݣ��ӳٶϿ�  sock: %I64d\n", user_->socket);
			return;
		}
		// ��������
		init_user(user_init_type::disconnect, user_);
		// ׼������
		DWORD flags = TF_REUSE_SOCKET;
		// Ͷ��disconnectex����
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
				printf("Ͷ��disconnectex����ʧ��(error: %d)\n", err_id);
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
		// ������ü���
		del_user(user_, true);
		// ��ӡʣ��ͻ���
		std::string remaine;
		char socks[16] = {};
		if (_users.size() < 20) {
			for (auto& i : _users) {
				sprintf_s(socks, "%I64d,", i.second->socket);
				remaine.append(socks);
			}
		}
		printf("close_users num: %zd, ʣ��: %s\n", _users.size(), remaine.c_str());
	}

	void server::forced_disconnect(user_ptr& user_) {
		// Ͷ�ݼ��
		raii::critical r1(&user_->cri);
		if (user_->get_mark(io_mark_type::disconnect)) {
			return;
		}
		user_->set_mark(io_mark_type::disconnect);
		// ��������
		init_user(user_init_type::disconnect, user_);
		// ȡ����Ͷ�ݵ�IO
		CancelIoEx((HANDLE)user_->socket, &user_->context.disconnect->overlapped);
		// ��ֹ��/��
		shutdown(user_->socket, SD_BOTH);
		// ɾ���û�
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
			case io_type::write: {	//��ע: δ���ִ���
				return false;
			} break;
			case io_type::read: {	//��ע���ͻ��������Ͽ�
				printf("%s(error: %d)\n", "�ͻ��������Ͽ�", err_id);
				post_disconnect(user);
			} break;
			}
		} break;
		case WAIT_TIMEOUT: {		//��ע���ͻ����쳣�Ͽ�
			if (send(user->socket, "", 0, 0) == SOCKET_ERROR) {
				printf("%s(error: %d)\n", "�ͻ����쳣�Ͽ�", err_id);
				post_disconnect(user);
			}
			else {
				printf("%s(error: %d)\n", "�ȴ��Ĳ�����ʱ", err_id);
				return false;
			}
		} break;
		default: {
			printf("%s(error: %d)\n", "��������", err_id);
			post_disconnect(user);
		}
		}
		return true;
	}

	void server::start(uint16_t port_) {
		if (_port == 0) {
			_port = port_;
		}
		// ��ʼ���ٽ���
		for (uint8_t i = 0; i < server_use::shared_num; InitializeCriticalSection(&_cris[i++]));
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
			_accept_context.reserve(_max_work_num);
			for (int i = 0; i < _max_work_num; ++i) {
				// ��������user
				user_ptr user(creator_user());
				init_user(user_init_type::accept, user);
				// ���������������б�
				_accept_context.push_back(user);
				// Ͷ��accept����
				post_accept(user);
			}
		}
		catch (const char* err) {
			printf("%s(error: %d)\n", err, WSAGetLastError());
			clear_all();
			return;
		}
		// ���������߳�
		for (int i = 0; i < _work_num; ++i) {
			_thread_pool->add(std::move([&]() {
				uint32_t thread_id = (*(uint32_t*)&std::this_thread::get_id());
				printf("�����߳�(%d)����\n", thread_id);
				// ׼������
				BOOL status;								//��ɶ˿�״̬
				io_context* context;						//Ͷ�ݵ�������
				user_ptr user;								//Ͷ�ݵ�client
				OVERLAPPED_ENTRY entry[entry_count];		//�ص��ṹ����
				OVERLAPPED_ENTRY* current_entry = nullptr;	//��ǰ�ص��ṹ
				DWORD re_num;								//��������
				bool no_error = true;						//�޴����ʶ
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
							// ����������δ���������
							if (no_error) {
								try {
									// I/O����
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
										printf("%s\n", "Ĭ��io����(����)");

									} break;
									default: {
										printf("I/O���������쳣\n");
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
				printf("�����߳�(%d)����\n", thread_id);
				if (--_work_num == 0) {
					restart();
				}
				}));
		}
	}

	void server::close() {
		if (_listen && _listen->socket != INVALID_SOCKET) {
			_socket_pool.del(_listen->socket);
			// ֪ͨ�߳��˳�
			SetEvent(_exit_notice);
			// ֪ͨ��ɶ˿ڲ����˳�
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
		// ��ȫ���
		if (user_->get_status(user_status::wait_close)) {
			return;
		}
		// �洢��д������
		user_->io_data.write(data_, len_);
		// ��ֹ������
		if (user_->get_mark(io_mark_type::write)) {
			//printf("send_to_individual��ֹ������: sock: %I64d\n", user->socket);
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