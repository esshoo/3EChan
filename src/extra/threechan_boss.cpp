#include "extra/threechan_boss.h"

#include "extra/threechan_tuning.h"

#include <algorithm>
#include <cmath>

namespace {

const ThreeChanSettings& Settings() {
    return ThreeChanTuning::GetConst();
}

bool Enabled() {
    const ThreeChanSettings& settings = Settings();

    return settings.masterEnabled
        && settings.bosses.global.enabled;
}

float SafeMultiplier(float value) {
    return std::clamp(
        value,
        0.05f,
        10.0f);
}

s32 ScaleInteger(
    s32 original,
    float multiplier,
    s32 minimum,
    s32 maximum) {

    const s64 value =
        static_cast<s64>(
            std::llround(
                static_cast<double>(original)
                * static_cast<double>(
                    SafeMultiplier(multiplier))));

    return static_cast<s32>(
        std::clamp<s64>(
            value,
            static_cast<s64>(minimum),
            static_cast<s64>(maximum)));
}

s32 ScaleRateFrames(
    s32 original,
    float rateMultiplier) {

    if (original <= 0) {
        return original;
    }

    const double rate =
        static_cast<double>(
            SafeMultiplier(rateMultiplier));

    const s32 value =
        static_cast<s32>(
            std::llround(
                static_cast<double>(original)
                / rate));

    return std::clamp(
        value,
        1,
        100000);
}

s32 ScaleTimeFrames(
    s32 original,
    float timeMultiplier,
    float speedMultiplier = 1.0f) {

    if (original <= 0) {
        return original;
    }

    const double time =
        static_cast<double>(
            SafeMultiplier(timeMultiplier));

    const double speed =
        static_cast<double>(
            SafeMultiplier(speedMultiplier));

    const s32 value =
        static_cast<s32>(
            std::llround(
                static_cast<double>(original)
                * time
                / speed));

    return std::clamp(
        value,
        1,
        100000);
}

s32 ScaleChance(
    s32 originalChance,
    float multiplier) {

    return ScaleInteger(
        originalChance,
        multiplier,
        0,
        100);
}

u16 ScaleDamage(
    u16 originalDamage,
    float specificMultiplier) {

    if (!Enabled()) {
        return originalDamage;
    }

    const double multiplier =
        static_cast<double>(
            SafeMultiplier(
                Settings().bosses.global.damageMultiplier))
        * static_cast<double>(
            SafeMultiplier(
                specificMultiplier));

    const s64 value =
        static_cast<s64>(
            std::llround(
                static_cast<double>(originalDamage)
                * multiplier));

    return static_cast<u16>(
        std::clamp<s64>(
            value,
            0,
            65535));
}

}

namespace ThreeChanBoss {

bool IsEnabled() {
    return Enabled();
}

s32 ResolveDecisionFrames(
    s32 originalFrames) {

    if (!Enabled()) {
        return originalFrames;
    }

    return ScaleRateFrames(
        originalFrames,
        Settings()
            .bosses
            .global
            .decisionSpeedMultiplier);
}

s32 ResolveRecoveryFrames(
    s32 originalFrames) {

    if (!Enabled()) {
        return originalFrames;
    }

    return ScaleTimeFrames(
        originalFrames,
        1.0f,
        Settings()
            .bosses
            .global
            .recoverySpeedMultiplier);
}

// ------------------------------------------------------------
// Butch
// ------------------------------------------------------------

s32 ResolveButchAttackDistance(
    s32 originalDistance) {

    if (!Enabled()) {
        return originalDistance;
    }

    return ScaleInteger(
        originalDistance,
        Settings()
            .bosses
            .butch
            .attackDistanceMultiplier,
        1,
        32767);
}

bool ShouldButchStompRoll(
    s32 roll) {

    if (!Enabled()) {
        return roll < 30;
    }

    const s32 stompChance =
        ScaleChance(
            30,
            Settings()
                .bosses
                .butch
                .stompFrequencyMultiplier);

    return roll < stompChance;
}

bool ShouldButchChargeRoll(
    s32 roll) {

    if (!Enabled()) {
        return roll >= 30
            && roll < 60;
    }

    const s32 stompChance =
        ScaleChance(
            30,
            Settings()
                .bosses
                .butch
                .stompFrequencyMultiplier);

    const s32 chargeChance =
        ScaleChance(
            30,
            Settings()
                .bosses
                .butch
                .chargePressureMultiplier);

    const s32 upper =
        std::min(
            100,
            stompChance + chargeChance);

    return roll >= stompChance
        && roll < upper;
}

u16 ResolveButchStompDamage(
    u16 originalDamage) {

    return ScaleDamage(
        originalDamage,
        Settings()
            .bosses
            .butch
            .stompDamageMultiplier);
}

// ------------------------------------------------------------
// Grontar
// ------------------------------------------------------------

s32 ResolveGrontarCloseRange(
    s32 originalDistance) {

    if (!Enabled()) {
        return originalDistance;
    }

    return ScaleInteger(
        originalDistance,
        Settings()
            .bosses
            .grontar
            .closeAttackRangeMultiplier,
        1,
        32767);
}

s32 ResolveGrontarFarRange(
    s32 originalDistance) {

    if (!Enabled()) {
        return originalDistance;
    }

    return ScaleInteger(
        originalDistance,
        Settings()
            .bosses
            .grontar
            .farAttackRangeMultiplier,
        1,
        32767);
}

u32 ResolveGrontarThrowRange(
    u32 originalDistance) {

    if (!Enabled()) {
        return originalDistance;
    }

    const s32 scaled =
        ScaleInteger(
            static_cast<s32>(originalDistance),
            Settings()
                .bosses
                .grontar
                .throwRangeMultiplier,
            1,
            32767);

    return static_cast<u32>(scaled);
}

s32 ResolveGrontarTrackingFrames(
    s32 originalFrames) {

    if (!Enabled()
        || originalFrames <= 0) {
        return originalFrames;
    }

    return ScaleInteger(
        originalFrames,
        Settings()
            .bosses
            .grontar
            .targetTrackingMultiplier,
        0,
        120);
}

s32 ResolveGrontarDiveRollRange(
    s32 originalDistance) {

    if (!Enabled()) {
        return originalDistance;
    }

    return ScaleInteger(
        originalDistance,
        Settings()
            .bosses
            .grontar
            .diveRollPressureMultiplier,
        1,
        32767);
}

s32 ResolveGrontarDiveRollAngle(
    s32 originalAngle) {

    if (!Enabled()) {
        return originalAngle;
    }

    return ScaleInteger(
        originalAngle,
        Settings()
            .bosses
            .grontar
            .diveRollPressureMultiplier,
        1,
        32767);
}

// ------------------------------------------------------------
// Dante
// ------------------------------------------------------------

s32 ResolveDantePhaseThreshold(
    s32 originalFixedScale,
    s32 phaseNumber) {

    if (!Enabled()) {
        return originalFixedScale;
    }

    const ThreeChanDanteTuning& dante =
        Settings().bosses.dante;

    s32 percent = -1;

    if (phaseNumber == 2) {
        percent = dante.phase2HealthPercent;
    }
    else if (phaseNumber == 3) {
        percent = dante.phase3HealthPercent;
    }

    if (percent < 0) {
        return originalFixedScale;
    }

    percent = std::clamp(
        percent,
        0,
        100);

    return static_cast<s32>(
        (
            static_cast<s64>(percent)
            * 0x10000LL
            + 50LL
        )
        / 100LL);
}

s32 ResolveDanteMissileRadius(
    s32 originalRadius) {

    if (!Enabled()) {
        return originalRadius;
    }

    return ScaleInteger(
        originalRadius,
        Settings()
            .bosses
            .dante
            .missileRadiusMultiplier,
        1,
        32767);
}

u16 ResolveDanteMissileDamage(
    u16 originalDamage) {

    return ScaleDamage(
        originalDamage,
        Settings()
            .bosses
            .dante
            .missileDamageMultiplier);
}

s32 ResolveDanteVolleyFrameThreshold(
    s32 originalFrames) {

    if (!Enabled()) {
        return originalFrames;
    }

    return ScaleRateFrames(
        originalFrames,
        Settings()
            .bosses
            .dante
            .missileFrequencyMultiplier);
}

s32 ResolveDanteTargetMissileFrameThreshold(
    s32 originalFrames) {

    if (!Enabled()) {
        return originalFrames;
    }

    return ScaleTimeFrames(
        originalFrames,
        Settings()
            .bosses
            .dante
            .targetedMissileTimingMultiplier);
}

s32 ResolveDanteMissileRecoveryFrames(
    s32 originalFrames) {

    if (!Enabled()) {
        return originalFrames;
    }

    return ScaleTimeFrames(
        originalFrames,
        Settings()
            .bosses
            .dante
            .missileRecoveryMultiplier,
        Settings()
            .bosses
            .global
            .recoverySpeedMultiplier);
}

// ------------------------------------------------------------
// Paul
// ------------------------------------------------------------

s32 ResolvePaulCloseRange(
    s32 originalDistance) {

    if (!Enabled()) {
        return originalDistance;
    }

    return ScaleInteger(
        originalDistance,
        Settings()
            .bosses
            .paul
            .closeAttackRangeMultiplier,
        1,
        32767);
}

s32 ResolvePaulDanceFrames(
    s32 originalFrames) {

    if (!Enabled()) {
        return originalFrames;
    }

    return ScaleTimeFrames(
        originalFrames,
        Settings()
            .bosses
            .paul
            .danceTimeMultiplier);
}

s32 ResolvePaulRecoveryFrames(
    s32 originalFrames) {

    if (!Enabled()) {
        return originalFrames;
    }

    return ScaleTimeFrames(
        originalFrames,
        Settings()
            .bosses
            .paul
            .recoveryTimeMultiplier,
        Settings()
            .bosses
            .global
            .recoverySpeedMultiplier);
}

s32 ResolvePaulBackoffChance(
    s32 originalChance) {

    if (!Enabled()) {
        return originalChance;
    }

    const float aggression =
        SafeMultiplier(
            Settings()
                .bosses
                .paul
                .aggressionMultiplier);

    const s32 result =
        static_cast<s32>(
            std::llround(
                static_cast<double>(
                    originalChance)
                / static_cast<double>(
                    aggression)));

    return std::clamp(
        result,
        0,
        100);
}

s32 ResolvePaulAttackChance(
    s32 originalChance) {

    if (!Enabled()) {
        return originalChance;
    }

    return ScaleChance(
        originalChance,
        Settings()
            .bosses
            .paul
            .aggressionMultiplier);
}

// ------------------------------------------------------------
// Oscar
// ------------------------------------------------------------

s32 ResolveOscarCloseRange(
    s32 originalDistance) {

    if (!Enabled()) {
        return originalDistance;
    }

    return ScaleInteger(
        originalDistance,
        Settings()
            .bosses
            .oscar
            .closeRangeMultiplier,
        1,
        32767);
}

s32 ResolveOscarMidRange(
    s32 originalDistance) {

    if (!Enabled()) {
        return originalDistance;
    }

    return ScaleInteger(
        originalDistance,
        Settings()
            .bosses
            .oscar
            .midRangeMultiplier,
        1,
        32767);
}

s32 ResolveOscarCoordinationAngle(
    s32 originalAngle) {

    if (!Enabled()) {
        return originalAngle;
    }

    return ScaleInteger(
        originalAngle,
        Settings()
            .bosses
            .oscar
            .henchmanCoordinationMultiplier,
        1,
        32767);
}

s32 ResolveOscarHenchmanWindow(
    s32 originalRange) {

    if (!Enabled()) {
        return originalRange;
    }

    return ScaleInteger(
        originalRange,
        Settings()
            .bosses
            .oscar
            .henchmanCoordinationMultiplier,
        1,
        65535);
}

s32 ResolveOscarSyncDelay(
    s32 originalFrames) {

    if (!Enabled()) {
        return originalFrames;
    }

    return ScaleTimeFrames(
        originalFrames,
        Settings()
            .bosses
            .oscar
            .henchmanSyncDelayMultiplier,
        Settings()
            .bosses
            .global
            .decisionSpeedMultiplier);
}

}