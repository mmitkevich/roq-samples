
#pragma once
#include <cstdint>
#include <cmath>
#include <limits>

#include <roq/side.h>
#include "roq/utils/compare.h"
#include "roq/numbers.h"
#include "roq/format.h"

#include "roq/shared/price.h"

namespace roq {
inline namespace shared {

//! Price and Volume
struct Quote {
  Quote() = default;
  Quote(Side side, price_t price, volume_t quantity = 0)
  : side (side)
  , price (price)
  , quantity (quantity)
  {}
  Quote(price_t price, volume_t quantity = 0)
  : side (Side::UNDEFINED)
  , price (price)
  , quantity (quantity)
  {}

  void reset() {
    side = Side::UNDEFINED;
    price = undefined_price;
    quantity = 0.;
  }

  bool empty() const { return is_undefined_price(price) || utils::compare(quantity, 0.) == 0; }
  
  friend inline bool operator==(const Quote& lhs, const Quote& rhs) { 
    return utils::compare(lhs.price, rhs.price) == 0 && 
            utils::compare(lhs.quantity, rhs.quantity) == 0; 
  }
  Side side = Side::UNDEFINED;
  price_t price = undefined_price;  //!< limit price
  volume_t quantity {0.};    //!< total quantity
};

template<class Quote>
struct SingleQuote {
  SingleQuote(const Quote& quote): quote(quote) {}

  price_t price() const { return quote.price; }
  void set_price(price_t price) { quote.price = price; }
  volume_t quantity() const { return quote.quantity; }
  void set_quantity(volume_t val) { quote.quantity = val; }

  bool empty() const { return quote.empty(); }
  std::size_t size() const { return quote.empty() ? 0 : 1;}
  Quote& operator[](std::size_t index) { return quote; }
  const Quote& operator[](std::size_t index) const { return quote; }

  Quote quote;
};

} // namespace shared
} // namespace roq


template <>
struct fmt::formatter<roq::shared::Quote> : public roq::formatter {
  template <typename Context>
  auto format(const roq::shared::Quote &value, Context &context) {
    using namespace roq::literals;
    return roq::format_to(context.out(), "{}:{}"_fmt, value.price, value.quantity);
  }
};
