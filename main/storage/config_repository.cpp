#include "storage/config_repository.h"
#include <cstdlib>
#include <cstring>
#include "esp_log.h"

static constexpr char kLogTag[] = "ConfigRepo";

namespace pulse {
namespace storage {

struct PersistedConfig {
  uint32_t magic = 0x53444643;
  uint16_t schemaVersion = 1;
  uint16_t freqMinHz = 100;
  uint16_t freqMaxHz = 600;
  uint16_t distanceMinMm = 50;
  uint16_t distanceMaxMm = 5000;
  uint16_t sensorGpio = 38;
  char webUsername[16] = {'a', 'd', 'm', 'i', 'n', '\0'};
  char webPassword[16] = {'p', 'u', 'l', 's', 'e', '\0'};
  char apSsid[33] = {};
  char apPassword[64] = {};
  uint16_t webServerAlwaysOn = 1;
  uint16_t webServerTimeoutMinutes = 15;
};

ConfigRepository::ConfigRepository(NvsStore* store) : store_(store) {}

esp_err_t ConfigRepository::Load(SafeDockConfig* config) {
  if (config == nullptr || store_ == nullptr) return ESP_ERR_INVALID_ARG;

  PersistedConfig record;
  esp_err_t err = store_->ReadBlob(kNamespace, kBlobKey, &record, sizeof(PersistedConfig));

  if (err == ESP_ERR_NVS_NOT_FOUND) {
    *config = SafeDockConfig{};
    ESP_LOGI(kLogTag, "NVS vazio, usando defaults");
    return ESP_OK;
  }
  if (err != ESP_OK) {
    ESP_LOGW(kLogTag, "Leitura NVS falhou: %s, usando defaults", esp_err_to_name(err));
    *config = SafeDockConfig{};
    return ESP_OK;  // não propaga erro — fallback seguro
  }
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

  ESP_LOGI(kLogTag, "Config carregada: ssid='%.*s' alwaysOn=%u",
           (int)sizeof(config->apSsid), config->apSsid, config->webServerAlwaysOn);
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

esp_err_t ConfigRepository::SaveSsidCode(uint16_t code) {
  if (store_ == nullptr) return ESP_ERR_INVALID_STATE;
  return store_->WriteU16(kNamespace, "ssid_code", code);
}

esp_err_t ConfigRepository::LoadSsidCode(uint16_t* code) {
  if (store_ == nullptr || code == nullptr) return ESP_ERR_INVALID_ARG;
  esp_err_t err = store_->ReadU16(kNamespace, "ssid_code", code);
  if (err == ESP_ERR_NVS_NOT_FOUND) { *code = 0; return ESP_OK; }
  return err;
}

esp_err_t ConfigRepository::SaveGeoConfig(uint16_t adjacentMm,
    int16_t offsetMm, uint16_t alarmMinMm, uint16_t alarmMaxMm, uint16_t alarmMode) {
  if (store_ == nullptr) return ESP_ERR_INVALID_STATE;
  esp_err_t e;
  if ((e = store_->WriteU16(kNamespace, "geo_adj", adjacentMm)) != ESP_OK) return e;
  if ((e = store_->WriteI16(kNamespace, "geo_offs", offsetMm)) != ESP_OK) return e;
  if ((e = store_->WriteU16(kNamespace, "geo_amin", alarmMinMm)) != ESP_OK) return e;
  if ((e = store_->WriteU16(kNamespace, "geo_amax", alarmMaxMm)) != ESP_OK) return e;
  if ((e = store_->WriteU16(kNamespace, "geo_mode", alarmMode)) != ESP_OK) return e;
  return ESP_OK;
}

esp_err_t ConfigRepository::LoadGeoConfig(uint16_t* adjacentMm,
    int16_t* offsetMm, uint16_t* alarmMinMm, uint16_t* alarmMaxMm, uint16_t* alarmMode) {
  if (store_ == nullptr) return ESP_ERR_INVALID_ARG;
  auto def = [](esp_err_t e, uint16_t* v, uint16_t d) { *v = (e == ESP_OK) ? *v : d; };
  def(store_->ReadU16(kNamespace, "geo_adj", adjacentMm), adjacentMm, 2000);
  if (store_->ReadI16(kNamespace, "geo_offs", offsetMm) != ESP_OK) *offsetMm = 0;
  def(store_->ReadU16(kNamespace, "geo_amin", alarmMinMm), alarmMinMm, 500);
  def(store_->ReadU16(kNamespace, "geo_amax", alarmMaxMm), alarmMaxMm, 1500);
  def(store_->ReadU16(kNamespace, "geo_mode", alarmMode), alarmMode, 0);
  return ESP_OK;
}

}  // namespace storage
}  // namespace pulse
