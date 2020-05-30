template <uint32_t load_num>
socket_pool<load_num>::socket_pool(int protocol) : _protocol(protocol) {
	// 初始化临界区
	InitializeCriticalSection(&_cri);
}

template <uint32_t load_num>
socket_pool<load_num>::~socket_pool() {
	{
		raii::critical r1(&_cri);
		for (auto i : _use_socket) {
			del(i);
		}
		_use_socket.clear();
	}
	// 释放临界区
	DeleteCriticalSection(&_cri);
};

template <uint32_t load_num>
void socket_pool<load_num>::add_new() {
	raii::critical r1(&_cri);
	SOCKET sock;
	BOOL optval = TRUE;
	for (uint32_t i = 0; i < load_num; ++i) {
		sock = INVALID_SOCKET;
		for (uint8_t j = 0; j < 3; ++j) {
			sock = WSASocket(AF_INET, SOCK_STREAM, _protocol, NULL, 0, WSA_FLAG_OVERLAPPED);
			if (sock != INVALID_SOCKET) {
				continue;
			}
		}
		_use_socket.push_back(sock);
	}
}

template <uint32_t load_num>
SOCKET socket_pool<load_num>::get() {
	raii::critical r1(&_cri);
	if (_use_socket.empty()) {
		add_new();
	}
	SOCKET sock = _use_socket.front();
	_use_socket.pop_front();
	return sock;
}

template <uint32_t load_num>
inline void socket_pool<load_num>::rec(SOCKET sock) {
	if (sock == INVALID_SOCKET) {
		return;
	}
	raii::critical r1(&_cri);
	_use_socket.push_back(sock);
}

template <uint32_t load_num>
inline void socket_pool<load_num>::del(SOCKET& sock, bool reset) {
	if (sock != INVALID_SOCKET) {
		closesocket(sock);
	}
	if (reset) {
		sock = INVALID_SOCKET;
	}
}