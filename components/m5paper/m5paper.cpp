// SPDX-License-Identifier: GPL-3.0-or-later

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "m5paper.h"

namespace esphome {
namespace m5paper {

static const char *TAG = "m5paper.component";

void M5PaperComponent::setup() {
    ESP_LOGD(TAG, "m5paper starting up!");

    this->main_power_pin_->setup();
    this->main_power_pin_->pin_mode(gpio::FLAG_OUTPUT);

    this->battery_power_pin_->setup();
    this->battery_power_pin_->pin_mode(gpio::FLAG_OUTPUT);

    if (this->sd_cs_pin_) {
        this->sd_cs_pin_->setup();
    	this->sd_cs_pin_->pin_mode(gpio::FLAG_OUTPUT);
    	this->sd_cs_pin_->digital_write(true);
    }

    this->main_power_pin_->digital_write(true);
    delay(100);
    this->battery_power_pin_->digital_write(true);

}

void M5PaperComponent::shutdown_main_power() {
    ESP_LOGD(TAG, "Shutting Down Power");
    this->main_power_pin_->digital_write(false);
}

void M5PaperComponent::update() {
    this->status_clear_warning();
}

void M5PaperComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "Empty custom sensor");
}

} //namespace m5paper
} //namespace esphome
