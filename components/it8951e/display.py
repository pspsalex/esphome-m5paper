# SPDX-License-Identifier: GPL-3.0-or-later

import esphome.codegen as cg
from esphome import pins
from esphome import automation
import esphome.config_validation as cv
from esphome.components import display, spi
from esphome.const import (
    CONF_NAME,
    CONF_ID,
    CONF_RESET_PIN,
    CONF_PAGES,
    CONF_LAMBDA,
    CONF_REVERSED,
)

from esphome.const import __version__ as ESPHOME_VERSION

DEPENDENCIES = ['spi']

CONF_DISPLAY_CS_PIN = "display_cs_pin"
CONF_READY_PIN = "ready_pin"

it8951e_ns = cg.esphome_ns.namespace('it8951e')
IT8951EDisplay = it8951e_ns.class_(
    'IT8951EDisplay', cg.PollingComponent, spi.SPIDevice, display.DisplayBuffer
)
ClearAction = it8951e_ns.class_("ClearAction", automation.Action)

CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(IT8951EDisplay),
            cv.Optional(CONF_NAME): cv.string,
            cv.Required(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_READY_PIN): pins.gpio_input_pin_schema,
            cv.Required(CONF_DISPLAY_CS_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_REVERSED): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(spi.spi_device_schema(cs_pin_required=False)),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)

@automation.register_action(
    "IT8951E.clear",
    ClearAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(IT8951EDisplay),
        }
    ),
)
async def bm8563_read_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
    if cv.Version.parse(ESPHOME_VERSION) < cv.Version.parse("2023.12.0"):
        await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    if CONF_DISPLAY_CS_PIN in config:
        cs = await cg.gpio_pin_expression(config[CONF_DISPLAY_CS_PIN])
        cg.add(var.set_cs_pin(cs))
    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))
    if CONF_READY_PIN in config:
        ready = await cg.gpio_pin_expression(config[CONF_READY_PIN])
        cg.add(var.set_ready_pin(ready))
    if CONF_REVERSED in config:
        cg.add(var.set_reversed(config[CONF_REVERSED]))
