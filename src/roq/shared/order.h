#pragma once
#include <cstdint>
#include <deque>
#include <string_view>
#include <charconv>

#include "roq/client.h"
#include "roq/numbers.h"
#include "roq/shared/price.h"
#include "roq/shared/quote.h"
#include "roq/shared/type_traits.h"

namespace roq {
inline namespace shared {

using order_id_t = uint32_t;
constexpr static order_id_t undefined_order_id = 0;

using routing_id_t = uint64_t;

//! Order transaction id (NEW,MODIFY,CANCEL)
struct order_txid_t {
    order_id_t order_id  = undefined_order_id;    //! 1:1 to exchange_order_id    
    order_id_t client_order_id = undefined_order_id;   //! ClOrdId

    order_txid_t() = default;
    
    order_txid_t(order_id_t order_id, order_id_t client_order_id)
    : order_id(order_id)
    , client_order_id(client_order_id) {}
    
    order_txid_t(order_id_t order_id, std::string_view routing_id) {
      this->order_id = order_id;
      this->client_order_id = undefined_order_id;
      assert(routing_id.size()==sizeof(client_order_id));
      std::memcpy(&this->client_order_id, routing_id.data(), std::min(sizeof(client_order_id), routing_id.size()));
    }

    std::string_view routing_id() const { return std::string_view((const char*)&client_order_id, sizeof(client_order_id)); };

    friend bool operator==(const order_txid_t& lhs, const order_txid_t& rhs) {
      return lhs.client_order_id == rhs.client_order_id && lhs.order_id==rhs.order_id;
    }
};

struct Order {
  enum flags_t : uint32_t {
    EMPTY          = 0,
    WORKING        = 1,
    PENDING_NEW    = 2,
    PENDING_CANCEL = 4,
    PENDING_MODIFY = 8,
    PRICE          = 16,
    QUANTITY       = 32
  };
  Order() = default;
  Order(Quote quote, uint32_t flags=EMPTY)
  : quote(quote)
  , flags(flags) {}

  operator Quote& () { return quote; }

  void reset() {
    flags = EMPTY;
    quote.reset();
  }

  bool empty() const { return flags==EMPTY; }

  bool is_pending() const { return flags & (PENDING_NEW|PENDING_MODIFY); }
  bool is_pending_cancel() const { return flags & PENDING_CANCEL; }
  bool is_working() const { return flags & WORKING; }
  Order& tail_order() { Order* tail = this; while(tail->next_order) tail=tail->next_order; return *tail; }
  
  auto price() const { return quote.price; }
  auto quantity() const { return quote.quantity; }
  auto side() const { return quote.side; }

  Quote quote {};
  uint32_t flags {};
  volume_t est_volume_before = 0.;  // estimated volume before this order in the level
  Order* next_order {}; //! next order in the level
};

} // namespace shared
} // namespace roq

namespace std {
template<>
struct hash<roq::order_txid_t> {
  std::size_t operator()(const roq::shared::order_txid_t& txid) {
    return txid.order_id | (static_cast<std::size_t>(txid.client_order_id)<<(8*sizeof(txid.client_order_id)));
  }
};
}