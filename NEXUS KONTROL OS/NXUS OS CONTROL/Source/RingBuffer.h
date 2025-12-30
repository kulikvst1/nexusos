#pragma once
#include<array>
#include<iterator>

template
<typename T, size_t S>
class RingBuffer
{
	static constexpr size_t SIZE = S + 1;
public:

	class Iterator
	{
		friend class RingBuffer;

	public:
		typedef T iterator_type;
		typedef std::random_access_iterator_tag iterator_category;
		typedef iterator_type value_type;
		typedef ptrdiff_t difference_type;
		typedef iterator_type& reference;
		typedef iterator_type* pointer;

		pointer m_ptr;
		size_t m_end;
	private:
		Iterator(T* ptr, size_t end) :
			m_ptr(ptr), m_end(end)
		{}
	public:
		Iterator(const Iterator& it) :
			m_ptr(it.m_ptr), m_end(it.m_end)
		{}

		bool operator==(Iterator const& other) const
		{
			return m_end == other.m_end;
		}

		bool operator!=(Iterator const& other) const
		{
			return m_end != other.m_end;
		}

		typename Iterator::reference operator*() const
		{
			return m_ptr[m_end];
		}

		Iterator& operator++()
		{
			m_end++;
			m_end %= SIZE;
			return *this;
		}

		Iterator operator++(int)
		{
			auto tmp = *this;
			++* this;
			return tmp;
		}

		Iterator& operator--()
		{
			m_end--;
			m_end %= SIZE;
			return *this;
		}

		Iterator operator--(int)
		{
			auto tmp = *this;
			--* this;
			return tmp;
		}

		Iterator& operator+=(difference_type n)
		{
			m_end += n;
			m_end %= SIZE;
			return *this;
		}

		Iterator operator+(difference_type n)
		{
			Iterator tmp = *this;
			return tmp += n;
		}

		Iterator& operator-=(difference_type n)
		{
			return *this += -n;
		}

		Iterator operator-(difference_type n)
		{
			Iterator tmp = *this;
			return tmp -= n;
		}

		difference_type operator-(Iterator const& other)
		{
			return ((SIZE - other.m_end) + m_end) % SIZE;
		}

		Iterator operator[](size_t n)
		{
			return *this + n;
		}
	};

	RingBuffer() = default;
	~RingBuffer() = default;

	void push(T value)
	{
		m_buffer[m_last_idx++] = value;
		constexpr size_t size = SIZE;
		m_last_idx %= size;
	}

	auto begin()
	{
		return Iterator(&m_buffer[0], m_last_idx + 1);
	}

	auto end()
	{
		return Iterator(&m_buffer[0], m_last_idx);
	}

private:

	std::array<T, SIZE> m_buffer{ 0 };
	size_t m_last_idx{ 0 };
};