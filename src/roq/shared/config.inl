/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "config.h"


namespace roq {
inline namespace shared {

template<class Flags>
void Config<Flags>::load_config_file(std::string_view config_file_path) {
    document_ = toml::parse(config_file_path);
}

template<class Flags>
void Config<Flags>::dispatch(Handler &handler) const {
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

} // namesapce shared
}  // namespace roq
