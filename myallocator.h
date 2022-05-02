#include<cstdlib>
#include<mutex>
#include<memory>
#include<iostream>
#include<cstring>
/*
* ��ֲSGI STL�����ռ�������Դ��
* ��ͬ��nginx�ڴ�أ������ռ���������ʹ�õ����ڴ����Ҫע����߳�����
* nginx�ڴ�أ�����һ���̴߳���һ���ڴ��
* �������ռ���������������������ʹ�õģ���vector���󡣶�һ��vector������п����ڶ���̲߳���ʹ��
* 
* 
* ��ֲ�ɹ���һ���̰߳�ȫ�Ķ����ռ������� my_allocator
*/
//  һ���ռ�������
template<typename T>
class first_level_my_allocator
{
public:
	T* allocate(size_t size)
	{
		T* p = malloc(sizeof(T)*size);
		return p;
	}

	void deallocate(T *p)
	{
		free(p);
	}
	
	void construct(T* p, const T& _val)
	{
		new (p) T(_val);
	}

	void construct(T* p, T&& rval)
	{
		new (p) T(std::move(rval));
	}

	void destory(T* p)
	{
		p->~T();
	}
};


template <int __inst>
class __malloc_alloc_template {

private:
	static void* _S_oom_realloc(void*, size_t);
	//  Ԥ�ƵĻص����� _S_oom_malloc -> __malloc_alloc_oom_handler
	static void* _S_oom_malloc(size_t);
	static void (*__malloc_alloc_oom_handler)();
public:
	//  ���Է����ڴ�
	static void* allocate(size_t __n)
	{
		void* __result = malloc(__n);
		if (0 == __result) __result = _S_oom_malloc(__n);	//  �����ͷ�nbytes�ڴ棬���ظ�result
		return __result;
	}

	//  �ͷ�
	static void deallocate(void* __p, size_t /* __n */)
	{
		free(__p);
	}

	//  ���·���new_sz��С�ڴ� 
	static void* reallocate(void* __p, size_t /* old_sz */, size_t __new_sz)
	{
		void* __result = realloc(__p, __new_sz);
		if (nullptr == __result) __result = _S_oom_realloc(__p, __new_sz);
		return __result;
	}
	
	//  �û�ͨ������ӿ���Ԥ���Լ��Ļص������������ͷ��ڴ�������ڴ治�������
	static void (*__set_malloc_handler(void (*__f)()))()
	{
		void (*__old)() = __malloc_alloc_oom_handler;
		__malloc_alloc_oom_handler = __f;
		return(__old);
	}

};


//  my_allocator���õ���__malloc_alloc_template�е�����������һ����Ա
template <int __inst>
void*
__malloc_alloc_template<__inst>::_S_oom_malloc(size_t __n)
{
	void (*__my_malloc_handler)();
	void* __result;

	for (;;) {
		__my_malloc_handler = __malloc_alloc_oom_handler;
		if (nullptr == __my_malloc_handler) { throw std::bad_alloc(); }
		(*__my_malloc_handler)();
		__result = malloc(__n);
		if (__result) return(__result);
	}
}


template <int __inst>
void* __malloc_alloc_template<__inst>::_S_oom_realloc(void* __p, size_t __n)
{
	void (*__my_malloc_handler)();
	void* __result;

	for (;;) {
		__my_malloc_handler = __malloc_alloc_oom_handler;
		if (nullptr == __my_malloc_handler) { throw std::bad_alloc(); }
		(*__my_malloc_handler)();
		__result = realloc(__p, __n);
		if (__result) return(__result);
	}
}

template <int __inst>
void (*__malloc_alloc_template<__inst>::__malloc_alloc_oom_handler)() = nullptr;
using malloc_alloc = __malloc_alloc_template<0>;




//  �����ռ������� ����ӿ�Ϊallocate��deallocate��reallocate��construct��destroy
template<typename T>
class my_allocator
{
public:
	using value_type = T;
	using _Newfirst = T;
	using _From_primary = my_allocator;
	constexpr my_allocator() noexcept {}
	constexpr my_allocator(const my_allocator&) noexcept = default;
	template <class _Other>
	constexpr my_allocator(const my_allocator<_Other>&) noexcept {}

	//  �����ڴ�
	T* allocate(size_t __n);

	//  �ͷ��ڴ�
	void deallocate(void* __p, size_t __n);

	//  ���·����ڴ�
	void* reallocate(void* __p, size_t __old_sz, size_t __new_sz);

	//  ��������
	template<typename Ty>
	void construct(T *__p,Ty&& val)
	{
		new (__p) T(std::forward<Ty>(val));
	}

	//  ���ٶ���
	void destroy(T* __p)
	{
		__p->~T();
	}

private:
	// Really we should use static const int x = N
	// instead of enum { x = N }, but few compilers accept the former.
	// enum { _ALIGN = 8 };								//  ����������8bytes��ʼ����8bytesΪ���뷽ʽ��һֱ���䵽128Bytes
	static const int _ALIGN = 8;
	// enum { _MAX_BYTES = 128 };							//  �����С   >128�Ͳ���ŵ��ڴ�����ˣ�Ҳ�������ö����ռ�������������һ���ռ�������
	static const int _MAX_BYTES = 128;
	// enum { _NFREELISTS = 16 };  						//  ����������Ա����  _MAX_BYTES/_ALIGN
	static const int _NFREELISTS = 16;
	

	//  ��byte�ϵ���8�ı���
	static size_t _S_round_up(size_t __bytes)
	{
		return (((__bytes)+(size_t)_ALIGN - 1) & ~((size_t)_ALIGN - 1));
	}

	//  �����bytes��С��chunkӦ�ù��ص���������free-list���ĸ���Ա��
	static  size_t _S_freelist_index(size_t __bytes) {
		return (((__bytes)+(size_t)_ALIGN - 1) / (size_t)_ALIGN - 1);
	}

	//  �����ڴ�أ����ص�freelist��Ա�£�����������ڴ�顣
	static void* _S_refill(size_t __n);
	//  �ӻ�û�γ�������ԭʼ�ڴ���ȡ�ڴ���������������Ա��ȥ�γ����������ԭʼ�����ڴ治���ˣ����ٿ���
	static char* _S_chunk_alloc(size_t __size, int& __nobjs);


	//  ÿ��chunk�����Ϣ��_M_free_list_linkָ����һ��chunk��
	union _Obj {
		union _Obj* _M_free_list_link;
		char _M_client_data[1];    /* The client sees this.        */
	};

	//  ����free-list���ڴ�أ���Ҫ�����̰߳�ȫ
	static _Obj* volatile _S_free_list[_NFREELISTS];	//  ��ֹ���̻߳���
	static std::mutex mtx;


	//  static���������������ⶨ�塣
	static char* _S_start_free;         //   ����free�ڴ����ʼstartλ�� 
	static char* _S_end_free;           //   ����free�ڴ�Ľ���endλ�� ) 
	static size_t _S_heap_size;         //   �ܹ�malloc�����ڴ��С����Ϊmalloc�ǴӶ�heap������ģ����Խ�heapsize��
	
};


//  static �������������ⶨ��/��ʼ��
template<typename T>
std::mutex my_allocator<T>::mtx;

template <typename T>
char* my_allocator<T>::_S_start_free = 0;
template <typename T>
char* my_allocator<T>::_S_end_free = 0;
template <typename T>
size_t my_allocator<T>::_S_heap_size = 0;

template <typename T>
typename my_allocator<T>::_Obj* volatile
my_allocator<T> ::_S_free_list[my_allocator<T>::_NFREELISTS] 
= { nullptr,nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, };



template<typename T>
T* my_allocator<T>::allocate(size_t __n)	
{
	//  ����Ӧ���õ���bytes
	__n = __n * sizeof(T);		//  ��Ϊvector������������Ԫ�ظ���
	//  std::cout << "user allocate " << __n << std::endl;
	void* __ret = 0;
	if (__n > (size_t)_MAX_BYTES) {
		__ret = malloc_alloc::allocate(__n);
	}
	else {
		_Obj* volatile* __my_free_list
			= _S_free_list + _S_freelist_index(__n);
		// Acquire the lock here with a constructor call.
		// This ensures that it is released in exit or during stack
		// unwinding.

		std::lock_guard<std::mutex> guard(mtx);

		_Obj* __result = *__my_free_list;
		if (__result == 0)
			__ret = _S_refill(_S_round_up(__n));
		else {
			*__my_free_list = __result->_M_free_list_link;
			__ret = __result;
		}
	}
	return static_cast<T*>(__ret);
}



template<typename T>
void* my_allocator<T>::_S_refill(size_t __n)  //  n��һ��chunk��Ĵ�С
{
	//  ����ָ����С���ڴ��    __nobjs��chunk�ڴ������ ������� __n��ÿ��chunk�ڴ���С��
	int __nobjs = 20;
	char* __chunk = _S_chunk_alloc(__n, __nobjs);
	_Obj* volatile* __my_free_list;
	_Obj* __result;
	_Obj* __current_obj;
	_Obj* __next_obj;
	int __i;
	//  __nobjs�����뵽��chunk����������ֻ���뵽һ��ʱ��ֱ�ӷ��ظ��ڴ�����һ��ʹ�á����轨������chunk�����ӹ�ϵ��������ص���Ӧ��freelist��Ա�¡�����Ϊֻ��һ����
	if (1 == __nobjs) return(__chunk);

	__my_free_list = _S_free_list + _S_freelist_index(__n);     //  �����ڴ���С����ڴ��Ӧ������freelist�ڼ�����Ա������ָ��

	//  ��̬��������ÿ��chunk��ͨ��Obj*���ָ����������
	//  ÿ���ڴ�飬��һ���ֵ��ڴ�ʱunion������Obj��������һ��Obj*ָ�룬��������ÿ�������ڴ�顣
	/* Build free list in chunk */
	__result = (_Obj*)__chunk;
	*__my_free_list = __next_obj = (_Obj*)(__chunk + __n);    //  __n��һ���ڴ��Ĵ�С����Ϊ��һ���ڴ��Ҫ�����ȥ��
	for (__i = 1; ; __i++) {
		__current_obj = __next_obj;
		__next_obj = (_Obj*)((char*)__next_obj + __n);          //   ά���ڴ��������  char* ���+__n��ƫ��n��bytes +n��Ϊ��һ����һ��chunk��
		if (__nobjs - 1 == __i) {                               //   �����ڴ����Ϊ0?
			__current_obj->_M_free_list_link = 0;
			break;
		}
		else {
			__current_obj->_M_free_list_link = __next_obj;
		}
	}
	return(__result);   //  ���ص�һ���ڴ��
}


/* We allocate memory in large chunks in order to avoid fragmenting     */
/* the malloc heap too much.                                            */
/* We assume that size is properly aligned.                             */
/* We hold the allocation lock.                                         */
template <typename T>
char* my_allocator<T>::_S_chunk_alloc(size_t __size,int& __nobjs)
{
	char* __result;
	size_t __total_bytes = __size * __nobjs;            //  �����ܹ���Ҫ������ڴ��С
	size_t __bytes_left = _S_end_free - _S_start_free;  //  __default_alloc_template<__threads, __inst> �ӿ�ʼ�����ڣ������ʣ����еĵ��ڴ��С�����������յġ�ֻ�ǿ��ٵġ�

	if (__bytes_left >= __total_bytes) {                //  ʣ��ı����ڴ湻֧������������ڴ��С��
		__result = _S_start_free;                       //  __result ��Ϊ�����ڴ��׵�ַ
		_S_start_free += __total_bytes;                 //  �ƶ�_S_start_free
		return(__result);                               //  [result , _S_start_free)  ��Ϊ����������
	}
	else if (__bytes_left >= __size) {                //  ʣ��Ĳ���֧��total����������֧��һ���ڴ�顣����ΪҪ���ص�������һ���ڴ���С��
		__nobjs = (int)(__bytes_left / __size);
		__total_bytes = __size * __nobjs;
		__result = _S_start_free;
		_S_start_free += __total_bytes;
		return(__result);
	}
	else {                                            //  ʣ���free�ڴ���һ���ڴ��Ҳ����֧��
		size_t __bytes_to_get =                         //  ��ʣ��Ŀ����ڴ治��ʱ����Ҫ���ͳmalloc�ڴ档���Ǽ������Ҫmalloc�ڴ�Ĵ�С������malloc����Ҫ���ڴ��(__total_bytes)������
			2 * __total_bytes + _S_round_up(_S_heap_size >> 4);
		// Try to make use of the left-over piece.
		// ʣ��ı����ڴ�bytes_left,�ֲ������������һ��chunk���С���Ͱ�����ڴ���ص�����������freelist��Ա�¡���ͷ�巨��
		if (__bytes_left > 0) {                         //  
			_Obj* volatile* __my_free_list =
				_S_free_list + _S_freelist_index(__bytes_left);

			((_Obj*)_S_start_free)->_M_free_list_link = *__my_free_list;
			*__my_free_list = (_Obj*)_S_start_free;
		}
		_S_start_free = (char*)malloc(__bytes_to_get);  //  ���ͳmalloc�ڴ�
		if (nullptr == _S_start_free) {                       //  mallocʧ��
			size_t __i;
			_Obj* volatile* __my_free_list;
			_Obj* __p;
			// Try to make do with what we have.  That can't
			// hurt.  We do not try smaller requests, since that tends
			// to result in disaster on multi-process machines.
			for (__i = __size;
				__i <= (size_t)_MAX_BYTES;
				__i += (size_t)_ALIGN) {
				__my_free_list = _S_free_list + _S_freelist_index(__i);
				__p = *__my_free_list;
				if (nullptr != __p) {
					*__my_free_list = __p->_M_free_list_link;
					_S_start_free = (char*)__p;
					_S_end_free = _S_start_free + __i;
					return(_S_chunk_alloc(__size, __nobjs));
					// Any leftover piece will eventually make it to the
					// right free list.
					//  �κ�һ��ʣ�µ��ڴ�Ƭ���ն�����ص���ȷ��freelist��Ա�¡�
				}
			}
			//  ��û��ʱ��ֻ��allcoate
			_S_end_free = nullptr;	// In case of exception.
			_S_start_free = (char*)malloc_alloc::allocate(__bytes_to_get);
			// This should either throw an
			// exception or remedy the situation.  Thus we assume it
			// succeeded.
		}
		_S_heap_size += __bytes_to_get;               //  _S_heap_size������Ϊֹ�ܹ�malloc�˶����ڴ�?
		_S_end_free = _S_start_free + __bytes_to_get; //  �ƶ�_S_end_freeָ�롣ָ������ڴ��ĩβ
		return(_S_chunk_alloc(__size, __nobjs));      //  �ݹ����
	}
}


/* __p may not be 0 */
template<typename T>
void my_allocator<T>::deallocate(void* __p, size_t __n)		//  ��Ϊvector�������������ֽڴ�С
{
	if (__n > (size_t)_MAX_BYTES)	//  n>128 ͬһ���ռ�������
	{
		malloc_alloc::deallocate(__p, __n);
	}
	else 
	{
		_Obj* volatile* __my_free_list
			= _S_free_list + _S_freelist_index(__n);  //  �ҵ���Ӧfreelist��Ա
		_Obj* __q = (_Obj*)__p;       //  

		// acquire lock
		std::lock_guard<std::mutex> guard(mtx);

		__q->_M_free_list_link = *__my_free_list;
		*__my_free_list = __q;
		// lock is released here
	}
}




template<typename T>
void* my_allocator<T>::reallocate(void* __p, size_t __old_sz, size_t __new_sz)
{
	void* __result;
	size_t __copy_sz;

	//  old new��>128byets����ô�õľ�Ӧ���Ǻ�һ���ռ�������һ���ķ���malloc
	if (__old_sz > (size_t)_MAX_BYTES && __new_sz > (size_t)_MAX_BYTES) {
		return(realloc(__p, __new_sz));
	}

	//  ������������chunk���Сһ�£���ô���أ�ֱ�ӷ���
	if (_S_round_up(__old_sz) == _S_round_up(__new_sz)) return(__p);

	//  ���¿���new_size��С�ڴ棬���������ݵ��¿��ٵ��ڴ���
	__result = allocate(__new_sz);
	__copy_sz = __new_sz > __old_sz ? __old_sz : __new_sz;
	memcpy(__result, __p, __copy_sz); //  ��������
	deallocate(__p, __old_sz);  //  �ͷ�
	return(__result);
}





