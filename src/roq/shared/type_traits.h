#pragma once

#include <absl/meta/type_traits.h> 
namespace roq {

template<template<class...> class F, class... T>
constexpr bool is_detected_v = absl::type_traits_internal::is_detected<F, T...>::value;

} // namespace roq

#define ROQ_DECLARE_HAS_MEMBER(member) \
template<class Clazzz, class...> \
using has_##member = decltype(std::declval<Clazzz&>().member)

#define ROQ_HAS_MEMBER(member, clazz) roq::is_detected_v<has_##member, clazz>
