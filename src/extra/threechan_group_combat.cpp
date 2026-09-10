#include "extra/threechan_group_combat.h"

#include "extra/threechan_combat.h"
#include "extra/threechan_tuning.h"

#include "ai/activezn.h"
#include "ai/behaviour.h"
#include "ai/colfight.h"
#include "ai/humanoid.h"
#include "ai/player.h"

#include "gen/fxp.h"
#include "gen/time.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace {

struct AttackSlot {
    Humanoid* owner = nullptr;
    u32 reservedFrame = 0;
};

struct ZoneRuntime {
    std::vector<Humanoid*> members;
    std::vector<AttackSlot> attackSlots;

    Humanoid* lastAttackStarter = nullptr;

    u32 lastAttackStartFrame = 0;
    f64 lastAttackStartTime = -1000.0;

    bool replacementCredit = false;
};

std::unordered_map<ActiveZone*, ZoneRuntime> g_zones;

bool RuntimeEnabled() {
    const ThreeChanSettings& settings =
        ThreeChanTuning::GetConst();

    return settings.masterEnabled
        && settings.groupCombat.enabled;
}

bool IsAliveAndActive(
    const Humanoid* humanoid) {

    return humanoid
        && humanoid->health != 0
        && (humanoid->flags & TF_ACTIVATED) != 0
        && humanoid->actionState != static_cast<s32>(AS_DEAD);
}

bool IsEngaged(
    const Humanoid* humanoid) {

    if (!IsAliveAndActive(humanoid)
        || !humanoid->behaviour) {
        return false;
    }

    const s32 range =
        humanoid->behaviour->ndmsRangeBand;

    return range == 2 || range == 3;
}

bool IsAttackState(s32 state) {

    if (state < static_cast<s32>(AS_PUNCH_ATTACK)
        || state > static_cast<s32>(AS_THROW_PICKUP)) {
        return false;
    }

    if (state == static_cast<s32>(AS_COMBAT_IDLE)
        || state == static_cast<s32>(AS_PICKUP)) {
        return false;
    }

    return true;
}

u32 CurrentFrame() {
    return g_time
        ? g_time->GetFrameCounter()
        : 0;
}

s32 EffectiveEngagedLimit() {
    const ThreeChanSettings& settings =
        ThreeChanTuning::GetConst();

    if (!RuntimeEnabled()
        || settings.groupCombat.maxEngagedEnemies <= 0) {
        return 0;
    }

    return std::clamp(
        settings.groupCombat.maxEngagedEnemies,
        1,
        std::min(
            settings.advanced.hardEnemySafetyCap,
            32));
}

s32 EffectiveAttackLimit() {
    const ThreeChanSettings& settings =
        ThreeChanTuning::GetConst();

    if (!RuntimeEnabled()
        || settings.groupCombat.maxSimultaneousAttacks <= 0) {
        return 0;
    }

    return std::clamp(
        settings.groupCombat.maxSimultaneousAttacks,
        1,
        std::min(
            settings.advanced.hardSimultaneousAttackSafetyCap,
            16));
}

bool OwnsAttackSlot(
    const ZoneRuntime& state,
    const Humanoid* humanoid) {

    for (const AttackSlot& slot : state.attackSlots) {
        if (slot.owner == humanoid) {
            return true;
        }
    }

    return false;
}

void ReconcileAttackSlots(
    ZoneRuntime& state) {

    const u32 frame = CurrentFrame();
    bool releasedAny = false;

    state.attackSlots.erase(
        std::remove_if(
            state.attackSlots.begin(),
            state.attackSlots.end(),
            [&](const AttackSlot& slot) {

                Humanoid* owner = slot.owner;

                if (!owner || !IsAliveAndActive(owner)) {
                    releasedAny = true;
                    return true;
                }

                if (IsAttackState(owner->actionState)) {
                    return false;
                }

                if ((frame - slot.reservedFrame) <= 1u) {
                    return false;
                }

                releasedAny = true;
                return true;
            }),
        state.attackSlots.end());

    if (releasedAny) {
        state.replacementCredit = true;
    }
}

s32 CountOtherEngaged(
    const ZoneRuntime& state,
    const Humanoid* excluding) {

    s32 count = 0;

    for (Humanoid* member : state.members) {
        if (!member
            || member == excluding
            || !ThreeChanCombat::IsRegularEnemy(member)) {
            continue;
        }

        if (IsEngaged(member)) {
            count++;
        }
    }

    return count;
}

bool HasAlternativeAttacker(
    const ZoneRuntime& state,
    const Humanoid* current) {

    for (Humanoid* member : state.members) {
        if (!member
            || member == current
            || !ThreeChanCombat::IsRegularEnemy(member)
            || !IsEngaged(member)
            || OwnsAttackSlot(state, member)) {
            continue;
        }

        return true;
    }

    return false;
}

f64 EffectiveAttackGap(
    const ZoneRuntime& state) {

    const ThreeChanGroupCombatTuning& cfg =
        ThreeChanTuning::GetConst().groupCombat;

    f64 gap =
        static_cast<f64>(
            std::max(
                0,
                cfg.minimumAttackGapMs))
        / 1000.0;

    if (cfg.chainPressure
        && !state.attackSlots.empty()) {
        gap *= 0.5;
    }

    return gap;
}

}

namespace ThreeChanGroupCombat {

void RegisterMember(
    ActiveZone* zone,
    Humanoid* humanoid) {

    if (!zone || !humanoid) {
        return;
    }

    ZoneRuntime& state = g_zones[zone];

    if (std::find(
            state.members.begin(),
            state.members.end(),
            humanoid) == state.members.end()) {

        state.members.push_back(humanoid);
    }
}

void UnregisterMember(
    ActiveZone* zone,
    Humanoid* humanoid) {

    if (!zone || !humanoid) {
        return;
    }

    auto it = g_zones.find(zone);

    if (it == g_zones.end()) {
        return;
    }

    ZoneRuntime& state = it->second;

    state.members.erase(
        std::remove(
            state.members.begin(),
            state.members.end(),
            humanoid),
        state.members.end());

    state.attackSlots.erase(
        std::remove_if(
            state.attackSlots.begin(),
            state.attackSlots.end(),
            [humanoid](const AttackSlot& slot) {
                return slot.owner == humanoid;
            }),
        state.attackSlots.end());

    if (state.lastAttackStarter == humanoid) {
        state.lastAttackStarter = nullptr;
    }

    if (state.members.empty()
        && state.attackSlots.empty()) {
        g_zones.erase(it);
    }
}

void ForgetHumanoid(
    Humanoid* humanoid) {

    if (!humanoid) {
        return;
    }

    for (auto it = g_zones.begin();
         it != g_zones.end();) {

        ZoneRuntime& state = it->second;

        state.members.erase(
            std::remove(
                state.members.begin(),
                state.members.end(),
                humanoid),
            state.members.end());

        state.attackSlots.erase(
            std::remove_if(
                state.attackSlots.begin(),
                state.attackSlots.end(),
                [humanoid](const AttackSlot& slot) {
                    return slot.owner == humanoid;
                }),
            state.attackSlots.end());

        if (state.lastAttackStarter == humanoid) {
            state.lastAttackStarter = nullptr;
        }

        if (state.members.empty()
            && state.attackSlots.empty()) {
            it = g_zones.erase(it);
        }
        else {
            ++it;
        }
    }
}

void ForgetZone(
    ActiveZone* zone) {

    if (zone) {
        g_zones.erase(zone);
    }
}

void SyncHumanoid(
    Humanoid& humanoid) {

    if (&humanoid
        == static_cast<Humanoid*>(Player::s_player)) {
        return;
    }

    ActiveZone* zone = humanoid.activeZone;

    if (!zone) {
        return;
    }

    RegisterMember(zone, &humanoid);

    ZoneRuntime& state = g_zones[zone];
    ReconcileAttackSlots(state);

    if (RuntimeEnabled()
        && ThreeChanCombat::IsRegularEnemy(&humanoid)) {

        FightingCollision::InsertHumanoid(&humanoid);
    }
}

s32 GetFightingCollisionCapacity() {

    if (!RuntimeEnabled()) {
        return 12;
    }

    return std::clamp(
        ThreeChanTuning::GetConst()
            .advanced
            .intendedFightingCollisionCapacity,
        12,
        32);
}

bool TryResolveAllowedToMoveIn(
    ActiveZone* zone,
    Humanoid* humanoid,
    s32& outAllowed) {

    if (!RuntimeEnabled()
        || !zone
        || !humanoid
        || !ThreeChanCombat::IsRegularEnemy(humanoid)) {
        return false;
    }

    const s32 limit = EffectiveEngagedLimit();

    if (limit <= 0) {
        return false;
    }

    RegisterMember(zone, humanoid);

    const s32 otherEngaged =
        CountOtherEngaged(
            g_zones[zone],
            humanoid);

    outAllowed =
        otherEngaged < limit
            ? 1
            : 0;

    return true;
}

bool TryBeginAttack(
    Humanoid* humanoid) {

    if (!humanoid
        || !ThreeChanCombat::IsRegularEnemy(humanoid)
        || !RuntimeEnabled()) {
        return true;
    }

    const s32 limit = EffectiveAttackLimit();

    if (limit <= 0) {
        return true;
    }

    ActiveZone* zone = humanoid->activeZone;

    if (!zone) {
        return true;
    }

    RegisterMember(zone, humanoid);

    ZoneRuntime& state = g_zones[zone];

    ReconcileAttackSlots(state);

    if (OwnsAttackSlot(state, humanoid)) {
        return true;
    }

    if (static_cast<s32>(state.attackSlots.size()) >= limit) {
        return false;
    }

    const ThreeChanGroupCombatTuning& cfg =
        ThreeChanTuning::GetConst().groupCombat;

    const u32 frame = CurrentFrame();

    if (cfg.attackerRotation
        && state.lastAttackStarter == humanoid
        && (frame - state.lastAttackStartFrame) < 30u
        && HasAlternativeAttacker(state, humanoid)) {

        return false;
    }

    const f64 now = Time::GetTimeInSeconds();
    const f64 gap = EffectiveAttackGap(state);

    const bool bypassGap =
        cfg.immediateReplacement
        && state.replacementCredit;

    if (!bypassGap
        && gap > 0.0
        && (now - state.lastAttackStartTime) < gap) {
        return false;
    }

    AttackSlot slot;
    slot.owner = humanoid;
    slot.reservedFrame = frame;

    state.attackSlots.push_back(slot);

    state.lastAttackStarter = humanoid;
    state.lastAttackStartFrame = frame;
    state.lastAttackStartTime = now;
    state.replacementCredit = false;

    return true;
}

s32 ResolveNavigationBias(
    const ActiveZone* zone,
    const Humanoid* owner,
    s32 sideFieldAngle,
    s32 originalBias) {

    if (!RuntimeEnabled()
        || EffectiveEngagedLimit() <= 3
        || !zone
        || !owner
        || !ThreeChanCombat::IsRegularEnemy(owner)) {
        return originalBias;
    }

    auto it =
        g_zones.find(
            const_cast<ActiveZone*>(zone));

    if (it == g_zones.end()) {
        return originalBias;
    }

    s32 bias = 50;

    for (Humanoid* member : it->second.members) {
        if (!member
            || member == owner
            || !ThreeChanCombat::IsRegularEnemy(member)
            || !IsAliveAndActive(member)) {
            continue;
        }

        if (IsPointInFieldOf(
                member->pos,
                owner->pos,
                sideFieldAngle,
                0x4000,
                0x4000)) {

            bias += 30;
        }
        else {
            bias -= 30;
        }
    }

    return std::clamp(bias, 0, 100);
}

ThreeChanGroupCombatStats GetStats() {

    ThreeChanGroupCombatStats result;

    result.trackedZones =
        static_cast<s32>(g_zones.size());

    for (auto& entry : g_zones) {

        ZoneRuntime& state = entry.second;

        ReconcileAttackSlots(state);

        result.trackedHumanoids +=
            static_cast<s32>(state.members.size());

        result.activeAttackers +=
            static_cast<s32>(state.attackSlots.size());

        for (Humanoid* member : state.members) {
            if (ThreeChanCombat::IsRegularEnemy(member)
                && IsEngaged(member)) {

                result.engagedHumanoids++;
            }
        }
    }

    return result;
}

void ResetRuntime() {
    g_zones.clear();
}

}