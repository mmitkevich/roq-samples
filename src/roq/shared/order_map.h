#pragma once

#include "roq/shared/order.h"
#include <absl/container/flat_hash_map.h>

namespace roq {
inline namespace shared {

///  orders and transactions (working, pending, canceling, etc)
struct LimitOrdersMap : private absl::flat_hash_map<order_txid_t, LimitOrder> {
    using Base = absl::flat_hash_map<order_txid_t, LimitOrder>;

    template<class Context>
    order_txid_t create_order(order_txid_t id, const LimitOrder& new_order, Context& context);
    template<class Context>
    order_txid_t cancel_order(order_txid_t id, Context& context);
    template<class Context>
    order_txid_t modify_order(order_txid_t id, const  LimitOrder& new_order, Context& context);
    template<class Context>
    void flush_orders(Context& context);

    using Base::contains, Base::begin, Base::end, Base::operator[], Base::erase, Base::size, Base::find;
private:
    template<class Context>
    order_txid_t do_create_order(order_txid_t id, const LimitOrder& new_order, Context& context);
    template<class Context>
    order_txid_t do_modify_order(order_txid_t id, const  LimitOrder& new_order, Context& context);
private:
    std::deque<std::pair<order_txid_t, LimitOrder>> pending_orders_; //!< pending to be inserted into orders_
};


} // shared
} // roq