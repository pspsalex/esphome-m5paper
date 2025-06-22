// SPDX-License-Identifier: GPL-3.0-or-later

#include "esphome/core/log.h"
#include "it8951e.h"
#include "it8951e_priv.h"
#include "esphome/core/application.h"
#include "esphome/core/gpio.h"

#include <list>
#include <memory>


namespace esphome {
namespace it8951e {

static const char *TAG = "it8951e.display";

static constexpr uint16_t PREAMBLE_COMMAND = 0x6000;
static constexpr uint16_t PREAMBLE_WRITE_DATA = 0x0000;
static constexpr uint16_t PREAMBLE_READ_DATA = 0x1000;


#ifdef ARDUINO
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#else
using std::make_unique;
#endif


#if defined(IT8951E_ENABLE_DEBUG_LOGGING) && (IT8951E_ENABLE_DEBUG_LOGGING==1)
#define IT8951E_LOGD ESP_LOGD
#else
#define IT8951E_LOGD(...)
#endif

class IT8951EDisplay::Impl
{
  public:
    Impl(IT8951EDisplay *parent) : parent(parent) {}

    void setup();
    void clear(bool const init) const;
    void write_buffer_to_display(uint16_t const x, uint16_t const y, uint16_t const w, uint16_t const h) const;
    void notify_update(uint16_t const x, uint16_t const y, uint16_t const w, uint16_t const h);

    size_t get_buffer_size() const;
    void init_buffer(size_t buffer_size);
    void put_pixel(int const x, int const y, Color const color);
    void do_update();

    char lut_version[17] = {0};
    char fw_version[17] = {0};

    uint16_t width = 960;
    uint16_t height = 540;

    bool reversed = false;

    GPIOPin *reset_pin = nullptr;
    GPIOPin *ready_pin = nullptr;
    GPIOPin *cs_pin = nullptr;

  private:
    IT8951EDisplay *parent;

    struct Rect {
        uint16_t x, y, w, h;
    };

    std::list<Rect> update_areas;

    uint8_t *buffer = nullptr;

    uint32_t last_update_time = 0;
    bool schedule_clean = false;

    uint16_t image_buffer_address_high = 0x0012;
    uint16_t image_buffer_address_low = 0x36e0;

    void reset();

    bool wait_comms_ready(uint32_t const timeout = 3000) const;
    bool wait_display_ready(uint32_t const timeout = 3000) const;

    void send_command(Command const command) const;
    void send_command_with_args(Command const cmd, uint16_t const * const args, uint16_t const length) const;
    void write_word(uint16_t const data) const;
    uint16_t read_word() const;
    void read_bytes(void * const buf, uint16_t const length) const;
    void update_device_info();

    uint16_t read_register(Register const address) const;
    void write_register(Register const address, uint16_t const data) const;
    void set_area(uint16_t const x, uint16_t const y, uint16_t const w, uint16_t const h) const;
    void update_area(uint16_t const x, uint16_t const y, uint16_t const w, uint16_t const h, UpdateMode const mode) const;
    void set_target_memory_addr(uint16_t const address_high, uint16_t const address_low) const;

};


/**
 * @brief Guard class to allow the CS pin to be automatically deactivated
 */
class SelectDevice
{
    public:
        SelectDevice(GPIOPin *pin) : cs_pin(pin) { this->cs_pin->digital_write(false); }
        ~SelectDevice() { this->cs_pin->digital_write(true); }

    private:
        GPIOPin *cs_pin;
};


/**
 * @brief Allocate memory for the local screen buffer
 * @param buffer_size Size of buffer to allocate
 */
void IT8951EDisplay::Impl::init_buffer(size_t buffer_size)
{
    ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
    this->buffer = allocator.allocate(buffer_size);
    if (this->buffer == nullptr)
    {
        ESP_LOGE(TAG, "Could not allocate buffer for display!");
        return;
    }
}


/**
 * @brief Setup the display and needed GPIO pins
 */
void IT8951EDisplay::Impl::setup()
{

    this->reset_pin->setup();
    this->reset_pin->digital_write(true);
    this->reset_pin->pin_mode(gpio::FLAG_OUTPUT);
    this->reset();

    this->cs_pin->setup();
    this->cs_pin->digital_write(true);
    this->cs_pin->pin_mode(gpio::FLAG_OUTPUT);

    this->ready_pin->setup();
    this->ready_pin->pin_mode(gpio::FLAG_INPUT);

    this->update_device_info();

    this->init_buffer(this->get_buffer_size());

    this->send_command(Command::TCON_SYS_RUN);

    this->write_register(Register::I80PCR, 0x0001);

    // Set vcom to -2.30v
    IT8951E_LOGD(TAG, "Set VCOM");
    uint16_t const args[2] = {0x0001, 2300};
    this->send_command_with_args(Command::I80_CMD_VCOM, args, 2);
}


/**
 * @brief Reset the display
 */
void IT8951EDisplay::Impl::reset()
{
    this->reset_pin->digital_write(true);
    this->reset_pin->digital_write(false);
    delay(20);
    this->reset_pin->digital_write(true);
    delay(100);
}


/**
 * @brief Block until the ready pin goes high
 * @param timeout Timeout in ms
 *
 * @return true if ready pin is high within the timeout, false otherwise
 */
bool IT8951EDisplay::Impl::wait_comms_ready(uint32_t const timeout) const
{
    uint32_t const start_time = millis();
    while (millis() - start_time < timeout)
    {
        if (this->ready_pin->digital_read())
        {
            return true;
        }
        delay(10);
    }
    return false;
}


/**
 * @brief Send a command to the display
 * @param command Command to write
 */
void IT8951EDisplay::Impl::send_command(Command const command) const
{
    IT8951E_LOGD(TAG, "Write command 0x%02x", command);
    if (!this->wait_comms_ready())
    {
        ESP_LOGE(TAG, "Display busy trying to write preamble for command 0x%04x", command);

        return;
    }

    SelectDevice display(this->cs_pin);

    this->parent->write_byte16(PREAMBLE_COMMAND);

    if (!this->wait_comms_ready())
    {
        ESP_LOGE(TAG, "Display busy trying to write command 0x%04x", command);
        return;
    }

    this->parent->write_byte16(static_cast<uint16_t>(command));
}


/**
 * @brief Write a word (uint16_t) to the display
 * @param data Data to write
 */
void IT8951EDisplay::Impl::write_word(uint16_t const data) const
{
    IT8951E_LOGD(TAG, "Write word 0x%04x", data);
    if (!this->wait_comms_ready())
    {
        ESP_LOGE(TAG, "Display busy trying to write preamble for writing 0x%04x", data);
        return;
    }

    SelectDevice display(this->cs_pin);
    this->parent->write_byte16(PREAMBLE_WRITE_DATA);

    if (!this->wait_comms_ready())
    {
        ESP_LOGE(TAG, "Display busy trying to write 0x%04x", data);
        return;
    }
    this->parent->write_byte16(data);
}


/**
 * @brief Read a multiple bytes from the display.
 *
 * Read multiple bytes from the display into the given address.
 *
 * A read shorter than 4 bytes will read 4 bytes from the device over SPI and return only
 * the requested number of bytes.
 *
 * Chaining multiple read_bytes calls, with sizes less than 4 bytes may result in a
 * misaligned read and data loss.
 *
 * This is confirmed on ESP32 when DMA is enabled (automatically usually)
 *
 * @arg buf Pointer to the buffer to read into
 * @arg length Number of bytes to read
 */
void IT8951EDisplay::Impl::read_bytes(void * const buf, uint16_t const length) const
{
    if (!this->wait_comms_ready())
    {
        ESP_LOGE(TAG, "Display not ready to receive read data preamble");
        return;
    }

    SelectDevice display(this->cs_pin);
    this->parent->write_byte16(PREAMBLE_READ_DATA);

    if (!this->wait_comms_ready())
    {
        ESP_LOGE(TAG, "Display not ready to receive read data dummy bytes");
        return;
    }

    this->parent->write_byte16(PREAMBLE_WRITE_DATA);
    if (!this->wait_comms_ready())
    {
        ESP_LOGE(TAG, "Display not ready to send data");
        return;
    }

    this->parent->transfer_array(reinterpret_cast<uint8_t *>(buf), length);
}


/**
 * @brief Read a word (uint16_t) from the display
 *
 * @return uint16_t The word read from the display
 */
uint16_t IT8951EDisplay::Impl::read_word() const
{
    uint16_t read_data;
    this->read_bytes(&read_data, 2);

    // IT8951E is big-endian and ESP32 is little-endian
    return ((read_data & 0xFF00) >> 8) | ((read_data & 0x00FF) << 8);
}


/**
 * @brief Send to the display a command with arguments
 *
 * @param cmd Command to send
 * @param args Arguments to send, uint16_t. Endianness conversion is done automatically.
 * @param length Number of arguments
 */
void IT8951EDisplay::Impl::send_command_with_args(Command const cmd, uint16_t const * const args, uint16_t const length) const
{
    this->send_command(cmd);
    if (!this->wait_comms_ready())
    {
        ESP_LOGE(TAG, "Display not ready to receive command arguments preamble");
        return;
    }

    SelectDevice display(this->cs_pin);
    this->parent->write_byte16(PREAMBLE_WRITE_DATA);

    for (uint16_t argument = 0; argument < length; argument++)
    {
        if (!this->wait_comms_ready())
        {
            ESP_LOGE(TAG, "Display not ready to receive command argument #%d", argument);
            return;
        }
        this->parent->write_byte16(args[argument]);
    }
}


/**
 * @brief Read a display register
 * @param address Register address
 *
 * @return Register value
 */
uint16_t IT8951EDisplay::Impl::read_register(Register const address) const
{
    this->send_command(Command::TCON_REG_RD);
    this->write_word(static_cast<uint16_t>(address));
    uint16_t word = this->read_word();
    return word;
}


/**
 * @brief Write a display register
 * @param address Register address
 * @param data Data to write to register
 */
void IT8951EDisplay::Impl::write_register(Register const address, uint16_t const data) const {
    this->send_command(Command::TCON_REG_WR);  // tcon write reg command
    this->write_word(static_cast<uint16_t>(address));
    this->write_word(data);
}


/**
 * @brief Block until the display is ready
 * @param timeout Timeout in ms
 *
 * @return true if display is ready within the timeout, false otherwise
 */
bool IT8951EDisplay::Impl::wait_display_ready(uint32_t const timeout) const
{
    uint32_t const start_time = millis();
    while (millis() - start_time > timeout)
    {
        if (this->read_register(Register::LUTAFSR) == 0)
        {
            return true;
        }
        App.feed_wdt();
    }
    return false;
}


/**
 * @brief Sets the width, height, and image buffer address based on the info from the display
 */
void IT8951EDisplay::Impl::update_device_info()
{
    uint8_t device_info[40] = {};

    this->send_command(Command::I80_CMD_GET_DEV_INFO);
    this->read_bytes(device_info, 40);

    uint16_t width = (device_info[0] << 8) | device_info[1];
    uint16_t height = (device_info[2] << 8) | device_info[3];

    if ( (width < 50) || (width > 2048) || (height < 50) || (height > 2048))
    {
        ESP_LOGE(TAG, "Implausible display dimensions: %d x %d. Check the SPI clock speeds", width, height);
        this->parent->mark_failed();
        return;
    }

    this->width = width;
    this->height = height;

    this->image_buffer_address_low = (device_info[4] << 8) | device_info[5];
    this->image_buffer_address_high = (device_info[6] << 8) | device_info[7];

    memcpy(lut_version, device_info + 8, 16);
    memcpy(fw_version, device_info + 24, 16);
    lut_version[16] = 0;
    fw_version[16] = 0;

    IT8951E_LOGD(TAG, "Width: %d, Height: %d, LUT: %s, FW: %s, Mem:%x%04x",
        width,
        height,
        lut_version,
        fw_version,
        this->image_buffer_address_high,
        this->image_buffer_address_low
    );
}


/**
 * @brief Set the image area the image buffer gets rendered into
 *
 * By default the image format is considered to be big endian, 4 bits per pixel (16 grayscale levels)
 *
 * @param x X Coordinate of the draw window. Must be a multiple of 4
 * @param y Y Coordinate of the draw window
 * @param w Width of the draw window. Must be a multiple of 4
 * @param h Height of the draw window.
 */
void IT8951EDisplay::Impl::set_area(uint16_t const x, uint16_t const y, uint16_t const w, uint16_t const h) const
{
    uint16_t args[5];
    args[0] = (static_cast<uint16_t>(Endianness::BIG) << 8) | (static_cast<uint16_t>(PixelMode::BPP_4) << 4) | (static_cast<uint16_t>(Rotation::ROTATE_0));
    args[1] = (x + 3) & 0xFFFC;
    args[2] = y;
    args[3] = (w + 3) & 0xFFFC;
    args[4] = h;
    this->send_command_with_args(Command::TCON_LD_IMG_AREA, args, 5);
}


/**
 * @brief Refresh the given EPD area with the data in the framebuffer.
 * @param x X Coordinate of the area
 * @param y Y Coordinate of the area
 * @param w Area width
 * @param h Area height
 * @param mode Display update mode. See enum for more info on the modes
 */
void IT8951EDisplay::Impl::update_area(uint16_t const x, uint16_t const y, uint16_t const w, uint16_t const h, UpdateMode const mode) const
{
    if (mode == UpdateMode::None)
    {
        return;
    }

    uint16_t args[7];
    args[0] = (x + 3) & 0xFFFC;
    args[1] = y;
    args[2] = ((((x + w) > this->width) ? (this->width - x) : w) + 3) & 0xFFFC;
    args[3] = ((y + h) > this->height) ? (this->height - y) : h;
    args[4] = static_cast<uint16_t>(mode);
    args[5] = this->image_buffer_address_low;
    args[6] = this->image_buffer_address_high;

    this->wait_display_ready();
    this->send_command_with_args(Command::I80_CMD_DPY_BUF_AREA, args, 7);
}


/**
 * @brief Set the buffer memory address for the display
 * @param address_high High-word of the IT8951E buffer address
 * @param address_low Low-word of the IT8951E buffer address
 */
void IT8951EDisplay::Impl::set_target_memory_addr(uint16_t const address_high, uint16_t const address_low) const
{
    this->write_register(Register::LISARH, address_high);
    this->write_register(Register::LISAR, address_low);
}


/**
 * @brief Clear display
 * @param init If true, a display update is performed, clearing the display irrespective of the buffer data
 */
void IT8951EDisplay::Impl::clear(bool const init) const
{
    this->set_target_memory_addr(this->image_buffer_address_high, this->image_buffer_address_low);
    this->set_area(0, 0, width, height);

    if (this->buffer)
    {
        SelectDevice display(this->cs_pin);
        this->parent->write_byte16(PREAMBLE_WRITE_DATA);
        memset(this->buffer, this->reversed ? 0x00 : 0xFF, this->get_buffer_size());
        this->parent->transfer_array(this->buffer, this->get_buffer_size());
    }

    this->send_command(Command::TCON_LD_IMG_END);

    if (init)
    {
        this->update_area(0, 0, width, height, UpdateMode::Init);
    }
}


/**
 * @brief Get the size of the local display buffer
 */
size_t IT8951EDisplay::Impl::get_buffer_size() const
{
    return this->width * this->height / 2;
}


/**
 * @brief Write the image at the specified location, Partial update
 * @param x X coordinate of the draw window. Will be rounded up to the nearest multiple of 4
 * @param y Y coordinate of the draw window
 * @param w Draw window width. Will be rounded up to the nearest multiple of 4
 * @param h Draw window height
 * @param buffer Pointer to the buffer containing the image data
 */
void IT8951EDisplay::Impl::write_buffer_to_display(uint16_t const x, uint16_t const y, uint16_t const w, uint16_t const h) const
{
    if (buffer == nullptr)
    {
        ESP_LOGE(TAG, "No buffer to read data from");
        return;
    }

    if ((x > this->width) || (y > this->height))
    {
        ESP_LOGE(TAG, "Pos (%d, %d) out of bounds.", x, y);
        return;
    }

    this->set_target_memory_addr(this->image_buffer_address_high, this->image_buffer_address_low);
    this->set_area(x, y, w, h);

    {
        SelectDevice display(this->cs_pin);
        this->parent->write_byte16(PREAMBLE_WRITE_DATA);
        for (uint32_t cursor_y = y; cursor_y < y + h; cursor_y++) {
            uint32_t pos = cursor_y*(this->width >> 1) + (((x + 3) & 0xFFFC) >> 1);
            this->parent->write_array(buffer + pos, ((w + 3) & 0xFFFC) >> 1);
        }
    }

    this->send_command(Command::TCON_LD_IMG_END);

    this->update_area(x, y, w, h, UpdateMode::GLR16);
}


/**
 * @brief Write a pixel in the internal frame buffer at the specified location with the mentioned RGB color
 * @param x X coordinate of the pixel
 * @param y Y coordinate of the pixel
 * @param Color color of the pixel
 */
void HOT IT8951EDisplay::Impl::put_pixel(int const x, int const y, Color const color)
{
    // Validation happens outside this function
    uint32_t internal_color = ((color.r*77) + (color.g*151) + (color.b*28)) >> 12;
    int32_t index = y * (this->width >> 1) + (x >> 1);

    if (this->reversed)
    {
        internal_color = (~internal_color) & 0xFu;
    }

    if (x & 0x1)
    {
        this->buffer[index] &= 0xF0;
        this->buffer[index] |= internal_color;
    }
    else
    {
        this->buffer[index] &= 0x0F;
        this->buffer[index] |= internal_color << 4;
    }
}


/**
 * @brief Notify the display that the buffer has been updated
 * @param x X coordinate of the updated image region
 * @param y Y coordinate of the updated image region
 * @param w Width of the updated area
 * @param h Height of the updated area
 */
void IT8951EDisplay::Impl::notify_update(uint16_t const x, uint16_t const y, uint16_t const w, uint16_t const h)
{
    IT8951E_LOGD(TAG, "Notify update: %d, %d, %d, %d", x, y, w, h);
    // Check if two rectangles overlap
    auto overlap = [](const Rect &a, const Rect &b)
    {
        return !(((a.x + a.w) <= b.x) || ((b.x + b.w) <= a.x) || ((a.y + a.h) <= b.y) || ((b.y + b.h) <= a.y));
    };

    // Merge two rectangles
    auto merge = [](const Rect &a, const Rect &b)
    {
        uint16_t x = std::min(a.x, b.x);
        uint16_t y = std::min(a.y, b.y);
        uint16_t w = std::max(a.x + a.w, b.x + b.w) - x;
        uint16_t h = std::max(a.y + a.h, b.y + b.h) - y;
        return Rect{x, y, w, h};
    };

    Rect new_rect = {x, y, w, h};

    bool merged = false;

    for (auto &rect : this->update_areas)
    {
        if (overlap(rect, new_rect))
        {
            IT8951E_LOGD(TAG, "(%d, %d, %d, %d) overlaps (%d, %d, %d, %d)", rect.x, rect.y, rect.w, rect.h, new_rect.x, new_rect.y, new_rect.w, new_rect.h);
            rect = merge(rect, new_rect);
            IT8951E_LOGD(TAG, "Merged into (%d, %d, %d, %d)", rect.x, rect.y, rect.w, rect.h);
            merged = true;
            break;
        }
    }

    if (!merged)
    {
        IT8951E_LOGD(TAG, "Pushing (%d, %d, %d, %d)", new_rect.x, new_rect.y, new_rect.w, new_rect.h);
        this->update_areas.push_back(new_rect);
    }
}


/**
 * @brief Transfer the local frame buffer to the display, and update the EPD.
 */
void IT8951EDisplay::Impl::do_update()
{
    if (this->update_areas.size())
    {
        for (auto &rect : this->update_areas)
        {
            IT8951E_LOGD(TAG, "Pushing area (%d, %d) --> (%d, %d) to display", rect.x, rect.y, rect.x + rect.w, rect.y + rect.h);
            this->write_buffer_to_display(rect.x, rect.y, rect.w, rect.h);
        }
        this->update_areas.clear();
        this->last_update_time = millis();
        this->schedule_clean = true;
    }

    if ((this->schedule_clean) && (millis() - this->last_update_time > 20000))
    {
        // Display data is already transferred, the IT8951E must only refresh the EPD
        IT8951E_LOGD(TAG, "Inactivity - cleaning display.");
        this->update_area(0, 0, this->width, this->height, UpdateMode::GC16);
        this->last_update_time = millis();
        this->schedule_clean = false;
    }
}



/**
 * @brief Main constructor
 */
IT8951EDisplay::IT8951EDisplay() :
    display::DisplayBuffer(),
    spi::SPIDevice<spi_bit_order, spi_clock_polarity, spi_clock_phase, spi_data_rate>()
{
    this->m = make_unique<Impl>(this);
}


/**
 * @brief Get the loop priority. Defines when during the main loop() the display update is checked
 * @return 0.0f, the loop priority
 */
float IT8951EDisplay::get_loop_priority() const
{
    return 0.0f;
}


/**
 * @brief Get the setup priority. Should initialize the display before other components like lvgl
 * @return HARDWARE, the setup priority
 */
float IT8951EDisplay::get_setup_priority() const
{
    return setup_priority::HARDWARE;
}


/**
 * @brief Set the reset pin
 * @param pin Reset pin
 */
void IT8951EDisplay::set_reset_pin(GPIOPin *pin)
{
    this->m->reset_pin = pin;
}


/*
 * @brief Set the ready pin
 * @param pin Ready pin
 */
void IT8951EDisplay::set_ready_pin(GPIOPin *pin)
{
    this->m->ready_pin = pin;
}


/**
 * @brief Set the chip select pin
 * @param pin Chip select pin
 */
void IT8951EDisplay::set_cs_pin(GPIOPin *pin)
{
    this->m->cs_pin = pin;
}


/**
 * @brief Get the width of the display
 * @return width of the display
 */
int IT8951EDisplay::get_width_internal()
{
    return this->m->width;
}


/**
 * @brief Get the height of the display
 * @return height of the display
 */
int IT8951EDisplay::get_height_internal()
{
    return this->m->height;
}


/**
 * @brief Initialize the local display buffer
 * @param buffer_length Length of the buffer
 */
void IT8951EDisplay::init_internal_(uint32_t buffer_length)
{
    this->m->init_buffer(buffer_length);
}


/**
 * @brief Clear the display
 */
void IT8951EDisplay::clear()
{
    this->m->clear(true);
}


/**
 * @brief Setup the display
 */
void IT8951EDisplay::setup()
{
    IT8951E_LOGD(TAG, "Init Starting.");

    this->spi_setup();

    this->m->setup();

    IT8951E_LOGD(TAG, "Clearing display...");
    this->m->clear(true);

    IT8951E_LOGD(TAG, "Init SUCCESS.");
}


/**
 * @brief Update the display. Called in the main loop
 */
void IT8951EDisplay::update()
{
    this->do_update_();
    this->m->do_update();
}


/**
 * @brief Draw pixels from a given input buffer to the specified location
 *
 * The function is overloaded to allow a more efficient scheduling of the display update and input parameter validation
 *
 * @param x_start X coordinate of the top left corner
 * @param y_start Y coordinate of the top left corner
 * @param w Width of the image
 * @param h Height of the image
 * @param ptr Pointer to the source image data
 * @param order Color order
 * @param bitness Color bitness
 * @param big_endian Endianness
 * @param x_offset X offset
 * @param y_offset Y offset
 * @param x_pad X padding
 */
void IT8951EDisplay::draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                             display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad)
{

    if (ptr == nullptr)
    {
        return;
    }

    if ((x_start >= this->m->width) || (y_start >= this->m->height) || (x_start < 0) || (y_start < 0) || (w <= 0) || (h <= 0))
    {
        return;
    }

    if ((x_start + w) > this->m->width)
    {
        w = this->m->width - x_start;
    }

    if ((y_start + h) > this->m->height)
    {
        h = this->m->height - y_start;
    }

    Display::draw_pixels_at(x_start, y_start, w, h, ptr, order, bitness, big_endian, x_offset, y_offset, x_pad);

    this->m->notify_update(x_start, y_start, w, h);
}


/**
 * @brief Set the display to reversed mode.
 * @param reversed Reverses display colors if true.
 */
void IT8951EDisplay::set_reversed(bool reversed)
{
    this->m->reversed = reversed;
}


/**
 * @brief Draw a pixel at the specified location
 * @param x X coordinate of the pixel
 * @param y Y coordinate of the pixel
 * @param color Color of the pixel
 */
void HOT IT8951EDisplay::draw_absolute_pixel_internal(int x, int y, Color color)
{
    this->m->put_pixel(x, y, color);
}


/**
 * @brief Dump display information
 */
void IT8951EDisplay::dump_config()
{
    ESP_LOGCONFIG(TAG, "IT8951E:");
    ESP_LOGCONFIG(TAG, "  Size: %dx%d (WxH)", this->m->width, this->m->height);
    ESP_LOGCONFIG(TAG, "  Reversed: %s", (this->m->reversed ? "yes" : "no"));
    ESP_LOGCONFIG(TAG, "  FW version:  '%s'", this->m->fw_version);
    ESP_LOGCONFIG(TAG, "  LUT version: '%s'", this->m->lut_version);
}

}  // namespace empty_spi_sensor
}  // namespace esphome
