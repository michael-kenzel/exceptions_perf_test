#ifndef INCLUDED_DYNAMIC_ARRAY
#define INCLUDED_DYNAMIC_ARRAY

#pragma once

#include <type_traits>
#include <limits>
#include <cstddef>
#include <utility>
#include <initializer_list>
#include <new>
#include <memory>
#include <algorithm>


template <typename T>
class dynamic_array
{
public:
	using value_type = T;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	constexpr size_type max_size() const noexcept
	{
		return std::numeric_limits<difference_type>::max();
	}

private:
	struct element_storage_t
	{
		union
		{
			T v;
		};

		element_storage_t() = default;

		template <typename A>
		element_storage_t& operator =(A&& a)
		{
			construct(std::forward<A>(a));
			return *this;
		}

		template <typename... Args>
		T* construct(Args&&... args) noexcept
		{
			static_assert(std::is_nothrow_constructible_v<T, Args&&...>);
			return new (this) T(std::forward<Args>(args)...);
		}

		void destruct() noexcept
		{
			static_assert(std::is_nothrow_destructible_v<T>);
			v.~T();
		}
	};

	std::unique_ptr<element_storage_t[]> buffer;
	size_type num_elements = 0;
	size_type max_num_elements = 0;

	static auto allocStorage(size_type size) noexcept
	{
		return std::unique_ptr<element_storage_t[]> { new (std::nothrow) element_storage_t[size] };
	}

	void destroyContent() noexcept
	{
		for (auto p = &buffer[0] + num_elements; p >= &buffer[0]; --p)
			p->destruct();
	}

	void moveContent(std::unique_ptr<element_storage_t[]>&& new_buffer) noexcept
	{
		if constexpr (std::is_nothrow_move_constructible_v<T>)
		{
			std::move(&buffer[0], &buffer[0] + num_elements, &new_buffer[0]);
		}
		else
		{
			static_assert(std::is_nothrow_copy_constructible_v<T>);
			std::copy(&buffer[0], &buffer[0] + num_elements, &new_buffer[0]);
			destroyContent();
		}

		buffer = std::move(new_buffer);
	}

	size_type expandCapacity(size_type new_size) const noexcept
	{
		if (max_num_elements > max_size() - max_num_elements / 2)
			return max_size();
		return std::max(max_num_elements + max_num_elements / 2, new_size);
	}

	[[nodiscard]]
	bool grow(size_type new_size) noexcept
	{
		if (new_size > max_num_elements)
		{
			if (new_size > max_size())
				return false;
			size_type new_capacity = expandCapacity(new_size);
			auto new_buffer = allocStorage(new_capacity);
			if (!new_buffer)
				return false;
			moveContent(std::move(new_buffer));
			max_num_elements = new_capacity;
		}

		num_elements = new_size;

		return true;
	}

public:
	dynamic_array() = default;

	dynamic_array(dynamic_array&&) = default;

	dynamic_array& operator =(dynamic_array&&) = default;

	~dynamic_array()
	{
		destroyContent();
	}

	template <typename... Args>
	[[nodiscard]]
	bool emplace_back(Args&&... args) noexcept
	{
		if (!grow(num_elements + 1))
			return false;
		buffer[num_elements - 1].construct(std::forward<Args>(args)...);
		return true;
	}

	[[nodiscard]]
	bool push_back(const T& v) noexcept
	{
		return emplace_back(v);
	}

	const T& operator [](size_type i) const noexcept
	{
		return buffer[i].v;
	}

	T& operator [](size_type i) noexcept
	{
		return buffer[i].v;
	}

	size_type size() const noexcept
	{
		return num_elements;
	}

	size_type capacity() const noexcept
	{
		return max_num_elements;
	}

	friend size_type size(const dynamic_array& arr) noexcept
	{
		return arr.num_elements;
	}
};

#endif  // INCLUDED_DYNAMIC_ARRAY
