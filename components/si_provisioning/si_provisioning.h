#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include <string>

namespace esphome {
namespace si_provisioning {

// Persisted state. Fixed sizes keep this a POD so ESPHome's preferences
// API can serialise it as a single blob.
struct ProvData {
  bool provisioned;
  char mqtt_user[64];
  char mqtt_pass[64];
  char mqtt_topic_prefix[64];
};

// V2 design: the AP-mode WiFi setup is now handled by ESPHome's stock
// captive_portal: component. This class only handles the registration
// step — POST to the provisioning server, persist returned creds, reboot.
//
// Triggered from a YAML button on_press lambda once the device is on WiFi.
class SiProvisioning : public Component {
 public:
  void setup() override;
  float get_setup_priority() const override { return 250.0f; }

  void set_device_type(const std::string &v) { device_type_ = v; }
  void set_register_endpoint(const std::string &v) { register_endpoint_ = v; }

  // Called from on_boot (priority 250) before MQTT connects. Reads stored
  // creds and applies them to the global MQTT client.
  void boot_apply();

  // Called from a button on_press lambda. Performs the HTTPS POST, persists
  // returned creds, reboots into provisioned mode.
  void register_with_code(const std::string &code);

  // Clear stored creds and reboot. Useful for re-provisioning.
  void wipe();

  bool is_provisioned() const { return data_.provisioned; }

 protected:
  bool perform_registration_(const std::string &reg_code, std::string &err_out);
  void persist_and_reboot_(const std::string &user,
                           const std::string &pass,
                           const std::string &topic_prefix);

  std::string device_type_;
  std::string register_endpoint_;

  ESPPreferenceObject pref_;
  ProvData data_{};
};

}  // namespace si_provisioning
}  // namespace esphome
