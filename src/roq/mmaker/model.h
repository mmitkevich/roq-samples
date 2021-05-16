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
  //! called when quotes (best bid, best ask) are updated
  template<class Strategy>
  void quotes_updated(Strategy& strategy, instrument_id_t iid);
 
  //! called to validate instrument
  template<class Strategy>
  bool validate(Strategy& strategy, instrument_id_t iid);

 protected:
  price_t spread_ = NaN;
};


}  // namespace mmaker
}  // namespace roq
