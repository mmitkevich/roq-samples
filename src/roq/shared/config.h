/* Copyright (c) 2017-2021, Hans Erik Thrane */

#pragma once
#include "roq/client/config.h"
#include <toml++/toml.h>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace roq {
inline namespace shared {

template<class Flags>
struct Config final : client::Config {
 public:
  using Document = toml::table;

  Config(std::string_view config_file_path="")
  : config_file_path_(config_file_path) {
    load_config_file(config_file_path_);
  }

  Config(Config &&) = default;
  Config(const Config &) = delete;

  template<class Key>
  auto operator[](Key key) { return document_[key]; }
  template<class Key>
  const auto operator[](Key key) const { return document_[key]; }

  double as_double(std::string_view key) const {
    const auto* val =  document_[key].as_floating_point();
    assert(val);
    if(!val) {
      throw_bad_type(key, "double");
    }
    return val->get();
  }

  const std::string& as_string(std::string_view key) const {
    const auto* val =  document_[key].as_string();
    assert(val);
    if(!val) {
      throw_bad_type(key, "string");
    }
    return val->get();
  }

  static void throw_bad_type(std::string_view key, std::string_view type) {
      std::stringstream ss;
      ss<<"key '"<<key<<"' value of type '"<<type<<"' expected";
      throw std::logic_error(ss.str());
  }

  Document& document() { return document_;}
  const Document& document() const { return document_;}
 protected:
  void load_config_file(std::string_view config_file_path);
  void dispatch(Handler &handler) const override;
 protected:
  std::string config_file_path_;
  Document document_;
};

}  // mmaker
}  // namespace roq

#include "config.inl"
