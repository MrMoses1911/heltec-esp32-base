#ifndef PULSE_SAFEDOCK_STORAGE_CONFIG_REPOSITORY_H
#define PULSE_SAFEDOCK_STORAGE_CONFIG_REPOSITORY_H

#include <array>
#include <cstdint>
#include "esp_err.h"
#include "storage/nvs_store.h"

namespace pulse {
namespace storage {

struct SafeDockConfig {
  uint16_t schemaVersion = 1;
  uint16_t freqMinHz = 100;
  uint16_t freqMaxHz = 600;
  uint16_t distanceMinMm = 50;
  uint16_t distanceMaxMm = 5000;
  uint16_t sensorGpio = 38;
  uint16_t apSsidCode = 0;
  char webUsername[16] = {'a', 'd', 'm', 'i', 'n', '\0'};
  char webPassword[16] = {'p', 'u', 'l', 's', 'e', '\0'};
  char apSsid[33] = {};
  char apPassword[64] = {};
  uint16_t webServerAlwaysOn = 0;
  uint16_t webServerTimeoutMinutes = 15;
};

class ConfigRepository {
 public:
  explicit ConfigRepository(NvsStore* store);

  esp_err_t Load(SafeDockConfig* config);
  esp_err_t Save(const SafeDockConfig& config);
  esp_err_t SaveSsidCode(uint16_t code);
  esp_err_t LoadSsidCode(uint16_t* code);
  esp_err_t FactoryReset();

 private:
  static constexpr const char* kNamespace = "safedock";
  static constexpr const char* kBlobKey = "config";
  static constexpr uint32_t kMagic = 0x53444643;

  NvsStore* store_;
};

}  // namespace storage
}  // namespace pulse

#endif
