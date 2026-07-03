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
  uint16_t adjacentMm = 2000;        // distância sensor→doca (mm)
  int16_t offsetMm = 0;              // correção +/- mm
  uint16_t alarmMinMm = 500;         // zona de alarme min (mm)
  uint16_t alarmMaxMm = 1500;        // zona de alarme max (mm)
  uint16_t alarmGpio = 34;           // GPIO do alarme
  uint16_t alarmMode = 0;            // 0=zona (min≤X≤max), 1=limite (X<min ∨ X>max)
  uint16_t sensorFilterMode = 1;     // 0=EMA direto, 1=Buffer+Mediana+EMA
  char webUsername[16] = {'a', 'd', 'm', 'i', 'n', '\0'};
  char webPassword[16] = {'p', 'u', 'l', 's', 'e', '\0'};
  char apSsid[33] = {};
  char apPassword[64] = {};
  uint16_t webServerAlwaysOn = 1;
  uint16_t webServerTimeoutMinutes = 15;
};

class ConfigRepository {
 public:
  explicit ConfigRepository(NvsStore* store);

  esp_err_t Load(SafeDockConfig* config);
  esp_err_t Save(const SafeDockConfig& config);
  esp_err_t SaveSsidCode(uint16_t code);
  esp_err_t LoadSsidCode(uint16_t* code);
  esp_err_t SaveGeoConfig(uint16_t adjacentMm, int16_t offsetMm,
                          uint16_t alarmMinMm, uint16_t alarmMaxMm, uint16_t alarmMode);
  esp_err_t LoadGeoConfig(uint16_t* adjacentMm, int16_t* offsetMm,
                          uint16_t* alarmMinMm, uint16_t* alarmMaxMm, uint16_t* alarmMode);
  esp_err_t SaveSensorMode(uint16_t mode);
  esp_err_t LoadSensorMode(uint16_t* mode);
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
