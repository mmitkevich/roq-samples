
#pragma once
#include <cstdint>
#include <cmath>
#include <initializer_list>
#include <iterator>
#include <limits>

#include <roq/side.h>
#include "roq/utils/compare.h"
#include "roq/numbers.h"
#include "roq/format.h"

#include "roq/shared/price.h"

namespace roq {
inline namespace shared {



inline int to_dir(Side side) {
  switch(side) {
    case Side::BUY: return 1;
    case Side::SELL: return -1;
    default: assert(false); return 0;
  }
}

//! Price and Volume
struct Quote {
  Quote() = default;

  void reset() {
    side = Side::UNDEFINED;
    price = undefined_price;
    quantity = 0.;
  }

  Side get_side() const { return side; }

  price_t get_price() const { return price; }
  void set_price(price_t val) { price = val; }

  volume_t get_quantity() const { return quantity; }
  void set_quantity(volume_t val) { quantity = val; }

  std::size_t size() const { return 1; }
  Quote& operator[](std::size_t index) { return *this; }
  
  bool empty() const { return is_undefined_price(price) || utils::compare(quantity, 0.) == 0; }
  
  friend inline bool operator==(const Quote& lhs, const Quote& rhs) { 
    return utils::compare(lhs.price, rhs.price) == 0 && 
            utils::compare(lhs.quantity, rhs.quantity) == 0; 
  }

  Side side = Side::UNDEFINED;
  price_t price = undefined_price;  //!< limit price
  volume_t quantity {0.};    //!< total quantity
};

struct GridQuotes : Quote {
  std::size_t size() const { return std::ceil(quote.quantity/tick.quantity); }
  
  bool empty() const { return size()==0; }

  Quote operator[](std::size_t i) const {
    int dir = to_dir(side);
    price_t price_b = dir>0 ? std::floor(price/tick.price)*tick.price : std::ceil(price/tick.price)*tick.price;
    price_b -= dir*i*tick.price;
    auto result = Quote {
      .side = side,
      .price = i==0 ? price: price_b,
      .quantity = i==0 ? quantity - (size()-1)*tick.quantity : tick.quantity};
    return result;
  }
  
  Quote tick = Quote {.price = INFINITY, .quantity=0};
};
///
template<uint32_t MAX_SIZE>
struct Quotes {
  Quotes(std::initializer_list<Quote>&& vals) {
    size_ = vals.size();
    if(vals.size()<MAX_SIZE)
      std::copy(vals.begin(),vals.end(), std::begin(data_));
    else 
      std::copy(vals.begin(),vals.begin()+MAX_SIZE, std::begin(data_));
  }

  price_t price() const {
    if(empty())
      return undefined_price;
    else
      return data_[0].price;
  }
  
  volume_t quantity() const {
    volume_t qty = 0;
    for(auto& q:data_) {
      qty += q.quantity;
    }
    return qty;
  }
  
  void set_price(price_t price) {
    if(!empty()) {
      price_t diff = price - data_[0].price;
      for(auto& quote: data_) {
        quote.price += diff;
      }
    } else {
      size_ = 1;
      data_[0].price = price;
    }
  }
  
  void set_quantity(volume_t quantity) {
    if(!empty()) {
      volume_t diff = quantity - this->quantity();
      for(std::size_t i=0; i<size_; i++) {
        data_[i].quantity += diff;
      }
    } else {
      size_=1;
      data_[0].quantity = quantity;
    }
  }
  bool empty() const { return data_.empty(); }
  std::size_t size() const { return size_; }
  Quote& operator[](std::size_t index) { return data_[index]; }
  const Quote& operator[](std::size_t index) const { return data_[index]; }

private:
  std::size_t size_{};
  std::array<Quote, MAX_SIZE> data_;
};

template<uint32_t MAX_SIZE>
auto make_quotes(std::initializer_list<Quote>&& items) {
  assert(items.size()<=MAX_SIZE);
  return Quotes<MAX_SIZE>(std::move(items));
}
auto make_empty_quotes() {
  return Quotes<0>({});
}

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

template <uint32_t MAX_SIZE>
struct fmt::formatter<roq::shared::Quotes<MAX_SIZE>> : public roq::formatter {
  template <typename Context>
  auto format(const roq::shared::Quotes<MAX_SIZE> &value, Context &context) {
    using namespace roq::literals;
    return roq::format_to(context.out(), "{}:{}"_fmt, value.price(), value.quantity());
  }
};

template <>
struct fmt::formatter<roq::shared::GridQuotes> : public roq::formatter {
  template <typename Context>
  auto format(const roq::shared::GridQuotes &value, Context &context) {
    using namespace roq::literals;
    return roq::format_to(context.out(), "{}:{}"_fmt, value.price(), value.quantity());
  }
};