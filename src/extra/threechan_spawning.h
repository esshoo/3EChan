#pragma once

#include "core.h"

class EnemyGenerator;
class Humanoid;

struct ThreeChanSpawningStats {
    s32 trackedGenerators = 0;
    s32 liveGeneratedEnemies = 0;
    s32 pendingWaves = 0;
    s32 pausedGenerators = 0;
};

namespace ThreeChanSpawning {

void CaptureGenerator(EnemyGenerator& generator);

void OnGeneratorReset(EnemyGenerator& generator);

void ForgetGenerator(EnemyGenerator* generator);

void ForgetHumanoid(Humanoid* humanoid);

void SyncGenerator(EnemyGenerator& generator);

bool IsPaused(EnemyGenerator& generator);

bool CanProcessPendingWave(EnemyGenerator& generator);

bool CanGenerateAnother(EnemyGenerator& generator);

void OnEnemyGenerated(
    EnemyGenerator& generator,
    Humanoid* humanoid);

void OnWaveProcessed(EnemyGenerator& generator);

// Prevent custom expanded encounters from completing while their
// additional generated enemies are still alive.
bool ShouldBlockLevelComplete(EnemyGenerator& generator);

ThreeChanSpawningStats GetStats();

void ResetRuntime();

}