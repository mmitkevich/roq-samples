
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
 
  Quote(Side side, price_t price, volume_t volume)
  : side_(side)
  , price_(price)
  , quantity_(volume) {}

  Quote(price_t price, volume_t volume)
  : price_(price)
  , quantity_(volume) {}  

  Quote(price_t price)
  : price_(price) {}  

  void reset() {
    side_ = Side::UNDEFINED;
    price_ = undefined_price;
    quantity_ = 0.;
  }

  Side side() const { return side_; }
  void set_side(Side val) { side_ = val;}

  price_t price() const { return price_; }
  void set_price(price_t val) { price_ = val; }

  volume_t quantity() const { return quantity_; }
  void set_quantity(volume_t val) { quantity_ = val; }

  // vector-like
  std::size_t size() const { return 1; }
  Quote& operator[](std::size_t index) { return *this; }
  const Quote& operator[](std::size_t index)const  { return *this; }
  
  bool empty() const { return is_undefined_price(price_) || utils::compare(quantity_, 0.) == 0; }
  
  friend inline bool operator==(const Quote& lhs, const Quote& rhs) { 
    return utils::compare(lhs.price_, rhs.price_) == 0 && 
            utils::compare(lhs.quantity_, rhs.quantity_) == 0; 
  }

  Side side_ = Side::UNDEFINED;
  price_t price_ = undefined_price;  //!< limit price
  volume_t quantity_ {0.};    //!< total quantity
};

struct GridQuote : Quote {
  using Quote::Quote;
  GridQuote(Quote quote, Quote tick)
  : Quote(quote)
  , tick_(tick) {}

  std::size_t size() const { return std::ceil(quantity_/tick_.quantity_); }
  
  bool empty() const { return size()==0; }

  Quote operator[](std::size_t i) const {
    int dir = to_dir(side_);
    price_t price_b = dir>0 ? std::floor(price_/tick_.price_)*tick_.price_ : std::ceil(price_/tick_.price_)*tick_.price_;
    price_b -= dir*i*tick_.price();
    Quote result {
      this->side_,
      i==0 ? price_: price_b,
      i==0 ? quantity_ - (size()-1)*tick_.quantity_ : tick_.quantity_
    };
    return result;
  }
  const Quote& tick() const { return tick_; }
  auto& set_tick(const Quote& val) { tick_ = val; return *this; }

  Quote tick_ { INFINITY, 0 };
};
///
template<uint32_t MAX_SIZE>
struct QuoteArray {
  QuoteArray(std::initializer_list<Quote>&& vals) {
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
      return data_[0].price();
  }
  
  volume_t quantity() const {
    volume_t qty = 0;
    for(auto& quote : data_) {
      qty += quote.quantity();
    }
    return qty;
  }
  
  void set_price(price_t val) {
    if(!empty()) {
      price_t diff = val - data_[0].price();
      for(auto& quote: data_) {
        quote.set_price(quote.price() + diff);
      }
    } else {
      size_ = 1;
      data_[0].set_price(val);
    }
  }
  
  void set_quantity(volume_t val) {
    if(!empty()) {
      volume_t diff = val - this->quantity();
      for(std::size_t i=0; i<size_; i++) {
        data_[i].quantity += diff;
      }
    } else {
      size_=1;
      data_[0].quantity = val;
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
  return QuoteArray<MAX_SIZE>(std::move(items));
}

inline auto make_empty_quotes() {
  return QuoteArray<0>({});
}

} // namespace shared
} // namespace roq


template <>
struct fmt::formatter<roq::shared::Quote> : public roq::formatter {
  template <typename Context>
  auto format(const roq::shared::Quote &value, Context &context) {
    using namespace roq::literals;
    return roq::format_to(context.out(), "{}:{}"_fmt, value.price(), value.quantity());
  }
};

template <uint32_t MAX_SIZE>
struct fmt::formatter<roq::shared::QuoteArray<MAX_SIZE>> : public roq::formatter {
  template <typename Context>
  auto format(const roq::shared::QuoteArray<MAX_SIZE> &value, Context &context) {
    using namespace roq::literals;
    return roq::format_to(context.out(), "{}:{}"_fmt, value.price(), value.quantity());
  }
};

template <>
struct fmt::formatter<roq::shared::GridQuote> : public roq::formatter {
  template <typename Context>
  auto format(const roq::shared::GridQuote &value, Context &context) {
    using namespace roq::literals;
    return roq::format_to(context.out(), "{}:{}"_fmt, value.price(), value.quantity());
  }
};