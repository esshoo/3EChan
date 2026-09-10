#pragma once

#include "core.h"

class Humanoid;
class Thing;

namespace ThreeChanCombat {

bool IsBossType(u16 type);
bool IsRegularEnemyType(u16 type);
bool IsRegularEnemy(const Humanoid* humanoid);

void ForgetHumanoid(Humanoid* humanoid);

// Per-humanoid runtime synchronization.
void SyncHumanoid(Humanoid& humanoid);

// Called after the original AI has selected movement values.
void ApplyPostControl(Humanoid& humanoid);

// Called after ProcessAction so the currently playing attack animation
// can be tuned without modifying the original animation data.
void ApplyAnimationTuning(Humanoid& humanoid);

// Damage arriving at a humanoid from another Thing.
s32 ResolveIncomingDamage(
    Humanoid* victim,
    Thing* source,
    s32 damage);

// Behaviour values. Bosses and the player return the original value.
s32 ResolveAttackFrequency(
    const Humanoid* owner,
    s32 originalValue);

s32 ResolveAggression(
    const Humanoid* owner,
    s32 originalValue);

s32 ResolveDistancing(
    const Humanoid* owner,
    s32 originalValue);

s32 ResolveCircling(
    const Humanoid* owner,
    s32 originalValue);

s32 ResolvePunchChance(
    const Humanoid* owner,
    s32 originalValue);

s32 ResolveKickChance(
    const Humanoid* owner,
    s32 originalValue);

s32 ResolveThrowChance(
    const Humanoid* owner,
    s32 originalValue);

s32 ResolveComboChance(
    const Humanoid* owner,
    s32 originalValue);

// Takes the delay already selected by the original AI and applies
// either Decision Speed or the custom think range.
s32 ResolveThinkDelay(
    const Humanoid* owner,
    s32 originalDelay);

}