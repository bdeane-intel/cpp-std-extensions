#pragma once

#include <algorithm>
#include <array>
#if __has_include(<bit>)
#include <bit>
#endif
#if __has_include(<concepts>)
#include <concepts>
#endif
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <limits>
#if __has_include(<span>)
#include <span>
#endif
#include <type_traits>
#include <utility>

namespace reng {

#if defined(__clang__)
#define MUSTTAIL [[clang::musttail]]
#else
#define MUSTTAIL
#endif

#if __cpp_lib_concepts < 202002L
template <typename From, typename To>
concept convertible_to = true;
template <typename T, typename U>
concept same_as = true;

template <typename I>
concept forward_iterator = true;
template <typename S, typename I>
concept sentinel_for = true;
template <typename I, typename T>
concept output_iterator = true;

template <typename T>
concept unsigned_integral = true;
#else
template <typename From, typename To>
concept convertible_to = std::convertible_to<From, To>;
template <typename T, typename U>
concept same_as = std::same_as<T, U>;

template <typename I>
concept forward_iterator = std::forward_iterator<I>;
template <typename S, typename I>
concept sentinel_for = std::sentinel_for<S, I>;
template <typename I, typename T>
concept output_iterator = std::output_iterator<I, T>;
#endif

#if __cpp_lib_int_pow2 < 202002L
template <unsigned_integral T>
constexpr auto has_single_bit(T t) noexcept -> bool {
    return t != 0 and (t & (t - 1)) == 0;
}

template <unsigned_integral T> constexpr auto bit_width(T t) noexcept -> int {
    int width{};
    while (t > 0) {
        t >>= 1u;
        ++width;
    }
    return width;
}
#else
using std::bit_width;
using std::has_single_bit;
#endif

#if __cpp_lib_span < 202002L
template <typename T, std::size_t N> struct span {
    template <typename It>
    constexpr span(It first, It last) noexcept
        : ptr{&*first}, size{static_cast<std::size_t>(
                            std::distance(first, last))} {}

    template <typename It>
    constexpr span(It first, std::size_t n) noexcept : ptr{&*first}, size{n} {}

    constexpr auto data() const noexcept -> T * { return ptr; }

    T *ptr;
    std::size_t size;
};
#else
template <typename T, std::size_t N> using span = std::span<T, N>;
#endif

#if __cpp_lib_ranges < 201811L
template <typename T>
using iter_value_t =
    typename std::iterator_traits<std::remove_cvref_t<T>>::value_type;
#else
template <typename T> using iter_value_t = std::iter_value_t<T>;
#endif

// An index config contains four things:
// - branch factor: the factor by which level N differs from level N-1
// - unroll factor: when computing a index update, how to unroll the loop
// - binary operation: how to compute the better of two values
// - initial_value: the unit value for the binary operation

template <typename T> struct min_index_config {
    [[nodiscard]] constexpr auto branch_factor() const
        -> std::integral_constant<std::size_t, 4>;
    [[nodiscard]] constexpr auto unroll_factor() const
        -> std::integral_constant<int, 2>;
    [[nodiscard]] constexpr auto operator()(T x, T y) const -> T {
        return std::min(x, y);
    }
    constexpr static auto initial_value = std::numeric_limits<T>::max();
};

template <typename C, typename T>
concept index_config_for =
    requires(C conf, T value) {
        { conf.branch_factor().value } -> convertible_to<std::size_t>;
        { conf.branch_factor().value } -> convertible_to<std::size_t>;
        { conf.unroll_factor().value } -> convertible_to<int>;
        { conf(value, value) } -> same_as<T>;
        requires same_as<T, std::remove_const_t<decltype(C::initial_value)>>;
    };

template <typename Config>
constexpr auto branch_factor =
    decltype(std::declval<Config>().branch_factor())::value;

template <typename Config>
constexpr auto unroll_factor =
    decltype(std::declval<Config>().unroll_factor())::value;

// Given an array size N, the index size is the space required to store the
// rolled-up results according to the branch factor.

template <std::size_t N, std::size_t BranchFactor>
    requires(N % BranchFactor == 0)
constexpr auto index_size = N / BranchFactor
                            + index_size<N / BranchFactor, BranchFactor>;

template <std::size_t N> constexpr auto index_size<N, N> = std::size_t{1};

// A round zips the odd and even elements of a range using the op, producing an
// output half the size of the input. The loop is unrolled according to
// UnrollFactor.

template <int UnrollFactor, forward_iterator I, sentinel_for<I> S,
          output_iterator<iter_value_t<I>> O, typename BinOp>
constexpr auto compute_index_round(I first, S last, O dst, BinOp op) -> O {
    while (first != last) {
        [&]<auto... Is>(std::integer_sequence<int, Is...>) {
            auto const compute = [&]() -> void {
                *dst++ = op(*first, *std::next(first));
                std::advance(first, 2);
            };
            ((static_cast<void>(Is), compute()), ...);
        }
        (std::make_integer_sequence<int, UnrollFactor>{});
    }
    return dst;
}

// A branch reduces a range of N elements to one element by rounds.

template <int UnrollFactor, typename T, std::size_t N, typename BinOp>
    requires(has_single_bit(N))
constexpr auto compute_index_branch(span<T, N> input, BinOp op) -> T {
    auto first = std::data(input);
    auto last = std::next(first, N);

    std::array<std::remove_const_t<T>, N - 2> intermediate{};
    auto dst = std::data(intermediate);
    for (auto i = bit_width(N) - 2; i > 0; --i) {
        first = std::exchange(
            dst, compute_index_round<UnrollFactor>(first, last, dst, op));
        last = dst;
    }

    return op(*first, *(std::next(first)));
}

// indexed_array<T, N> behaves like std::array<T, N>, except:
// - non-const iteration is not available, i.e. begin/end/rbegin/rend always
//   return const_iterator and range-for may observe const elements only.
// - operations that return a (mutable) reference instead return a proxy object
//   that supports assignment.
// - at<I> is provided for compile-time indexing.
// - an index is kept according to the config. Assigning to
//   a.at<I>()/a[n]/a.front()/a.back() incurs a log(N) index bookkeepping
//   computation.
// - best() returns the current best value - according to config - in O(1).
// - indexed_array is not an aggregate.
//
// Note:
// - indexed_array reports its size as N, although it has extra storage
//   which is not observable.
// - indexed_array models std::regular. But note that although indexed_array
//   behaves like std::array, it is not comparable with std::array.
// - fill and swap also work as on std::array.
// - other operations on std::array are currently unsupported.
// - indexing into the unobservable storage is unchecked, out of contract and
//   therefore UB.

template <typename T, std::size_t N, index_config_for<T> Config>
class indexed_array
    : public std::array<T, N + index_size<N, branch_factor<Config>>> {
    using base_t = std::array<T, N + index_size<N, branch_factor<Config>>>;
    constexpr static auto step = branch_factor<Config>;
    constexpr static auto mask = ~(step - 1u);

    template <std::size_t I> struct proxy {
        constexpr auto operator=(T const &value) -> proxy & {
            static_cast<base_t &>(arr)[I] = value;
            arr.compute_update<I>();
            return *this;
        }
        constexpr auto operator=(T &&value) -> proxy & {
            static_cast<base_t &>(arr)[I] = std::move(value);
            arr.compute_update<I>();
            return *this;
        }

        // NOLINTNEXTLINE(google-explicit-constructor)
        constexpr explicit(false) operator T() const {
            return static_cast<base_t const &>(arr)[I];
        }

        indexed_array &arr;
    };

    struct rt_proxy {
        constexpr auto operator=(T const &value) -> rt_proxy & {
            static_cast<base_t &>(arr)[index] = value;
            arr.compute_update(index);
            return *this;
        }
        constexpr auto operator=(T &&value) -> rt_proxy & {
            static_cast<base_t &>(arr)[index] = std::move(value);
            arr.compute_update(index);
            return *this;
        }

        // NOLINTNEXTLINE(google-explicit-constructor)
        constexpr explicit(false) operator T() const {
            return static_cast<base_t const &>(arr)[index];
        }

        indexed_array &arr;
        std::size_t index;
    };

    template <std::size_t SrcOffset, std::size_t DstOffset, std::size_t DstSize>
    constexpr static auto compute_update(auto src_base, auto dst_base) -> void {
        if constexpr (DstSize != 0) {
            auto const src = std::next(src_base, SrcOffset);
            auto const dst = std::next(dst_base, DstOffset);
            *dst = compute_index_branch<unroll_factor<Config>>(
                span<T, step>{src, step}, Config{});

            constexpr auto new_src_offset = DstOffset & mask;
            constexpr auto new_dst_offset = new_src_offset / step;
            constexpr auto new_dst_size = DstSize / step;

            MUSTTAIL return compute_update<new_src_offset, new_dst_offset,
                                           new_dst_size>(
                dst_base, std::next(dst_base, DstSize));
        }
    }

    template <std::size_t I> constexpr auto compute_update() -> void {
        constexpr auto src_offset = I & mask;
        constexpr auto dst_offset = src_offset / step;
        constexpr auto dst_size = N / step;
        auto const src_base = std::begin(static_cast<base_t &>(*this));
        auto const dst_base = std::next(src_base, N);
        compute_update<src_offset, dst_offset, dst_size>(src_base, dst_base);
    }

    constexpr auto compute_update(std::size_t index) -> void {
        auto src_offset = index & mask;
        auto dst_offset = src_offset / step;
        auto dst_size = N / step;
        auto src_base = std::begin(static_cast<base_t &>(*this));
        auto dst_base = std::next(src_base, N);
        using diff_t =
            typename std::iterator_traits<decltype(src_base)>::difference_type;

        while (dst_size != 0) {
            auto const src =
                std::next(src_base, static_cast<diff_t>(src_offset));
            auto const dst =
                std::next(dst_base, static_cast<diff_t>(dst_offset));
            *dst = compute_index_branch<unroll_factor<Config>>(
                span<T, step>{src, step}, Config{});

            src_base = dst_base;
            std::advance(dst_base, dst_size);
            dst_size /= step;

            src_offset = dst_offset & mask;
            dst_offset = src_offset / step;
        }
    }

    constexpr auto compute_full_index() -> void {
        auto num_steps = N / step;
        auto first = std::begin(static_cast<base_t &>(*this));
        auto dst = std::next(first, N);
        while (num_steps > 0) {
            for (auto i = std::size_t{}; i != num_steps; ++i) {
                *dst++ = compute_index_branch<unroll_factor<Config>>(
                    span<T, step>{first, step}, Config{});
                std::advance(first, step);
            }
            num_steps /= step;
        }
    }

  public:
    constexpr indexed_array() {
        static_cast<base_t &>(*this).fill(Config::initial_value);
    }

    constexpr indexed_array(std::initializer_list<T> ilist) : base_t{} {
        auto i = static_cast<base_t &>(*this).begin();
        for (auto item : ilist) {
            *i++ = item;
        }
        while (i != static_cast<base_t &>(*this).end()) {
            *i++ = Config::initial_value;
        }
        compute_full_index();
    }

    [[nodiscard]] constexpr auto best() const noexcept ->
        typename base_t::const_reference {
        return static_cast<base_t const &>(*this).back();
    }

    [[nodiscard]] constexpr auto size() const noexcept ->
        typename base_t::size_type {
        return N;
    }
    [[nodiscard]] constexpr auto max_size() const noexcept ->
        typename base_t::size_type {
        return N;
    }

    [[nodiscard]] constexpr auto data() const noexcept ->
        typename base_t::const_pointer {
        return base_t::data();
    }

    [[nodiscard]] constexpr auto front() noexcept -> proxy<0> {
        return {*this};
    }
    [[nodiscard]] constexpr auto front() const noexcept ->
        typename base_t::const_reference {
        return *begin();
    }

    [[nodiscard]] constexpr auto back() noexcept -> proxy<N - 1> {
        return {*this};
    }
    [[nodiscard]] constexpr auto back() const noexcept ->
        typename base_t::const_reference {
        return *std::next(begin(), N - 1);
    }

    [[nodiscard]] constexpr auto begin() const noexcept ->
        typename base_t::const_iterator {
        return std::begin(static_cast<base_t const &>(*this));
    }
    [[nodiscard]] constexpr auto end() const noexcept ->
        typename base_t::const_iterator {
        return std::next(begin(), N);
    }
    [[nodiscard]] constexpr auto cend() const noexcept ->
        typename base_t::const_iterator {
        return std::next(std::cbegin(*this), N);
    }

    [[nodiscard]] constexpr auto rbegin() const noexcept {
        return typename base_t::const_reverse_iterator{
            std::next(std::begin(*this), N)};
    }
    [[nodiscard]] constexpr auto crbegin() const noexcept {
        return typename base_t::const_reverse_iterator{
            std::next(std::begin(*this), N)};
    }

    [[nodiscard]] constexpr auto operator[](typename base_t::size_type pos)
        -> rt_proxy {
        return {*this, pos};
    }
    [[nodiscard]] constexpr auto
    operator[](typename base_t::size_type pos) const ->
        typename base_t::const_reference {
        return static_cast<base_t const &>(*this)[pos];
    }

    template <std::size_t I>
        requires(I < N)
    [[nodiscard]] constexpr auto at() const ->
        typename base_t::const_reference {
        return static_cast<base_t const &>(*this)[I];
    }
    template <std::size_t I>
        requires(I < N)
    [[nodiscard]] constexpr auto at() -> proxy<I> {
        return {*this};
    }
};

template <typename T, std::size_t N>
using min_indexed_array = indexed_array<T, N, min_index_config<T>>;

} // namespace reng
