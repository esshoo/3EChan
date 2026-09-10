#pragma once

#include "core.h"

class ActiveZone;
class Humanoid;

struct ThreeChanGroupCombatStats {
    s32 trackedZones = 0;
    s32 trackedHumanoids = 0;
    s32 engagedHumanoids = 0;
    s32 activeAttackers = 0;
};

namespace ThreeChanGroupCombat {

void RegisterMember(
    ActiveZone* zone,
    Humanoid* humanoid);

void UnregisterMember(
    ActiveZone* zone,
    Humanoid* humanoid);

void ForgetHumanoid(
    Humanoid* humanoid);

void ForgetZone(
    ActiveZone* zone);

void SyncHumanoid(
    Humanoid& humanoid);

s32 GetFightingCollisionCapacity();

bool TryResolveAllowedToMoveIn(
    ActiveZone* zone,
    Humanoid* humanoid,
    s32& outAllowed);

bool TryBeginAttack(
    Humanoid* humanoid);

s32 ResolveNavigationBias(
    const ActiveZone* zone,
    const Humanoid* owner,
    s32 sideFieldAngle,
    s32 originalBias);

ThreeChanGroupCombatStats GetStats();

void ResetRuntime();

}