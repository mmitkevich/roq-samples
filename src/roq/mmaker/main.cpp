/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "roq/api.h"

#include "application.h"

using namespace roq::literals;

namespace {
static const auto DESCRIPTION = "roq::mmaker"_sv;
}  // namespace

int main(int argc, char **argv) {
  return roq::mmaker::Application(argc, argv, DESCRIPTION, ROQ_VERSION).run();
}
