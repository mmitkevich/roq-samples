/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "ema.h"

#include <algorithm>
#include <cmath>


namespace roq {

EMA::EMA(double alpha, uint32_t warmup)
: alpha_(alpha)
, countdown_(warmup)
, warmup_(warmup)
{}

void EMA::reset() {
  value_ = NaN;
  countdown_ = warmup_;
}

double EMA::update(double value) {
  countdown_ = std::max<uint32_t>(1u, countdown_) - 1u;
  if (std::isnan(value_))
    value_ = value;  // initialize
  else
    value_ = alpha_ * value + (1.0 - alpha_) * value_;
  return value_;
}

}  // namespace roq
