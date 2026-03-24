#include <cassert>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <iostream>
#include <utility>

#include "cyclic_queue.h"

namespace
{
	struct MoveAware
	{
		std::string value;
		bool moved_from = false;

		MoveAware() = default;

		explicit MoveAware(std::string v)
			: value(std::move(v))
		{
		}

		MoveAware(const MoveAware&) = default;

		MoveAware& operator=(const MoveAware&) = default;

		MoveAware(MoveAware&& other) noexcept
			: value(std::move(other.value))
		{
			other.moved_from = true;
		}

		MoveAware& operator=(MoveAware&& other) noexcept
		{
			if (this != &other)
			{
				value = std::move(other.value);
				other.moved_from = true;
			}
			return *this;
		}
	};

	struct EmplaceOnly
	{
		int id;
		std::string name;

		EmplaceOnly(int i, std::string n)
			: id(i), name(std::move(n))
		{
		}
	};

	void test_default_constructed_queue_is_empty()
	{
		CiclicQueue<int, 4> q;

		assert(q.IsEmpty());
		assert(!q.IsFull());
		assert(q.GetQueueSize() == 0);
		assert(q.GetQueueCapacity() == 4);

		int out = -1;
		assert(!q.Pop(out));
	}

	void test_push_pop_single_element()
	{
		CiclicQueue<int, 4> q;

		q.Push(42);

		assert(!q.IsEmpty());
		assert(q.GetQueueSize() == 1);

		int out = 0;
		assert(q.Pop(out));
		assert(out == 42);

		assert(q.IsEmpty());
		assert(q.GetQueueSize() == 0);
	}

	void test_fifo_order()
	{
		CiclicQueue<int, 8> q;

		q.Push(1);
		q.Push(2);
		q.Push(3);

		int out = 0;

		assert(q.Pop(out));
		assert(out == 1);

		assert(q.Pop(out));
		assert(out == 2);

		assert(q.Pop(out));
		assert(out == 3);

		assert(q.IsEmpty());
	}

	void test_full_and_size()
	{
		CiclicQueue<int, 3> q;

		q.Push(10);
		q.Push(20);
		q.Push(30);

		assert(!q.IsEmpty());
		assert(q.IsFull());
		assert(q.GetQueueSize() == 3);

		int out = 0;
		assert(q.Pop(out));
		assert(out == 10);

		assert(!q.IsFull());
		assert(q.GetQueueSize() == 2);
	}

	void test_overwrite_oldest_when_full()
	{
		CiclicQueue<int, 3> q;

		q.Push(1);
		q.Push(2);
		q.Push(3);
		q.Push(4); // должен вытеснить 1

		assert(q.GetQueueSize() == 3);
		assert(q.IsFull());

		int out = 0;

		assert(q.Pop(out));
		assert(out == 1);

		assert(q.Pop(out));
		assert(out == 2);

		assert(q.Pop(out));
		assert(out == 3);

		assert(q.IsEmpty());
	}

	void test_copy_constructor()
	{
		CiclicQueue<int, 5> src;
		src.Push(10);
		src.Push(20);
		src.Push(30);

		CiclicQueue<int, 5> copy(src);

		assert(copy.GetQueueSize() == 3);

		int out = 0;
		assert(copy.Pop(out));
		assert(out == 10);

		assert(copy.Pop(out));
		assert(out == 20);

		assert(copy.Pop(out));
		assert(out == 30);

		assert(copy.IsEmpty());

		// Проверим, что оригинал не испорчен
		assert(src.Pop(out));
		assert(out == 10);
	}

	void test_copy_assignment()
	{
		CiclicQueue<int, 5> src;
		src.Push(7);
		src.Push(8);
		src.Push(9);

		CiclicQueue<int, 5> dst;
		dst.Push(100);
		dst.Push(200);

		dst = src;

		assert(dst.GetQueueSize() == 3);

		int out = 0;
		assert(dst.Pop(out));
		assert(out == 7);

		assert(dst.Pop(out));
		assert(out == 8);

		assert(dst.Pop(out));
		assert(out == 9);

		assert(dst.IsEmpty());
	}

	void test_copy_from_larger_than_capacity_keeps_last_elements()
	{
		CiclicQueue<int, 3> src;

		src.Push(1);
		src.Push(2);
		src.Push(3);
		src.Push(4);
		src.Push(5);

		CiclicQueue<int, 3> copy(src);

		int out = 0;
		assert(copy.Pop(out));
		assert(out == 1);

		assert(copy.Pop(out));
		assert(out == 2);

		assert(copy.Pop(out));
		assert(out == 3);

		assert(copy.IsEmpty());
	}

	void test_interleaved_push_pop()
	{
		CiclicQueue<int, 3> q;
		int out = 0;

		q.Push(1);
		q.Push(2);

		assert(q.Pop(out));
		assert(out == 1);

		q.Push(3);
		q.Push(4); // теперь в очереди должны быть 2,3,4

		assert(q.Pop(out));
		assert(out == 2);

		assert(q.Pop(out));
		assert(out == 3);

		assert(q.Pop(out));
		assert(out == 4);

		assert(q.IsEmpty());
	}

	void test_spsc_threads()
	{
		constexpr int N = 100000;
		CiclicQueue<int, 1024> q;

		std::atomic<bool> producer_done{ false };
		std::vector<int> consumed;
		consumed.reserve(N);

		std::thread producer([&]()
			{
				for (int i = 0; i < N; ++i)
				{
					// push не блокирующий, при переполнении вытесняет старые элементы,
					// поэтому producer подождет, пока consumer освободит место.
					while (q.IsFull())
					{
						std::this_thread::yield();
					}
					q.Push(i);
				}
				producer_done.store(true, std::memory_order_release);
			});

		std::thread consumer([&]()
			{
				int value = 0;
				while (!producer_done.load(std::memory_order_acquire) || !q.IsEmpty())
				{
					if (q.Pop(value))
					{
						consumed.push_back(value);
					}
					else
					{
						std::this_thread::yield();
					}
				}
			});

		producer.join();
		consumer.join();

		assert(static_cast<int>(consumed.size()) == N);

		for (int i = 0; i < N; ++i)
		{
			assert(consumed[static_cast<std::size_t>(i)] == i);
		}
		std::cout << "test_spsc_threads passed\n";
	}

	void test_powof2_overflow() {
		constexpr size_t capacity = 100000;
		constexpr size_t cycleCount = 5000000000 / capacity;
		CiclicQueue<uint32_t, capacity> queue;

		for (size_t i = 0; i < cycleCount; i++) {
			for (size_t j = 0; j < capacity; j++) {
				queue.Push(i - j);
			}
			std::cout << i <<"puhed\n";
			for (size_t j = 0; j < capacity; j++) {
				uint32_t v;
				auto g = i - j;
				assert(queue.Pop(v));
				assert(v == (i - j));
			}
			std::cout << i << "poped\n";
		}
	}
}

int main()
{
	test_default_constructed_queue_is_empty();
	test_push_pop_single_element();
	test_fifo_order();
	test_full_and_size();
	test_overwrite_oldest_when_full();
	test_copy_constructor();
	test_copy_assignment();
	test_copy_from_larger_than_capacity_keeps_last_elements();
	test_interleaved_push_pop();
	test_spsc_threads();
	test_powof2_overflow();
	std::cout << "All tests passed\n";
	return 0;
}