#include "bm8563.h"
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/log.h"
#include <cerrno>

namespace esphome {
namespace bm8563 {

static const char *TAG = "bm8563.sensor";

enum class BM8563TimerFreq : uint8_t {
    FREQ_4096HZ = 0,
    FREQ_64HZ = 1,
    FREQ_1HZ = 2,
    FREQ_MINUTE = 3  // or FREQ_1_60HZ
};
static constexpr uint8_t BM8563_ADDR_CONTROL_REG2  = 0x01;
static constexpr uint8_t BM8563_ADDR_TIMER_CONTROL = 0x0E;
static constexpr uint8_t BM8563_ADDR_TIMER_COUNTER = 0x0F;

static constexpr uint8_t BM8563_TIMER_ENABLE       = 1 << 7;
static constexpr uint8_t BM8563_FLAG_AF            = 1 << 3;
static constexpr uint8_t BM8563_FLAG_TF            = 1 << 2;
static constexpr uint8_t BM8563_FLAG_TIE           = 1 << 0;

void BM8563::setup()
{
    // Ensure RTC is running
    this->write_byte_16(0, 0);

    this->clear_alarm();

    this->setupComplete = true;
}

void BM8563::update()
{
    if (!this->setupComplete) {
        return;
    }
    this->read_time();
}

void BM8563::dump_config()
{
    ESP_LOGCONFIG(TAG, "BM8563:");
    ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
    ESP_LOGCONFIG(TAG, "  setupComplete: %s",
                this->setupComplete ? "true" : "false");
}

void BM8563::write_time()
{
  auto now = time::RealTimeClock::utcnow();
  if (!now.is_valid()) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC.");
    return;
  }

  uint8_t buf[7] = {
      byte_to_bcd2(now.second),
      byte_to_bcd2(now.minute),
      byte_to_bcd2(now.hour),
      byte_to_bcd2(now.day_of_month),
      byte_to_bcd2(now.day_of_week - 1),
      byte_to_bcd2(now.month),
      byte_to_bcd2((uint8_t)(now.year % 100)),
  };

  if (now.year < 2000) [[unlikely]] {
    buf[5] |= 0x80;
  }

  ESP_LOGI(TAG, "Writing to RTC %02x-%02x-%02x %02x:%02x:%02x, weekday %d",
           buf[6], buf[5], buf[3], buf[2], buf[1], buf[0], buf[4]);
  this->write_register(0x02, buf, 7);
}

void BM8563::read_time()
{
    uint8_t buf[7] = {0};

    this->read_register(0x02, buf, 7);

    ESPTime rtc_time {
        .second = bcd2_to_byte(buf[0] & 0x7f),
        .minute = bcd2_to_byte(buf[1] & 0x7f),
        .hour = bcd2_to_byte(buf[2] & 0x3f),
        .day_of_week = static_cast<uint8_t>(bcd2_to_byte(buf[4] & 0x07) + 1),
        .day_of_month = bcd2_to_byte(buf[3] & 0x3f),
        .day_of_year = 1,
        .month = bcd2_to_byte(buf[5] & 0x1f),
        .year = bcd2_to_byte(buf[6]),
        .is_dst = false,
        .timestamp = 0,
    };

    if (buf[5] & 0x80) {
        rtc_time.year += 1900;
    } else {
        rtc_time.year += 2000;
    }

    if (buf[0] & 0x80) {
        ESP_LOGW(TAG, "RTC time is invalid. Not synchronizing.");
        return;
    }

    ESP_LOGI(TAG, "Read from RTC %04d-%02d-%02d %2d:%02d:%02d, weekday %d",
            rtc_time.year, rtc_time.month, rtc_time.day_of_month, rtc_time.hour, rtc_time.minute,
            rtc_time.second, rtc_time.day_of_week);

    rtc_time.recalc_timestamp_utc(false);
    ESP_LOGD(TAG, "RTC time: %lld", rtc_time.timestamp);
    if ( rtc_time.timestamp > 0) {
        time::RealTimeClock::synchronize_epoch_(rtc_time.timestamp);
    } else {
        ESP_LOGE(TAG, "RTC time is invalid. Not synchronizing device clock to RTC time.");
    }
}

uint8_t BM8563::bcd2_to_byte(uint8_t value)
{
    return ((value >> 4) * 10) + (value & 0x0F);
}

uint8_t BM8563::byte_to_bcd2(uint8_t value)
{
    return ((value / 10) << 4) | (value % 10);
}

void BM8563::clear_alarm()
{
    ESP_LOGI(TAG, "Clear alarm");
    uint8_t control_reg2;

    // Get the current control status register 2 value
    this->read_byte(BM8563_ADDR_CONTROL_REG2, &control_reg2);

    // Clear out the timer interrupt flag and timer interrupt enable bit
    control_reg2 &= ~(BM8563_FLAG_TF | BM8563_FLAG_TIE);

    // Don't touch the Alarm Flag
    control_reg2 |= BM8563_FLAG_AF;

    // Write the updated control register 2 value back to the device
    this->write_byte(BM8563_ADDR_CONTROL_REG2, control_reg2);

    // Datasheet recommends to set the timer counter divider to 1/60Hz when not in use, to reduce power consumption
    this->write_byte(BM8563_ADDR_TIMER_COUNTER, static_cast<uint8_t>(BM8563TimerFreq::FREQ_MINUTE));
}

void BM8563::set_fuzzy_alarm(uint32_t msec)
{
    ESP_LOGI(TAG, "Set alarm for: %u ms", msec);
    // Frequencies: 4.096 Hz, 64Hz, 1Hzm 1/60 Hz
    // Counter range: [0,255]

    // Maximum time achievable with each frequency (counter = 255)
    // TD = 0: 4096 Hz  -> max = 255 * (1000/4096) =         62 ms
    // TD = 1:   64 Hz  -> max = 255 * (1000/64)   =      3`984 ms
    // TD = 2:    1 Hz  -> max = 255 * 1000        =    255`000 ms
    // TD = 3: 1/60 Hz  -> max = 255 * 60000       = 15`300`000 ms

    BM8563TimerFreq timer_frequency;
    uint16_t counter_value;

    if (msec <= 62) {
        timer_frequency = BM8563TimerFreq::FREQ_4096HZ;
        counter_value = (msec * 4096 + 500) / 1000;  // Round to nearest
    } else if (msec <= 3984) {
        timer_frequency = BM8563TimerFreq::FREQ_64HZ;
        counter_value = (msec * 64 + 500) / 1000;  // Round to nearest
    } else if (msec <= 255000) {
        timer_frequency = BM8563TimerFreq::FREQ_1HZ;
        counter_value = (msec + 500) / 1000;  // Round to nearest
    } else {
        timer_frequency = BM8563TimerFreq::FREQ_MINUTE;
        counter_value = (msec + 30000) / 60000;  // Round to nearest
    }

    if (counter_value > 255) {
        counter_value = 255;
    }

    ESP_LOGD(TAG, "Setting timer counter to %d and frequency %d", counter_value, timer_frequency);

    // Enable timer interrupt and clear any timer flag. Alarm flag is not touched
    uint8_t control_reg2;
    this->read_byte(BM8563_ADDR_CONTROL_REG2, &control_reg2);
    control_reg2 |= BM8563_FLAG_TIE | BM8563_FLAG_AF;
    control_reg2 &= ~BM8563_FLAG_TF;
    this->write_byte(BM8563_ADDR_CONTROL_REG2, control_reg2);

    this->write_byte(BM8563_ADDR_TIMER_COUNTER, counter_value);
    this->write_byte(BM8563_ADDR_TIMER_CONTROL, static_cast<uint8_t>(BM8563_TIMER_ENABLE | static_cast<uint8_t>(timer_frequency)));

}

} // namespace bm8563
} // namespace esphome
