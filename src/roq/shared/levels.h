#pragma once

#include <roq/utils/compare.h>
#include <cassert>
#include <deque>
#include "roq/shared/price.h"

namespace roq {
inline namespace shared {

//! price level
struct Level {
  Level(price_t price = undefined_price)
  : price(price) {
    reset();
  }

  ///! expected volume after all transactions will execute successfully
  volume_t expected_volume() const { return working_volume + pending_volume - canceling_volume; }
  
  bool empty() const { 
    if(utils::compare(desired_volume, 0.)!=0)
      return false;
    if(utils::compare(working_volume,0.)!=0)
      return false;
    if(utils::compare(pending_volume,0.)!=0)
      return false;
    if(utils::compare(canceling_volume,0.)!=0)
      return false;
    return true;
  }

  volume_t free_volume() const { return desired_volume - expected_volume(); }
  
  void reset() {
    desired_volume = 0;
    working_volume = 0;
    pending_volume = 0;
    canceling_volume = 0;
  }

  price_t price;
  volume_t desired_volume;
  volume_t working_volume;
  volume_t pending_volume;
  volume_t canceling_volume;
};



//! Sorted array of price levels. price should be proportional to step
template<class T, int DIR>
struct Levels {
  Levels(price_t tick_size=undefined_price, std::size_t capacity=1024)
  : tick_size_(tick_size) {
    //data_.reserve(capacity);
  }

  auto begin() { return data_.begin(); }
  auto end() { return data_.end(); }

  auto begin() const { return data_.begin(); }
  auto end() const { return data_.end(); }

  T& nth(std::size_t index) {
    assert(index>=0);
    assert(index<size());
    return data_[index];
  }
  
  //! element at price. container resizes automatically
  T& operator[](price_t price) {
    assert(!is_undefined_price(tick_size_));
    price_t price_b = price_round_bottom<DIR>(price, tick_size_);
    assert(utils::compare(price, price_b)==0);
    ssize_t index = 0;
    if(!empty()) {
      index = ((ssize_t)(top_price_ - price_b)/tick_size_) * DIR;
    } else {
      top_price_ = price_b;
    }
    for(; index<0; index++) {
      top_price_ += DIR*tick_size_;
      data_.emplace_front(top_price_);
    }
    ssize_t sze;
    price_t btm_price;
    for(sze=data_.size(), btm_price = top_price_ - DIR * sze * tick_size_;
        sze <= index;
        sze=data_.size(), btm_price-=DIR*tick_size_) 
    {
      data_.emplace_back(btm_price);
    }
    assert(index>=0 && index<data_.size());
    auto& result =  data_[index];
    return result;
  } 
  
  void erase(price_t price) {
    int cmp_top = shared::price_compare<DIR>(price, top_price_);
    auto btm = bottom();
    int cmp_bottom = shared::price_compare<DIR>(price, btm);
    if(cmp_top<0 || cmp_bottom>0)
      return;
    (*this)[price].reset();
    shrink();
  }

  void shrink() {
    while(!data_.empty()) {
      auto& top = data_.front();
      auto& btm = data_.back();
      if(top.empty()) {
        top_price_ -= DIR*tick_size_;
        data_.pop_front();
      } else if(btm.empty()) {
        data_.pop_back();
      } else {
        break;
      }
    }
  }

  void clear() {
    data_.clear();
    top_price_ = undefined_price;
  }

  bool empty() const { return data_.empty(); }
  
  std::size_t size() const { return data_.size(); }
  
  price_t price(std::size_t i) { return top_price_ + i*tick_size_; }

  price_t top() const { return top_price_; }
  
  price_t bottom() const { return top_price_ - DIR*data_.size()*tick_size_; }
  
  price_t tick_size() const { return tick_size_;}
  void set_tick_size(price_t val) { 
    assert(empty());
    tick_size_ = val; 
  }
  template<class Pred> 
  price_t find_top(Pred&& pred) {
      price_t price = top_price_;
      for(auto it = data_.begin(); it!=data_.end(); it++) {
          if(pred(*it))
            return price;
          price -= DIR*tick_size_;
      }
      return price; // one after bottom
  }

  template<class Pred> 
  price_t find_bottom(Pred&& pred) {
      price_t price = bottom();
      for(auto it = data_.rbegin(); it!=data_.rend(); it++) {
          if(pred(*it))
            return price;
          price += DIR*tick_size_;
      }
      return price; // one after top
  }
private:
  price_t top_price_ = undefined_price;
  price_t tick_size_ = undefined_price;
  std::deque<T> data_;
};
} // namespace shared
} // namespace roq


template <>
struct fmt::formatter<roq::shared::Level> : public roq::formatter {
  template <typename Context>
  auto format(const roq::shared::Level &value, Context &context) {
    using namespace roq::literals;
    return roq::format_to(context.out(), "price: {}, desired:{}, pending:{}, working:{}, canceling:{}",
      value.price, value.desired_volume, value.pending_volume, value.working_volume, value.canceling_volume);
  }
};
