/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include <gtest/gtest.h>
#include "roq/logging.h"


namespace detail {
roq::detail::sink_t log = [](const std::string_view& message) { std::cerr << message << std::endl; };
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  roq::detail::INFO = roq::detail::WARNING = roq::detail::ERROR = roq::detail::CRITICAL = detail::log;
  return RUN_ALL_TESTS();
}
