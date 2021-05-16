#pragma once

#include "roq/shared/strategy.h"
#include "model.h"

namespace roq {
namespace mmaker {

template<class Model, class Flags>
struct Strategy : shared::Strategy<Strategy<Model, Flags>> {
  using Base = shared::Strategy<Strategy<Model, Flags>>;
  using Base::Base;

  bool enable_trading() { return Flags::enable_trading(); }
  std::string_view account() { return Flags::account(); }
  Model& model() { return model_; }
  auto sample_freq() { return std::chrono::seconds{Flags::sample_freq_secs()}; }
private:
  Model model_;
};

} // shared
} // roq