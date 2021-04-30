/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "config.h"

#include "flags.h"

namespace roq {
namespace mmaker {

void Config::dispatch(Handler &handler) const {
  // accounts
  handler(client::Account{
      .regex = Flags::account(),
  });
  // symbols
  handler(client::Symbol{
      .regex = Flags::symbol(),
      .exchange = Flags::exchange(),
  });
  // currencies
  handler(client::Symbol{
      .regex = Flags::currencies(),
  });
}

} // namesapce mmaker
}  // namespace roq
