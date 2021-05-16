#pragma once
#include <array>
#include <vector>

#include "roq/shared/instrument.h"

namespace roq {
namespace shared {

struct Model {
  static const constexpr size_t MAX_DEPTH = 3u;
  using Depth = std::array<Layer, MAX_DEPTH>;
  
  Model() = default;
  Model(Model &&) = default;
  Model(const Model &) = delete;

  constexpr static instrument_id_t INSTRUMENT(uint32_t i) { return instrument_id_t(i); }

  template<class Context>
  void quotes_updated(instrument_id_t iid, Context&) {}

  template<class Context>
  void depth_updated(instrument_id_t iid, Context&) {}

  template<class Context>
  void time_updated(Context&) {}

  template<class Context>
  void position_updated(instrument_id_t iid, Context&) {}

  template<class Context>
  bool validate(instrument_id_t iid, Context&) { return true; }

  // some basic validators
  template<class Context>
  bool has_bid_ask(instrument_id_t iid, Context&);
};    

} // shared
} // roq