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

namespace detail {
template<class Iterator, class Fn>
struct TransformedIterator {
  
  TransformedIterator(Iterator it, Fn&& fn):iterator_(it), fn_(fn) {}
  auto operator*(){ return fn(*iterator_); }
  auto operator++(){ iterator_++; return *this; }
  auto operator--(){ iterator_--; return *this; }
  auto operator->() { return fn(*iterator_); }

  Iterator iterator_;
  Fn fn_;
};
} // namespace detail

template<class Iterator, class Fn>
auto transform_iterator(Iterator iterator, Fn&&fn) {
  return detail::TransformedIterator<Iterator, Fn>(iterator, std::move(fn));
}

} // namespace ranges
} // namespace roq
