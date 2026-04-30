#include "si_provisioning.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/components/mqtt/mqtt_client.h"
#include "esphome/components/json/json_util.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <esp_wifi.h>

namespace esphome {
extern globals::RestoringGlobalsComponent<std::string> *mqtt_user;
extern globals::RestoringGlobalsComponent<std::string> *mqtt_pass;
extern globals::RestoringGlobalsComponent<std::string> *mqtt_topic_prefix;
extern globals::RestoringGlobalsComponent<bool> *provisioned;
}

namespace esphome {
namespace si_provisioning {

static const char *const TAG = "si_provisioning";

void SiProvisioning::setup() {
  ESP_LOGI(TAG, "Provisioning component ready");
}

void SiProvisioning::boot_apply() {
  if (provisioned->value() && !mqtt_user->value().empty()) {
    auto *c = mqtt::global_mqtt_client;
    c->set_username(mqtt_user->value().c_str());
    c->set_password(mqtt_pass->value().c_str());
    c->set_topic_prefix(mqtt_topic_prefix->value());
    ESP_LOGI(TAG, "MQTT credentials applied");
    return;
  }

  start_portal_();
}

void SiProvisioning::loop() {
  if (dns_) dns_->processNextRequest();
}

void SiProvisioning::start_portal_() {
  WiFi.softAP("SI-Setup", "solairesetup");

  dns_ = new DNSServer();
  dns_->start(53, "*", WiFi.softAPIP());

  server_ = new AsyncWebServer(80);

  server_->on("/", HTTP_GET, [&](AsyncWebServerRequest *req) {
    req->send(200, "text/plain", "Provisioning active");
  });

  server_->on("/provision", HTTP_POST, [&](AsyncWebServerRequest *req) {
    std::string ssid = req->getParam("ssid", true)->value().c_str();
    std::string pass = req->getParam("password", true)->value().c_str();
    std::string code = req->getParam("code", true)->value().c_str();

    req->send(200, "text/plain", "Provisioning...");

    App.scheduler.set_timeout(this, "prov", 500, [=]() {
      provision_(ssid, pass, code);
    });
  });

  server_->begin();
}

void SiProvisioning::provision_(std::string ssid, std::string password, std::string code) {
  WiFi.begin(ssid.c_str(), password.c_str());

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(200);
  }

  if (WiFi.status() != WL_CONNECTED) {
    ESP_LOGE(TAG, "WiFi failed");
    return;
  }

  WiFiClientSecure client;
  client.setFingerprint(register_fingerprint_.c_str());

  HTTPClient https;
  https.begin(client, register_endpoint_.c_str());
  https.addHeader("Content-Type", "application/json");

  std::string body = "{\"registration_code\":\"" + code + "\"}";
  int httpCode = https.POST(body);

  if (httpCode != 200) {
    ESP_LOGE(TAG, "HTTP error");
    return;
  }

  String resp = https.getString();
  https.end();

  std::string user, pass, prefix;

  json::parse_json(resp.c_str(), [&](JsonObject root) {
    user = root["mqtt_username"].as<const char *>();
    pass = root["mqtt_password"].as<const char *>();
    prefix = root["topic_prefix"].as<const char *>();
    return true;
  });

  mqtt_user->value() = user;
  mqtt_pass->value() = pass;
  mqtt_topic_prefix->value() = prefix;
  provisioned->value() = true;

  global_preferences->sync();

  ESP_LOGI(TAG, "Provisioned successfully");

  delay(500);
  App.safe_reboot();
}

}  
}
