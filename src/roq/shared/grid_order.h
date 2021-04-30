#pragma once

#include <absl/container/flat_hash_map.h>
#include <roq/create_order.h>
#include <roq/utils/compare.h>
#include <algorithm>
#include <cmath>
#include "roq/shared/order.h"
#include "roq/shared/price.h"
#include "roq/shared/levels.h"
#include "roq/shared/ranges.h"
#include "absl/container/flat_hash_map.h"

namespace roq {
inline namespace shared {


inline int to_dir(Side side) {
  switch(side) {
    case Side::BUY: return 1;
    case Side::SELL: return -1;
    default: assert(false); return 0;
  }
}


//! GridOrder moves a grid of orders
template<class Self, int DIR>
struct GridOrder {  
  static_assert(DIR==1 || DIR==-1);
  GridOrder(Self* self, price_t tick_size=NaN)
  : self_(self)
  , levels_(tick_size) {}

  Side side() const { return DIR>0 ? Side::BUY: Side::SELL; }
  
  template<class Quotes>
  void modify(const Quotes& quotes);

  template<class Quotes>
  void create(const Quotes& quotes) { modify(quotes); }
  
  void set_tick_size(price_t val) {
    levels_.set_tick_size(val);
  }
  
  void reset();

  void execute();

  void order_updated(const OrderUpdate& update);

protected:
  void order_canceled(order_txid_t order_id, Order& order, const OrderUpdate& order_update);
  void order_rejected(order_txid_t order_id, Order& order, const OrderUpdate& order_update);
  void order_completed(order_txid_t order_id, Order& order, const OrderUpdate& order_update);
  void order_working(order_txid_t order_id, Order& order, const OrderUpdate& order_update);
private:
  Self* self() { return self_; }
  order_txid_t create_order(const Order& order);
  order_txid_t cancel_order(order_txid_t order_id);
  order_txid_t modify_order(order_txid_t order_id,  const Order& new_order);

private:
  Self* self_{};
  Levels<Level, DIR> levels_;       //!< sorted array of price levels
  absl::flat_hash_map<order_txid_t, Order> orders_;        //!< queue of orders (working, pending, canceling, moving)
};

} // namespace shared
} // namespace roq