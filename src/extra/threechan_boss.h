#pragma once

#include "core.h"

class Humanoid;

namespace ThreeChanBoss {

bool IsEnabled();

s32 ResolveDecisionFrames(s32 originalFrames);
s32 ResolveRecoveryFrames(s32 originalFrames);

// Butch
s32 ResolveButchAttackDistance(s32 originalDistance);
bool ShouldButchStompRoll(s32 roll);
bool ShouldButchChargeRoll(s32 roll);
u16 ResolveButchStompDamage(u16 originalDamage);

// Grontar
s32 ResolveGrontarCloseRange(s32 originalDistance);
s32 ResolveGrontarFarRange(s32 originalDistance);
u32 ResolveGrontarThrowRange(u32 originalDistance);
s32 ResolveGrontarTrackingFrames(s32 originalFrames);
s32 ResolveGrontarDiveRollRange(s32 originalDistance);
s32 ResolveGrontarDiveRollAngle(s32 originalAngle);

// Dante
s32 ResolveDantePhaseThreshold(
    s32 originalFixedScale,
    s32 phaseNumber);

s32 ResolveDanteMissileRadius(s32 originalRadius);
u16 ResolveDanteMissileDamage(u16 originalDamage);

s32 ResolveDanteVolleyFrameThreshold(
    s32 originalFrames);

s32 ResolveDanteTargetMissileFrameThreshold(
    s32 originalFrames);

s32 ResolveDanteMissileRecoveryFrames(
    s32 originalFrames);

// Paul
s32 ResolvePaulCloseRange(s32 originalDistance);
s32 ResolvePaulDanceFrames(s32 originalFrames);
s32 ResolvePaulRecoveryFrames(s32 originalFrames);
s32 ResolvePaulBackoffChance(s32 originalChance);
s32 ResolvePaulAttackChance(s32 originalChance);

// Oscar
s32 ResolveOscarCloseRange(s32 originalDistance);
s32 ResolveOscarMidRange(s32 originalDistance);

s32 ResolveOscarCoordinationAngle(
    s32 originalAngle);

s32 ResolveOscarHenchmanWindow(
    s32 originalRange);

s32 ResolveOscarSyncDelay(
    s32 originalFrames);

}