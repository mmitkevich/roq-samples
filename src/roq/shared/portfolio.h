#pragma once
#include <string_view>
#include <cmath>
#include <cassert>

#include "roq/numbers.h"

namespace roq {
inline namespace shared {

struct Position {
    Position(double val = NaN)
    : value(val) {}
    operator double() const { assert(!std::isnan(value)); return value; }
    bool empty() const { return std::isnan(value); }
    void reset() { value = NaN; }
public:
    double value;
};


struct Account {
    Account() = default;
    Account(std::string_view account)
    : account(account) {}
    bool compare(std::string_view rhs) const { return rhs.compare(account); }
public:
    std::string_view account;
};

struct Portfolio : Account {
    using Account::Account;
};

}
}