/* Copyright (c) 2017-2021, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/service.h"

#include "roq/span.h"

namespace roq {
namespace mmaker {


class Application final : public Service {
 public:
  using Service::Service;

 protected:
  int main_helper(const roq::span<std::string_view> &args);

  int main(int argc, char **argv) override;
};


}  // namespace mmaker
}  // namespace roq
