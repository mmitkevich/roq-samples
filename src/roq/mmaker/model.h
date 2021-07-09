/* Copyright (c) 2017-2021, Hans Erik Thrane */

#pragma once

#include "roq/shared/model.h"
#include "roq/shared/ema.h"

namespace roq {
namespace mmaker {

// Model calculates quotes for single instrument
// Model uses quotes from many instruments
struct Model : shared::Model {
  
  constexpr static instrument_id_t QUOTING = INSTRUMENT(0);  // which one we're quoting

  template<class Config>
  Model(const Config& config) {
    configure(config);
  }

  template<class Config>
  void configure(const Config& config);
  
  //! called when quotes (best bid, best ask) are updated
  template<class Strategy>
  void quotes_updated(Strategy& strategy, instrument_id_t iid);
 
  //! called to validate instrument
  template<class Strategy>
  bool validate(Strategy& strategy, instrument_id_t iid);

  template<class Instrument>
  volume_t quoting_qty(const Instrument& ins) const { return ins.refdata().min_trade_vol(); }
  
  template<class Instrument>
  price_t quoting_spread(const Instrument& ins) const { return quoting_spread_ * tick_size(ins); } 

  template<class Instrument>
  price_t tick_size(const Instrument& ins) const { return ins.refdata().tick_size(); }
 protected:
  price_t quoting_spread_ = NaN;
};


}  // namespace mmaker
}  // namespace roq

#include "model.inl"