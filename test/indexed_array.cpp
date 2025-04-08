#include <reng/indexed_array.hpp>

#include <gtest/gtest.h>

#if __has_include(<concepts>)
#include <concepts>
#endif
#include <cstddef>
#include <iterator>

TEST(indexed_array_test, index_config) {
    static_assert(reng::index_config_for<reng::min_index_config<int>, int>);
}

TEST(indexed_array_test, index_size_for) {
    static_assert(reng::index_size<4u, 4u> == 1u);
    static_assert(reng::index_size<16u, 4u> == 4u + 1u);
    static_assert(reng::index_size<64u, 4u> == 16u + 4u + 1u);
}

TEST(indexed_array_test, compute_index_round) {
    std::array input{1, 2, 3, 4};
    std::array<int, 2> output{};
    constexpr auto op = [](int x, int y) -> int { return std::min(x, y); };

    auto const o = reng::compute_index_round<2>(
        std::cbegin(input), std::cend(input), std::begin(output), op);
    EXPECT_EQ(o, std::end(output));
    EXPECT_EQ(output, (std::array{1, 3}));
}

TEST(indexed_array_test, compute_index_branch) {
    constexpr auto op = [](int x, int y) -> int { return std::min(x, y); };
    {
        constexpr std::array input{2, 3, 1, 4};
        auto const result = reng::compute_index_branch<2>(
            reng::span<int const, 4>{std::cbegin(input), std::cend(input)}, op);
        EXPECT_EQ(result, 1);
    }
    {
        constexpr std::array input{2, 3, 1, 4, 8, 5, 6, 7};
        auto const result = reng::compute_index_branch<2>(
            reng::span<int const, 4>{std::cbegin(input), std::cend(input)}, op);
        EXPECT_EQ(result, 1);
    }
}

TEST(indexed_array_test, array_types) {
    using array_t = reng::min_indexed_array<int, 4>;
    static_assert(reng::same_as<typename array_t::value_type, int>);
    static_assert(reng::same_as<typename array_t::size_type, std::size_t>);
    static_assert(
        reng::same_as<typename array_t::difference_type, std::ptrdiff_t>);
    static_assert(reng::same_as<typename array_t::reference, int &>);
    static_assert(
        reng::same_as<typename array_t::const_reference, int const &>);
    static_assert(reng::same_as<typename array_t::pointer, int *>);
    static_assert(reng::same_as<typename array_t::const_pointer, int const *>);
}

TEST(indexed_array_test, const_array_functions) {
    const reng::min_indexed_array<int, 4u> input{};
    EXPECT_EQ(input.size(), 4u);
    EXPECT_EQ(input.max_size(), 4u);
    EXPECT_FALSE(input.empty());
}

TEST(indexed_array_test, const_begin_end) {
    const reng::min_indexed_array<int, 4u> input{1, 2, 3, 4};
    EXPECT_EQ(input[0], 1);
    EXPECT_EQ(input.front(), 1);
    EXPECT_EQ(*input.begin(), 1);
    EXPECT_EQ(*input.cbegin(), 1);
    EXPECT_EQ(*input.data(), 1);

    EXPECT_EQ(input[3], 4);
    EXPECT_EQ(input.back(), 4);
    EXPECT_EQ(input.end(), std::next(std::begin(input), 4));
    EXPECT_EQ(input.cend(), std::next(std::cbegin(input), 4));
    EXPECT_EQ(*input.rbegin(), 4);
    EXPECT_EQ(*input.crbegin(), 4);
}

TEST(indexed_array_test, initial_best) {
    {
        const reng::min_indexed_array<int, 4> input{1, 2, 3, 4};
        EXPECT_EQ(input.best(), 1);
    }
    {
        const reng::min_indexed_array<int, 16> input{
            1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
        EXPECT_EQ(input.best(), 1);
    }
}

TEST(indexed_array_test, const_indexing) {
    const reng::min_indexed_array<int, 4> input{1, 2, 3, 4};
    EXPECT_EQ(input[2], 3);
}

TEST(indexed_array_test, mutable_indexing) {
    {
        reng::min_indexed_array<int, 4> input{1, 2, 3, 4};
        EXPECT_EQ(input.best(), 1);
        input[2] = -1;
        EXPECT_EQ(input[2], -1);
        EXPECT_EQ(input.best(), -1);
    }
    {
        reng::min_indexed_array<int, 16> input{1, 2,  3,  4,  5,  6,  7,  8,
                                               9, 10, 11, 12, 13, 14, 15, 16};
        EXPECT_EQ(input.best(), 1);
        input[2] = -1;
        EXPECT_EQ(input[2], -1);
        EXPECT_EQ(input.best(), -1);
    }
}

TEST(indexed_array_test, mutable_front) {
    {
        reng::min_indexed_array<int, 4> input{1, 2, 3, 4};
        EXPECT_EQ(input.best(), 1);
        input.front() = -1;
        EXPECT_EQ(input.front(), -1);
        EXPECT_EQ(input.best(), -1);
    }
    {
        reng::min_indexed_array<int, 16> input{1, 2,  3,  4,  5,  6,  7,  8,
                                               9, 10, 11, 12, 13, 14, 15, 16};
        EXPECT_EQ(input.best(), 1);
        input.front() = -1;
        EXPECT_EQ(input.front(), -1);
        EXPECT_EQ(input.best(), -1);
    }
}

TEST(indexed_array_test, mutable_back) {
    {
        reng::min_indexed_array<int, 4> input{1, 2, 3, 4};
        EXPECT_EQ(input.best(), 1);
        input.back() = -1;
        EXPECT_EQ(input.back(), -1);
        EXPECT_EQ(input.best(), -1);
    }
    {
        reng::min_indexed_array<int, 16> input{1, 2,  3,  4,  5,  6,  7,  8,
                                               9, 10, 11, 12, 13, 14, 15, 16};
        EXPECT_EQ(input.best(), 1);
        input.back() = -1;
        EXPECT_EQ(input.back(), -1);
        EXPECT_EQ(input.best(), -1);
    }
}

TEST(indexed_array_test, compile_time_at) {
    {
        const reng::min_indexed_array<int, 4> input{1, 2, 3, 4};
        EXPECT_EQ(input.at<2u>(), 3);
    }
    {
        reng::min_indexed_array<int, 4> input{1, 2, 3, 4};
        EXPECT_EQ(input.best(), 1);
        input.at<2u>() = -1;
        EXPECT_EQ(input.at<2u>(), -1);
        EXPECT_EQ(input.best(), -1);
    }
    {
        reng::min_indexed_array<int, 16> input{1, 2,  3,  4,  5,  6,  7,  8,
                                               9, 10, 11, 12, 13, 14, 15, 16};
        EXPECT_EQ(input.best(), 1);
        input.at<7u>() = -1;
        EXPECT_EQ(input.at<7u>(), -1);
        EXPECT_EQ(input.best(), -1);
    }
}

TEST(indexed_array_test, const_iteration) {
    {
        reng::min_indexed_array<int, 4> input{1, 2, 3, 4};
        std::size_t index{};
        for (auto &i : input) {
            static_assert(reng::same_as<decltype(i), int const &>);
            EXPECT_LT(index, 4);
            EXPECT_EQ(i, input[index++]);
        }
    }
    {
        reng::min_indexed_array<int, 4> input{1, 2, 3, 4};
        std::size_t index{};
        for (auto i : input) {
            static_assert(reng::same_as<decltype(i), int>);
            EXPECT_LT(index, 4);
            EXPECT_EQ(i, input[index++]);
        }
    }
}

TEST(indexed_array_test, partial_initialization) {
    const reng::min_indexed_array<int, 4> input{1, 2, 3};
    EXPECT_EQ(input.best(), 1);
}

TEST(indexed_array_test, default_initialization) {
    reng::min_indexed_array<int, 4> input{};
    input[0] = 1;
    EXPECT_EQ(input.best(), 1);
}

TEST(indexed_array_test, equality) {
    auto const a1 = reng::min_indexed_array<int, 4>{1, 2, 3, 4};
    auto const a2 = reng::min_indexed_array<int, 4>{1, 2, 3, 4};
    EXPECT_EQ(a1, a2);
    auto const a3 = reng::min_indexed_array<int, 4>{1, 2, 3};
    EXPECT_NE(a1, a3);
}

namespace test {
#if __cpp_lib_concepts < 202002L
template <typename T>
concept regular = true;
#else
template <typename T>
concept regular = std::regular<T>;
#endif
} // namespace test

TEST(indexed_array_test, regularity) {
    using array_t = reng::min_indexed_array<int, 4>;
    static_assert(test::regular<array_t>);
}

TEST(indexed_array_test, fill) {
    auto a1 = reng::min_indexed_array<int, 4>{1, 2, 3, 4};
    EXPECT_EQ(a1.best(), 1);
    a1.fill(4);
    EXPECT_EQ(a1.best(), 4);
}

TEST(indexed_array_test, swap) {
    auto a1 = reng::min_indexed_array<int, 4>{1, 2, 3, 4};
    auto a2 = reng::min_indexed_array<int, 4>{5, 6, 7, 8};

    using std::swap;
    swap(a1, a2);
    EXPECT_EQ(a1, (reng::min_indexed_array<int, 4>{5, 6, 7, 8}));
    EXPECT_EQ(a2, (reng::min_indexed_array<int, 4>{1, 2, 3, 4}));
}
