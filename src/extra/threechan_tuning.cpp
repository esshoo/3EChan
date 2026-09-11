#include "extra/threechan_tuning.h"
#include "extra/cheats.h"
#include "extra/modloader.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace {

ThreeChanSettings g_settings = {};
std::string g_activeProfile = "Original";
std::string g_lastMessage = "Ready.";
bool g_initialized = false;

constexpr const char* PROFILE_DIR = "userfiles/3EChan/profiles";
void CaptureCurrentCheatState(ThreeChanCheatSettings& cheats) {
#if NEW_CHEATS
    cheats.allDragons =
        IsCheatEnabled(CheatOption::AllDragons);

    cheats.allLevels =
        IsCheatEnabled(CheatOption::AllLevels);

    cheats.godMode =
        IsCheatEnabled(CheatOption::GodMode);

    cheats.onePunchMan =
        IsCheatEnabled(CheatOption::OnePunchMan);

    cheats.heavenBound =
        IsCheatEnabled(CheatOption::HeavenBound);

    cheats.bobbleHead =
        IsCheatEnabled(CheatOption::BobbleHead);

    cheats.stuntquake =
        IsCheatEnabled(CheatOption::Stuntquake);

    cheats.mirrorWorld =
        IsCheatEnabled(CheatOption::MirrorWorld);

    cheats.lives99 =
        IsCheatEnabled(CheatOption::Lives99);
#else
    (void)cheats;
#endif
}

bool CaptureCurrentModState(ThreeChanModProfileSettings& mods) {
#ifdef MOD_LOADER
    bool systemEnabled = true;
    std::vector<ModConfiguredState> states;

    if (!ModLoader::Instance().GetConfiguredModStates(
            systemEnabled,
            states)) {
        return false;
    }

    mods.systemEnabled = systemEnabled;
    mods.mods.clear();
    mods.mods.reserve(states.size());

    for (const ModConfiguredState& state : states) {
        ThreeChanModState profileState;
        profileState.folder = state.folder;
        profileState.enabled = state.enabled;

        mods.mods.push_back(std::move(profileState));
    }

    return true;
#else
    (void)mods;
    return false;
#endif
}
bool ApplyModState(const ThreeChanModProfileSettings& mods) {
#ifdef MOD_LOADER
    std::vector<ModConfiguredState> states;
    states.reserve(mods.mods.size());

    for (const ThreeChanModState& state : mods.mods) {
        if (state.folder.empty()) {
            continue;
        }

        ModConfiguredState configured;
        configured.folder = state.folder;
        configured.enabled = state.enabled;
        states.push_back(std::move(configured));
    }

    return ModLoader::Instance().ApplyConfiguredModStates(
        mods.systemEnabled,
        states);
#else
    (void)mods;
    return false;
#endif
}

void ApplyCheatState(const ThreeChanCheatSettings& cheats) {
#if NEW_CHEATS
    SetCheatEnabled(CheatOption::AllDragons, cheats.allDragons);
    SetCheatEnabled(CheatOption::AllLevels, cheats.allLevels);
    SetCheatEnabled(CheatOption::GodMode, cheats.godMode);
    SetCheatEnabled(CheatOption::OnePunchMan, cheats.onePunchMan);
    SetCheatEnabled(CheatOption::HeavenBound, cheats.heavenBound);
    SetCheatEnabled(CheatOption::BobbleHead, cheats.bobbleHead);
    SetCheatEnabled(CheatOption::Stuntquake, cheats.stuntquake);
    SetCheatEnabled(CheatOption::MirrorWorld, cheats.mirrorWorld);
    SetCheatEnabled(CheatOption::Lives99, cheats.lives99);
#else
    (void)cheats;
#endif
}
using ValueMap = std::unordered_map<std::string, std::string>;

std::string MakeKey(const char* section, const char* key) {
    return std::string(section) + "." + key;
}

std::string Trim(const std::string& input) {
    size_t first = 0;
    while (first < input.size()
        && std::isspace(static_cast<unsigned char>(input[first]))) {
        ++first;
    }

    size_t last = input.size();
    while (last > first
        && std::isspace(static_cast<unsigned char>(input[last - 1]))) {
        --last;
    }

    return input.substr(first, last - first);
}

std::string SanitizeProfileName(const char* rawName) {
    if (!rawName) {
        return {};
    }

    std::string clean;

    for (const char c : std::string(rawName)) {
        const unsigned char uc = static_cast<unsigned char>(c);

        if (std::isalnum(uc)
            || c == '-'
            || c == '_'
            || c == ' '
            || c == '.') {
            clean.push_back(c);
        }
    }

    clean = Trim(clean);

    while (!clean.empty() && clean.back() == '.') {
        clean.pop_back();
    }

    if (clean.size() > 48) {
        clean.resize(48);
    }

    return clean;
}

std::filesystem::path ProfilePath(const std::string& profile) {
    return std::filesystem::path(PROFILE_DIR) / (profile + ".ini");
}

bool ParseBoolValue(const std::string& value, bool& outValue) {
    std::string lowered;
    lowered.reserve(value.size());

    for (char c : value) {
        lowered.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(c))));
    }

    if (lowered == "1"
        || lowered == "true"
        || lowered == "yes"
        || lowered == "on") {
        outValue = true;
        return true;
    }

    if (lowered == "0"
        || lowered == "false"
        || lowered == "no"
        || lowered == "off") {
        outValue = false;
        return true;
    }

    return false;
}

bool LoadIni(const std::filesystem::path& path, ValueMap& values) {
    std::ifstream file(path);

    if (!file.is_open()) {
        return false;
    }

    std::string section;
    std::string line;

    while (std::getline(file, line)) {
        line = Trim(line);

        if (line.empty()
            || line[0] == ';'
            || line[0] == '#') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            section = Trim(line.substr(1, line.size() - 2));
            continue;
        }

        const size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string key = Trim(line.substr(0, equals));
        const std::string value = Trim(line.substr(equals + 1));

        if (!section.empty() && !key.empty()) {
            values[MakeKey(section.c_str(), key.c_str())] = value;
        }
    }

    return true;
}

void ReadBool(
    const ValueMap& values,
    const char* section,
    const char* key,
    bool& field) {

    const auto it = values.find(MakeKey(section, key));
    if (it == values.end()) {
        return;
    }

    bool parsed = field;
    if (ParseBoolValue(it->second, parsed)) {
        field = parsed;
    }
}

void ReadString(
    const ValueMap& values,
    const char* section,
    const char* key,
    std::string& field) {

    const auto it = values.find(MakeKey(section, key));

    if (it == values.end()) {
        return;
    }

    field = it->second;
}

void ReadInt(
    const ValueMap& values,
    const char* section,
    const char* key,
    int& field) {

    const auto it = values.find(MakeKey(section, key));
    if (it == values.end()) {
        return;
    }

    try {
        field = std::stoi(it->second);
    }
    catch (...) {
    }
}

void ReadFloat(
    const ValueMap& values,
    const char* section,
    const char* key,
    float& field) {

    const auto it = values.find(MakeKey(section, key));
    if (it == values.end()) {
        return;
    }

    try {
        field = std::stof(it->second);
    }
    catch (...) {
    }
}

void WriteBool(std::ofstream& file, const char* key, bool value) {
    file << key << '=' << (value ? 1 : 0) << '\n';
}

void WriteInt(std::ofstream& file, const char* key, int value) {
    file << key << '=' << value << '\n';
}

void WriteFloat(std::ofstream& file, const char* key, float value) {
    file << key << '='
         << std::fixed
         << std::setprecision(4)
         << value
         << '\n';
}

void ClampSettings(ThreeChanSettings& s) {
    auto clampMul = [](float& v) {
        v = std::clamp(v, 0.05f, 10.0f);
    };

    auto& e = s.enemies;

    clampMul(e.healthMultiplier);
    clampMul(e.damageMultiplier);
    clampMul(e.attackFrequencyMultiplier);
    clampMul(e.aggressionMultiplier);
    clampMul(e.distancingMultiplier);
    clampMul(e.circlingMultiplier);
    clampMul(e.decisionSpeedMultiplier);
    clampMul(e.runningSpeedMultiplier);
    clampMul(e.strafingSpeedMultiplier);
    clampMul(e.turnSpeedMultiplier);
    clampMul(e.attackAnimationSpeedMultiplier);
    clampMul(e.punchChanceMultiplier);
    clampMul(e.kickChanceMultiplier);
    clampMul(e.throwChanceMultiplier);
    clampMul(e.comboChanceMultiplier);
    clampMul(e.stunDurationMultiplier);
    clampMul(e.knockdownRecoveryMultiplier);
    clampMul(e.getUpSpeedMultiplier);

    e.minThinkFrames = std::clamp(e.minThinkFrames, 0, 300);
    e.maxThinkFrames = std::clamp(e.maxThinkFrames, 0, 300);

    if (e.maxThinkFrames < e.minThinkFrames) {
        e.maxThinkFrames = e.minThinkFrames;
    }

    auto& group = s.groupCombat;
    group.maxEngagedEnemies = std::clamp(group.maxEngagedEnemies, 0, 32);
    group.maxSimultaneousAttacks =
        std::clamp(group.maxSimultaneousAttacks, 0, 8);
    group.minimumAttackGapMs =
        std::clamp(group.minimumAttackGapMs, 0, 5000);

    auto& spawn = s.spawning;
    clampMul(spawn.enemyCountMultiplier);
    spawn.waveDelaySeconds = std::clamp(spawn.waveDelaySeconds, 0.0f, 30.0f);
    spawn.spawnBurstOverride =
        std::clamp(spawn.spawnBurstOverride, 0, 16);
    spawn.maxAliveOverride =
        std::clamp(spawn.maxAliveOverride, 0, 32);
    spawn.activeZoneThresholdOffset =
        std::clamp(spawn.activeZoneThresholdOffset, -16, 16);

    auto& bg = s.bosses.global;
    clampMul(bg.healthMultiplier);
    clampMul(bg.damageMultiplier);
    clampMul(bg.movementSpeedMultiplier);
    clampMul(bg.decisionSpeedMultiplier);
    clampMul(bg.attackSpeedMultiplier);
    clampMul(bg.recoverySpeedMultiplier);

    clampMul(s.bosses.butch.stompDamageMultiplier);
    clampMul(s.bosses.butch.attackDistanceMultiplier);
    clampMul(s.bosses.butch.stompFrequencyMultiplier);
    clampMul(s.bosses.butch.chargePressureMultiplier);
    clampMul(s.bosses.butch.potFrequencyMultiplier);

    clampMul(s.bosses.grontar.closeAttackRangeMultiplier);
    clampMul(s.bosses.grontar.farAttackRangeMultiplier);
    clampMul(s.bosses.grontar.throwRangeMultiplier);
    clampMul(s.bosses.grontar.targetTrackingMultiplier);
    clampMul(s.bosses.grontar.diveRollPressureMultiplier);

    clampMul(s.bosses.dante.missileDamageMultiplier);
    clampMul(s.bosses.dante.missileRadiusMultiplier);
    clampMul(s.bosses.dante.missileFrequencyMultiplier);
    clampMul(s.bosses.dante.missileRecoveryMultiplier);
    clampMul(s.bosses.dante.targetedMissileTimingMultiplier);

    s.bosses.dante.phase2HealthPercent =
        std::clamp(s.bosses.dante.phase2HealthPercent, -1, 100);
    s.bosses.dante.phase3HealthPercent =
        std::clamp(s.bosses.dante.phase3HealthPercent, -1, 100);

    clampMul(s.bosses.paul.closeAttackRangeMultiplier);
    clampMul(s.bosses.paul.danceTimeMultiplier);
    clampMul(s.bosses.paul.recoveryTimeMultiplier);
    clampMul(s.bosses.paul.aggressionMultiplier);

    clampMul(s.bosses.oscar.closeRangeMultiplier);
    clampMul(s.bosses.oscar.midRangeMultiplier);
    clampMul(s.bosses.oscar.henchmanCoordinationMultiplier);
    clampMul(s.bosses.oscar.henchmanSyncDelayMultiplier);
    clampMul(s.bosses.oscar.aggressionMultiplier);

    auto& c = s.camera;

    int cameraMode = static_cast<int>(c.mode);
    cameraMode = std::clamp(cameraMode, 0, 3);
    c.mode = static_cast<ThreeChanCameraMode>(cameraMode);

    c.followFov = std::clamp(c.followFov, 1, 30);
    c.rigidDistance = std::clamp(c.rigidDistance, 100, 10000);
    c.rigidHeight = std::clamp(c.rigidHeight, -3000, 10000);
    c.rigidSpinRate = std::clamp(c.rigidSpinRate, 0, 32767);
    c.rigidFov = std::clamp(c.rigidFov, 1, 30);

    c.fixedSafeMarginXPercent =
        std::clamp(c.fixedSafeMarginXPercent, 0.0f, 49.0f);
    c.fixedSafeMarginYPercent =
        std::clamp(c.fixedSafeMarginYPercent, 0.0f, 49.0f);

    c.fixedTransitionTime =
        std::clamp(c.fixedTransitionTime, 0.0f, 5.0f);
    c.fixedMinimumHoldTime =
        std::clamp(c.fixedMinimumHoldTime, 0.0f, 10.0f);
    c.fixedRepositionCooldown =
        std::clamp(c.fixedRepositionCooldown, 0.0f, 10.0f);
    c.fixedFov = std::clamp(c.fixedFov, 1, 30);

    c.thirdPersonDistance =
        std::clamp(c.thirdPersonDistance, 100, 6000);
    c.thirdPersonHeight =
        std::clamp(c.thirdPersonHeight, -2000, 5000);
    c.thirdPersonTargetHeight =
        std::clamp(c.thirdPersonTargetHeight, -2000, 5000);
    c.thirdPersonShoulderOffset =
        std::clamp(c.thirdPersonShoulderOffset, -2500, 2500);

    c.thirdPersonFov =
        std::clamp(c.thirdPersonFov, 1, 30);
    c.thirdPersonMinFov =
        std::clamp(c.thirdPersonMinFov, 1, 30);
    c.thirdPersonMaxFov =
        std::clamp(c.thirdPersonMaxFov, 1, 30);

    if (c.thirdPersonMaxFov < c.thirdPersonMinFov) {
        c.thirdPersonMaxFov = c.thirdPersonMinFov;
    }

    c.thirdPersonPositionLag =
        std::clamp(c.thirdPersonPositionLag, 0.0f, 50.0f);
    c.thirdPersonRotationLag =
        std::clamp(c.thirdPersonRotationLag, 0.0f, 50.0f);
    c.thirdPersonTargetLag =
        std::clamp(c.thirdPersonTargetLag, 0.0f, 50.0f);

    c.thirdPersonAutoCenterStrength =
        std::clamp(c.thirdPersonAutoCenterStrength, 0.0f, 5.0f);
    c.thirdPersonAutoCenterDelay =
        std::clamp(c.thirdPersonAutoCenterDelay, 0.0f, 10.0f);

    c.thirdPersonFollowDirection =
        std::clamp(c.thirdPersonFollowDirection, 0, 2);

    c.thirdPersonCollisionPadding =
        std::clamp(c.thirdPersonCollisionPadding, 0, 1000);
    c.thirdPersonMinimumDistance =
        std::clamp(c.thirdPersonMinimumDistance, 50, 3000);

    s.advanced.intendedFightingCollisionCapacity =
        std::clamp(s.advanced.intendedFightingCollisionCapacity, 12, 64);
    s.advanced.hardEnemySafetyCap =
        std::clamp(s.advanced.hardEnemySafetyCap, 3, 64);
    s.advanced.hardSimultaneousAttackSafetyCap =
        std::clamp(s.advanced.hardSimultaneousAttackSafetyCap, 1, 16);
}

} // namespace

namespace ThreeChanTuning {

void Initialize() {
    if (g_initialized) {
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(PROFILE_DIR, ec);

    g_settings = ThreeChanSettings{};
    g_activeProfile = "Original";

    if (ec) {
        g_lastMessage =
            "Warning: could not create userfiles/3EChan/profiles.";
    }
    else {
        g_lastMessage = "3EChan tuning system initialized.";
    }

    g_initialized = true;
}

ThreeChanSettings& Get() {
    Initialize();
    return g_settings;
}

const ThreeChanSettings& GetConst() {
    Initialize();
    return g_settings;
}

void ResetToOriginal() {
    Initialize();

    g_settings = ThreeChanSettings{};
    g_activeProfile = "Original";
    g_lastMessage = "Restored original 3EChan tuning values.";
}

bool SaveProfile(const char* rawName) {
    Initialize();

    const std::string name = SanitizeProfileName(rawName);

    if (name.empty()) {
        g_lastMessage = "Profile name is empty.";
        return false;
    }

    if (name == "Original" || name == "original") {
        g_lastMessage = "Original is reserved and cannot be overwritten.";
        return false;
    }

    ClampSettings(g_settings);

    if (g_settings.cheats.includeInProfile) {
        CaptureCurrentCheatState(g_settings.cheats);
    }

    if (g_settings.mods.includeInProfile &&
        !CaptureCurrentModState(g_settings.mods)) {
        g_lastMessage = "Could not capture current mod configuration.";
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(PROFILE_DIR, ec);

    if (ec) {
        g_lastMessage = "Could not create profile directory.";
        return false;
    }

    std::ofstream file(ProfilePath(name), std::ios::trunc);

    if (!file.is_open()) {
        g_lastMessage = "Could not open profile for writing.";
        return false;
    }

    file << "; 3EChan runtime tuning profile\n";
    file << "; Values are intentionally external so tuning does not require a rebuild.\n\n";

    file << "[ProfileFeatures]\n";
    WriteBool(file, "IncludeCheats", g_settings.cheats.includeInProfile);
    WriteBool(file, "IncludeMods", g_settings.mods.includeInProfile);

    file << "\n[Cheats]\n";
    const auto& ch = g_settings.cheats;
    WriteBool(file, "AllDragons", ch.allDragons);
    WriteBool(file, "AllLevels", ch.allLevels);
    WriteBool(file, "GodMode", ch.godMode);
    WriteBool(file, "OnePunchMan", ch.onePunchMan);
    WriteBool(file, "HeavenBound", ch.heavenBound);
    WriteBool(file, "BobbleHead", ch.bobbleHead);
    WriteBool(file, "Stuntquake", ch.stuntquake);
    WriteBool(file, "MirrorWorld", ch.mirrorWorld);
    WriteBool(file, "Lives99", ch.lives99);

    file << "\n[Mods]\n";
    const auto& modProfile = g_settings.mods;
    WriteBool(file, "SystemEnabled", modProfile.systemEnabled);
    WriteInt(file, "Count", static_cast<int>(modProfile.mods.size()));

    for (size_t i = 0; i < modProfile.mods.size(); ++i) {
        const ThreeChanModState& state = modProfile.mods[i];
        file << "Folder" << i << '=' << state.folder << '\n';
        file << "Enabled" << i << '=' << (state.enabled ? 1 : 0) << '\n';
    }

    file << "\n[General]\n";
    WriteBool(file, "MasterEnabled", g_settings.masterEnabled);
    WriteBool(file, "ApplyChangesLive", g_settings.applyChangesLive);

    file << "\n[Enemies]\n";
    const auto& e = g_settings.enemies;
    WriteBool(file, "Enabled", e.enabled);
    WriteFloat(file, "HealthMultiplier", e.healthMultiplier);
    WriteFloat(file, "DamageMultiplier", e.damageMultiplier);
    WriteFloat(file, "AttackFrequencyMultiplier", e.attackFrequencyMultiplier);
    WriteFloat(file, "AggressionMultiplier", e.aggressionMultiplier);
    WriteFloat(file, "DistancingMultiplier", e.distancingMultiplier);
    WriteFloat(file, "CirclingMultiplier", e.circlingMultiplier);
    WriteFloat(file, "DecisionSpeedMultiplier", e.decisionSpeedMultiplier);
    WriteBool(file, "UseCustomThinkRange", e.useCustomThinkRange);
    WriteInt(file, "MinThinkFrames", e.minThinkFrames);
    WriteInt(file, "MaxThinkFrames", e.maxThinkFrames);
    WriteFloat(file, "RunningSpeedMultiplier", e.runningSpeedMultiplier);
    WriteFloat(file, "StrafingSpeedMultiplier", e.strafingSpeedMultiplier);
    WriteFloat(file, "TurnSpeedMultiplier", e.turnSpeedMultiplier);
    WriteFloat(file, "AttackAnimationSpeedMultiplier", e.attackAnimationSpeedMultiplier);
    WriteFloat(file, "PunchChanceMultiplier", e.punchChanceMultiplier);
    WriteFloat(file, "KickChanceMultiplier", e.kickChanceMultiplier);
    WriteFloat(file, "ThrowChanceMultiplier", e.throwChanceMultiplier);
    WriteFloat(file, "ComboChanceMultiplier", e.comboChanceMultiplier);
    WriteFloat(file, "StunDurationMultiplier", e.stunDurationMultiplier);
    WriteFloat(file, "KnockdownRecoveryMultiplier", e.knockdownRecoveryMultiplier);
    WriteFloat(file, "GetUpSpeedMultiplier", e.getUpSpeedMultiplier);

    file << "\n[GroupCombat]\n";
    const auto& g = g_settings.groupCombat;
    WriteBool(file, "Enabled", g.enabled);
    WriteInt(file, "MaxEngagedEnemies", g.maxEngagedEnemies);
    WriteInt(file, "MaxSimultaneousAttacks", g.maxSimultaneousAttacks);
    WriteInt(file, "MinimumAttackGapMs", g.minimumAttackGapMs);
    WriteBool(file, "AttackerRotation", g.attackerRotation);
    WriteBool(file, "ChainPressure", g.chainPressure);
    WriteBool(file, "ImmediateReplacement", g.immediateReplacement);

    file << "\n[Spawning]\n";
    const auto& sp = g_settings.spawning;
    WriteBool(file, "Enabled", sp.enabled);
    WriteFloat(file, "EnemyCountMultiplier", sp.enemyCountMultiplier);
    WriteFloat(file, "WaveDelaySeconds", sp.waveDelaySeconds);
    WriteInt(file, "SpawnBurstOverride", sp.spawnBurstOverride);
    WriteInt(file, "MaxAliveOverride", sp.maxAliveOverride);
    WriteInt(file, "ActiveZoneThresholdOffset", sp.activeZoneThresholdOffset);
    WriteBool(file, "PauseGenerators", sp.pauseGenerators);

    file << "\n[Bosses]\n";
    const auto& bg = g_settings.bosses.global;
    WriteBool(file, "Enabled", bg.enabled);
    WriteFloat(file, "HealthMultiplier", bg.healthMultiplier);
    WriteFloat(file, "DamageMultiplier", bg.damageMultiplier);
    WriteFloat(file, "MovementSpeedMultiplier", bg.movementSpeedMultiplier);
    WriteFloat(file, "DecisionSpeedMultiplier", bg.decisionSpeedMultiplier);
    WriteFloat(file, "AttackSpeedMultiplier", bg.attackSpeedMultiplier);
    WriteFloat(file, "RecoverySpeedMultiplier", bg.recoverySpeedMultiplier);

    file << "\n[Butch]\n";
    WriteFloat(file, "StompDamageMultiplier", g_settings.bosses.butch.stompDamageMultiplier);
    WriteFloat(file, "AttackDistanceMultiplier", g_settings.bosses.butch.attackDistanceMultiplier);
    WriteFloat(file, "StompFrequencyMultiplier", g_settings.bosses.butch.stompFrequencyMultiplier);
    WriteFloat(file, "ChargePressureMultiplier", g_settings.bosses.butch.chargePressureMultiplier);
    WriteFloat(file, "PotFrequencyMultiplier", g_settings.bosses.butch.potFrequencyMultiplier);

    file << "\n[Grontar]\n";
    WriteFloat(file, "CloseAttackRangeMultiplier", g_settings.bosses.grontar.closeAttackRangeMultiplier);
    WriteFloat(file, "FarAttackRangeMultiplier", g_settings.bosses.grontar.farAttackRangeMultiplier);
    WriteFloat(file, "ThrowRangeMultiplier", g_settings.bosses.grontar.throwRangeMultiplier);
    WriteFloat(file, "TargetTrackingMultiplier", g_settings.bosses.grontar.targetTrackingMultiplier);
    WriteFloat(file, "DiveRollPressureMultiplier", g_settings.bosses.grontar.diveRollPressureMultiplier);

    file << "\n[Dante]\n";
    WriteInt(file, "Phase2HealthPercent", g_settings.bosses.dante.phase2HealthPercent);
    WriteInt(file, "Phase3HealthPercent", g_settings.bosses.dante.phase3HealthPercent);
    WriteFloat(file, "MissileDamageMultiplier", g_settings.bosses.dante.missileDamageMultiplier);
    WriteFloat(file, "MissileRadiusMultiplier", g_settings.bosses.dante.missileRadiusMultiplier);
    WriteFloat(file, "MissileFrequencyMultiplier", g_settings.bosses.dante.missileFrequencyMultiplier);
    WriteFloat(file, "MissileRecoveryMultiplier", g_settings.bosses.dante.missileRecoveryMultiplier);
    WriteFloat(file, "TargetedMissileTimingMultiplier", g_settings.bosses.dante.targetedMissileTimingMultiplier);

    file << "\n[Paul]\n";
    WriteFloat(file, "CloseAttackRangeMultiplier", g_settings.bosses.paul.closeAttackRangeMultiplier);
    WriteFloat(file, "DanceTimeMultiplier", g_settings.bosses.paul.danceTimeMultiplier);
    WriteFloat(file, "RecoveryTimeMultiplier", g_settings.bosses.paul.recoveryTimeMultiplier);
    WriteFloat(file, "AggressionMultiplier", g_settings.bosses.paul.aggressionMultiplier);

    file << "\n[Oscar]\n";
    WriteFloat(file, "CloseRangeMultiplier", g_settings.bosses.oscar.closeRangeMultiplier);
    WriteFloat(file, "MidRangeMultiplier", g_settings.bosses.oscar.midRangeMultiplier);
    WriteFloat(file, "HenchmanCoordinationMultiplier", g_settings.bosses.oscar.henchmanCoordinationMultiplier);
    WriteFloat(file, "HenchmanSyncDelayMultiplier", g_settings.bosses.oscar.henchmanSyncDelayMultiplier);
    WriteFloat(file, "AggressionMultiplier", g_settings.bosses.oscar.aggressionMultiplier);

    file << "\n[Camera]\n";
    const auto& c = g_settings.camera;
    WriteBool(file, "Enabled", c.enabled);
    WriteInt(file, "Mode", static_cast<int>(c.mode));

    WriteInt(file, "FollowFov", c.followFov);
    WriteFloat(file, "FollowMovementSmoothingMultiplier", c.followMovementSmoothingMultiplier);
    WriteFloat(file, "FollowTrackingSmoothingMultiplier", c.followTrackingSmoothingMultiplier);

    WriteInt(file, "RigidDistance", c.rigidDistance);
    WriteInt(file, "RigidHeight", c.rigidHeight);
    WriteInt(file, "RigidSpinRate", c.rigidSpinRate);
    WriteInt(file, "RigidFov", c.rigidFov);
    WriteFloat(file, "RigidMovementSmoothingMultiplier", c.rigidMovementSmoothingMultiplier);
    WriteFloat(file, "RigidTrackingSmoothingMultiplier", c.rigidTrackingSmoothingMultiplier);

    WriteFloat(file, "FixedSafeMarginXPercent", c.fixedSafeMarginXPercent);
    WriteFloat(file, "FixedSafeMarginYPercent", c.fixedSafeMarginYPercent);
    WriteBool(file, "FixedSmoothTransition", c.fixedSmoothTransition);
    WriteFloat(file, "FixedTransitionTime", c.fixedTransitionTime);
    WriteFloat(file, "FixedMinimumHoldTime", c.fixedMinimumHoldTime);
    WriteFloat(file, "FixedRepositionCooldown", c.fixedRepositionCooldown);
    WriteInt(file, "FixedTargetHeightOffset", c.fixedTargetHeightOffset);
    WriteInt(file, "FixedFov", c.fixedFov);
    WriteBool(file, "FixedUseOriginalRails", c.fixedUseOriginalRails);
    WriteBool(file, "FixedTrackJackie", c.fixedTrackJackie);
    WriteBool(file, "FixedShowSafeFrame", c.fixedShowSafeFrame);
    WriteBool(file, "FixedShowCameraAnchor", c.fixedShowCameraAnchor);

    WriteInt(file, "ThirdPersonDistance", c.thirdPersonDistance);
    WriteInt(file, "ThirdPersonHeight", c.thirdPersonHeight);
    WriteInt(file, "ThirdPersonTargetHeight", c.thirdPersonTargetHeight);
    WriteInt(file, "ThirdPersonShoulderOffset", c.thirdPersonShoulderOffset);
    WriteInt(file, "ThirdPersonFov", c.thirdPersonFov);
    WriteInt(file, "ThirdPersonMinFov", c.thirdPersonMinFov);
    WriteInt(file, "ThirdPersonMaxFov", c.thirdPersonMaxFov);
    WriteFloat(file, "ThirdPersonPositionLag", c.thirdPersonPositionLag);
    WriteFloat(file, "ThirdPersonRotationLag", c.thirdPersonRotationLag);
    WriteFloat(file, "ThirdPersonTargetLag", c.thirdPersonTargetLag);
    WriteFloat(file, "ThirdPersonAutoCenterStrength", c.thirdPersonAutoCenterStrength);
    WriteFloat(file, "ThirdPersonAutoCenterDelay", c.thirdPersonAutoCenterDelay);
    WriteInt(file, "ThirdPersonFollowDirection", c.thirdPersonFollowDirection);
    WriteBool(file, "ThirdPersonCollision", c.thirdPersonCollision);
    WriteInt(file, "ThirdPersonCollisionPadding", c.thirdPersonCollisionPadding);
    WriteInt(file, "ThirdPersonMinimumDistance", c.thirdPersonMinimumDistance);
    WriteBool(file, "ThirdPersonSmoothTurns", c.thirdPersonSmoothTurns);
    WriteBool(file, "ThirdPersonSmoothJumps", c.thirdPersonSmoothJumps);
    WriteBool(file, "ThirdPersonAutoRecoverBehindJackie", c.thirdPersonAutoRecoverBehindJackie);
    WriteBool(file, "ThirdPersonShowDesiredCamera", c.thirdPersonShowDesiredCamera);
    WriteBool(file, "ThirdPersonShowCollisionLine", c.thirdPersonShowCollisionLine);
    WriteBool(file, "ThirdPersonShowTarget", c.thirdPersonShowTarget);

    file << "\n[Inspector]\n";
    WriteBool(file, "ShowSelectedEnemyRuntime", g_settings.inspector.showSelectedEnemyRuntime);
    WriteBool(file, "ShowBehaviourRuntime", g_settings.inspector.showBehaviourRuntime);
    WriteBool(file, "ShowCombatSlotRuntime", g_settings.inspector.showCombatSlotRuntime);
    WriteBool(file, "ShowDamageRuntime", g_settings.inspector.showDamageRuntime);

    file << "\n[Telemetry]\n";
    WriteBool(file, "Enabled", g_settings.telemetry.enabled);
    WriteBool(file, "ShowEnemyCounts", g_settings.telemetry.showEnemyCounts);
    WriteBool(file, "ShowCombatSlots", g_settings.telemetry.showCombatSlots);
    WriteBool(file, "ShowAttackRate", g_settings.telemetry.showAttackRate);
    WriteBool(file, "ShowDamageRate", g_settings.telemetry.showDamageRate);
    WriteBool(file, "ShowSelectedAI", g_settings.telemetry.showSelectedAI);
    WriteBool(file, "ShowBossRuntime", g_settings.telemetry.showBossRuntime);

    file << "\n[Advanced]\n";
    WriteBool(file, "SafeMode", g_settings.advanced.safeMode);
    WriteInt(file, "IntendedFightingCollisionCapacity", g_settings.advanced.intendedFightingCollisionCapacity);
    WriteInt(file, "HardEnemySafetyCap", g_settings.advanced.hardEnemySafetyCap);
    WriteInt(file, "HardSimultaneousAttackSafetyCap", g_settings.advanced.hardSimultaneousAttackSafetyCap);
    WriteBool(file, "LogRuntimeChanges", g_settings.advanced.logRuntimeChanges);

    file.close();

    if (!file) {
        g_lastMessage = "Failed while writing profile.";
        return false;
    }

    g_activeProfile = name;
    g_lastMessage = "Profile saved: " + name;
    return true;
}

bool LoadProfile(const char* rawName) {
    Initialize();

    const std::string name = SanitizeProfileName(rawName);

    if (name.empty()) {
        g_lastMessage = "Profile name is empty.";
        return false;
    }

    if (name == "Original" || name == "original") {
        ResetToOriginal();
        return true;
    }

    ValueMap values;

    if (!LoadIni(ProfilePath(name), values)) {
        g_lastMessage = "Could not load profile: " + name;
        return false;
    }

    ThreeChanSettings loaded = {};
    ReadBool(
        values,
        "ProfileFeatures",
        "IncludeCheats",
        loaded.cheats.includeInProfile);

    ReadBool(
        values,
        "ProfileFeatures",
        "IncludeMods",
        loaded.mods.includeInProfile);

    auto& ch = loaded.cheats;

    ReadBool(values, "Cheats", "AllDragons", ch.allDragons);
    ReadBool(values, "Cheats", "AllLevels", ch.allLevels);
    ReadBool(values, "Cheats", "GodMode", ch.godMode);
    ReadBool(values, "Cheats", "OnePunchMan", ch.onePunchMan);
    ReadBool(values, "Cheats", "HeavenBound", ch.heavenBound);
    ReadBool(values, "Cheats", "BobbleHead", ch.bobbleHead);
    ReadBool(values, "Cheats", "Stuntquake", ch.stuntquake);
    ReadBool(values, "Cheats", "MirrorWorld", ch.mirrorWorld);
    ReadBool(values, "Cheats", "Lives99", ch.lives99);

    if (loaded.mods.includeInProfile) {
        auto& modProfile = loaded.mods;

        ReadBool(values, "Mods", "SystemEnabled", modProfile.systemEnabled);

        int modCount = 0;
        ReadInt(values, "Mods", "Count", modCount);
        modCount = std::clamp(modCount, 0, 256);

        modProfile.mods.clear();
        modProfile.mods.reserve(static_cast<size_t>(modCount));

        for (int i = 0; i < modCount; ++i) {
            ThreeChanModState state;

            const std::string folderKey = "Folder" + std::to_string(i);
            const std::string enabledKey = "Enabled" + std::to_string(i);

            ReadString(values, "Mods", folderKey.c_str(), state.folder);
            ReadBool(values, "Mods", enabledKey.c_str(), state.enabled);

            if (!state.folder.empty()) {
                modProfile.mods.push_back(std::move(state));
            }
        }
    }

    ReadBool(values, "General", "MasterEnabled", loaded.masterEnabled);
    ReadBool(values, "General", "ApplyChangesLive", loaded.applyChangesLive);

    auto& e = loaded.enemies;
    ReadBool(values, "Enemies", "Enabled", e.enabled);
    ReadFloat(values, "Enemies", "HealthMultiplier", e.healthMultiplier);
    ReadFloat(values, "Enemies", "DamageMultiplier", e.damageMultiplier);
    ReadFloat(values, "Enemies", "AttackFrequencyMultiplier", e.attackFrequencyMultiplier);
    ReadFloat(values, "Enemies", "AggressionMultiplier", e.aggressionMultiplier);
    ReadFloat(values, "Enemies", "DistancingMultiplier", e.distancingMultiplier);
    ReadFloat(values, "Enemies", "CirclingMultiplier", e.circlingMultiplier);
    ReadFloat(values, "Enemies", "DecisionSpeedMultiplier", e.decisionSpeedMultiplier);
    ReadBool(values, "Enemies", "UseCustomThinkRange", e.useCustomThinkRange);
    ReadInt(values, "Enemies", "MinThinkFrames", e.minThinkFrames);
    ReadInt(values, "Enemies", "MaxThinkFrames", e.maxThinkFrames);
    ReadFloat(values, "Enemies", "RunningSpeedMultiplier", e.runningSpeedMultiplier);
    ReadFloat(values, "Enemies", "StrafingSpeedMultiplier", e.strafingSpeedMultiplier);
    ReadFloat(values, "Enemies", "TurnSpeedMultiplier", e.turnSpeedMultiplier);
    ReadFloat(values, "Enemies", "AttackAnimationSpeedMultiplier", e.attackAnimationSpeedMultiplier);
    ReadFloat(values, "Enemies", "PunchChanceMultiplier", e.punchChanceMultiplier);
    ReadFloat(values, "Enemies", "KickChanceMultiplier", e.kickChanceMultiplier);
    ReadFloat(values, "Enemies", "ThrowChanceMultiplier", e.throwChanceMultiplier);
    ReadFloat(values, "Enemies", "ComboChanceMultiplier", e.comboChanceMultiplier);
    ReadFloat(values, "Enemies", "StunDurationMultiplier", e.stunDurationMultiplier);
    ReadFloat(values, "Enemies", "KnockdownRecoveryMultiplier", e.knockdownRecoveryMultiplier);
    ReadFloat(values, "Enemies", "GetUpSpeedMultiplier", e.getUpSpeedMultiplier);

    auto& g = loaded.groupCombat;
    ReadBool(values, "GroupCombat", "Enabled", g.enabled);
    ReadInt(values, "GroupCombat", "MaxEngagedEnemies", g.maxEngagedEnemies);
    ReadInt(values, "GroupCombat", "MaxSimultaneousAttacks", g.maxSimultaneousAttacks);
    ReadInt(values, "GroupCombat", "MinimumAttackGapMs", g.minimumAttackGapMs);
    ReadBool(values, "GroupCombat", "AttackerRotation", g.attackerRotation);
    ReadBool(values, "GroupCombat", "ChainPressure", g.chainPressure);
    ReadBool(values, "GroupCombat", "ImmediateReplacement", g.immediateReplacement);

    auto& sp = loaded.spawning;
    ReadBool(values, "Spawning", "Enabled", sp.enabled);
    ReadFloat(values, "Spawning", "EnemyCountMultiplier", sp.enemyCountMultiplier);
    ReadFloat(values, "Spawning", "WaveDelaySeconds", sp.waveDelaySeconds);
    ReadInt(values, "Spawning", "SpawnBurstOverride", sp.spawnBurstOverride);
    ReadInt(values, "Spawning", "MaxAliveOverride", sp.maxAliveOverride);
    ReadInt(values, "Spawning", "ActiveZoneThresholdOffset", sp.activeZoneThresholdOffset);
    ReadBool(values, "Spawning", "PauseGenerators", sp.pauseGenerators);

    auto& bg = loaded.bosses.global;
    ReadBool(values, "Bosses", "Enabled", bg.enabled);
    ReadFloat(values, "Bosses", "HealthMultiplier", bg.healthMultiplier);
    ReadFloat(values, "Bosses", "DamageMultiplier", bg.damageMultiplier);
    ReadFloat(values, "Bosses", "MovementSpeedMultiplier", bg.movementSpeedMultiplier);
    ReadFloat(values, "Bosses", "DecisionSpeedMultiplier", bg.decisionSpeedMultiplier);
    ReadFloat(values, "Bosses", "AttackSpeedMultiplier", bg.attackSpeedMultiplier);
    ReadFloat(values, "Bosses", "RecoverySpeedMultiplier", bg.recoverySpeedMultiplier);

    ReadFloat(values, "Butch", "StompDamageMultiplier", loaded.bosses.butch.stompDamageMultiplier);
    ReadFloat(values, "Butch", "AttackDistanceMultiplier", loaded.bosses.butch.attackDistanceMultiplier);
    ReadFloat(values, "Butch", "StompFrequencyMultiplier", loaded.bosses.butch.stompFrequencyMultiplier);
    ReadFloat(values, "Butch", "ChargePressureMultiplier", loaded.bosses.butch.chargePressureMultiplier);
    ReadFloat(values, "Butch", "PotFrequencyMultiplier", loaded.bosses.butch.potFrequencyMultiplier);

    ReadFloat(values, "Grontar", "CloseAttackRangeMultiplier", loaded.bosses.grontar.closeAttackRangeMultiplier);
    ReadFloat(values, "Grontar", "FarAttackRangeMultiplier", loaded.bosses.grontar.farAttackRangeMultiplier);
    ReadFloat(values, "Grontar", "ThrowRangeMultiplier", loaded.bosses.grontar.throwRangeMultiplier);
    ReadFloat(values, "Grontar", "TargetTrackingMultiplier", loaded.bosses.grontar.targetTrackingMultiplier);
    ReadFloat(values, "Grontar", "DiveRollPressureMultiplier", loaded.bosses.grontar.diveRollPressureMultiplier);

    ReadInt(values, "Dante", "Phase2HealthPercent", loaded.bosses.dante.phase2HealthPercent);
    ReadInt(values, "Dante", "Phase3HealthPercent", loaded.bosses.dante.phase3HealthPercent);
    ReadFloat(values, "Dante", "MissileDamageMultiplier", loaded.bosses.dante.missileDamageMultiplier);
    ReadFloat(values, "Dante", "MissileRadiusMultiplier", loaded.bosses.dante.missileRadiusMultiplier);
    ReadFloat(values, "Dante", "MissileFrequencyMultiplier", loaded.bosses.dante.missileFrequencyMultiplier);
    ReadFloat(values, "Dante", "MissileRecoveryMultiplier", loaded.bosses.dante.missileRecoveryMultiplier);
    ReadFloat(values, "Dante", "TargetedMissileTimingMultiplier", loaded.bosses.dante.targetedMissileTimingMultiplier);

    ReadFloat(values, "Paul", "CloseAttackRangeMultiplier", loaded.bosses.paul.closeAttackRangeMultiplier);
    ReadFloat(values, "Paul", "DanceTimeMultiplier", loaded.bosses.paul.danceTimeMultiplier);
    ReadFloat(values, "Paul", "RecoveryTimeMultiplier", loaded.bosses.paul.recoveryTimeMultiplier);
    ReadFloat(values, "Paul", "AggressionMultiplier", loaded.bosses.paul.aggressionMultiplier);

    ReadFloat(values, "Oscar", "CloseRangeMultiplier", loaded.bosses.oscar.closeRangeMultiplier);
    ReadFloat(values, "Oscar", "MidRangeMultiplier", loaded.bosses.oscar.midRangeMultiplier);
    ReadFloat(values, "Oscar", "HenchmanCoordinationMultiplier", loaded.bosses.oscar.henchmanCoordinationMultiplier);
    ReadFloat(values, "Oscar", "HenchmanSyncDelayMultiplier", loaded.bosses.oscar.henchmanSyncDelayMultiplier);
    ReadFloat(values, "Oscar", "AggressionMultiplier", loaded.bosses.oscar.aggressionMultiplier);

    auto& c = loaded.camera;

    ReadBool(values, "Camera", "Enabled", c.enabled);

    int mode = static_cast<int>(c.mode);
    ReadInt(values, "Camera", "Mode", mode);
    c.mode = static_cast<ThreeChanCameraMode>(mode);

    ReadInt(values, "Camera", "FollowFov", c.followFov);
    ReadFloat(values, "Camera", "FollowMovementSmoothingMultiplier", c.followMovementSmoothingMultiplier);
    ReadFloat(values, "Camera", "FollowTrackingSmoothingMultiplier", c.followTrackingSmoothingMultiplier);

    ReadInt(values, "Camera", "RigidDistance", c.rigidDistance);
    ReadInt(values, "Camera", "RigidHeight", c.rigidHeight);
    ReadInt(values, "Camera", "RigidSpinRate", c.rigidSpinRate);
    ReadInt(values, "Camera", "RigidFov", c.rigidFov);
    ReadFloat(values, "Camera", "RigidMovementSmoothingMultiplier", c.rigidMovementSmoothingMultiplier);
    ReadFloat(values, "Camera", "RigidTrackingSmoothingMultiplier", c.rigidTrackingSmoothingMultiplier);

    ReadFloat(values, "Camera", "FixedSafeMarginXPercent", c.fixedSafeMarginXPercent);
    ReadFloat(values, "Camera", "FixedSafeMarginYPercent", c.fixedSafeMarginYPercent);
    ReadBool(values, "Camera", "FixedSmoothTransition", c.fixedSmoothTransition);
    ReadFloat(values, "Camera", "FixedTransitionTime", c.fixedTransitionTime);
    ReadFloat(values, "Camera", "FixedMinimumHoldTime", c.fixedMinimumHoldTime);
    ReadFloat(values, "Camera", "FixedRepositionCooldown", c.fixedRepositionCooldown);
    ReadInt(values, "Camera", "FixedTargetHeightOffset", c.fixedTargetHeightOffset);
    ReadInt(values, "Camera", "FixedFov", c.fixedFov);
    ReadBool(values, "Camera", "FixedUseOriginalRails", c.fixedUseOriginalRails);
    ReadBool(values, "Camera", "FixedTrackJackie", c.fixedTrackJackie);
    ReadBool(values, "Camera", "FixedShowSafeFrame", c.fixedShowSafeFrame);
    ReadBool(values, "Camera", "FixedShowCameraAnchor", c.fixedShowCameraAnchor);

    ReadInt(values, "Camera", "ThirdPersonDistance", c.thirdPersonDistance);
    ReadInt(values, "Camera", "ThirdPersonHeight", c.thirdPersonHeight);
    ReadInt(values, "Camera", "ThirdPersonTargetHeight", c.thirdPersonTargetHeight);
    ReadInt(values, "Camera", "ThirdPersonShoulderOffset", c.thirdPersonShoulderOffset);
    ReadInt(values, "Camera", "ThirdPersonFov", c.thirdPersonFov);
    ReadInt(values, "Camera", "ThirdPersonMinFov", c.thirdPersonMinFov);
    ReadInt(values, "Camera", "ThirdPersonMaxFov", c.thirdPersonMaxFov);
    ReadFloat(values, "Camera", "ThirdPersonPositionLag", c.thirdPersonPositionLag);
    ReadFloat(values, "Camera", "ThirdPersonRotationLag", c.thirdPersonRotationLag);
    ReadFloat(values, "Camera", "ThirdPersonTargetLag", c.thirdPersonTargetLag);
    ReadFloat(values, "Camera", "ThirdPersonAutoCenterStrength", c.thirdPersonAutoCenterStrength);
    ReadFloat(values, "Camera", "ThirdPersonAutoCenterDelay", c.thirdPersonAutoCenterDelay);
    ReadInt(values, "Camera", "ThirdPersonFollowDirection", c.thirdPersonFollowDirection);
    ReadBool(values, "Camera", "ThirdPersonCollision", c.thirdPersonCollision);
    ReadInt(values, "Camera", "ThirdPersonCollisionPadding", c.thirdPersonCollisionPadding);
    ReadInt(values, "Camera", "ThirdPersonMinimumDistance", c.thirdPersonMinimumDistance);
    ReadBool(values, "Camera", "ThirdPersonSmoothTurns", c.thirdPersonSmoothTurns);
    ReadBool(values, "Camera", "ThirdPersonSmoothJumps", c.thirdPersonSmoothJumps);
    ReadBool(values, "Camera", "ThirdPersonAutoRecoverBehindJackie", c.thirdPersonAutoRecoverBehindJackie);
    ReadBool(values, "Camera", "ThirdPersonShowDesiredCamera", c.thirdPersonShowDesiredCamera);
    ReadBool(values, "Camera", "ThirdPersonShowCollisionLine", c.thirdPersonShowCollisionLine);
    ReadBool(values, "Camera", "ThirdPersonShowTarget", c.thirdPersonShowTarget);

    ReadBool(values, "Inspector", "ShowSelectedEnemyRuntime", loaded.inspector.showSelectedEnemyRuntime);
    ReadBool(values, "Inspector", "ShowBehaviourRuntime", loaded.inspector.showBehaviourRuntime);
    ReadBool(values, "Inspector", "ShowCombatSlotRuntime", loaded.inspector.showCombatSlotRuntime);
    ReadBool(values, "Inspector", "ShowDamageRuntime", loaded.inspector.showDamageRuntime);

    ReadBool(values, "Telemetry", "Enabled", loaded.telemetry.enabled);
    ReadBool(values, "Telemetry", "ShowEnemyCounts", loaded.telemetry.showEnemyCounts);
    ReadBool(values, "Telemetry", "ShowCombatSlots", loaded.telemetry.showCombatSlots);
    ReadBool(values, "Telemetry", "ShowAttackRate", loaded.telemetry.showAttackRate);
    ReadBool(values, "Telemetry", "ShowDamageRate", loaded.telemetry.showDamageRate);
    ReadBool(values, "Telemetry", "ShowSelectedAI", loaded.telemetry.showSelectedAI);
    ReadBool(values, "Telemetry", "ShowBossRuntime", loaded.telemetry.showBossRuntime);

    ReadBool(values, "Advanced", "SafeMode", loaded.advanced.safeMode);
    ReadInt(values, "Advanced", "IntendedFightingCollisionCapacity", loaded.advanced.intendedFightingCollisionCapacity);
    ReadInt(values, "Advanced", "HardEnemySafetyCap", loaded.advanced.hardEnemySafetyCap);
    ReadInt(values, "Advanced", "HardSimultaneousAttackSafetyCap", loaded.advanced.hardSimultaneousAttackSafetyCap);
    ReadBool(values, "Advanced", "LogRuntimeChanges", loaded.advanced.logRuntimeChanges);

    ClampSettings(loaded);

    if (loaded.mods.includeInProfile &&
        !ApplyModState(loaded.mods)) {
        g_lastMessage = "Could not apply mod configuration.";
        return false;
    }

    if (loaded.cheats.includeInProfile) {
        ApplyCheatState(loaded.cheats);
    }

    g_settings = loaded;
    g_activeProfile = name;
    g_lastMessage = "Profile loaded: " + name;

    return true;
}

std::vector<std::string> ListProfiles() {
    Initialize();

    std::vector<std::string> result;
    result.emplace_back("Original");

    std::error_code ec;

    if (!std::filesystem::exists(PROFILE_DIR, ec)) {
        return result;
    }

    for (const auto& entry :
         std::filesystem::directory_iterator(PROFILE_DIR, ec)) {

        if (ec) {
            break;
        }

        if (!entry.is_regular_file()) {
            continue;
        }

        const auto path = entry.path();

        if (path.extension() != ".ini") {
            continue;
        }

        const std::string name = path.stem().string();

        if (!name.empty()
            && name != "Original"
            && name != "original") {
            result.emplace_back(name);
        }
    }

    if (result.size() > 1) {
        std::sort(result.begin() + 1, result.end());
    }

    return result;
}

const std::string& GetActiveProfileName() {
    Initialize();
    return g_activeProfile;
}

const char* GetLastMessage() {
    Initialize();
    return g_lastMessage.c_str();
}

} // namespace ThreeChanTuning
