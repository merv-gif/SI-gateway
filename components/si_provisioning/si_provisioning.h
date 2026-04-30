#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include <string>

class AsyncWebServer;
class DNSServer;

namespace esphome {
namespace si_provisioning {

// Owns the AP-mode captive portal (SSID + password + registration code form),
// performs the pinned-TLS POST to the registration endpoint, persists the
// returned MQTT credentials, and (at boot) pushes those credentials into the
// global MQTT client *before* it connects.
class SiProvisioning : public Component {
 public:
  void setup() override;
  void loop() override;
  // Run before mqtt (AFTER_WIFI = 100). Globals restore at ~1000 so we still
  // see the persisted creds.
  float get_setup_priority() const override { return 250.0f; }

  void set_device_type(const std::string &v) { device_type_ = v; }
  void set_register_endpoint(const std::string &v) { register_endpoint_ = v; }
  void set_register_fingerprint(const std::string &v) { register_fingerprint_ = v; }

  // Called from on_boot lambda. Reads globals and either:
  //   - applies stored creds to the global MQTT client and lets normal boot
  //     proceed, OR
  //   - starts the AP + provisioning web server and disables MQTT for now.
  void boot_apply();

 protected:
  void start_provisioning_portal_();
  bool perform_registration_(const std::string &ssid,
                             const std::string &password,
                             const std::string &reg_code,
                             std::string &err_out);
  void persist_and_reboot_(const std::string &user,
                           const std::string &pass,
                           const std::string &topic_prefix,
                           const std::string &ssid,
                           const std::string &wifi_pass);
  std::string mac_suffix_(
