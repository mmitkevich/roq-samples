#pragma once

#include <absl/container/flat_hash_map.h>
#include <roq/create_order.h>
#include <roq/utils/compare.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include "roq/shared/order.h"
#include "roq/shared/price.h"
#include "roq/shared/levels.h"
#include "roq/shared/ranges.h"
#include "absl/container/flat_hash_map.h"

namespace roq {
inline namespace shared {


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
  
  const auto& levels() { return levels_; }
  const auto& orders() { return orders_;}

  void reset();

  void execute();

  void order_updated(const OrderUpdate& update);

  template<class Out>
  void dump(Out& out);
protected:
  void order_canceled(order_txid_t id, LimitOrder& order, const OrderUpdate& order_update);
  void order_rejected(order_txid_t id, LimitOrder& order, const OrderUpdate& order_update);
  void order_completed(order_txid_t id, LimitOrder& order, const OrderUpdate& order_update);
  void order_working(order_txid_t id, LimitOrder& order, const OrderUpdate& order_update);
private:
  Self* self() { return self_; }
  order_txid_t create_order(order_txid_t id, const LimitOrder& order);
  order_txid_t cancel_order(order_txid_t id);
  order_txid_t modify_order(order_txid_t id, const LimitOrder& new_order);

private:
  Self* self_{};
  Levels<Level, DIR> levels_;       //!< sorted array of price levels
  absl::flat_hash_map<order_txid_t, LimitOrder> orders_;        //!< queue of orders (working, pending, canceling, moving)
  std::deque<std::pair<order_txid_t, LimitOrder>> pending_orders_; //! buffer for additional pending orders (due to hashmap limitations)
  friend class fmt::formatter<roq::shared::GridOrder<Self, DIR>>;
};

} // namespace shared
} // namespace roq


template <class Self, int DIR>
struct fmt::formatter<roq::shared::GridOrder<Self, DIR>> : public roq::formatter {
  template <typename Context>
  auto format(const roq::shared::GridOrder<Self, DIR> &value, Context &context) {
    using namespace roq::literals;
    auto out = context.out();
    roq::format_to(out, "orders#{}:[\n"_fmt, value.orders_.size());
    for(auto& [id, order]: value.orders_) {
      roq::format_to(out, " {{order_id: {}, routing_id: {}, {}}},\n"_fmt, id.order_id, id.routing_id_, order);
    }
    roq::format_to(out, "], levels#{}:[\n"_fmt, value.levels_.size());
    for(auto const& level: value.levels_) {
      roq::format_to(out, " {{{}}},\n"_fmt, level);
    }
    roq::format_to(out, "]"_sv);
    return out;
  }
};
