/* Copyright (c) 2017-2021, Hans Erik Thrane */

#pragma once
#include "roq/client/config.h"
#include <toml++/toml.h>
#include <toml++/toml_array.h>
#include <toml++/toml_table.h>
#include "roq/logging.h"
#include "roq/format.h"
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <typeinfo>
#include <variant>
#include "roq/shared/ranges.h"
#include "roq/shared/type_traits.h"

namespace roq {
inline namespace shared {

template<class Flags>
struct Config final : client::Config {
 public:
  using Document = toml::table;
  using Element = toml::node;
  Config(const Element& node)
  : node_(node)
  {}
  Config(const Document& doc) 
  : node_(doc) {}
  
  const toml::node& impl() const { return node_; }

  static Document parse_file(std::string_view config_file_path) {
    log::debug("load_config_file(`{}`)"_sv, config_file_path);
    return toml::parse_file(config_file_path);
  }

  Config(Config &&) = default;
  Config(const Config &) = default;

  Config operator[](std::size_t key) { 
    if(node_.is_array())
      return (*node_.as_array())[key];
    else throw std::logic_error("toml::node is not an array");
  }
  Config operator[](std::size_t key) const  { 
    if(node_.is_array())
      return (*node_.as_array())[key];
    else throw std::logic_error("toml::node is not an array");
  };
  Config operator[](std::string_view key) { 
    if(node_.is_table())
      return *(*node_.as_table())[key].node();
    else throw std::logic_error("toml::node is not a table");
  }
  Config operator[](std::string_view key) const  { 
    if(node_.is_table())
      return *(*node_.as_table())[key].node();
    else throw std::logic_error("toml::node is not a table");
  }
  struct const_iterator {
    const_iterator(const toml::node& node, bool first)
    : node_(node) {
      if(node_.is_array())
        array_it_ = first ? node_.as_array()->begin() : node_.as_array()->end();
      else if(node_.is_table())
        table_it_ = first ? node_.as_table()->begin() : node_.as_table()->end();
    };
    
    const_iterator(const toml::node& node, toml::const_array_iterator it)
    : node_(node)
    , array_it_(it) {};

    const_iterator(const toml::node& node, toml::const_table_iterator it)
    : node_(node)
    , table_it_(it) {};
    
    const_iterator(const const_iterator& rhs)
    : node_(rhs.node_) {
      if(node_.is_array())
        array_it_ = rhs.array_it_;
      else if(node_.is_table())
        table_it_ = rhs.table_it_;
    }
    

    const Config parent() { return node_; }

    Config operator*() { 
      if(node_.is_array()) {
        assert(array_it_!=node_.as_array()->end());
        return Config(*array_it_);
      }
      if(node_.is_table()) {
        assert(table_it_!=node_.as_table()->end());
        return Config(table_it_->second);
      }
      throw std::logic_error("node is not array or table");
    }
    
    Config operator->() { return operator*(); }

    const_iterator operator++() { return visit([this](auto& it){ auto rit = ++it; return const_iterator(node_, rit); }); }
    const_iterator operator--() { return visit([this](auto& it){ auto rit = --it; return const_iterator(node_, rit); }); }
    
    template<class Fn>
    auto visit(Fn&& fn) {
      if(node_.is_array()) 
        return fn(array_it_);
      else if(node_.is_table())
        return fn(table_it_); 
      else throw std::logic_error("toml::node is not an array or table");
    }
    union {
      toml::const_array_iterator array_it_;
      toml::const_table_iterator table_it_;
    };
    const toml::node& node_;
  };

  const_iterator begin() const { 
    return const_iterator(node_, true);
  }
  const_iterator end() const { 
    return const_iterator(node_, false);
  }
  friend bool operator==(const_iterator lhs, const_iterator rhs) {
    if(&lhs.node_ != &rhs.node_) return false;
    if(lhs.node_.is_array()) return lhs.array_it_ == rhs.array_it_;
    if(lhs.node_.is_table()) return lhs.table_it_ == rhs.table_it_;
    throw std::logic_error("unsupported toml::node type");
  }
  friend bool operator!=(const_iterator lhs, const_iterator rhs) {
    return !operator==(lhs,rhs);
  }
  auto& as_table() {
    if(!node_.is_table())
      throw std::logic_error("node is not a table");
    return *node_.as_table();
  }
  const auto& as_table() const {
    if(!node_.is_table())
      throw std::logic_error("node is not a table");
    return *node_.as_table();
  }
  
  template<class T>
  T value_or(std::string_view key, T&& default_value) const {
    auto* n = as_table()[key].node();
    if(!n)
      throw_bad_type(key, "T");
    return n->value_or(key, std::move(default_value));
  }
  template<class T>
  T value(std::string_view key) const {
    auto* n = as_table()[key].node();
    if(!n)
      throw_bad_type(key, "T");
    return n->template value<T>(key);
  }
  template<class T>
  T value_exact(std::string_view key) const {
    auto* n = as_table()[key].node();
    if(!n)
      throw_bad_type(key, "T");
    return n->template value_exact<T>(key);
  }
  double double_value(std::string_view key) const {
    auto* n = as_table()[key].node();
    if(!n || !n->is_floating_point())
      throw_bad_type(key, "double");
    return n->as_floating_point()->get();
  }
   double integer_value(std::string_view key) const {
    auto* n = as_table()[key].node();
    if(!n || !n->is_integer())
      throw_bad_type(key, "integer");
    return n->as_integer()->get();
  }
  const std::string& string_value(std::string_view key) const {
    const auto* n = as_table()[key].node();
    if(!n || !n->is_string())
      throw_bad_type(key, "string");
    return n->as_string()->get();
  }

  static void throw_bad_type(std::string_view key, std::string_view type) {
      std::stringstream ss;
      ss<<"key '"<<key<<"' value of type '"<<type<<"' expected";
      throw std::logic_error(ss.str());
  }
 protected:

  void dispatch(Handler &handler) const override {
    // accounts
    handler(client::Account{
        .regex = Flags::account(),
    });
    // symbols
    handler(client::Symbol{
        .regex = Flags::symbol(),
        .exchange = Flags::exchange(),
    });
    // currencies
    handler(client::Symbol{
        .regex = Flags::currencies(),
    });
  }
 protected:
  const toml::node& node_;
};

}  // mmaker
}  // namespace roq

template <class Flags>
struct fmt::formatter<roq::shared::Config<Flags>> : public roq::formatter {
  template <typename Context>
  auto format(const roq::shared::Config<Flags> &value, Context &context) {
    using namespace roq::literals;
    std::stringstream ss;
    ss << toml::json_formatter{ value.impl() };
    return roq::format_to(context.out(), "{}", ss.str());
  }
};

#include "config.inl"
