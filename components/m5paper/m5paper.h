#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"

namespace esphome {
namespace m5paper {

class M5PaperComponent : public PollingComponent {
    void setup() override;
    void update() override;
    void dump_config() override;

    public:
        void set_battery_power_pin(GPIOPin *power) { this->battery_power_pin_ = power; }
        void set_main_power_pin(GPIOPin *power) { this->main_power_pin_ = power; }
        void set_sd_cs_pin(GPIOPin *sdcs) { this->sd_cs_pin_ = sdcs; }
        void shutdown_main_power();

        float get_setup_priority() const override { return setup_priority::BUS; }

    private:
        GPIOPin *battery_power_pin_{nullptr};
        GPIOPin *main_power_pin_{nullptr};
        GPIOPin *sd_cs_pin_{nullptr};
};

template<typename... Ts> class PowerAction : public Action<Ts...>, public Parented<M5PaperComponent> {
 public:
  void play(Ts... x) override { this->parent_->shutdown_main_power(); }
};


} //namespace m5paper
} //namespace esphome
