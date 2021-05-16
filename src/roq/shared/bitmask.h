#pragma once


#include <type_traits>
namespace roq {
namespace shared {

template<class T>
struct BitMask {
  using underlying_type = std::underlying_type_t<T>;
  using type = T;
  static_assert(std::is_enum_v<T>);

  BitMask(underlying_type flags={})
  : flags(flags) {}

  operator T() const { return (T)flags; }

  bool all(underlying_type mask) const {
      return (flags & mask) == mask;
  }
  bool none(underlying_type mask) const {
      return !(flags & mask);
  }
  bool set(underlying_type flag) {
    auto prev = flags;
    flags = type(flags | flag);
    return flags!=prev;
  }
  bool reset(underlying_type flag) {
    auto prev = flags;
    flags = type(flags & ~flag);
    return flags!=prev;
  }
  bool reset() {
    auto prev = flags;
    flags = 0;
    return prev!=flags;
  }
  bool set(type flag, bool val) {
    auto prev = flags;
    if(val) 
        flags = flags | flag;
    else
        flags = flags & (~flag);
    return flags!=prev;
  }
  bool test(underlying_type mask) const {
    return (flags & mask);
  }
private:
  underlying_type flags;
};

} // namespace shared
} // namespace roq