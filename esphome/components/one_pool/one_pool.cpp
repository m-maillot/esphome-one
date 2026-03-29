#include "one_pool.h"
#include "esphome/core/log.h"
#include <cstring>
#include <ctime>

namespace esphome {
namespace one_pool {

static const char *const TAG = "one_pool";


// ============================================================
// AES-128-ECB Implementation
// ============================================================
// clang-format off
const uint8_t AES128::SBOX[256] = {
  0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
  0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
  0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
  0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
  0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
  0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
  0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
  0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
  0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
  0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
  0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
  0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
  0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
  0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
  0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
  0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};
const uint8_t AES128::RCON[10] = {0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36};
// clang-format on

void AES128::set_key(const uint8_t *key) { this->key_expansion_(key); }

void AES128::key_expansion_(const uint8_t *key) {
  memcpy(this->round_keys_, key, 16);
  for (int i = 4; i < 44; i++) {
    uint8_t t[4];
    memcpy(t, &this->round_keys_[(i - 1) * 4], 4);
    if (i % 4 == 0) {
      uint8_t tmp = t[0];
      t[0] = SBOX[t[1]] ^ RCON[i / 4 - 1];
      t[1] = SBOX[t[2]];
      t[2] = SBOX[t[3]];
      t[3] = SBOX[tmp];
    }
    for (int j = 0; j < 4; j++)
      this->round_keys_[i * 4 + j] = this->round_keys_[(i - 4) * 4 + j] ^ t[j];
  }
}

void AES128::sub_bytes_(uint8_t *s) {
  for (int i = 0; i < 16; i++) s[i] = SBOX[s[i]];
}

void AES128::shift_rows_(uint8_t *s) {
  uint8_t t;
  t = s[1]; s[1] = s[5]; s[5] = s[9]; s[9] = s[13]; s[13] = t;
  t = s[2]; s[2] = s[10]; s[10] = t; t = s[6]; s[6] = s[14]; s[14] = t;
  t = s[15]; s[15] = s[11]; s[11] = s[7]; s[7] = s[3]; s[3] = t;
}

void AES128::mix_columns_(uint8_t *s) {
  for (int i = 0; i < 16; i += 4) {
    uint8_t a = s[i], b = s[i + 1], c = s[i + 2], d = s[i + 3];
    uint8_t e = a ^ b ^ c ^ d;
    auto xtime = [](uint8_t x) -> uint8_t { return ((x << 1) ^ (((x >> 7) & 1) * 0x1b)) & 0xff; };
    s[i] ^= e ^ xtime(a ^ b);
    s[i + 1] ^= e ^ xtime(b ^ c);
    s[i + 2] ^= e ^ xtime(c ^ d);
    s[i + 3] ^= e ^ xtime(d ^ a);
  }
}

void AES128::add_round_key_(uint8_t *state, const uint8_t *rk, int offset) {
  for (int i = 0; i < 16; i++) state[i] ^= rk[offset + i];
}

void AES128::encrypt_block(const uint8_t *in, uint8_t *out) {
  uint8_t state[16];
  memcpy(state, in, 16);
  add_round_key_(state, this->round_keys_, 0);
  for (int r = 1; r < 10; r++) {
    sub_bytes_(state);
    shift_rows_(state);
    mix_columns_(state);
    add_round_key_(state, this->round_keys_, r * 16);
  }
  sub_bytes_(state);
  shift_rows_(state);
  add_round_key_(state, this->round_keys_, 160);
  memcpy(out, state, 16);
}

// ============================================================
// Hex helpers
// ============================================================
bool OnePoolClient::hex_to_bytes_(const std::string &hex, uint8_t *out, size_t len) {
  if (hex.length() != len * 2) return false;
  for (size_t i = 0; i < len; i++) {
    char hi = hex[i * 2], lo = hex[i * 2 + 1];
    auto nibble = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return c - 'a' + 10;
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      return -1;
    };
    int h = nibble(hi), l = nibble(lo);
    if (h < 0 || l < 0) return false;
    out[i] = (h << 4) | l;
  }
  return true;
}

// ============================================================
// Setup
// ============================================================
void OnePoolClient::setup() {
  // Setup AES with hardcoded PRIVATE_KEY
  const uint8_t private_key[] = {0x11, 0x41, 0xa8, 0x05, 0x37, 0x44, 0x4a, 0x6a,
                                  0x85, 0x88, 0x8d, 0x84, 0x11, 0x5f, 0x28, 0x11};
  this->aes_.set_key(private_key);

  ESP_LOGI(TAG, "One Pool client initialized, shared_key=%s", this->shared_key_hex_.c_str());
}

void OnePoolClient::loop() {
  // Nothing to poll — everything is event-driven via gattc_event_handler
}

// ============================================================
// Auth response computation
// ============================================================
void OnePoolClient::compute_auth_response_(const uint8_t *nonce, size_t nonce_len, uint8_t *response) {
  // Parse shared key
  uint8_t shared_key[8];
  if (!hex_to_bytes_(this->shared_key_hex_, shared_key, 8)) {
    ESP_LOGE(TAG, "Invalid shared_key hex: %s", this->shared_key_hex_.c_str());
    return;
  }

  // Build plaintext: sharedKey (8B) + reversed(nonce) (8B) = 16B
  uint8_t plaintext[16];
  memcpy(plaintext, shared_key, 8);
  for (size_t i = 0; i < nonce_len && i < 8; i++) {
    plaintext[8 + i] = nonce[nonce_len - 1 - i];
  }

  ESP_LOGD(TAG, "Plaintext: %s",
           format_hex(plaintext, 16).c_str());

  // AES-128-ECB encrypt
  uint8_t encrypted[16];
  this->aes_.encrypt_block(plaintext, encrypted);

  // Reverse encrypted bytes for BLE write
  for (int i = 0; i < 16; i++) {
    response[15 - i] = encrypted[i];
  }

  ESP_LOGD(TAG, "Auth response: %s",
           format_hex(response, 16).c_str());
}

// ============================================================
// GATT Client Event Handler
// ============================================================
void OnePoolClient::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                         esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "BLE connected");
        this->state_ = State::DISCOVERING;
        this->authenticated_ = false;
      } else {
        ESP_LOGW(TAG, "BLE connect failed, status=%d", param->open.status);
      }
      break;
    }

    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGI(TAG, "BLE disconnected");
      this->state_ = State::IDLE;
      this->authenticated_ = false;
      this->handle_randomkey_ = 0;
      this->handle_encryptkey_ = 0;
      this->handle_control_ = 0;
      this->handle_status_ = 0;
      this->handle_status_ccc_ = 0;
      this->handle_datetime_ = 0;
      this->pump_state_ = false;
      this->light_state_ = false;
      this->update_sub_components_();
      if (this->ble_connected_sensor_) this->ble_connected_sensor_->publish_state(false);
      this->disconnect_count_++;
      if (this->ble_disconnects_sensor_) this->ble_disconnects_sensor_->publish_state(this->disconnect_count_);
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      ESP_LOGI(TAG, "Service discovery complete");

      // Find all characteristic handles
      auto *randomkey = this->parent()->get_characteristic(UUID_SYSTEM, UUID_RANDOMKEY);
      if (randomkey) {
        this->handle_randomkey_ = randomkey->handle;
        ESP_LOGD(TAG, "FBDE0001 handle: 0x%04x", this->handle_randomkey_);
      }

      auto *encryptkey = this->parent()->get_characteristic(UUID_SYSTEM, UUID_ENCRYPTKEY);
      if (encryptkey) {
        this->handle_encryptkey_ = encryptkey->handle;
        ESP_LOGD(TAG, "FBDE0003 handle: 0x%04x", this->handle_encryptkey_);
      }

      auto *control = this->parent()->get_characteristic(UUID_SVC_DEVICE, UUID_CONTROL);
      if (control) {
        this->handle_control_ = control->handle;
        ESP_LOGD(TAG, "FBDE0101 handle: 0x%04x", this->handle_control_);
      }

      auto *status = this->parent()->get_characteristic(UUID_SVC_DEVICE, UUID_STATUS);
      if (status) {
        this->handle_status_ = status->handle;
        this->handle_status_ccc_ = status->handle + 1;  // CCC descriptor is typically next handle
        ESP_LOGD(TAG, "FBDE0104 handle: 0x%04x", this->handle_status_);
      }

      auto *datetime = this->parent()->get_characteristic(UUID_TIME_SVC, UUID_DATETIME);
      if (datetime) {
        this->handle_datetime_ = datetime->handle;
        ESP_LOGD(TAG, "DateTime handle: 0x%04x", this->handle_datetime_);
      }

      if (this->handle_randomkey_ && this->handle_encryptkey_) {
        this->start_auth_();
      } else {
        ESP_LOGE(TAG, "Missing auth characteristics!");
      }
      break;
    }

    case ESP_GATTC_READ_CHAR_EVT: {
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Read failed, handle=0x%04x status=%d", param->read.handle, param->read.status);
        break;
      }

      if (param->read.handle == this->handle_randomkey_ && this->state_ == State::READING_NONCE) {
        ESP_LOGI(TAG, "Nonce received: %s (%d bytes)",
                 format_hex(param->read.value, param->read.value_len).c_str(),
                 param->read.value_len);

        // Compute and write auth response
        uint8_t response[16];
        this->compute_auth_response_(param->read.value, param->read.value_len, response);

        auto status = esp_ble_gattc_write_char(this->parent()->get_gattc_if(),
                                                this->parent()->get_conn_id(),
                                                this->handle_encryptkey_,
                                                16, response,
                                                ESP_GATT_WRITE_TYPE_RSP,
                                                ESP_GATT_AUTH_REQ_NONE);
        if (status == ESP_OK) {
          this->state_ = State::WRITING_AUTH;
          ESP_LOGI(TAG, "Auth response sent");
        } else {
          ESP_LOGE(TAG, "Failed to write auth response: %d", status);
        }
      }
      // Handle initial status read response
      else if (param->read.handle == this->handle_status_ && this->state_ == State::READY) {
        if (param->read.value_len >= 1) {
          ESP_LOGI(TAG, "Initial status read: 0x%02x", param->read.value[0]);
          this->parse_status_(param->read.value[0]);
        }
      }
      break;
    }

    case ESP_GATTC_WRITE_CHAR_EVT: {
      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Write failed, handle=0x%04x status=%d", param->write.handle, param->write.status);
        break;
      }

      if (param->write.handle == this->handle_encryptkey_ && this->state_ == State::WRITING_AUTH) {
        ESP_LOGI(TAG, "Auth accepted!");
        this->sync_rtc_();
      } else if (param->write.handle == this->handle_datetime_ && this->state_ == State::SYNCING_RTC) {
        ESP_LOGI(TAG, "RTC synced");
        this->subscribe_status_();
      } else if (param->write.handle == this->handle_control_) {
        ESP_LOGD(TAG, "Command written OK, reading back status...");
        // Read status to confirm actual device state
        if (this->handle_status_) {
          esp_ble_gattc_read_char(this->parent()->get_gattc_if(),
                                  this->parent()->get_conn_id(),
                                  this->handle_status_, ESP_GATT_AUTH_REQ_NONE);
        }
      }
      break;
    }

    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (param->reg_for_notify.status == ESP_GATT_OK && param->reg_for_notify.handle == this->handle_status_) {
        ESP_LOGI(TAG, "Status notifications registered");
        this->state_ = State::READY;
        this->authenticated_ = true;
        if (this->ble_connected_sensor_) this->ble_connected_sensor_->publish_state(true);
        ESP_LOGI(TAG, "=== READY ===");

        // Read initial status from FBDE0104 (status char, not control)
        esp_ble_gattc_read_char(this->parent()->get_gattc_if(),
                                this->parent()->get_conn_id(),
                                this->handle_status_, ESP_GATT_AUTH_REQ_NONE);
      } else if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGE(TAG, "Failed to register notifications: status=%d", param->reg_for_notify.status);
      }
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle == this->handle_status_) {
        if (param->notify.value_len >= 1) {
          this->parse_status_(param->notify.value[0]);
        }
      }
      break;
    }

    default:
      break;
  }
}

// ============================================================
// Auth flow steps
// ============================================================
void OnePoolClient::start_auth_() {
  ESP_LOGI(TAG, "Starting authentication...");
  this->state_ = State::READING_NONCE;
  auto status = esp_ble_gattc_read_char(this->parent()->get_gattc_if(),
                                         this->parent()->get_conn_id(),
                                         this->handle_randomkey_,
                                         ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read nonce: %d", status);
  }
}

void OnePoolClient::sync_rtc_() {
  if (!this->handle_datetime_) {
    ESP_LOGD(TAG, "No datetime handle, skipping RTC sync");
    this->subscribe_status_();
    return;
  }

  this->state_ = State::SYNCING_RTC;

  time_t now = ::time(nullptr);
  struct tm *tm = localtime(&now);

  uint8_t buf[7];
  uint16_t year = tm->tm_year + 1900;
  buf[0] = year & 0xFF;
  buf[1] = (year >> 8) & 0xFF;
  buf[2] = tm->tm_mon + 1;
  buf[3] = tm->tm_mday;
  buf[4] = tm->tm_hour;
  buf[5] = tm->tm_min;
  buf[6] = tm->tm_sec;

  esp_ble_gattc_write_char(this->parent()->get_gattc_if(),
                            this->parent()->get_conn_id(),
                            this->handle_datetime_,
                            7, buf,
                            ESP_GATT_WRITE_TYPE_RSP,
                            ESP_GATT_AUTH_REQ_NONE);
  ESP_LOGD(TAG, "RTC sync: %04d-%02d-%02d %02d:%02d:%02d",
           year, buf[2], buf[3], buf[4], buf[5], buf[6]);
}

void OnePoolClient::subscribe_status_() {
  if (!this->handle_status_) {
    ESP_LOGW(TAG, "No status handle");
    this->state_ = State::READY;
    this->authenticated_ = true;
    return;
  }

  this->state_ = State::SUBSCRIBING_STATUS;

  auto status = esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(),
                                                    this->parent()->get_remote_bda(),
                                                    this->handle_status_);
  if (status != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register for notifications: %d", status);
  }
}

// ============================================================
// Status parsing
// ============================================================
void OnePoolClient::parse_status_(uint8_t value) {
  bool new_pump = false, new_light = false;

  switch (value) {
    case 0x00:
    case 0x02:
      new_pump = false; new_light = false;
      break;
    case 0x05:
      new_pump = true; new_light = false;
      break;
    case 0x28:
      new_pump = false; new_light = true;
      break;
    case 0x2D:
      new_pump = true; new_light = true;
      break;
    default:
      ESP_LOGW(TAG, "Unknown status byte: 0x%02x", value);
      return;
  }

  if (new_pump != this->pump_state_ || new_light != this->light_state_) {
    ESP_LOGI(TAG, "Status: pump=%s light=%s (0x%02x)",
             new_pump ? "ON" : "OFF", new_light ? "ON" : "OFF", value);
  }

  this->pump_state_ = new_pump;
  this->light_state_ = new_light;
  this->update_sub_components_();
}

void OnePoolClient::update_sub_components_() {
  if (this->pump_switch_) this->pump_switch_->publish_state(this->pump_state_);
  if (this->light_switch_) this->light_switch_->publish_state(this->light_state_);
  for (auto *bs : this->binary_sensors_) {
    bs->publish_state(bs->get_is_pump() ? this->pump_state_ : this->light_state_);
  }
}

// ============================================================
// Send command
// ============================================================
void OnePoolClient::send_command(bool pump, bool light) {
  if (!this->authenticated_ || !this->handle_control_) {
    ESP_LOGW(TAG, "Not connected/authenticated, cannot send command");
    return;
  }

  uint8_t cmd = (pump ? 0x01 : 0x00) | (light ? 0x04 : 0x00);
  ESP_LOGI(TAG, "Sending command: 0x%02x (pump=%s, light=%s)",
           cmd, pump ? "ON" : "OFF", light ? "ON" : "OFF");

  auto status = esp_ble_gattc_write_char(this->parent()->get_gattc_if(),
                                          this->parent()->get_conn_id(),
                                          this->handle_control_,
                                          1, &cmd,
                                          ESP_GATT_WRITE_TYPE_RSP,
                                          ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_OK) {
    ESP_LOGE(TAG, "Failed to send command: %d", status);
  }
}

// ============================================================
// Switch write_state
// ============================================================
void OnePoolSwitch::write_state(bool state) {
  if (!this->parent_) return;
  bool pump = this->is_pump_ ? state : this->parent_->is_pump_on();
  bool light = this->is_pump_ ? this->parent_->is_light_on() : state;
  this->parent_->send_command(pump, light);
  // Publish optimistic state so HA doesn't revert the toggle
  this->publish_state(state);
}

}  // namespace one_pool
}  // namespace esphome
