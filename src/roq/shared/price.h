#pragma once
#include <cmath>
#include "roq/numbers.h"
#include "roq/utils/compare.h"

namespace roq {
inline namespace shared {

constexpr auto Eps = 1E-30; // TODO: std::numeric_limits:XXX

using price_t = double;
using volume_t = double;

constexpr static price_t undefined_price = NaN;
constexpr inline static bool is_undefined_price(price_t price) { return std::isnan(price); }

template<int DIR> 
price_t price_top(price_t x, price_t y) {
  if constexpr(DIR>0) {
    return std::max(x,y);
  } else {
    return std::min(x,y);
  }
}
template<int DIR> 
price_t price_bottom(price_t x, price_t y) {
  if constexpr(DIR>0) {
    return std::min(x,y);
  } else {
    return std::max(x,y);
  }
}

inline bool price_undefined(price_t x) { return std::isnan(x); }

// compare by priority in the book
template<int DIR>
int price_compare(price_t x, price_t y) {
  if constexpr(DIR>0) {
    return utils::compare(y,x); 
  } else {
    return utils::compare(x,y);
  }
}

template<int DIR>
price_t price_round_bottom(price_t x, price_t tick_size) {
  if constexpr(DIR>0) {
    return std::floor(x/tick_size)*tick_size;
  } else {
    return std::ceil(x/tick_size)*tick_size;
  }
}


} // namespace shared
} // namespace roq