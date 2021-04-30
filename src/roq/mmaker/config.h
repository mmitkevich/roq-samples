/* Copyright (c) 2017-2021, Hans Erik Thrane */

#pragma once

#include "roq/client/config.h"

namespace roq {
namespace mmaker {

class Config final : public client::Config {
 public:
  Config() {}

  Config(Config &&) = default;
  Config(const Config &) = delete;

 protected:
  void dispatch(Handler &handler) const override;
};

}  // mmaker
}  // namespace roq
