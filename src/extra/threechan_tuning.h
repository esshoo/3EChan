#pragma once

#include <string>
#include <vector>

enum class ThreeChanCameraMode : int {
    FollowOriginal = 0,
    RigidOriginal = 1,
    FixedFrameExperimental = 2,
    CloseThirdPersonExperimental = 3,
};

struct ThreeChanEnemyTuning {
    bool enabled = false;

    float healthMultiplier = 1.0f;
    float damageMultiplier = 1.0f;

    float attackFrequencyMultiplier = 1.0f;
    float aggressionMultiplier = 1.0f;
    float distancingMultiplier = 1.0f;
    float circlingMultiplier = 1.0f;

    float decisionSpeedMultiplier = 1.0f;

    bool useCustomThinkRange = false;
    int minThinkFrames = 0;
    int maxThinkFrames = 5;

    float runningSpeedMultiplier = 1.0f;
    float strafingSpeedMultiplier = 1.0f;
    float turnSpeedMultiplier = 1.0f;
    float attackAnimationSpeedMultiplier = 1.0f;

    float punchChanceMultiplier = 1.0f;
    float kickChanceMultiplier = 1.0f;
    float throwChanceMultiplier = 1.0f;
    float comboChanceMultiplier = 1.0f;

    float stunDurationMultiplier = 1.0f;
    float knockdownRecoveryMultiplier = 1.0f;
    float getUpSpeedMultiplier = 1.0f;
};

struct ThreeChanGroupCombatTuning {
    bool enabled = false;

    // 0 = preserve original ActiveZone/overlord behaviour.
    int maxEngagedEnemies = 0;
    int maxSimultaneousAttacks = 0;

    int minimumAttackGapMs = 0;

    bool attackerRotation = true;
    bool chainPressure = false;
    bool immediateReplacement = true;
};

struct ThreeChanSpawningTuning {
    bool enabled = false;

    float enemyCountMultiplier = 1.0f;
    float respawnDelayMultiplier = 1.0f;

    // 0 = original level value.
    int spawnBurstOverride = 0;
    int maxAliveOverride = 0;

    int activeZoneThresholdOffset = 0;

    bool pauseGenerators = false;
};

struct ThreeChanBossGlobalTuning {
    bool enabled = false;

    float healthMultiplier = 1.0f;
    float damageMultiplier = 1.0f;
    float movementSpeedMultiplier = 1.0f;
    float decisionSpeedMultiplier = 1.0f;
    float attackSpeedMultiplier = 1.0f;
    float recoverySpeedMultiplier = 1.0f;
};

struct ThreeChanButchTuning {
    float stompDamageMultiplier = 1.0f;
    float attackDistanceMultiplier = 1.0f;
    float stompFrequencyMultiplier = 1.0f;
    float chargePressureMultiplier = 1.0f;
    float potFrequencyMultiplier = 1.0f;
};

struct ThreeChanGrontarTuning {
    float closeAttackRangeMultiplier = 1.0f;
    float farAttackRangeMultiplier = 1.0f;
    float throwRangeMultiplier = 1.0f;
    float targetTrackingMultiplier = 1.0f;
    float diveRollPressureMultiplier = 1.0f;
};

struct ThreeChanDanteTuning {
    // -1 = original threshold.
    int phase2HealthPercent = -1;
    int phase3HealthPercent = -1;

    float missileDamageMultiplier = 1.0f;
    float missileRadiusMultiplier = 1.0f;
    float missileFrequencyMultiplier = 1.0f;
    float missileRecoveryMultiplier = 1.0f;
    float targetedMissileTimingMultiplier = 1.0f;
};

struct ThreeChanPaulTuning {
    float closeAttackRangeMultiplier = 1.0f;
    float danceTimeMultiplier = 1.0f;
    float recoveryTimeMultiplier = 1.0f;
    float aggressionMultiplier = 1.0f;
};

struct ThreeChanOscarTuning {
    float closeRangeMultiplier = 1.0f;
    float midRangeMultiplier = 1.0f;
    float henchmanCoordinationMultiplier = 1.0f;
    float henchmanSyncDelayMultiplier = 1.0f;
    float aggressionMultiplier = 1.0f;
};

struct ThreeChanBossTuning {
    ThreeChanBossGlobalTuning global;
    ThreeChanButchTuning butch;
    ThreeChanGrontarTuning grontar;
    ThreeChanDanteTuning dante;
    ThreeChanPaulTuning paul;
    ThreeChanOscarTuning oscar;
};

struct ThreeChanCameraTuning {
    bool enabled = false;

    ThreeChanCameraMode mode = ThreeChanCameraMode::FollowOriginal;

    // Original Follow tuning
    int followFov = 10;
    float followMovementSmoothingMultiplier = 1.0f;
    float followTrackingSmoothingMultiplier = 1.0f;

    // Original Rigid tuning
    int rigidDistance = 3200;
    int rigidHeight = 2000;
    int rigidSpinRate = 6500;
    int rigidFov = 10;
    float rigidMovementSmoothingMultiplier = 1.0f;
    float rigidTrackingSmoothingMultiplier = 1.0f;

    // Fixed Frame experimental camera
    float fixedSafeMarginXPercent = 20.0f;
    float fixedSafeMarginYPercent = 15.0f;

    bool fixedSmoothTransition = true;
    float fixedTransitionTime = 0.35f;

    float fixedMinimumHoldTime = 0.25f;
    float fixedRepositionCooldown = 0.20f;

    int fixedTargetHeightOffset = 0;
    int fixedFov = 10;

    bool fixedUseOriginalRails = true;
    bool fixedTrackJackie = false;

    bool fixedShowSafeFrame = false;
    bool fixedShowCameraAnchor = false;

    // Close Third Person experimental camera
    int thirdPersonDistance = 1100;
    int thirdPersonHeight = 700;
    int thirdPersonTargetHeight = 300;
    int thirdPersonShoulderOffset = 0;

    int thirdPersonFov = 10;
    int thirdPersonMinFov = 7;
    int thirdPersonMaxFov = 14;

    float thirdPersonPositionLag = 10.0f;
    float thirdPersonRotationLag = 10.0f;
    float thirdPersonTargetLag = 12.0f;

    float thirdPersonAutoCenterStrength = 0.80f;
    float thirdPersonAutoCenterDelay = 0.50f;

    // 0 = facing, 1 = movement, 2 = blended.
    int thirdPersonFollowDirection = 0;

    bool thirdPersonCollision = true;
    int thirdPersonCollisionPadding = 80;
    int thirdPersonMinimumDistance = 350;

    bool thirdPersonSmoothTurns = true;
    bool thirdPersonSmoothJumps = true;
    bool thirdPersonAutoRecoverBehindJackie = true;

    bool thirdPersonShowDesiredCamera = false;
    bool thirdPersonShowCollisionLine = false;
    bool thirdPersonShowTarget = false;
};

struct ThreeChanInspectorTuning {
    bool showSelectedEnemyRuntime = true;
    bool showBehaviourRuntime = true;
    bool showCombatSlotRuntime = true;
    bool showDamageRuntime = true;
};

struct ThreeChanTelemetryTuning {
    bool enabled = true;

    bool showEnemyCounts = true;
    bool showCombatSlots = true;
    bool showAttackRate = true;
    bool showDamageRate = true;
    bool showSelectedAI = true;
    bool showBossRuntime = true;
};

struct ThreeChanAdvancedTuning {
    bool safeMode = true;

    // Compile-time backing capacity will be raised once during integration.
    int intendedFightingCollisionCapacity = 32;

    int hardEnemySafetyCap = 32;
    int hardSimultaneousAttackSafetyCap = 8;

    bool logRuntimeChanges = false;
};

struct ThreeChanSettings {
    bool masterEnabled = false;
    bool applyChangesLive = true;

    ThreeChanEnemyTuning enemies;
    ThreeChanGroupCombatTuning groupCombat;
    ThreeChanSpawningTuning spawning;
    ThreeChanBossTuning bosses;
    ThreeChanCameraTuning camera;
    ThreeChanInspectorTuning inspector;
    ThreeChanTelemetryTuning telemetry;
    ThreeChanAdvancedTuning advanced;
};

namespace ThreeChanTuning {

void Initialize();

ThreeChanSettings& Get();
const ThreeChanSettings& GetConst();

void ResetToOriginal();

bool SaveProfile(const char* profileName);
bool LoadProfile(const char* profileName);

std::vector<std::string> ListProfiles();

const std::string& GetActiveProfileName();
const char* GetLastMessage();

}