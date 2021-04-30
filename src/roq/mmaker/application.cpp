/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "application.h"

#include <cassert>
#include <chrono>
#include <vector>

#include "roq/client.h"
#include "roq/exceptions.h"

#include "config.h"
#include "flags.h"
#include "strategy.h"

using namespace std::chrono_literals;
using namespace roq::literals;

namespace roq {
namespace mmaker {


int Application::main_helper(const roq::span<std::string_view> &args) {
  assert(!args.empty());
  if (args.size() == 1u)
    throw RuntimeErrorException("Expected arguments"_sv);
  if (args.size() != 2u)
    throw RuntimeErrorException("Expected exactly one argument"_sv);
  Config config;
  // note!
  //   absl::flags will have removed all flags and we're left with arguments
  //   arguments can be a list of either
  //   * unix domain sockets (trading) or
  //   * event logs (simulation)
  auto connections = args.subspan(1);
  if (Flags::simulation()) {
    // collector
    auto snapshot_frequency = 1s;
    auto collector = client::detail::SimulationFactory::create_collector(snapshot_frequency);
    // matcher
    auto market_data_latency = 1ms;
    auto order_manager_latency = 1ms;
    auto matcher = client::detail::SimulationFactory::create_matcher(
        "simple"_sv, Flags::exchange(), market_data_latency, order_manager_latency);
    // simulator
    client::Simulator(config, connections, *collector, *matcher).dispatch<Strategy>();
  } else {
    // trader
    client::Trader(config, connections).dispatch<Strategy>();
  }
  return EXIT_SUCCESS;
}

int Application::main(int argc, char **argv) {
  // wrap arguments (prefer to not work with raw pointers)
  std::vector<std::string_view> args;
  args.reserve(argc);
  for (int i = 0; i < argc; ++i)
    args.emplace_back(argv[i]);
  return main_helper(args);
}


}  // namespace mmaker
}  // namespace roq
