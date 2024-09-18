#include "mvi_wifi.h"

#include "mvi_log.h"

#include <ESPmDNS.h>

static String GetApName() {
  return String{"LightCannon_"} + String(WIFI_getChipId(), HEX);
}

void WiFiConnection::setup(uint16_t light_id,
  WiFiConnection::APModeCallback on_ap_mode,
  WiFiConnection::SaveConfigCallback on_save_config,
  WiFiConnection::ConnectCallback on_connect,
  WiFiConnection::DisconnectCallback on_disconnect) {
  on_ap_mode_ = on_ap_mode;
  on_save_config_ = on_save_config;
  on_connect_ = on_connect;
  on_disconnect_ = on_disconnect;

  // Explicitly set the WiFi mode to station mode
  WiFi.mode(WIFI_STA);

  // Update the light ID, which sets up the DHCP client ID
  setLightId(light_id);

  wifi_manager_.setConfigPortalBlocking(false);
  wifi_manager_.setClass("invert");
  wifi_manager_.addParameter(&light_id_param_);
  wifi_manager_.setAPCallback([this](WiFiManager*) {
    println("Entered AP mode");
    on_ap_mode_();
  });
  wifi_manager_.setPreSaveConfigCallback([this]() {
    // Attempt to parse the light ID parameter as a uint16_t
    const String value = light_id_param_.getValue();
    char* end;
    const unsigned long int light_id_long = strtoul(value.c_str(), &end, 10);
    if (*end != '\0' || light_id_long > 0xFFFF) {
      printf("Invalid light ID: %s\n", value.c_str());
      return false;
    }
    const uint16_t light_id = uint16_t(light_id_long);

    setLightId(light_id);
    on_save_config_(light_id);
    return true;
  });
}

void WiFiConnection::setLightId(uint16_t light_id) {
  light_id_ = light_id;
  const String light_id_str = light_id != LIGHT_ID_INVALID ? String(light_id) : String();
  const String hostname = "light-cannon" + light_id_str;
  const String instance_name = "Light Cannon " + light_id_str;

  printf("Setting light ID to %u, hostname to %s, instance name to %s\n",
    light_id,
    hostname.c_str(),
    instance_name.c_str());

  // Update DHCP client ID
  wifi_manager_.setHostname(hostname.c_str());
}

void WiFiConnection::connect() {
  if (isConnected()) { println("connect() called while already connected"); }

  const bool res = wifi_manager_.autoConnect(GetApName().c_str());
  printf("WiFi connection result: %d\n", res);
}

void WiFiConnection::disconnectAndReset() {
  if (!isConnected()) { println("disconnect() called while not connected"); }

  wifi_manager_.disconnect();
  wifi_manager_.resetSettings();

  on_disconnect_();
  connected_ = false;
}

void WiFiConnection::forceAPMode() {
  if (isConnected()) { disconnectAndReset(); }

  wifi_manager_.startConfigPortal(GetApName().c_str());
}

bool WiFiConnection::isConnected() const {
  return connected_;
}

void WiFiConnection::process() {
  wifi_manager_.process();

  if (!connected_ && WiFi.status() == WL_CONNECTED) {
    connected_ = true;

    // Setup mDNS
    const String light_id_str = light_id_ != LIGHT_ID_INVALID ? String(light_id_) : String();
    const String hostname = "light-cannon" + light_id_str;
    const String instance_name = "Light Cannon " + light_id_str;
    MDNS.end();
    if (!MDNS.begin(hostname)) { printf("MDNS.begin(%s) failed\n", hostname.c_str()); }
    MDNS.setInstanceName(instance_name.c_str());
    if (!MDNS.addService("artnet", "udp", 6454)) { println("MDNS.addService() failed"); }

    on_connect_();
  } else if (connected_ && WiFi.status() != WL_CONNECTED) {
    connected_ = false;
    on_disconnect_();
  }
}
