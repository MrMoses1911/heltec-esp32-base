#include "storage/config_repository.h"
#include <cstdlib>
#include <cstring>

namespace pulse {
namespace storage {

struct PersistedConfig {
  uint32_t magic = 0x53444643;
  uint16_t schemaVersion = 1;
  uint16_t freqMinHz = 90;
  uint16_t freqMaxHz = 660;
  uint16_t distanceMinMm = 40;
  uint16_t distanceMaxMm = 2200;
  uint16_t sensorGpio = 38;
  char webUsername[16] = {'a', 'd', 'm', 'i', 'n', '\0'};
  char webPassword[16] = {'p', 'u', 'l', 's', 'e', '\0'};
  char apSsid[33] = {};
  char apPassword[64] = {};
  uint16_t webServerAlwaysOn = 0;
  uint16_t webServerTimeoutMinutes = 15;
};

ConfigRepository::ConfigRepository(NvsStore* store) : store_(store) {}

esp_err_t ConfigRepository::Load(SafeDockConfig* config) {
  if (config == nullptr || store_ == nullptr) return ESP_ERR_INVALID_ARG;

  PersistedConfig record;
  esp_err_t err = store_->ReadBlob(kNamespace, kBlobKey, &record, sizeof(PersistedConfig));

  if (err == ESP_ERR_NVS_NOT_FOUND) {
    *config = SafeDockConfig{};
    return ESP_OK;
  }
  if (err != ESP_OK) return err;
  if (record.magic != kMagic) {
    *config = SafeDockConfig{};
    return ESP_OK;
  }

  config->schemaVersion = record.schemaVersion;
  config->freqMinHz = record.freqMinHz;
  config->freqMaxHz = record.freqMaxHz;
  config->distanceMinMm = record.distanceMinMm;
  config->distanceMaxMm = record.distanceMaxMm;
  config->sensorGpio = record.sensorGpio;
  std::memcpy(config->webUsername, record.webUsername, sizeof(config->webUsername));
  std::memcpy(config->webPassword, record.webPassword, sizeof(config->webPassword));
  std::memcpy(config->apSsid, record.apSsid, sizeof(config->apSsid));
  std::memcpy(config->apPassword, record.apPassword, sizeof(config->apPassword));
  config->webServerAlwaysOn = record.webServerAlwaysOn;
  config->webServerTimeoutMinutes = record.webServerTimeoutMinutes;

  return ESP_OK;
}

esp_err_t ConfigRepository::Save(const SafeDockConfig& config) {
  if (store_ == nullptr) return ESP_ERR_INVALID_STATE;

  PersistedConfig record;
  record.magic = kMagic;
  record.schemaVersion = config.schemaVersion;
  record.freqMinHz = config.freqMinHz;
  record.freqMaxHz = config.freqMaxHz;
  record.distanceMinMm = config.distanceMinMm;
  record.distanceMaxMm = config.distanceMaxMm;
  record.sensorGpio = config.sensorGpio;
  std::memcpy(record.webUsername, config.webUsername, sizeof(record.webUsername));
  std::memcpy(record.webPassword, config.webPassword, sizeof(record.webPassword));
  std::memcpy(record.apSsid, config.apSsid, sizeof(record.apSsid));
  std::memcpy(record.apPassword, config.apPassword, sizeof(record.apPassword));
  record.webServerAlwaysOn = config.webServerAlwaysOn;
  record.webServerTimeoutMinutes = config.webServerTimeoutMinutes;

  return store_->WriteBlob(kNamespace, kBlobKey, &record, sizeof(PersistedConfig));
}

esp_err_t ConfigRepository::FactoryReset() {
  return store_->EraseAll();
}

}  // namespace storage
}  // namespace pulse
