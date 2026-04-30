#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include <string>

class WebServer;
class DNSServer;

namespace esphome {
namespace si_provisioning {

struct ProvData {
  bool provisioned;
  char mqtt_user[64];
  char mqtt_pass[64];
  char mqtt_topic_prefix[64];
};

class SiProvisioning : public Component {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return 250.0f; }

  void set_device_type(const std::string &v) { device_type_ = v; }
  void set_register_endpoint(const std::string &v) { register_endpoint_ = v; }

  void boot_apply();
  void wipe();

 protected:
  void start_provisioning_portal_();
  void handle_form_();
  void handle_scan_();
  void handle_provision_();
  bool perform_registration_(const std::string &ssid,
                             const std::string &password,
                             const std::string &reg_code,
                             std::string &err_out);
  void persist_and_reboot_(const std::string &user,
                           const std::string &pass,
                           const std::string &topic_prefix,
                           const std::string &ssid,
                           const std::string &wifi_pass);
  std::string mac_suffix_(uint8_t bytes) const;
  std::string ap_ssid_() const;

  std::string device_type_;
  std::string register_endpoint_;

  ESPPreferenceObject pref_;
  ProvData data_{};

  WebServer *server_{nullptr};
  DNSServer *dns_{nullptr};
  bool portal_active_{false};
  uint32_t portal_started_at_{0};
};

}  // namespace si_provisioning
}  // namespace esphome
