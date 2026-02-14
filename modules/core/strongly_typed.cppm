export module penumbra.core:strongly_typed;

import std;
using std::uint32_t, std::uint64_t, std::size_t;

export namespace penumbra
{
        template <typename T, typename tag>
        struct strongly_typed
        {
                strongly_typed() = default;

                explicit constexpr strongly_typed(const T& value) noexcept(std::is_nothrow_copy_constructible<T>::value) : storage(value) {}
                explicit constexpr strongly_typed(T&& value) noexcept(std::is_nothrow_move_constructible<T>::value) : storage(std::move(value)) {}

                constexpr operator const T&() const
                {
                        return storage;
                }

                constexpr operator T&()
                {
                        return storage;
                }

                constexpr T get_storage() const
                {
                        return storage;
                }
        private:
                T storage;
        };
}

export template <typename T, typename tag>
struct std::hash<penumbra::strongly_typed<T, tag>>
{
	size_t operator()(const penumbra::strongly_typed<T, tag>& input) const noexcept
	{
		std::hash<T> hobj;
		return hobj(input.get_storage());
	}
};
