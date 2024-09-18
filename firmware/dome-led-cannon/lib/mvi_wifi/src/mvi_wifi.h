#pragma once

// Manages connecting to WiFi or starting an access point with a captive portal for configuring
// WiFi settings

#include <WiFiManager.h>

#include <cstdint>
#include <functional>

constexpr uint16_t LIGHT_ID_INVALID = 0xFF;

class WiFiConnection {
public:
  using APModeCallback = std::function<void()>;
  using SaveConfigCallback = std::function<void(uint16_t)>;
  using ConnectCallback = std::function<void()>;
  using DisconnectCallback = std::function<void()>;

  // Initialize the WiFi connection manager
  void setup(uint16_t light_id,
    APModeCallback on_ap_mode,
    SaveConfigCallback on_save_config,
    ConnectCallback on_connect,
    DisconnectCallback on_disconnect);

  // Sets the hostname to be used for DHCP client ID and mDNS
  void setLightId(uint16_t light_id);

  // Attempt to connect to WiFi using the stored SSID and password. If no credentials are stored,
  // or the WiFi connection fails, start a captive portal to allow the user to enter WiFi settings
  void connect();

  // Disconnect from the current WiFi network and erase stored credentials
  void disconnectAndReset();

  // Disconnect from the current WiFi network and start a captive portal
  void forceAPMode();

  // Returns true if the device is connected to a WiFi network as a station
  bool isConnected() const;

  // Call from the main loop() to handle the captive portal and WiFi connection
  void process();

private:
  WiFiManager wifi_manager_;
  WiFiManagerParameter light_id_param_{"light_id", "Light ID", "0", 4, "0"};
  APModeCallback on_ap_mode_;
  SaveConfigCallback on_save_config_;
  ConnectCallback on_connect_;
  DisconnectCallback on_disconnect_;
  uint16_t light_id_ = LIGHT_ID_INVALID;
  bool connected_ = false;
};
