#include "extra/threechan_combat.h"

#include "extra/threechan_tuning.h"
#include "extra/threechan_group_combat.h"
#include "extra/threechan_spawning.h"

#include "ai/humanoid.h"
#include "ai/player.h"
#include "ai/thing.h"

#include "gen/animstruct.h"
#include "gen/model.h"

#include "p3d/p3dmath.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace {

struct HumanoidRuntimeState {
    bool healthCaptured = false;
    u16 baseMaxHealth = 0;
    u16 lastAppliedMaxHealth = 0;

    bool moveTracked = false;
    s32 baseMoveSpeed = 0;
    s32 lastAppliedMoveSpeed = 0;

    bool turnTracked = false;
    u16 baseTurnRate = 0;
    u16 lastAppliedTurnRate = 0;

    AnimStructure* animation = nullptr;
    bool animationTracked = false;
    s32 baseAnimationSpeed = FIX16_ONE;
    s32 lastAppliedAnimationSpeed = FIX16_ONE;
};

std::unordered_map<Humanoid*, HumanoidRuntimeState> g_runtime;

bool MasterEnabled() {
    return ThreeChanTuning::GetConst().masterEnabled;
}

bool IsSuppressedByNis(const Humanoid& humanoid) {
    return humanoid.actionState == static_cast<s32>(AS_NIS_MODE)
        || (humanoid.flags & TF_DIRECTOR_ACTIVE) != 0
        || (humanoid.flags2 & TF2_NIS_MASK) != 0;
}

float RegularEnemyHealthMultiplier() {
    const ThreeChanSettings& settings =
        ThreeChanTuning::GetConst();

    if (!settings.masterEnabled
        || !settings.enemies.enabled) {
        return 1.0f;
    }

    return settings.enemies.healthMultiplier;
}

float BossHealthMultiplier() {
    const ThreeChanSettings& settings =
        ThreeChanTuning::GetConst();

    if (!settings.masterEnabled
        || !settings.bosses.global.enabled) {
        return 1.0f;
    }

    return settings.bosses.global.healthMultiplier;
}

float HealthMultiplierFor(const Humanoid& humanoid) {
    if (ThreeChanCombat::IsBossType(humanoid.thingType)) {
        return BossHealthMultiplier();
    }

    if (ThreeChanCombat::IsRegularEnemy(&humanoid)) {
        return RegularEnemyHealthMultiplier();
    }

    return 1.0f;
}

float MovementMultiplierFor(const Humanoid& humanoid) {
    const ThreeChanSettings& settings =
        ThreeChanTuning::GetConst();

    if (!settings.masterEnabled) {
        return 1.0f;
    }

    if (ThreeChanCombat::IsBossType(humanoid.thingType)) {
        if (!settings.bosses.global.enabled) {
            return 1.0f;
        }

        return settings.bosses.global.movementSpeedMultiplier;
    }

    if (ThreeChanCombat::IsRegularEnemy(&humanoid)
        && settings.enemies.enabled) {
        return settings.enemies.runningSpeedMultiplier;
    }

    return 1.0f;
}

float TurnMultiplierFor(const Humanoid& humanoid) {
    const ThreeChanSettings& settings =
        ThreeChanTuning::GetConst();

    if (!settings.masterEnabled
        || !settings.enemies.enabled
        || !ThreeChanCombat::IsRegularEnemy(&humanoid)) {
        return 1.0f;
    }

    return settings.enemies.turnSpeedMultiplier;
}

float AttackAnimationMultiplierFor(
    const Humanoid& humanoid) {

    const ThreeChanSettings& settings =
        ThreeChanTuning::GetConst();

    if (!settings.masterEnabled) {
        return 1.0f;
    }

    if (ThreeChanCombat::IsBossType(humanoid.thingType)) {
        if (!settings.bosses.global.enabled) {
            return 1.0f;
        }

        return settings.bosses.global.attackSpeedMultiplier;
    }

    if (ThreeChanCombat::IsRegularEnemy(&humanoid)
        && settings.enemies.enabled) {
        return settings.enemies.attackAnimationSpeedMultiplier;
    }

    return 1.0f;
}

bool IsAttackAnimationState(s32 state) {
    switch (state) {
        case AS_PUNCH_ATTACK:
        case AS_KICK_ATTACK:
        case AS_BACK_GRAB_LATCH:
        case AS_BACK_GRAB:
        case AS_BACK_GRAB_RELEASE:
        case AS_COUNTER_ATTACK_PRE_LATCH:
        case AS_COUNTER_ATTACK_LATCH:
        case AS_COUNTER_ATTACK:
        case AS_COUNTER_ATTACK_RECOVERY:
        case AS_THROW_PICKUP:
            return true;

        default:
            return false;
    }
}

s32 ScalePercentValue(
    const Humanoid* owner,
    s32 originalValue,
    float multiplier) {

    if (!owner
        || !ThreeChanCombat::IsRegularEnemy(owner)
        || !MasterEnabled()
        || !ThreeChanTuning::GetConst().enemies.enabled) {
        return originalValue;
    }

    const s32 result =
        static_cast<s32>(
            std::lround(
                static_cast<double>(originalValue)
                * static_cast<double>(multiplier)));

    return std::clamp(result, 0, 100);
}

u16 BuildScaledMaxHealth(
    u16 original,
    float multiplier) {

    const s32 scaled =
        static_cast<s32>(
            std::lround(
                static_cast<double>(original)
                * static_cast<double>(multiplier)));

    return static_cast<u16>(
        std::clamp(
            scaled,
            1,
            65535));
}

u16 PreserveHealthRatio(
    u16 health,
    u16 oldMax,
    u16 newMax) {

    if (health == 0) {
        return 0;
    }

    if (oldMax == 0) {
        return newMax;
    }

    const u32 scaled =
        static_cast<u32>(
            (
                static_cast<u64>(health)
                * static_cast<u64>(newMax)
                + static_cast<u64>(oldMax / 2)
            )
            / static_cast<u64>(oldMax));

    return static_cast<u16>(
        std::clamp<u32>(
            scaled,
            1u,
            static_cast<u32>(newMax)));
}

void RestoreAnimationIfNeeded(
    HumanoidRuntimeState& state,
    AnimStructure* animation) {

    if (!state.animationTracked
        || !animation) {
        state.animationTracked = false;
        return;
    }

    if (animation == state.animation
        && animation->speed
            == state.lastAppliedAnimationSpeed) {
        animation->speed =
            state.baseAnimationSpeed;
    }

    state.animationTracked = false;
}

} // namespace

namespace ThreeChanCombat {

bool IsBossType(u16 type) {
    switch (type) {
        case AITypes::TT_GRONTAR:
        case AITypes::TT_PAUL:
        case AITypes::TT_OSCAR:
        case AITypes::TT_DANTE:
        case AITypes::TT_BUTCH:
            return true;

        default:
            return false;
    }
}

bool IsRegularEnemyType(u16 type) {
    if (type < AITypes::TT_HUMANOID_FIRST
        || type > AITypes::TT_HUMANOID_LAST) {
        return false;
    }

    return !IsBossType(type);
}

bool IsRegularEnemy(
    const Humanoid* humanoid) {

    if (!humanoid
        || humanoid
            == static_cast<const Humanoid*>(
                Player::s_player)) {
        return false;
    }

    return IsRegularEnemyType(
        humanoid->thingType);
}

void ForgetHumanoid(
    Humanoid* humanoid) {

    if (!humanoid) {
        return;
    }

    ThreeChanGroupCombat::ForgetHumanoid(humanoid);
    ThreeChanSpawning::ForgetHumanoid(humanoid);
    g_runtime.erase(humanoid);
}

void SyncHumanoid(
    Humanoid& humanoid) {

    if (&humanoid
        == static_cast<Humanoid*>(
            Player::s_player)) {
        return;
    }

    ThreeChanGroupCombat::SyncHumanoid(humanoid);
    HumanoidRuntimeState& state =
        g_runtime[&humanoid];

    if (!state.healthCaptured) {
        state.healthCaptured = true;
        state.baseMaxHealth =
            humanoid.maxHealth;
        state.lastAppliedMaxHealth =
            humanoid.maxHealth;
    }

    if (IsSuppressedByNis(humanoid)) {
        return;
    }

    const float healthMultiplier =
        HealthMultiplierFor(humanoid);

    const u16 desiredMax =
        BuildScaledMaxHealth(
            state.baseMaxHealth,
            healthMultiplier);

    if (humanoid.maxHealth != desiredMax) {
        const u16 oldMax =
            humanoid.maxHealth;

        humanoid.health =
            PreserveHealthRatio(
                humanoid.health,
                oldMax,
                desiredMax);

        humanoid.maxHealth =
            desiredMax;
    }

    state.lastAppliedMaxHealth =
        humanoid.maxHealth;

    const float turnMultiplier =
        TurnMultiplierFor(humanoid);

    if (turnMultiplier == 1.0f) {
        if (state.turnTracked
            && humanoid.turnRate
                == state.lastAppliedTurnRate) {

            humanoid.turnRate =
                state.baseTurnRate;
        }

        state.turnTracked = false;
    }
    else {
        if (!state.turnTracked
            || humanoid.turnRate
                != state.lastAppliedTurnRate) {

            state.baseTurnRate =
                humanoid.turnRate;
        }

        const s32 scaled =
            static_cast<s32>(
                std::lround(
                    static_cast<double>(
                        state.baseTurnRate)
                    * static_cast<double>(
                        turnMultiplier)));

        humanoid.turnRate =
            static_cast<u16>(
                std::clamp(
                    scaled,
                    1,
                    65535));

        state.lastAppliedTurnRate =
            humanoid.turnRate;

        state.turnTracked = true;
    }
}

void ApplyPostControl(
    Humanoid& humanoid) {

    if (&humanoid
        == static_cast<Humanoid*>(
            Player::s_player)
        || IsSuppressedByNis(humanoid)) {
        return;
    }

    HumanoidRuntimeState& state =
        g_runtime[&humanoid];

    const ThreeChanSettings& settings =
        ThreeChanTuning::GetConst();

    float multiplier = 1.0f;

    if (settings.masterEnabled) {
        if (IsBossType(humanoid.thingType)
            && settings.bosses.global.enabled) {

            multiplier =
                settings.bosses.global
                    .movementSpeedMultiplier;
        }
        else if (IsRegularEnemy(&humanoid)
            && settings.enemies.enabled) {

            // moveSpeed is the final AI movement value chosen by either
            // runningSpeed or strafingSpeed. The dedicated Behaviour
            // decisions still remain intact.
            multiplier =
                settings.enemies
                    .runningSpeedMultiplier;
        }
    }

    if (multiplier == 1.0f) {
        if (state.moveTracked
            && humanoid.moveSpeed
                == state.lastAppliedMoveSpeed) {

            humanoid.moveSpeed =
                state.baseMoveSpeed;
        }

        state.moveTracked = false;
        return;
    }

    if (!state.moveTracked
        || humanoid.moveSpeed
            != state.lastAppliedMoveSpeed) {

        state.baseMoveSpeed =
            humanoid.moveSpeed;
    }

    const s32 scaled =
        static_cast<s32>(
            std::lround(
                static_cast<double>(
                    state.baseMoveSpeed)
                * static_cast<double>(
                    multiplier)));

    humanoid.moveSpeed =
        std::clamp(
            scaled,
            -32768,
            32767);

    state.lastAppliedMoveSpeed =
        humanoid.moveSpeed;

    state.moveTracked = true;
}

void ApplyAnimationTuning(
    Humanoid& humanoid) {

    if (&humanoid
        == static_cast<Humanoid*>(
            Player::s_player)) {
        return;
    }

    Model* model =
        humanoid.model
        ? static_cast<Model*>(
            humanoid.model)
        : nullptr;

    AnimStructure* animation =
        model
        ? static_cast<AnimStructure*>(
            model->animStructure)
        : nullptr;

    HumanoidRuntimeState& state =
        g_runtime[&humanoid];

    if (!animation) {
        state.animation = nullptr;
        state.animationTracked = false;
        return;
    }

    if (state.animation != animation) {
        state.animation = animation;
        state.animationTracked = false;
    }

    if (IsSuppressedByNis(humanoid)
        || !IsAttackAnimationState(
            humanoid.actionState)) {

        RestoreAnimationIfNeeded(
            state,
            animation);

        return;
    }

    const float multiplier =
        AttackAnimationMultiplierFor(
            humanoid);

    if (multiplier == 1.0f) {
        RestoreAnimationIfNeeded(
            state,
            animation);

        return;
    }

    if (!state.animationTracked
        || animation->speed
            != state.lastAppliedAnimationSpeed) {

        state.baseAnimationSpeed =
            animation->speed;
    }

    const s64 scaled =
        static_cast<s64>(
            std::llround(
                static_cast<double>(
                    state.baseAnimationSpeed)
                * static_cast<double>(
                    multiplier)));

    animation->speed =
        static_cast<s32>(
            std::clamp<s64>(
                scaled,
                1,
                0x7FFFFFFFLL));

    state.lastAppliedAnimationSpeed =
        animation->speed;

    state.animationTracked = true;
}

s32 ResolveIncomingDamage(
    Humanoid* victim,
    Thing* source,
    s32 damage) {

    if (!victim
        || !source
        || damage <= 0) {
        return damage;
    }

    if (victim
        != static_cast<Humanoid*>(
            Player::s_player)) {
        return damage;
    }

    const ThreeChanSettings& settings =
        ThreeChanTuning::GetConst();

    if (!settings.masterEnabled) {
        return damage;
    }

    float multiplier = 1.0f;

    if (IsBossType(source->thingType)
        && settings.bosses.global.enabled) {

        multiplier =
            settings.bosses.global
                .damageMultiplier;
    }
    else if (IsRegularEnemyType(
                 source->thingType)
        && settings.enemies.enabled) {

        multiplier =
            settings.enemies
                .damageMultiplier;
    }

    const s64 scaled =
        static_cast<s64>(
            std::llround(
                static_cast<double>(damage)
                * static_cast<double>(
                    multiplier)));

    return static_cast<s32>(
        std::clamp<s64>(
            scaled,
            0,
            65535));
}

s32 ResolveAttackFrequency(
    const Humanoid* owner,
    s32 originalValue) {

    return ScalePercentValue(
        owner,
        originalValue,
        ThreeChanTuning::GetConst()
            .enemies
            .attackFrequencyMultiplier);
}

s32 ResolveAggression(
    const Humanoid* owner,
    s32 originalValue) {

    return ScalePercentValue(
        owner,
        originalValue,
        ThreeChanTuning::GetConst()
            .enemies
            .aggressionMultiplier);
}

s32 ResolveDistancing(
    const Humanoid* owner,
    s32 originalValue) {

    return ScalePercentValue(
        owner,
        originalValue,
        ThreeChanTuning::GetConst()
            .enemies
            .distancingMultiplier);
}

s32 ResolveCircling(
    const Humanoid* owner,
    s32 originalValue) {

    return ScalePercentValue(
        owner,
        originalValue,
        ThreeChanTuning::GetConst()
            .enemies
            .circlingMultiplier);
}

s32 ResolvePunchChance(
    const Humanoid* owner,
    s32 originalValue) {

    return ScalePercentValue(
        owner,
        originalValue,
        ThreeChanTuning::GetConst()
            .enemies
            .punchChanceMultiplier);
}

s32 ResolveKickChance(
    const Humanoid* owner,
    s32 originalValue) {

    return ScalePercentValue(
        owner,
        originalValue,
        ThreeChanTuning::GetConst()
            .enemies
            .kickChanceMultiplier);
}

s32 ResolveThrowChance(
    const Humanoid* owner,
    s32 originalValue) {

    return ScalePercentValue(
        owner,
        originalValue,
        ThreeChanTuning::GetConst()
            .enemies
            .throwChanceMultiplier);
}

s32 ResolveComboChance(
    const Humanoid* owner,
    s32 originalValue) {

    return ScalePercentValue(
        owner,
        originalValue,
        ThreeChanTuning::GetConst()
            .enemies
            .comboChanceMultiplier);
}

s32 ResolveThinkDelay(
    const Humanoid* owner,
    s32 originalDelay) {

    if (!owner
        || !IsRegularEnemy(owner)) {
        return originalDelay;
    }

    const ThreeChanSettings& settings =
        ThreeChanTuning::GetConst();

    if (!settings.masterEnabled
        || !settings.enemies.enabled) {
        return originalDelay;
    }

    const ThreeChanEnemyTuning& tuning =
        settings.enemies;

    if (tuning.useCustomThinkRange) {
        const s32 minDelay =
            std::max(
                0,
                tuning.minThinkFrames);

        const s32 maxDelay =
            std::max(
                minDelay,
                tuning.maxThinkFrames);

        const s32 range =
            maxDelay - minDelay;

        if (range <= 0) {
            return minDelay;
        }

        return minDelay
            + static_cast<s32>(
                rmRangedRandom(
                    static_cast<u32>(
                        range + 1)));
    }

    const float speed =
        std::max(
            0.05f,
            tuning.decisionSpeedMultiplier);

    const s32 result =
        static_cast<s32>(
            std::lround(
                static_cast<double>(
                    originalDelay)
                / static_cast<double>(
                    speed)));

    return std::clamp(
        result,
        0,
        300);
}

} // namespace ThreeChanCombat