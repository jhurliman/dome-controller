#include "mvi_animation.h"
#include "mvi_lights.h"
#include "mvi_log.h"
#include "mvi_storage.h"
#include "mvi_wifi.h"

#include <Arduino.h>
#include <ArtnetWiFi.h>

constexpr uint16_t kNumLeds = 10;
constexpr uint8_t kDataPin = 13;

enum class SystemState {
  Setup,
  Connecting,
  APMode,
  Connected,
  RemoteControl,
  Failed,
};

// Globals

SystemState g_prev_state = SystemState::Setup;
SystemState g_state = SystemState::Setup;
AnimationManager g_animations;
WiFiConnection g_wifi;
ArtnetWiFiReceiver g_artnet;
Lights g_leds{kNumLeds, kDataPin};
uint16_t g_light_id = LIGHT_ID_INVALID;

// Function prototypes

void HandleState(SystemState new_state, SystemState prev_state);
uint16_t GetLightId();
void InitialPattern();
[[noreturn]] void CriticalAbort();

void HandleDMX(
  const uint8_t* data, uint16_t size, const ArtDmxMetadata& meta, const ArtNetRemoteInfo& remote);

// Main program

void setup() {
  Serial.begin(115200);

  println("Initializing LEDs...");
  g_leds.setup();
  InitialPattern();
  g_leds.show();

  StorageSetup();

  // Check if the light ID is valid and set the initial hostname
  const uint16_t light_id = GetLightId();
  if (light_id != LIGHT_ID_INVALID) { printf("Loaded light ID: %u\n", light_id); }

  // Initialize the WiFi connection manager and start connecting
  g_wifi.setup(
    light_id,
    // On AP mode
    []() {
      println("Entered AP mode");
      g_state = SystemState::APMode;
    },
    // On save config
    [](uint16_t light_id) {
      // Save the light ID to EEPROM
      Storage storage{};
      storage.light_id = uint16_t(light_id);
      StorageWrite(storage);
      printf("Saved light ID: %u\n", storage.light_id);
    },
    // On connect
    []() {
      // Read the light ID from EEPROM
      Storage storage{};
      if (!StorageRead(storage)) {
        // WiFi credentials were set but light ID was not. Re-enter AP mode
        println("Failed to read light ID from EEPROM");
        g_wifi.forceAPMode();
        return;
      }
      g_light_id = storage.light_id;

      g_state = SystemState::Connected;
      printf("Connected to WiFi as light-cannon%d.local (%s)\n",
        g_light_id,
        WiFi.localIP().toString().c_str());

      // Start the ArtNet (DMX over WiFi) receiver
      g_artnet.begin();
      g_artnet.subscribeArtDmxUniverse(g_light_id, HandleDMX);
    },
    // On disconnect
    []() {
      println("Disconnected from WiFi");
      g_state = SystemState::Connecting;
    });
  g_wifi.connect();

  println("Setup complete");
}

void loop() {
  g_wifi.process();

  if (g_state == SystemState::Connected || g_state == SystemState::RemoteControl) {
    g_artnet.parse();
  }

  HandleState(g_state, g_prev_state);
  g_prev_state = g_state;

  g_animations.update();
  g_leds.show();
}

void HandleState(SystemState new_state, SystemState prev_state) {
  if (new_state != prev_state) {
    printf("State transition: %d -> %d\n", int(prev_state), int(new_state));
    switch (new_state) {
    case SystemState::Setup:
      // Should never transition back to Setup
      assert(false && "Invalid state transition to Setup");
      break;
    case SystemState::Connecting:
      g_animations.setAnimation([](time_ms elapsed) { SlowBluePulse(elapsed, 2000, g_leds); });
      break;
    case SystemState::APMode:
      g_animations.setAnimation([](time_ms elapsed) { GreenSpinner(elapsed, 2000, g_leds); });
      break;
    case SystemState::Connected:
      g_animations.setAnimation([](time_ms elapsed) { BlinkThenThrob(elapsed, 5000, g_leds); });
      break;
    case SystemState::RemoteControl:
      g_animations.stop();
      break;
    case SystemState::Failed:
      CriticalAbort();
      break;
    }
  }
}

void HandleDMX(
  const uint8_t* data, uint16_t size, const ArtDmxMetadata& meta, const ArtNetRemoteInfo& remote) {
  constexpr uint16_t kNumChannels = 4;

  // Set the global system state to "remote control" since we are receiving DMX data
  g_state = SystemState::RemoteControl;

  // Copy the DMX data to the LED strip
  const uint16_t light_count = std::min(uint16_t(size / kNumChannels), g_leds.size());
  for (uint16_t i = 0; i < light_count; i++) {
    const uint8_t r = data[i * kNumChannels + 0];
    const uint8_t g = data[i * kNumChannels + 1];
    const uint8_t b = data[i * kNumChannels + 2];
    const uint8_t w = data[i * kNumChannels + 3];

    const RgbwwColor color{r, g, b, w, w};
    g_leds.setPixelColor(i, color);
  }
}

uint16_t GetLightId() {
  if (g_light_id != LIGHT_ID_INVALID) { return g_light_id; }

  // Read the light ID from EEPROM
  Storage storage{};
  if (!StorageRead(storage)) { return LIGHT_ID_INVALID; }

  // Cache and return the light ID
  g_light_id = storage.light_id;
  return g_light_id;
}

void InitialPattern() {
  for (uint16_t i = 0; i < g_leds.size(); i++) {
    g_leds.setPixelColor(i, (i % 2 == 0) ? Lights::RoyalBlue : Lights::Green);
  }
}

// Blink the LEDs in a critical error state and hang execution
[[noreturn]] void CriticalAbort() {
  constexpr unsigned int kBlinkDelay_ms = 1000;
  while (true) {
    // Blink the LEDs
    print("!");
    g_leds.fillColor(Lights::Red);
    g_leds.show();
    vTaskDelay(kBlinkDelay_ms / portTICK_PERIOD_MS);
    print(".");
    g_leds.fillColor(Lights::Black);
    g_leds.show();
    vTaskDelay(kBlinkDelay_ms / portTICK_PERIOD_MS);
  }
}
