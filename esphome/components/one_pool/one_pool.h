#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include <array>
#include <string>

namespace esphome {
namespace one_pool {

using namespace esp32_ble_tracker;

// BLE UUIDs
static const ESPBTUUID UUID_SYSTEM = ESPBTUUID::from_raw("fbde0000-4c7b-4e67-8292-a9b8e686cf87");
static const ESPBTUUID UUID_RANDOMKEY = ESPBTUUID::from_raw("fbde0001-4c7b-4e67-8292-a9b8e686cf87");
static const ESPBTUUID UUID_ENCRYPTKEY = ESPBTUUID::from_raw("fbde0003-4c7b-4e67-8292-a9b8e686cf87");
static const ESPBTUUID UUID_SVC_DEVICE = ESPBTUUID::from_raw("fbde0100-4c7b-4e67-8292-a9b8e686cf87");
static const ESPBTUUID UUID_CONTROL = ESPBTUUID::from_raw("fbde0101-4c7b-4e67-8292-a9b8e686cf87");
static const ESPBTUUID UUID_STATUS = ESPBTUUID::from_raw("fbde0104-4c7b-4e67-8292-a9b8e686cf87");
static const ESPBTUUID UUID_TIME_SVC = ESPBTUUID::from_uint16(0x1805);
static const ESPBTUUID UUID_DATETIME = ESPBTUUID::from_uint16(0x2A08);

// ============================================================
// AES-128-ECB (minimal, no dependencies)
// ============================================================
class AES128 {
 public:
  void set_key(const uint8_t *key);
  void encrypt_block(const uint8_t *in, uint8_t *out);

 private:
  uint8_t round_keys_[176];
  void key_expansion_(const uint8_t *key);
  static void sub_bytes_(uint8_t *state);
  static void shift_rows_(uint8_t *state);
  static void mix_columns_(uint8_t *state);
  static void add_round_key_(uint8_t *state, const uint8_t *rk, int offset);
  static const uint8_t SBOX[256];
  static const uint8_t RCON[10];
};

// ============================================================
// Forward declarations
// ============================================================
class OnePoolClient;

// ============================================================
// Switch
// ============================================================
class OnePoolSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(OnePoolClient *parent) { this->parent_ = parent; }
  void set_type(bool is_pump) { this->is_pump_ = is_pump; }

 protected:
  void write_state(bool state) override;
  OnePoolClient *parent_{nullptr};
  bool is_pump_{true};
};

// ============================================================
// Binary Sensor
// ============================================================
class OnePoolBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  void set_parent(OnePoolClient *parent) { this->parent_ = parent; }
  void set_type(bool is_pump) { this->is_pump_ = is_pump; }
  bool get_is_pump() const { return this->is_pump_; }

 protected:
  OnePoolClient *parent_{nullptr};
  bool is_pump_{true};
};

// ============================================================
// Main BLE Client
// ============================================================
class OnePoolClient : public Component, public ble_client::BLEClientNode {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  void set_shared_key(const std::string &key) { this->shared_key_hex_ = key; }

  // BLE client callbacks
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  // Called by switches
  void send_command(bool pump, bool light);

  // Register sub-components
  void register_switch(OnePoolSwitch *sw, bool is_pump) {
    if (is_pump) this->pump_switch_ = sw;
    else this->light_switch_ = sw;
  }
  void register_binary_sensor(OnePoolBinarySensor *bs) {
    this->binary_sensors_.push_back(bs);
  }
  void set_ble_connected_sensor(binary_sensor::BinarySensor *sensor) {
    this->ble_connected_sensor_ = sensor;
  }
  void set_ble_disconnects_sensor(sensor::Sensor *sensor) {
    this->ble_disconnects_sensor_ = sensor;
  }

  bool is_pump_on() const { return this->pump_state_; }
  bool is_light_on() const { return this->light_state_; }
  bool is_ble_connected() const { return this->authenticated_; }

 protected:
  // Auth
  std::string shared_key_hex_;
  AES128 aes_;
  bool authenticated_{false};

  // BLE handles
  uint16_t handle_randomkey_{0};
  uint16_t handle_encryptkey_{0};
  uint16_t handle_control_{0};
  uint16_t handle_status_{0};
  uint16_t handle_status_ccc_{0};
  uint16_t handle_datetime_{0};

  // State
  bool pump_state_{false};
  bool light_state_{false};

  // Sub-components
  OnePoolSwitch *pump_switch_{nullptr};
  OnePoolSwitch *light_switch_{nullptr};
  std::vector<OnePoolBinarySensor *> binary_sensors_;
  binary_sensor::BinarySensor *ble_connected_sensor_{nullptr};
  sensor::Sensor *ble_disconnects_sensor_{nullptr};
  uint32_t disconnect_count_{0};

  // Auth flow state machine
  enum class State {
    IDLE,
    DISCOVERING,
    READING_NONCE,
    WRITING_AUTH,
    SYNCING_RTC,
    SUBSCRIBING_STATUS,
    READY,
  };
  State state_{State::IDLE};

  void start_auth_();
  void compute_auth_response_(const uint8_t *nonce, size_t nonce_len, uint8_t *response);
  void sync_rtc_();
  void subscribe_status_();
  void parse_status_(uint8_t value);
  void update_sub_components_();

  // Hex helpers
  static bool hex_to_bytes_(const std::string &hex, uint8_t *out, size_t len);
};

}  // namespace one_pool
}  // namespace esphome
