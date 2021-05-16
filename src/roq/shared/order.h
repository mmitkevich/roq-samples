#pragma once
#include <cstdint>
#include <deque>
#include <sstream>
#include <string>
#include <string_view>
#include <charconv>

#include "roq/client.h"
#include "roq/numbers.h"
#include "roq/shared/price.h"
#include "roq/shared/quote.h"
#include "roq/shared/type_traits.h"
#include "roq/format.h"
#include "roq/shared/bitmask.h"

namespace roq {
inline namespace shared {

using order_id_t = uint32_t;
constexpr static order_id_t undefined_order_id = 0;

using routing_id_t = uint64_t;

//! Order transaction id (NEW,MODIFY,CANCEL)
struct order_txid_t {
    order_id_t order_id  = undefined_order_id;    //! 1:1 to exchange_order_id    
    order_id_t routing_id_ = undefined_order_id;   //! ClOrdId

    order_txid_t() = default;
    
    order_txid_t(order_id_t order_id, order_id_t routing_id)
    : order_id(order_id)
    , routing_id_(routing_id) {}
    
    order_txid_t(order_id_t order_id, std::string_view routing_id) {
      this->order_id = order_id;
      this->routing_id_ = undefined_order_id;
      assert(routing_id.size()==sizeof(routing_id_));
      std::memcpy(&this->routing_id_, routing_id.data(), std::min(sizeof(routing_id_), routing_id.size()));
    }

    std::string_view routing_id() const { return std::string_view((const char*)&routing_id_, sizeof(routing_id_)); };

    friend bool operator==(const order_txid_t& lhs, const order_txid_t& rhs) {
      return lhs.routing_id_ == rhs.routing_id_ && lhs.order_id==rhs.order_id;
    }
};

struct LimitOrder : Quote {
  enum flags_t : uint32_t {
    EMPTY          = 0,
    WORKING        = 1,
    PENDING_NEW    = 2,
    PENDING_CANCEL = 4,
    PENDING_MODIFY = 8,
    PRICE          = 16,
    QUANTITY       = 32
  };
  LimitOrder() = default;
  LimitOrder(Quote quote, flags_t flags=EMPTY)
  : Quote(quote)
  , flags(flags) {}

  void reset() {
    flags = EMPTY;
    Quote::reset();
  }

  bool empty() const { return flags==EMPTY; }

  bool is_pending() const { return flags & (PENDING_NEW|PENDING_MODIFY); }
  bool is_pending_cancel() const { return flags & PENDING_CANCEL; }
  bool is_working() const { return flags & WORKING; }
  LimitOrder& tail_order() { LimitOrder* tail = this; while(tail->next_order) tail=tail->next_order; return *tail; }

public:
  shared::BitMask<flags_t> flags {};
  volume_t est_volume_before = 0.;  // estimated volume before this order in the level
  LimitOrder* next_order {}; //! next order in the level
  order_id_t prev_routing_id {undefined_order_id}; //! for pending modify store previous routing_id
};

} // shared
} // roq

#define ROQ_FLAGS_PRINT(os, flags, clazz, mask) if(flags & clazz::mask) os << "+" #mask;

namespace std {
template<>
struct hash<roq::order_txid_t> {
  std::size_t operator()(const roq::shared::order_txid_t& txid) {
    return txid.order_id | (static_cast<std::size_t>(txid.routing_id_)<<(8*sizeof(txid.order_id)));
  }
};
}

template <>
struct fmt::formatter<roq::shared::LimitOrder> : public roq::formatter {
  template <typename Context>
  auto format(const roq::shared::LimitOrder &order, Context &context) {
    using namespace roq::literals;
    return roq::format_to(context.out(), "side: {}, price: {}, quantity: {}, flags: {}, prev_routing_id: {}"_fmt,
      order.side(), order.price(), order.quantity(), order.flags, order.prev_routing_id);
  }
};

template <>
struct fmt::formatter<roq::order_txid_t> : public roq::formatter {
  template <typename Context>
  auto format(const roq::order_txid_t &value, Context &context) {
    using namespace roq::literals;
    return roq::format_to(context.out(), "order_id:{}, routing_id:{}"_fmt, value.order_id, value.routing_id_);
  }
};


template <>
struct fmt::formatter<roq::shared::LimitOrder::flags_t> : public roq::formatter {
  template <typename Context>
  auto format(const roq::shared::LimitOrder::flags_t &value, Context &context) {
    using namespace roq::literals;
    std::ostringstream os;
    ROQ_FLAGS_PRINT(os, value, roq::shared::LimitOrder, PENDING_NEW);
    ROQ_FLAGS_PRINT(os, value, roq::shared::LimitOrder, PENDING_MODIFY);
    ROQ_FLAGS_PRINT(os, value, roq::shared::LimitOrder, PENDING_CANCEL);
    ROQ_FLAGS_PRINT(os, value, roq::shared::LimitOrder, WORKING);
    return roq::format_to(context.out(), "{}"_fmt, os.str());
  }
};
