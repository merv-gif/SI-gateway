import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["wifi", "mqtt", "globals"]
AUTO_LOAD = ["json"]

si_provisioning_ns = cg.esphome_ns.namespace("si_provisioning")
SiProvisioning = si_provisioning_ns.class_("SiProvisioning", cg.Component)

CONF_DEVICE_TYPE = "device_type"
CONF_REGISTER_ENDPOINT = "register_endpoint"
CONF_REGISTER_CA_CERT = "register_ca_cert"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SiProvisioning),
        cv.Required(CONF_DEVICE_TYPE): cv.one_of("pool", "water", lower=True),
        cv.Required(CONF_REGISTER_ENDPOINT): cv.url,
        cv.Required(CONF_REGISTER_CA_CERT): cv.string_strict,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_device_type(config[CONF_DEVICE_TYPE]))
    cg.add(var.set_register_endpoint(config[CONF_REGISTER_ENDPOINT]))
    cg.add(var.set_register_ca_cert(config[CONF_REGISTER_CA_CERT]))
    cg.add_library("ESP Async WebServer", None)
    cg.add_library("DNSServer", None)
