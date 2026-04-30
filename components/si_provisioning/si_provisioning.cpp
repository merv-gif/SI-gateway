#include "si_provisioning.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/components/mqtt/mqtt_client.h"
#include "esphome/components/json/json_util.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_wifi.h>

namespace esphome {
extern globals::RestoringGlobalsComponent<std::string> *mqtt_user;
extern globals::RestoringGlobalsComponent<std::string> *mqtt_pass;
extern globals::RestoringGlobalsComponent<std::string> *mqtt_topic_prefix;
extern globals::RestoringGlobalsComponent<bool> *provisioned;
}  // namespace esphome

namespace esphome {
namespace si_provisioning {

static const char *const TAG = "si_provisioning";

std::string SiProvisioning::mac_suffix_(uint8_t bytes) const {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char buf[13];
  if (bytes == 2) {
    snprintf(buf, sizeof(buf), "%02X%02X", mac[4], mac[5]);
  } else {
    snprintf(buf, sizeof(buf), "%02X%02X%02X", mac[3], mac[4], mac[5]);
  }
  return std::string(buf);
}

std::string SiProvisioning::ap_ssid_() const {
  return std::string("SI-") +
         (device_type_ == "pool" ? "Pool" : "Water") +
         "-Setup-" + this->mac_suffix_(2);
}

void SiProvisioning::setup() {
  ESP_LOGI(TAG, "si_provisioning ready (device_type=%s)", device_type_.c_str());
}

void SiProvisioning::boot_apply() {
  const bool is_provisioned = provisioned->value();
  ESP_LOGI(TAG, "boot_apply: provisioned=%s", is_provisioned ? "true" : "false");

  if (is_provisioned && !mqtt_user->value().empty()) {
    auto *c = mqtt::global_mqtt_client;
    c->set_username(mqtt_user->value().c_str());
    c->set_password(mqtt_pass->value().c_str());
    c->set_topic_prefix(mqtt_topic_prefix->value());
    ESP_LOGI(TAG, "Applied stored MQTT creds (user=%s, prefix=%s)",
             mqtt_user->value().c_str(),
             mqtt_topic_prefix->value().c_str());
    return;
  }

  this->start_provisioning_portal_();
}

void SiProvisioning::loop() {
  if (!portal_active_) return;
  if (dns_ != nullptr) dns_->processNextRequest();
  if (server_ != nullptr) server_->handleClient();
}

static const char PORTAL_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Solaire Intelligence Setup</title>
<style>
  body{font-family:-apple-system,sans-serif;max-width:420px;margin:2em auto;padding:0 1em;color:#222}
  h1{font-size:1.3em}label{display:block;margin-top:1em;font-weight:600}
  input{width:100%;padding:.6em;font-size:1em;border:1px solid #ccc;border-radius:6px;box-sizing:border-box}
  button{margin-top:1.5em;width:100%;padding:.8em;font-size:1em;background:#0a7;color:#fff;border:0;border-radius:6px}
</style></head><body>
<h1>Connect this device</h1>
<p>Enter your WiFi details and the registration code from your welcome email.</p>
<form method="POST" action="/provision">
  <label>WiFi network<input name="ssid" required maxlength="32"></label>
  <label>WiFi password<input name="password" type="password" maxlength="64"></label>
  <label>Registration code<input name="code" required maxlength="32" autocapitalize="characters"></label>
  <button type="submit">Connect</button>
</form>
</body></html>
)HTML";

void SiProvisioning::start_provisioning_portal_() {
  if (portal_active_) return;
  ESP_LOGW(TAG, "Starting provisioning AP: %s", this->ap_ssid_().c_str());

  WiFi.softAPdisconnect(false);
  WiFi.softAP(this->ap_ssid_().c_str(), "solairesetup");
  delay(100);

  IPAddress ap_ip = WiFi.softAPIP();
  dns_ = new DNSServer();
  dns_->setErrorReplyCode(DNSReplyCode::NoError);
  dns_->start(53, "*", ap_ip);

  server_ = new WebServer(80);
  server_->on("/", HTTP_GET, [this]() { this->handle_form_(); });
  // Captive portal probe URLs — return the form for any of them.
  server_->on("/generate_204", HTTP_GET, [this]() { this->handle_form_(); });
  server_->on("/hotspot-detect.html", HTTP_GET, [this]() { this->handle_form_(); });
  server_->on("/connecttest.txt", HTTP_GET, [this]() { this->handle_form_(); });
  server_->onNotFound([this]() { this->handle_form_(); });
  server_->on("/provision", HTTP_POST, [this]() { this->handle_provision_(); });
  server_->begin();

  portal_active_ = true;
  portal_started_at_ = millis();
}

void SiProvisioning::handle_form_() {
  server_->send_P(200, "text/html", PORTAL_HTML);
}

void SiProvisioning::handle_provision_() {
  if (!server_->hasArg("ssid") || !server_->hasArg("code")) {
    server_->send(400, "text/plain", "Missing fields");
    return;
  }
  std::string ssid = server_->arg("ssid").c_str();
  std::string pwd = server_->hasArg("password")
                        ? std::string(server_->arg("password").c_str())
                        : std::string();
  std::string code = server_->arg("code").c_str();

  server_->send(200, "text/html",
                "<h2>Connecting...</h2><p>This device will reboot in a moment. "
                "If it doesn't reconnect, you'll see this network again.</p>");

  // Defer heavy work so the response can flush before we tear down WiFi.
  App.scheduler.set_timeout(this, "register", 500, [this, ssid, pwd, code]() {
    std::string err;
    if (!this->perform_registration_(ssid, pwd, code, err)) {
      ESP_LOGE(TAG, "Registration failed: %s", err.c_str());
    }
  });
}

bool SiProvisioning::perform_registration_(const std::string &ssid,
                                           const std::string &password,
                                           const std::string &reg_code,
                                           std::string &err_out) {
  ESP_LOGI(TAG, "Joining %s for registration...", ssid.c_str());
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
  }
  if (WiFi.status() != WL_CONNECTED) {
    err_out = "WiFi join timeout";
    return false;
  }

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char mac_str[18];
  snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  std::string body = "{\"mac\":\"" + std::string(mac_str) +
                     "\",\"device_type\":\"" + device_type_ +
                     "\",\"registration_code\":\"" + reg_code +
                     "\",\"firmware\":\"" ESPHOME_VERSION "\"}";

  // TODO(prod): replace setInsecure() with client.setCACert(ISRG_ROOT_X1)
  // before customer rollout. Bench tests run without TLS validation.
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

  this->persist_and_reboot_(user, pass, prefix, ssid, password);
  return true;
}

void SiProvisioning::persist_and_reboot_(const std::string &user,
                                         const std::string &pass,
                                         const std::string &topic_prefix,
                                         const std::string &ssid,
                                         const std::string &wifi_pass) {
  ESP_LOGI(TAG, "Persisting MQTT creds (user=%s, prefix=%s)",
           user.c_str(), topic_prefix.c_str());

  mqtt_user->value() = user;
  mqtt_pass->value() = pass;
  mqtt_topic_prefix->value() = topic_prefix;
  provisioned->value() = true;

  global_preferences->sync();

  wifi::global_wifi_component->save_wifi_sta(ssid, wifi_pass);

  delay(500);
  ESP_LOGI(TAG, "Rebooting into provisioned mode...");
  App.safe_reboot();
}

}  // namespace si_provisioning
}  // namespace esphome
