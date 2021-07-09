/* Copyright (c) 2017-2021, Hans Erik Thrane */

#pragma once

#include <string_view>
#include <chrono>

namespace roq {
namespace mmaker {

namespace flags {

struct Flags {
  static std::string_view exchange();
  static std::string_view symbol();
  static std::string_view account();
  static std::string_view currencies();
  static uint32_t sample_freq_secs();
  static auto sample_freq() { return std::chrono::seconds{sample_freq_secs()}; }
  static double ema_alpha();
  static uint32_t warmup();
  static bool enable_trading();
  static bool simulation();
  static std::string_view config_file();
};

}  // namespace flags

}  // namespace mmaker
}  // namespace roq
