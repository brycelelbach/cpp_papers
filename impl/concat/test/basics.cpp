#include "concat.hpp"
#include <catch2/catch_test_macros.hpp>
#include <list>
#include <vector>
#include <functional>
#include <memory>
#include <numeric>

#define TEST_POINT(x) TEST_CASE(x, "[basics]")

namespace {

template <typename... R>
concept concat_viewable = requires(R&&... r) {
    std::ranges::views::concat((R &&) r...);
};


template <typename R>
R f(int);

template <typename R>
using make_view_of = decltype(std::views::iota(0) | std::views::transform(f<R>));

struct Foo {};

struct Bar : Foo {};
struct Qux : Foo {};

struct MoveOnly {
    MoveOnly(MoveOnly const&) = delete;
    MoveOnly& operator=(MoveOnly const&) = delete;
    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(MoveOnly&&) = default;
};

static_assert(std::movable<MoveOnly>);
static_assert(!std::copyable<MoveOnly>);

struct BigCopyable {
    int bigdata;
};


} // namespace



TEST_POINT("motivation") {
    using V = std::vector<int>;
    V v1{1, 2, 3}, v2{4, 5};
    std::ranges::concat_view cv{v1, v2};
    // static_assert(std::ranges::range<decltype(cv)>);
    REQUIRE(std::ranges::size(cv) == 5);
}



TEST_POINT("concept") {

    using IntV = std::vector<int>;
    using IntL = std::list<int>;
    using FooV = std::vector<Foo>;
    using BarV = std::vector<Bar>;
    using QuxV = std::vector<Qux>;

    // single arg
    STATIC_CHECK(concat_viewable<IntV&>);
    STATIC_CHECK(!concat_viewable<IntV>); // because:
    STATIC_REQUIRE(!std::ranges::viewable_range<IntV>);
    STATIC_REQUIRE(std::ranges::viewable_range<IntV&>);

    // nominal use
    STATIC_CHECK(concat_viewable<IntV&, IntV&>);
    STATIC_CHECK(concat_viewable<IntV&, IntV const&>);
    STATIC_CHECK(concat_viewable<IntV&, std::vector<std::reference_wrapper<int>>&>);
    STATIC_CHECK(concat_viewable<IntV&, IntL&, IntV&>);
    STATIC_CHECK(concat_viewable<FooV&, BarV&>);
    STATIC_CHECK(concat_viewable<BarV&, FooV&>);
    STATIC_CHECK(concat_viewable<FooV&, BarV&, QuxV const&>);
    STATIC_CHECK(concat_viewable<IntV&, make_view_of<int&>>);
    STATIC_CHECK(concat_viewable<make_view_of<int&>, make_view_of<int&&>, make_view_of<int>>);
    STATIC_CHECK(concat_viewable<make_view_of<MoveOnly>, make_view_of<MoveOnly>&&>);


    // invalid concat use:
    STATIC_CHECK(!concat_viewable<>);
    STATIC_CHECK(!concat_viewable<IntV&, FooV&>);

    // common_reference_t is valid. but it is a prvalue which the 2nd range (lvalue ref) can not
    // assign to (needs copyable).
    // STATIC_CHECK(!concat_viewable<make_view_of<MoveOnly>, make_view_of<MoveOnly&>>);

    // Flag:
    STATIC_CHECK(!concat_viewable<BarV&, QuxV&, FooV&>);
    //    maybe a separate proposal for an explicitly specified value_type range?
    //    ref_t == Foo& would work just fine if it wasn't common_reference_t logic.

    // Flag:
    STATIC_CHECK(concat_viewable<make_view_of<BigCopyable>, make_view_of<BigCopyable&>>);
    //    common_reference_t is BigCopyable (a temporary). 2nd range has BigCopyable& type.
    //    so that means operator* will copy an lvalue to a temporary: a valid but most likely a
    //    useless operation. Should this be ignored as programmer error and silently accepted?
    //    Trouble is it may be too subtle to notice yet common.
    //    [TODO] an example with a transformed range that returns a value from a lambda, but meant
    //           to return a reference). Is there a better  solution, diagnostic, documentation at
    //           least?
    //    [TODO] mention in Design.
}



TEST_POINT("begin_basic") {

    using V = std::vector<int>;
    V v1{}, v2{4, 5}, v3{6};
    std::ranges::concat_view cv{v1, v2, v3};
    auto it = cv.begin();
    static_assert(std::same_as<decltype(*it), int&>);
    REQUIRE(*it == 4);
}

TEST_POINT("end_basic_common_range") {
    using V = std::vector<int>;
    V v1{}, v2{4, 5}, v3{6};
    std::ranges::concat_view cv{v1, v2, v3};
    auto it = cv.begin();
    auto st = cv.end();
    static_assert(std::same_as<decltype(it), decltype(st)>);
    auto it2 = std::as_const(cv).begin();
    auto st2 = std::as_const(cv).end();
    static_assert(std::same_as<decltype(it2), decltype(st2)>);
}

TEST_POINT("operator++") {
    using V = std::vector<int>;
    V v1{}, v2{4, 5}, v3{} ,v4{6};
    std::ranges::concat_view cv{v1, v2, v3, v4};
    auto it = cv.begin();
    auto st = cv.end();

    REQUIRE(*it == 4);
    ++it;
    REQUIRE(*it == 5);
    ++it;
    REQUIRE(*it == 6);
    ++it;
    REQUIRE(it == st);
}

TEST_POINT("compare with unreachable sentinel"){
    std::vector v{1};
    std::ranges::concat_view cv{v, std::views::iota(0)};
    auto it = std::ranges::begin(cv);
    auto st = std::ranges::end(cv);
    REQUIRE(it!=st);

    ++it;
    REQUIRE(it!=st);

    ++it;
    REQUIRE(it!=st);

    ++it;
    REQUIRE(it!=st);
}


TEST_POINT("compare with reachable sentinel"){
    std::vector v{1};
    std::ranges::concat_view cv{v, std::ranges::iota_view<int, size_t>(0, 2)};

    auto it = std::ranges::begin(cv);
    auto st = std::ranges::end(cv);
    REQUIRE(it!=st);

    ++it;
    REQUIRE(it!=st);

    ++it;
    REQUIRE(it!=st);

    ++it;
    REQUIRE(it==st);
}

constexpr int constexp_test(){
    std::ranges::concat_view cv{std::views::iota(0,5), std::views::iota(3,7)};
    return std::accumulate(cv.begin(), cv.end(), 0);
}

TEST_POINT("constexpr") {
    // Question: in libcxx, std::variant is not constexpr yet. can you try msvc stl to compile the following line?
    //static_assert(constexp_test()==28);
}

TEST_POINT("Sentinel") {
    // using V = std::vector<int>;
    // using W = std::list<int>;

    // add non-trivial combinations of underlying ranges/views and concept checks for
    // - sentinel size independent of number of ranges
    // - sentinel cross-const comparison
    // - sentinel being default constructible or not mirroring on last view's property
    // - ...
}