import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE

# web_server_base is what gives us ESPAsyncWebServer + AsyncTCP. Listing it
# as a dependency means ESPHome ensures those libs are available with a
# version compatible with whatever the rest of the firmware is using —
# avoids version-pin conflicts with the user's web_server: block.
DEPENDENCIES = ["wifi", "mqtt", "globals", "web_server_base"]
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

    # Bundled Arduino-ESP32 libs we use directly. ESPAsyncWebServer + AsyncTCP
    # come transitively via the web_server_base dependency above.
    if CORE.using_arduino:
        cg.add_library("FS", None)
        cg.add_library("WiFi", None)
        if CORE.is_esp32:
            cg.add_library("HTTPClient", None)
            cg.add_library("WiFiClientSecure", None)
            cg.add_library("DNSServer", None)
