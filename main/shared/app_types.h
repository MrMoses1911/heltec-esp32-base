#ifndef PULSE_SAFEDOCK_SHARED_APP_TYPES_H
#define PULSE_SAFEDOCK_SHARED_APP_TYPES_H

#include <array>
#include <cstring>
#include <cstdint>

namespace pulse {
namespace shared {

constexpr size_t kMaxRemotePeers = 32;
constexpr size_t kMaxWhitelistSlots = 64;
constexpr size_t kMaxRxBufferEntries = 5;
constexpr size_t kDeviceUidWordCount = 4;
constexpr size_t kGroupKeyWordCount = 4;

enum class ModbusSerialFormat : uint16_t {
  k8N1 = 1,
  k8E1 = 2,
  k8O1 = 3,
  k8N2 = 4,
};

enum class DeviceVariant : uint16_t {
  kUnknown = 0,
  k8Di = 1,
  k8Do = 2,
};

enum class GatewayCommunicationMode : uint16_t {
  kDisabled = 0,
  kInputMirror = 1,
  kModbusRemoteOutputs = 2,
  kBroadcast = 3,
};

enum class ChannelRemoteSourceType : uint16_t {
  kNone = 0,
  kGateway = 1,
};

enum class ChannelCommunicationMode : uint16_t {
  kDisabled = 0,
  kInputMirrorRemote = 1,
  kTemperatureAlarmRemote = 2,
  kTemperatureAlarmLocal = 3,
  kHumidityAlarmRemote = 4,
  kHumidityAlarmLocal = 5,
  kModbusControlRemote = 6,
  kModbusControlLocal = 7,
};

constexpr uint16_t kChannelRouteFlagAlarmUseTemperature = 0x0001;
constexpr uint16_t kChannelRouteFlagAlarmUseHumidity = 0x0002;

enum class LoRaConfigurationMode : uint16_t {
  kSimplified = 0,
  kAdvanced = 1,
};

enum class LoRaEnvironmentType : uint16_t {
  kOpenField = 0,
  kUrbanLight = 1,
  kSomeWalls = 2,
  kManyWalls = 3,
  kIndustrialLight = 4,
  kIndustrialHeavy = 5,
  kMountainous = 6,
};

enum class LoRaLineOfSight : uint16_t {
  kGood = 0,
  kPartial = 1,
  kNone = 2,
};

enum class LoRaProjectPriority : uint16_t {
  kBalanced = 0,
  kMoreDevices = 1,
  kMoreRange = 2,
  kMoreReliability = 3,
};

enum class DoChannelMode : uint16_t {
  kDisabled = 0,
  kModbusLocal = 1,
  kMirrorDiRemote = 2,
  kAlarmTempLocal = 3,
  kAlarmHumidLocal = 4,
  kAlarmTempOrHumidLocal = 5,
  kAlarmTempRemote = 6,
  kAlarmHumidRemote = 7,
  kAlarmTempOrHumidRemote = 8,
};

enum class DoOutputBehavior : uint16_t {
  kNormal = 0,
  kNormalRetentive = 1,
  kInverted = 2,
  kInvertedRetentive = 3,
};

enum class DoAlarmMode : uint16_t {
  kDisabled = 0,
  kRemoteTemperature = 1,
  kRemoteHumidity = 2,
  kRemoteTemperatureOrHumidity = 3,
  kLocalTemperature = 4,
  kLocalHumidity = 5,
  kLocalTemperatureOrHumidity = 6,
};

constexpr uint16_t kIoAlarmFlagSensorFault = 0x0001;
constexpr uint16_t kIoAlarmFlagPinConfiguration = 0x0002;
constexpr uint16_t kIoAlarmFlagTemperature = 0x0004;
constexpr uint16_t kIoAlarmFlagHumidity = 0x0008;
constexpr uint16_t kLocalAlarmFlagTemperature = 0x0001;
constexpr uint16_t kLocalAlarmFlagHumidity = 0x0002;
constexpr uint16_t kRemoteLinkFlagEncrypted = 0x0001;
constexpr uint16_t kRemoteLinkFlagUnknownSource = 0x0002;

enum class ButtonMenuOption : uint8_t {
  kDeviceInfo = 0,
  kWebserver = 1,
  kResources = 2,
  kMonitor = 3,
  kPairing = 4,
  kFactoryReset = 5,
  kGatewayLogin = 6,
};

enum class ButtonMenuScreen : uint8_t {
  kMain = 0,
  kResources = 1,
  kTxRx = 2,
  kLoRa = 3,
  kModbus = 4,
  kIo = 5,
  kMonitor = 6,
  kPairing = 7,
  kGroupPairing = 8,
  kGatewayLogin = 9,
  kGatewayAccess = 10,
  kGatewayPasswordChange = 11,
  kFactoryResetConfirm = 12,
};

constexpr uint16_t kGatewayMenuVisibleRuntime = 0x0001;
constexpr uint16_t kGatewayMenuVisibleWebserver = 0x0002;
constexpr uint16_t kGatewayMenuVisibleResources = 0x0004;
constexpr uint16_t kGatewayMenuVisibleMonitor = 0x0008;
constexpr uint16_t kGatewayMenuVisiblePairing = 0x0010;
constexpr uint16_t kGatewayMenuVisibleMaskAll =
    kGatewayMenuVisibleRuntime |
    kGatewayMenuVisibleWebserver |
    kGatewayMenuVisibleResources |
    kGatewayMenuVisibleMonitor |
    kGatewayMenuVisiblePairing;

enum class ButtonPairingMenuOption : uint8_t {
  kPairGateway = 0,
  kPairGroup = 1,
  kBack = 2,
};

enum class ButtonGroupPairingMenuOption : uint8_t {
  kCreateGroup = 0,
  kJoinGroup = 1,
  kAddGateway = 2,
  kChangeGroup = 3,
  kCreateNewGroup = 4,
  kLeaveGroup = 5,
  kBack = 6,
};

enum class ButtonResourceMenuOption : uint8_t {
  kTxRx = 0,
  kLoRa = 1,
  kModbus = 2,
  kIo = 3,
  kGatewayAccess = 4,
  kBack = 5,
};

enum class ButtonGatewayAccessMenuOption : uint8_t {
  kEnableDisable = 0,
  kChangePassword = 1,
  kBack = 2,
};

enum class ButtonFactoryResetConfirmOption : uint8_t {
  kYes = 0,
  kNo = 1,
};

enum class ButtonTxRxMenuOption : uint8_t {
  kLocalId = 0,
  kMode = 1,
  kSourceAlias = 2,
  kDestinationAlias = 3,
  kWhitelist = 4,
  kPeriod = 5,
  kBack = 6,
};

enum class ButtonLoRaMenuOption : uint8_t {
  kFrequency = 0,
  kBandwidth = 1,
  kSpreadingFactor = 2,
  kPower = 3,
  kBack = 4,
};

enum class ButtonModbusMenuOption : uint8_t {
  kEnabled = 0,
  kAddress = 1,
  kBaud = 2,
  kFormat = 3,
  kBack = 4,
};

enum class ButtonIoMenuOption : uint8_t {
  kChannel = 0,
  kDiMode = 1,
  kDiDebounce = 2,
  kDoMode = 3,
  kDoPulse = 4,
  kBack = 5,
};

enum class AutoPairingState : uint16_t {
  kIdle = 0,
  kWaiting = 1,
  kPairing = 2,
  kFinalized = 3,
};

enum class GroupPairingState : uint16_t {
  kIdle = 0,
  kWaiting = 1,
  kPairing = 2,
  kFinalized = 3,
};

enum class GroupPairingAction : uint16_t {
  kNone = 0,
  kCreateGroup = 1,
  kJoinGroup = 2,
  kAddGateway = 3,
  kChangeGroup = 4,
  kCreateNewGroup = 5,
  kLeaveGroup = 6,
};

enum class SystemState : uint16_t {
  kBoot = 1,
  kSelfTest = 2,
  kIdle = 3,
  kNormalOperation = 4,
  kApMode = 5,
  kConfigMode = 6,
  kOtaMode = 7,
  kDegraded = 8,
  kFault = 9,
  kRestartPending = 10,
  kFactoryResetPending = 11,
  kButtonMenu = 12,
  kWebserverStarting = 13,
  kWebserverStopping = 14,
};

constexpr size_t kMaxModbusMasterSlots = 128;
struct ModbusMasterRegisterConfig {
  uint8_t  slaveAddress    = 0;
  uint8_t  functionCode    = 3;
  uint16_t registerAddress = 0;
  uint8_t  registerCount   = 1;
};

struct WhitelistSlotConfig {
  uint16_t alias = 0;
  std::array<uint16_t, kDeviceUidWordCount> uidFullWords = {0, 0, 0, 0};
  uint16_t flags = 0;
};

struct ChannelRoutingConfig {
  ChannelCommunicationMode mode = ChannelCommunicationMode::kDisabled;
  ChannelRemoteSourceType sourceType = ChannelRemoteSourceType::kNone;
  uint16_t remoteAlias = 0;
  uint16_t remoteChannel = 0;
  uint16_t flags = 0;
};

struct DeviceConfig {
  DeviceConfig() {
    webUsername = {'a', 'd', 'm', 'i', 'n', '\0'};
    webPassword = {'p', 'u', 'l', 's', 'e', '\0'};
    fixedApIp = {'1', '9', '2', '.', '1', '6', '8', '.', '1', '0', '.', '1', '\0'};
    fixedApPassword.fill('\0');
    diCounterDebounceMs.fill(20);
    doPulseWidthsMs.fill(100);
    deviceUidFullWords.fill(0);
    whitelistSlots.fill(WhitelistSlotConfig{});
    channelRouting.fill(ChannelRoutingConfig{});
    modbusMasterRegisters.fill(ModbusMasterRegisterConfig{});
  }

  uint16_t schemaVersion = 34;
  uint16_t deviceAlias = 1;
  std::array<uint16_t, kDeviceUidWordCount> deviceUidFullWords{};
  DeviceVariant deviceVariant = DeviceVariant::kUnknown;
  uint16_t modbusSlaveAddress = 1;
  uint32_t modbusBaudRate = 9600;
  ModbusSerialFormat modbusSerialFormat = ModbusSerialFormat::k8N1;
  uint16_t modbusEnabled = 1;
  uint16_t telemetryBufferDepth = 4;
  uint16_t diModeMask = 0;
  uint16_t doModeMask = 0;
  std::array<uint16_t, 8> diCounterDebounceMs{};
  std::array<uint16_t, 8> doPulseWidthsMs{};
  int16_t temperatureAlarmMinX100 = static_cast<int16_t>(0x8000);
  int16_t temperatureAlarmMaxX100 = 0x7FFF;
  uint16_t humidityAlarmMinX100 = 0x8000;
  uint16_t humidityAlarmMaxX100 = 0x7FFF;
  std::array<uint16_t, 8> doAlarmModes{};
  std::array<ChannelRoutingConfig, 8> channelRouting{};
  std::array<uint16_t, 8> doChannelModes{};
  std::array<uint16_t, 8> doChannelSourceAliases{};
  std::array<uint16_t, 8> doChannelOutputBehaviors{};
  uint16_t groupRxFilterAlias = 0;
  uint16_t loraFrequencyMhzX10 = 9150;
  uint16_t loraBandwidthCode = 500;
  uint16_t loraSpreadingFactor = 9;
  uint16_t loraCodingRateCode = 0;
  int16_t loraTxPowerDbm = 10;
  uint16_t loraConfigurationMode = static_cast<uint16_t>(LoRaConfigurationMode::kSimplified);
  uint16_t loraScenarioRadiusMeters = 1000;
  uint16_t loraScenarioDeviceCount = 8;
  uint16_t loraScenarioTxIntervalSeconds = 300;
  uint16_t loraScenarioEnvironment = static_cast<uint16_t>(LoRaEnvironmentType::kOpenField);
  uint16_t loraScenarioLineOfSight = static_cast<uint16_t>(LoRaLineOfSight::kGood);
  uint16_t loraScenarioPriority = static_cast<uint16_t>(LoRaProjectPriority::kBalanced);
  uint16_t loraSelectedProfileId = 0;
  uint16_t telemetryTxPeriodicEnabled = 0;
  uint16_t telemetryTxPeriodSeconds = 10;
  uint16_t telemetryTxPeriodRandomizedEnabled = 0;
  uint16_t telemetryTxRandomRangeSeconds = 60;
  uint16_t telemetryTxModbusTriggerEnabled = 0;
  uint16_t telemetryTxButtonTriggerEnabled = 0;
  GatewayCommunicationMode gatewayCommunicationMode = GatewayCommunicationMode::kDisabled;
  uint16_t gatewayCommunicationDestinationAlias = 0;
  uint16_t gatewayCommunicationSourceAlias = 0;
  uint16_t gatewayCommunicationFilterEnabled = 0;
  uint16_t gatewayCommunicationWhitelistEnabled = 0;
  uint16_t gatewayCommunicationStateChangeTriggerEnabled = 0;
  uint16_t ioMirrorRetainRiseEnabled = 0;
  uint16_t webServerAlwaysOn = 0;
  uint16_t webServerTimeoutMinutes = 15;
  uint16_t gatewayMenuPasswordEnabled = 0;
  uint16_t gatewayMenuPasswordPin = 1234;
  uint16_t gatewayMenuVisibleWithoutLoginMask = 0;
  uint16_t gatewayMenuSessionTimeoutMinutes = 15;
  uint16_t autoPairingTimeoutMs = 60000;
  uint16_t groupKeyValid = 0;
  std::array<uint32_t, kGroupKeyWordCount> groupKeyWords{};
  std::array<char, 16> webUsername{};
  std::array<char, 16> webPassword{};
  std::array<char, 33> fixedApSsid{};
  std::array<char, 16> fixedApIp{};
  std::array<char, 64> fixedApPassword{};
  std::array<WhitelistSlotConfig, kMaxWhitelistSlots> whitelistSlots{};
  uint16_t modbusMasterEnabled         = 0;
  uint16_t modbusMasterTxMode          = 0;
  uint16_t modbusMasterTxPeriodSeconds = 30;
  uint16_t modbusMasterDestAlias       = 0;
  std::array<ModbusMasterRegisterConfig, kMaxModbusMasterSlots> modbusMasterRegisters{};
};

struct AutoPairingPeerInfo {
  bool valid = false;
  uint16_t whitelistSlot = 0xFFFF;
  uint16_t alias = 0;
  std::array<uint16_t, kDeviceUidWordCount> uidFullWords = {0, 0, 0, 0};
  int16_t lastRssi = 0;
  int16_t lastSnrX100 = 0;
};

struct AutoPairingStatus {
  bool active = false;
  AutoPairingState state = AutoPairingState::kIdle;
  uint16_t timeoutMs = 5000;
  uint16_t remainingMs = 0;
  AutoPairingPeerInfo peer{};
};

struct GroupPairingPeerInfo {
  bool valid = false;
  uint16_t alias = 0;
  std::array<uint16_t, kDeviceUidWordCount> uidFullWords = {0, 0, 0, 0};
  int16_t lastRssi = 0;
  int16_t lastSnrX100 = 0;
};

struct GroupPairingStatus {
  bool active = false;
  GroupPairingState state = GroupPairingState::kIdle;
  GroupPairingAction action = GroupPairingAction::kNone;
  uint16_t timeoutMs = 5000;
  uint16_t remainingMs = 0;
  GroupPairingPeerInfo peer{};
};

struct RemoteTelemetrySnapshot {
  bool valid = false;
  uint16_t protocolVersion = 0;
  uint16_t sequence = 0;
  uint16_t sourceAlias = 0;
  std::array<uint16_t, kDeviceUidWordCount> sourceUidFullWords = {0, 0, 0, 0};
  uint16_t communicationMode = 0;
  uint16_t deviceVariant = 0;
  uint16_t systemState = 0;
  uint16_t linkFlags = 0;
  uint16_t statusFlags = 0;
  int16_t temperatureX100 = 0;
  uint16_t humidityX100 = 0;
  uint16_t diModeMask = 0;
  uint16_t diDiscreteStateMask = 0;
  uint16_t doStateMask = 0;
  uint16_t ioAlarmFlags = 0;
  uint16_t uptimeMinutes = 0;
  int16_t lastRssi = 0;
  int16_t lastSnrX100 = 0;
  std::array<uint16_t, 8> diCounters = {0, 0, 0, 0, 0, 0, 0, 0};
  uint32_t lastUpdateMillis = 0;
};

struct LoRaRuntimeStatus {
  int16_t lastRssi = 0;
  int16_t lastSnrX100 = 0;
  uint16_t rxPacketCounter = 0;
  uint16_t txPacketCounter = 0;
  uint32_t lastRxMillis = 0;
};

struct RuntimeDiagnostics {
  uint16_t lastPersistError = 0;
  uint16_t lastLoraError = 0;
  uint16_t lastModbusError = 0;
  uint16_t lastGeneralErrorCode = 0;
  uint16_t lastRemoteTelemetryRejectReason = 0;
  uint16_t lastRemoteCommandRejectReason = 0;
  uint16_t lastRemoteDestinationRejectReason = 0;
};

}  // namespace shared
}  // namespace pulse

#endif
