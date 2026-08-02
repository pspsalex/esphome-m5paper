#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later

#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace it8951e {

static constexpr spi::SPIBitOrder spi_bit_order = spi::BIT_ORDER_MSB_FIRST;
static constexpr spi::SPIClockPolarity spi_clock_polarity = spi::CLOCK_POLARITY_LOW;
static constexpr spi::SPIClockPhase spi_clock_phase = spi::CLOCK_PHASE_LEADING;
static constexpr spi::SPIDataRate spi_data_rate = (spi::SPIDataRate)12000000;

class IT8951EDisplay: public display::DisplayBuffer,
                      public spi::SPIDevice<spi_bit_order, spi_clock_polarity, spi_clock_phase, spi_data_rate>
{
  public:
    IT8951EDisplay();

#ifdef USE_LOOP_PRIORITY
    float get_loop_priority() const override;
#endif

    float get_setup_priority() const override;

    void set_reset_pin(GPIOPin *pin);
    void set_ready_pin(GPIOPin *pin);
    void set_cs_pin(GPIOPin *pin);
    void set_reversed(bool reversed);
    void set_full_refresh_interval(uint32_t ms);
    void set_full_refresh_min_idle(uint32_t ms);

    void setup() override;
    void update() override;
    void clear();
    void dump_config() override;

    display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_GRAYSCALE; }

    void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                        display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad);
  protected:
    void init_internal_(uint32_t buffer_length);
    void draw_absolute_pixel_internal(int x, int y, Color color) override;

    int get_width_internal() override;
    int get_height_internal() override;

  private:
    class Impl;
    std::unique_ptr<Impl> m;
};

template<typename... Ts> class ClearAction : public Action<Ts...>, public Parented<IT8951EDisplay> {
public:
void play(Ts... x) override { this->parent_->clear(); }
};

}  // namespace it8951e
}  // namespace esphome
