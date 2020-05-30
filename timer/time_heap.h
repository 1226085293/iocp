#pragma once
#include <future>
#include <functional>
#include "raii/critical.h"

class time_heap {
private:
	// �¼���Ϣ
	class event {
		friend class time_heap;
	private:
		intptr_t				_origin;	//Դ��ַ
		uint64_t				_ms;		//��ʱʱ��
		int32_t					_count;		//ִ����
		std::function<void()>	_func;		//�ص�����
	protected:
	public:
		event(intptr_t origin_, uint64_t ms_, int32_t count_, std::function<void()>&& func_);
		event(const event&) = delete;
		event& operator =(const event&) = delete;
		~event() = default;
		// ɾ������
		void del();
	};

	// ��������
	template <class T>
	class result_data {
	private:
		event*										_info;		//�¼���Ϣ
		std::shared_ptr<std::packaged_task<T()>>	_pack_func;	//�Ѱ�װ�ĺ���
	protected:
	public:
		bool					vaild;		//��Ч��
		int32_t					count;		//ִ����
		int32_t					run_count;	//��ִ����

		result_data(int32_t count_, std::shared_ptr<std::packaged_task<T()>> pack_func_);
		result_data(const result_data&) = delete;
		result_data& operator =(const result_data&) = delete;
		~result_data() = default;

		// ��ȡִ�н��
		T get();
		// ɾ����ʱ��
		void del(event* info_ = nullptr);
	};

	// �ѽڵ�
	struct node {
		uint64_t	ms;
		event*		info;

		node() = default;
		node(const node&) = delete;
		node(node&& that_);
		node& operator =(const node& that_);
		node& operator =(node&& that_);
		~node() = default;
	};
	std::atomic<bool>					_death;			//����
	CRITICAL_SECTION					_cri;			//�ٽ���
	node*								_heap;			//��С��
	uint32_t							_max_size;		//���洢��
	uint32_t							_size;			//�洢��
	uint64_t							_event_time;	//����ʱ��
	HANDLE								_event;			//�¼�
	static std::unique_ptr<time_heap>	_instance;		//ʵ��ָ��

	// ��ӽڵ�
	void add_node();
	// ɾ���ڵ�
	void del_node(uint32_t index_);
	// ɾ��ָ���±궨ʱ��
	void del_timer(uint32_t index_);
	// ���ö�ʱ��
	void reset_timer(uint32_t index_);
	// ����
	void expansion();
protected:
public:
	time_heap(uint32_t max_size_ = 64);
	~time_heap();
	// ����
	static time_heap& instance();
	// ��ȡ�൱ǰʱ��(����)
	uint64_t get_ms();
	// ���
	template<class Func, class... Args>
	auto add(double second_, uint32_t count_, Func&& func_, Args&& ... args_);
};

#include "source/time_heap.icc"