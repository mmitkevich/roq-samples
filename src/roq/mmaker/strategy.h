/* Copyright (c) 2017-2021, Hans Erik Thrane */

#pragma once

#include "roq/mmaker/flags.h"
#include "roq/mmaker/model.h"
#include "roq/shared/grid_order.h"
#include "roq/shared/strategy.h"
#include "roq/shared/config.h"

namespace roq {
namespace mmaker {

struct Strategy : roq::shared::Strategy<Strategy, shared::QuotingInstrument<shared::GridOrder>> {
    using Base = roq::shared::Strategy<Strategy, shared::QuotingInstrument<shared::GridOrder>>;
    using Config = shared::Config<mmaker::Flags>;
    using Model = mmaker::Model;

    Strategy(client::Dispatcher& dispatcher, Config& config) 
    : Base(dispatcher)
    , config_(config)
    , model_(config) {}

    Model& model() { return model_; }
    Config& config() { return config_; }
private:
    Config& config_;
    Model model_;    
};

} // mmaker
} // roq