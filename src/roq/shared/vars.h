#pragma once

#include <absl/container/flat_hash_map.h>
#include <variant>
#include <vector>
#include <cstdint>

namespace roq {
namespace shared {

using InstrumentId = uint32_t;
using VarId = std::string_view;

using NumericVar = std::vector<double>;
using IntegerVar = std::vector<int64_t>;
using InstrumentVar = std::vector<InstrumentId>;
using Var = std::variant<NumericVar, IntegerVar, InstrumentVar>;

struct Variables {
  Var& operator[](std::string_view id) {
    assert(data_.contains(id));
    return data_[id];
  }
  template<class Var>
  Var& get(std::string_view id) {
    auto it = data_.find(id);
    data_.emplace(id, Var{});
  }
private:
  absl::flat_hash_map<std::string, Var> data_;
};

} // namespace shared
} // namespace roq