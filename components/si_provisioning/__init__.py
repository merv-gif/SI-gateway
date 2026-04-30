import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE

DEPENDENCIES = ["wifi", "mqtt"]
AUTO_LOAD = ["json"]

si_provisioning_ns = cg.esphome_ns.namespace("si_provisioning")
SiProvisioning = si_provisioning_ns.class_("SiProvisioning", cg.Component)

CONF_DEVICE_TYPE = "device_type"
CONF_REGISTER_ENDPOINT = "register_endpoint"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SiProvisioning),
        cv.Required(CONF_DEVICE_TYPE): cv.one_of("pool", "water", lower=True),
        cv.Required(CONF_REGISTER_ENDPOINT): cv.url,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_device_type(config[CONF_DEVICE_TYPE]))
    cg.add(var.set_register_endpoint(config[CONF_REGISTER_ENDPOINT]))

    # Slimmed deps — captive portal + web server are now ESPHome's built-ins.
    # We only need WiFi (for MAC) and HTTPClient + WiFiClientSecure for the
    # registration POST.
    if CORE.using_arduino:
        cg.add_library("WiFi", None)
        if CORE.is_esp32:
            cg.add_library("HTTPClient", None)
            cg.add_library("WiFiClientSecure", None)
