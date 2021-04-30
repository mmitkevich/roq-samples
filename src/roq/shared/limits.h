#pragma once
#include <cmath>
#include "roq/numbers.h"

namespace roq {
inline namespace shared {

struct Range {
  Range() = default;
  Range(double min, double max) : min(min), max(max) {}
  double size() { return max-min; }
  double min{NaN};
  double max{NaN};
};

template<class Instrument>
struct PositionLimit {
  PositionLimit(Instrument& instrument)
  : instrument(instrument) {}

  template<class Quote>
  bool validate(Quote& bid, Quote& ask) const {
      bid.quantity = std::fmin(bid.quantity, position.max - instrument.position());
      ask.quantity = std::fmin(bid.quantity, position.max - instrument.position());
      return true;
  }
  Range position {-INFINITY, +INFINITY};
  Instrument& instrument;
};

}
}