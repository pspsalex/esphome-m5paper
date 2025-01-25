# esphome driver for IT8951E driven EPD

The driver was developed for the M5Paper, but should work for other devices that use this display.

To setup the driver, configure the SPI bus and a display element.

The display requires a CS pin, a RESET and a READY GPIO pin to be defined.

Below is a minimal configuration example:

```yaml
# minimal configuration:
external_components:
  - source:
      type: git
      url: https://github.com/pspsalex/esphome-m5paper
      ref: master
    components: [it8951e]

spi:
  - id: epd_spi
    clk_pin: GPIO14
    mosi_pin:
      number: GPIO12
      ignore_strapping_warning: true
    miso_pin: GPIO13
    interface: spi3

display:
  - platform: it8951e
    id: my_display
    display_cs_pin:
      number: GPIO15
      ignore_strapping_warning: true
    reset_pin: GPIO23
    ready_pin: GPIO27
    rotation: 0
    reversed: false
    auto_clear_enabled: false
    update_interval: 100ms
    show_test_card: true
    #lambda: |-
    #    // Print the string "Hello World!" at [0,10]
    #    it.print(0, 10, id(my_font), "Hello World!");
```
