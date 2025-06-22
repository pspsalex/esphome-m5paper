import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import i2c, time
from esphome.const import CONF_ID

DEPENDENCIES = ['i2c']

CONF_I2C_ADDR = 0x51

bm8563 = cg.esphome_ns.namespace('bm8563')
BM8563 = bm8563.class_('BM8563', cg.Component, i2c.I2CDevice)
WriteTimeAction = bm8563.class_("WriteTimeAction", automation.Action)
ReadTimeAction = bm8563.class_("ReadTimeAction", automation.Action)
SetAlarmAction = bm8563.class_("SetAlarmAction", automation.Action)

CONFIG_SCHEMA = time.TIME_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(BM8563),
}).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(CONF_I2C_ADDR))

@automation.register_action(
    "bm8563.clear_alarm",
    WriteTimeAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(BM8563),
        }
    ),
)
async def bm8563_clear_alarm_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var

@automation.register_action(
    "bm8563.write_time",
    WriteTimeAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(BM8563),
        }
    ),
)
async def bm8563_write_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var

@automation.register_action(
    "bm8563.read_time",
    ReadTimeAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(BM8563),
        }
    ),
)
async def bm8563_read_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var

CONF_AFTER="after"
@automation.register_action(
    "bm8563.set_fuzzy_alarm",
    SetAlarmAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(BM8563),
            cv.Required(CONF_AFTER): cv.templatable(
                cv.All(
                    cv.positive_time_period_milliseconds,
                    cv.Range(max=cv.TimePeriod(minutes=255)),
                )
            ),
        },
        key=CONF_AFTER,
    ),
)
async def bm8563_set_fuzzy_alarm_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config["after"], args, cg.uint32)
    cg.add(var.set_fuzzy_alarm(template_))
    return var

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await time.register_time(var, config)
