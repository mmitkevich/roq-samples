#pragma once

#include "roq/client.h"
#include "roq/shared/type_traits.h"

namespace roq {
inline namespace shared {

template<class T>
struct OrderTraits {
  ROQ_DECLARE_HAS_MEMBER(order_id);
  ROQ_DECLARE_HAS_MEMBER(price);
  ROQ_DECLARE_HAS_MEMBER(quantity);

  CreateOrder merge(const T& value, CreateOrder&& result) {
    if constexpr(ROQ_HAS_MEMBER(order_id, T)) {
      result.order_id = value.order_id;
    }
    if constexpr(ROQ_HAS_MEMBER(price, T)) {
      result.price = value.price;
    }
    if constexpr(ROQ_HAS_MEMBER(quantity, T)) {
      result.quantity = value.quantity;
    }
    return result;
  }
};

} // namespace shared
} // namespace roq