#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace bm8563 {

class BM8563 : public time::RealTimeClock, public i2c::I2CDevice {
    public:
        void setup() override;
        void update() override;
        void dump_config() override;

        void write_time();
        void read_time();
        void clear_alarm();

        void set_fuzzy_alarm(uint32_t msec);

    protected:
        uint8_t bcd2_to_byte(uint8_t value);
        uint8_t byte_to_bcd2(uint8_t value);

        bool setupComplete = false;
};

template<typename... Ts> class WriteTimeAction : public Action<Ts...>, public Parented<BM8563> {
    public:
        void play(Ts... x) override { this->parent_->write_time(); }
};

template<typename... Ts> class ReadTimeAction : public Action<Ts...>, public Parented<BM8563> {
    public:
        void play(Ts... x) override { this->parent_->read_time(); }
};

template<typename... Ts> class ClearAlarmAction : public Action<Ts...>, public Parented<BM8563> {
    public:
        void play(Ts... x) override { this->parent_->clear_alarm(); }
};

template<typename... Ts> class SetAlarmAction : public Action<Ts...>, public Parented<BM8563> {
    public:
        TEMPLATABLE_VALUE(uint32_t, fuzzy_alarm);

        void play(Ts... x) override {
            auto alarm = this->fuzzy_alarm_.value(x...);
            this->parent_->set_fuzzy_alarm(alarm);
        }
};

}  // namespace bm8563
}  // namespace esphome
