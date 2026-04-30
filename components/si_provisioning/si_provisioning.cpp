#include "si_provisioning.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/preferences.h"
#include "esphome/components/mqtt/mqtt_client.h"
#include "esphome/components/json/json_util.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <string.h>

namespace esphome {
namespace si_provisioning {

static const char *const TAG = "si_provisioning";

// Stable hash for our NVS slot. Don't change across firmware versions.
static constexpr uint32_t SI_PROV_PREF_HASH = 0x5170526Fu;  // "SiPro"

void SiProvisioning::setup() {
  ESP_LOGI(TAG, "si_provisioning ready (device_type=%s)", device_type_.c_str());
}

void SiProvisioning::boot_apply() {
  pref_ = global_preferences->make_preference<ProvData>(SI_PROV_PREF_HASH);
  if (!pref_.load(&data_)) {
    data_ = ProvData{};
  }

  ESP_LOGI(TAG, "boot_apply: provisioned=%s", data_.provisioned ? "true" : "false");

  if (data_.provisioned && data_.mqtt_user[0] != '\0') {
    auto *c = mqtt::global_mqtt_client;
    c->set_username(data_.mqtt_user);
    c->set_password(data_.mqtt_pass);
    std::string prefix(data_.mqtt_topic_prefix);
    c->set_topic_prefix(prefix, prefix);
    ESP_LOGI(TAG, "Applied stored MQTT creds (user=%s, prefix=%s)",
             data_.mqtt_user, data_.mqtt_topic_prefix);
  }
}

void SiProvisioning::register_with_code(const std::string &code) {
  if (data_.provisioned) {
    ESP_LOGW(TAG, "Device already provisioned. Press 'Wipe Provisioning' first to re-register.");
    return;
  }
  if (code.empty()) {
    ESP_LOGE(TAG, "Empty registration code — type the code first, then press Register.");
    return;
  }

  std::string err;
  if (!perform_registration_(code, err)) {
    ESP_LOGE(TAG, "Registration failed: %s", err.c_str());
  }
}

void SiProvisioning::wipe() {
  ESP_LOGW(TAG, "Provisioning wiped — rebooting");
  ProvData blank{};
  pref_.save(&blank);
  global_preferences->sync();
  delay(500);
  App.safe_reboot();
}

bool SiProvisioning::perform_registration_(const std::string &reg_code,
                                           std::string &err_out) {
  // We deliberately don't check WiFi.status() — ESPHome bypasses Arduino's
  // WiFi event handlers and that status can read stale even when the device
  // is happily on WiFi (which it must be, since this is triggered from a
  // button on web_server). If WiFi is genuinely down, https.begin() below
  // will fail with a meaningful error.

  uint8_t mac[6];
  WiFi.macAddress(mac);
  char mac_str[18];
  snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  std::string body = "{\"mac\":\"" + std::string(mac_str) +
                     "\",\"device_type\":\"" + device_type_ +
                     "\",\"registration_code\":\"" + reg_code +
                     "\",\"firmware\":\"" ESPHOME_VERSION "\"}";

  ESP_LOGI(TAG, "Registering: code=%s mac=%s endpoint=%s",
           reg_code.c_str(), mac_str, register_endpoint_.c_str());

  // TODO(prod): replace setInsecure() with client.setCACert(ISRG_ROOT_X1).
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15000);

  HTTPClient https;
  if (!https.begin(client, register_endpoint_.c_str())) {
    err_out = "HTTPS begin failed";
    return false;
  }
  https.addHeader("Content-Type", "application/json");

  int code = https.POST((uint8_t *) body.data(), body.size());
  if (code != 200) {
    err_out = "Server returned HTTP " + std::to_string(code);
    https.end();
    return false;
  }

  String resp = https.getString();
  https.end();
  ESP_LOGI(TAG, "Registration response: %s", resp.c_str());

  std::string user, pass, prefix;
  bool ok = json::parse_json(resp.c_str(), [&](JsonObject root) -> bool {
    if (!root["mqtt_username"].is<const char *>() ||
        !root["mqtt_password"].is<const char *>() ||
        !root["topic_prefix"].is<const char *>()) {
      return false;
    }
    user = root["mqtt_username"].as<const char *>();
    pass = root["mqtt_password"].as<const char *>();
    prefix = root["topic_prefix"].as<const char *>();
    return true;
  });
  if (!ok || user.empty() || prefix.empty()) {
    err_out = "Malformed registration response";
    return false;
  }

  this->persist_and_reboot_(user, pass, prefix);
  return true;
}

void SiProvisioning::persist_and_reboot_(const std::string &user,
                                         const std::string &pass,
                                         const std::string &topic_prefix) {
  ESP_LOGI(TAG, "Persisting MQTT creds (user=%s, prefix=%s)",
           user.c_str(), topic_prefix.c_str());

  ProvData d{};
  d.provisioned = true;
  strncpy(d.mqtt_user, user.c_str(), sizeof(d.mqtt_user) - 1);
  strncpy(d.mqtt_pass, pass.c_str(), sizeof(d.mqtt_pass) - 1);
  strncpy(d.mqtt_topic_prefix, topic_prefix.c_str(), sizeof(d.mqtt_topic_prefix) - 1);

  pref_.save(&d);
  global_preferences->sync();

  delay(500);
  ESP_LOGI(TAG, "Rebooting into provisioned mode...");
  App.safe_reboot();
}

}  // namespace si_provisioning
}  // namespace esphome
