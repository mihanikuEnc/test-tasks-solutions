/**
 * @file cyclic_queue.h
 * @author Латышев Михаил
 * @date 10.03.2026
 * 
 * @brief Шаблонная циклическая очередь с фиксированной ёмкостью
 *
 * @details Реализует потокобезопасную очередь для сценария "один писатель - один читатель"
 *          без использования блокировок.
 *
 * @tparam T Тип данных, хранящихся в очереди. Должен поддерживать:
 *           - конструктор копирования
 *           - оператор присваивания
 * @tparam CAPACITY Ёмкость очереди - максимальное количество хранимых элементов типа T
 *
 * @attention Соответствует следующим требованиям:
 *            - Память для хранения данных аллоцируется статически внутри очереди
 *            - Безопасна для одновременной записи и чтения из двух потоков
 *            - Не использует механизмы взаимной блокировки потоков (мьютексы)
 *            - Объекты уничтожаются сразу после извлечения из очереди
 *            - Поддерживает конструктор копирования и оператор присваивания
 *
 * @note При переполнении новые данные не записываются, метод записи возвращает false
 *
 * @warning Копирование очереди не является потокобезопасным - должно выполняться
 *          в момент, когда исходная очередь не используется потоками чтения/записи.
 */

#pragma once

#include <atomic>
#include <type_traits>

template<typename T, size_t CAPACITY>
class CiclicQueue
{
public:
	/**
	 * @brief Конструктор по умолчанию
	 *
	 * @details Инициализирует индексы чтения и записи нулевыми значениями.
	 * Память под элементы не инициализируется. Очередь создается пустой.
	 */
	CiclicQueue() : head_(0), tail_(0)
	{
	}

	/**
	 * @brief Конструктор копирования
	 *
	 * @details Создает копию очереди. Копируются элементы из диапазона [head, tail).
	 *          Для каждого элемента вызывается конструктор копирования типа T в сырую память.
	 *
	 * @param other Исходная очередь для копирования
	 */
	CiclicQueue(const CiclicQueue& other) : head_(other.head_.load(std::memory_order_acquire)),
		tail_(other.tail_.load(std::memory_order_acquire))
	{
		const uint64_t current_tail = tail_.load(std::memory_order_relaxed);
		const uint64_t current_head = head_.load(std::memory_order_relaxed);
		for (uint64_t current_element = current_head; current_element < current_tail; current_element++)
		{
			uint64_t current_index = current_element % CAPACITY;
			const T* new_element = reinterpret_cast<const T*>(&other.queue_[current_index]);
			new(&queue_[current_index]) T(*new_element);
		}
	}

	/**
	 * @brief Оператор присваивания
	 *
	 * @details Выполняет копирование очереди с предварительным уничтожением всех текущих элементов.
	 *			Реализована защита от самоприсваивания.
	 *
	 * @param other Исходная очередь для копирования
	 * @return Ссылка на текущую очередь после копирования
	 */
	CiclicQueue& operator=(const CiclicQueue& other)
	{
		// Проверяем на самокопирование
		if (this == &other)
		{
			return *this;
		}
		// Очищаем текущую очередь
		uint64_t current_tail = tail_.load(std::memory_order_acquire);
		uint64_t current_head = head_.load(std::memory_order_acquire);
		for (uint64_t current_element = current_head; current_element < current_tail; current_element++)
		{
			uint64_t current_index = current_element % CAPACITY;
			const T* old_element = reinterpret_cast<T*>(&queue_[current_index]);
			old_element->~T();
		}
		// Копируем head_ и tail_ из other
		uint64_t new_head = other.head_.load(std::memory_order_acquire);
		uint64_t new_tail = other.tail_.load(std::memory_order_acquire);
		// Копируем элементы очереди
		for (uint64_t new_current_element = new_head; new_current_element < new_tail; new_current_element++)
		{
			uint64_t current_index = new_current_element % CAPACITY;
			const T* new_element = reinterpret_cast<const T*>(&other.queue_[current_index]);
			new(&queue_[current_index]) T(*new_element);
		}
		// Обновляем head_ и tail_ атомарно 
		head_.store(new_head, std::memory_order_release);
		tail_.store(new_tail, std::memory_order_release);
		return *this;
	}

	/**
	 * @brief Извлечение элемента из очереди
	 *
	 * @details Пытается извлечь элемент из головы очереди.
	 *
	 * @param out Ссылка на out типа T, куда будет скопирован извлеченный элемент
	 * @return true если элемент успешно извлечен
	 * @return false если очередь пуста
	 */
	bool Pop(T& out)
	{
		const uint64_t current_tail = tail_.load(std::memory_order_relaxed);
		uint64_t current_head = head_.load(std::memory_order_acquire);
		// Проверяем пуста ли очередь
		if (current_tail == current_head)
		{
			return false;
		}
		// Копируем элемент в голове очереди в out и удаляем элемент из очереди
		uint64_t head_index = current_head % CAPACITY;
		const T* out_element = reinterpret_cast<T*>(&queue_[head_index]);
		out = *out_element;
		out_element->~T();
		// Корректирукем положение головы очереди
		head_.store(current_head + 1, std::memory_order_release);
		return true;
	}

	/**
	 * @brief Добавление элемента в очередь
	 *
	 * @details Добавляет новый элемент в хвост очереди.
	 *			При переполнении возвращает false.
	 *
	 * @param new_element Элемент для добавления в очередь
	 * @return true данные помещены в очередь
	 * @returt false данные не помещены в очередь
	 */
	bool Push(const T& new_element)
	{
		uint64_t current_tail = tail_.load(std::memory_order_relaxed);
		const uint64_t current_head = head_.load(std::memory_order_acquire);
		uint64_t tail_index = current_tail % CAPACITY;
		// Проверяем произошло ли переполнене
		if (current_tail - current_head >= CAPACITY)
		{
			return false;
		}
		// Добавляем новый элемент в очередь
		new(&queue_[tail_index]) T(new_element);
		// Корректируем положение хвоста очереди
		tail_.store(current_tail + 1, std::memory_order_release);
		return true;
	}

	/**
	 * @brief Проверка очереди на пустоту
	 * @return true если очередь пуста
	 * @return false если в очереди есть хотя бы один элемент
	 */
	const bool IsEmpty()
	{
		const uint64_t current_tail = tail_.load(std::memory_order_acquire);
		const uint64_t current_head = head_.load(std::memory_order_acquire);
		return current_tail == current_head;
	}

	/**
	 * @brief Проверка на переполнение очереди
	 * @return true если очередь заполнена полностью (идет перезапись данных)
	 * @return false если очередь не переполгнена
	 */
	const bool IsFull()
	{
		const uint64_t current_tail = tail_.load(std::memory_order_acquire);
		const uint64_t current_head = head_.load(std::memory_order_acquire);
		return current_tail - current_head == CAPACITY;
	}

	/**
	 * @brief Получение размера очереди
	 * @return Колличество элементов в очереди
	 */
	const uint64_t GetQueueSize()
	{
		if (IsFull())
		{
			return CAPACITY;
		}
		const uint64_t current_tail = tail_.load(std::memory_order_acquire);
		const uint64_t current_head = head_.load(std::memory_order_acquire);
		return current_tail - current_head;
	}

	/**
	 * @brief Получение максимальной ёмкости очереди
	 * @return CAPACITY - максимальное количество элементов, которое может хранить очередь
	 */
	static constexpr size_t GetQueueCapacity()
	{
		return CAPACITY;
	}

private:
	/**
	 * @brief Очередь хоранения данных
	 *
	 * @details Массив сырых байтов с выравниванием под тип T.
	 * Для статической аллокации памяти
	 */
	std::aligned_storage_t<sizeof(T), alignof(T)> queue_[CAPACITY];

	/**
	 * @brief Индекс чтения элемента из очереди
	 *
	 * @note Монотонно возрастает
	 */
	std::atomic<uint64_t> head_;

	/**
	 * @brief Индекс записи нового элемента в очередь
	 *
	 * @note Монотонно возрастает
	 */
	std::atomic<uint64_t> tail_;
};