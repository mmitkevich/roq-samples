#pragma once

#include <algorithm>
namespace roq {
namespace ranges {

template <typename T>
struct reverse_view { T& base_;  T base() const { return base_; }};
template <typename T>
auto begin (reverse_view<T> view) { return std::rbegin(view.base_); }
template <typename T>
auto end (reverse_view<T> view) { return std::rend(view.base_); }
template <typename T>
reverse_view<T> reverse (T&& base) { return { base }; }

} // namespace ranges
} // namespace roq
