#include "extra/threechan_spawning.h"

#include "extra/threechan_tuning.h"

#include "ai/generator.h"
#include "ai/humanoid.h"

#include "gen/time.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace {

struct GeneratorRuntime {
    bool captured = false;

    s32 originalTotalBudget = 0;
    s32 originalSpawnBurst = 0;
    s32 originalZoneThreshold = 0;

    std::vector<Humanoid*> liveEnemies;

    f64 nextWaveTime = 0.0;

    // Remains true until reset/destruction once a custom enemy-count
    // budget has actually been enabled.
    bool completionGuard = false;
};

std::unordered_map<EnemyGenerator*, GeneratorRuntime> g_generators;

bool RuntimeEnabled() {
    const ThreeChanSettings& settings =
        ThreeChanTuning::GetConst();

    return settings.masterEnabled
        && settings.spawning.enabled;
}

void CleanupLiveEnemies(
    GeneratorRuntime& runtime) {

    runtime.liveEnemies.erase(
        std::remove_if(
            runtime.liveEnemies.begin(),
            runtime.liveEnemies.end(),
            [](Humanoid* humanoid) {
                if (!humanoid) {
                    return true;
                }

                return humanoid->health == 0
                    || humanoid->actionState
                        == static_cast<s32>(AS_DEAD);
            }),
        runtime.liveEnemies.end());
}

s32 EffectiveTotalBudget(
    const GeneratorRuntime& runtime) {

    if (!RuntimeEnabled()) {
        return runtime.originalTotalBudget;
    }

    const float multiplier =
        ThreeChanTuning::GetConst()
            .spawning
            .enemyCountMultiplier;

    if (runtime.originalTotalBudget <= 0) {
        return 0;
    }

    s32 result =
        static_cast<s32>(
            std::llround(
                static_cast<double>(
                    runtime.originalTotalBudget)
                * static_cast<double>(
                    multiplier)));

    result = std::max(1, result);

    return std::clamp(
        result,
        1,
        512);
}

s32 EffectiveSpawnBurst(
    const GeneratorRuntime& runtime) {

    if (!RuntimeEnabled()) {
        return runtime.originalSpawnBurst;
    }

    const s32 overrideValue =
        ThreeChanTuning::GetConst()
            .spawning
            .spawnBurstOverride;

    if (overrideValue <= 0) {
        return runtime.originalSpawnBurst;
    }

    return std::clamp(
        overrideValue,
        1,
        16);
}

s32 EffectiveZoneThreshold(
    const GeneratorRuntime& runtime) {

    if (!RuntimeEnabled()) {
        return runtime.originalZoneThreshold;
    }

    return std::clamp(
        runtime.originalZoneThreshold
            + ThreeChanTuning::GetConst()
                .spawning
                .activeZoneThresholdOffset,
        -16,
        32);
}

s32 EffectiveMaxAlive() {

    if (!RuntimeEnabled()) {
        return 0;
    }

    const ThreeChanSettings& settings =
        ThreeChanTuning::GetConst();

    if (settings.spawning.maxAliveOverride <= 0) {
        return 0;
    }

    return std::clamp(
        settings.spawning.maxAliveOverride,
        1,
        std::min(
            settings.advanced.hardEnemySafetyCap,
            32));
}

void ApplyCurrentSettings(
    EnemyGenerator& generator,
    GeneratorRuntime& runtime) {

    CleanupLiveEnemies(runtime);

    const s32 totalBudget =
        EffectiveTotalBudget(runtime);

    if (RuntimeEnabled()
        && totalBudget != runtime.originalTotalBudget) {
        runtime.completionGuard = true;
    }

    generator.field200 = totalBudget;

    generator.field292 =
        EffectiveSpawnBurst(runtime);

    generator.field288 =
        EffectiveZoneThreshold(runtime);
}

}

namespace ThreeChanSpawning {

void CaptureGenerator(
    EnemyGenerator& generator) {

    GeneratorRuntime& runtime =
        g_generators[&generator];

    runtime.captured = true;

    runtime.originalTotalBudget =
        generator.field200;

    runtime.originalSpawnBurst =
        generator.field292;

    runtime.originalZoneThreshold =
        generator.field288;

    runtime.liveEnemies.clear();
    runtime.nextWaveTime = 0.0;
    runtime.completionGuard = false;

    ApplyCurrentSettings(
        generator,
        runtime);
}

void OnGeneratorReset(
    EnemyGenerator& generator) {

    auto it = g_generators.find(&generator);

    if (it == g_generators.end()
        || !it->second.captured) {
        return;
    }

    GeneratorRuntime& runtime =
        it->second;

    runtime.liveEnemies.clear();
    runtime.nextWaveTime = 0.0;
    runtime.completionGuard = false;

    ApplyCurrentSettings(
        generator,
        runtime);
}

void ForgetGenerator(
    EnemyGenerator* generator) {

    if (!generator) {
        return;
    }

    g_generators.erase(generator);
}

void ForgetHumanoid(
    Humanoid* humanoid) {

    if (!humanoid) {
        return;
    }

    for (auto& entry : g_generators) {

        std::vector<Humanoid*>& enemies =
            entry.second.liveEnemies;

        enemies.erase(
            std::remove(
                enemies.begin(),
                enemies.end(),
                humanoid),
            enemies.end());
    }
}

void SyncGenerator(
    EnemyGenerator& generator) {

    GeneratorRuntime& runtime =
        g_generators[&generator];

    if (!runtime.captured) {
        CaptureGenerator(generator);
        return;
    }

    ApplyCurrentSettings(
        generator,
        runtime);
}

bool IsPaused(
    EnemyGenerator& generator) {

    SyncGenerator(generator);

    return RuntimeEnabled()
        && ThreeChanTuning::GetConst()
            .spawning
            .pauseGenerators;
}

bool CanProcessPendingWave(
    EnemyGenerator& generator) {

    SyncGenerator(generator);

    if (!RuntimeEnabled()) {
        return true;
    }

    GeneratorRuntime& runtime =
        g_generators[&generator];

    const s32 maxAlive =
        EffectiveMaxAlive();

    if (maxAlive > 0
        && static_cast<s32>(
            runtime.liveEnemies.size()) >= maxAlive) {
        return false;
    }

    return Time::GetTimeInSeconds()
        >= runtime.nextWaveTime;
}

bool CanGenerateAnother(
    EnemyGenerator& generator) {

    SyncGenerator(generator);

    if (!RuntimeEnabled()) {
        return true;
    }

    const s32 maxAlive =
        EffectiveMaxAlive();

    if (maxAlive <= 0) {
        return true;
    }

    return static_cast<s32>(
        g_generators[&generator]
            .liveEnemies
            .size()) < maxAlive;
}

void OnEnemyGenerated(
    EnemyGenerator& generator,
    Humanoid* humanoid) {

    if (!humanoid) {
        return;
    }

    GeneratorRuntime& runtime =
        g_generators[&generator];

    if (!runtime.captured) {
        CaptureGenerator(generator);
    }

    if (std::find(
            runtime.liveEnemies.begin(),
            runtime.liveEnemies.end(),
            humanoid) == runtime.liveEnemies.end()) {

        runtime.liveEnemies.push_back(humanoid);
    }
}

void OnWaveProcessed(
    EnemyGenerator& generator) {

    GeneratorRuntime& runtime =
        g_generators[&generator];

    if (!runtime.captured) {
        CaptureGenerator(generator);
    }

    if (!RuntimeEnabled()) {
        runtime.nextWaveTime = 0.0;
        return;
    }

    const f32 delay =
        std::clamp(
            ThreeChanTuning::GetConst()
                .spawning
                .waveDelaySeconds,
            0.0f,
            30.0f);

    runtime.nextWaveTime =
        Time::GetTimeInSeconds()
        + static_cast<f64>(delay);
}

bool ShouldBlockLevelComplete(
    EnemyGenerator& generator) {

    auto it = g_generators.find(&generator);

    if (it == g_generators.end()) {
        return false;
    }

    GeneratorRuntime& runtime =
        it->second;

    CleanupLiveEnemies(runtime);

    return runtime.completionGuard
        && !runtime.liveEnemies.empty();
}

ThreeChanSpawningStats GetStats() {

    ThreeChanSpawningStats result;

    result.trackedGenerators =
        static_cast<s32>(
            g_generators.size());

    const bool paused =
        RuntimeEnabled()
        && ThreeChanTuning::GetConst()
            .spawning
            .pauseGenerators;

    for (auto& entry : g_generators) {

        EnemyGenerator* generator =
            entry.first;

        GeneratorRuntime& runtime =
            entry.second;

        CleanupLiveEnemies(runtime);

        result.liveGeneratedEnemies +=
            static_cast<s32>(
                runtime.liveEnemies.size());

        if (generator
            && generator->field296 != 0) {
            result.pendingWaves++;
        }

        if (generator && paused) {
            result.pausedGenerators++;
        }
    }

    return result;
}

void ResetRuntime() {

    for (auto& entry : g_generators) {

        EnemyGenerator* generator =
            entry.first;

        GeneratorRuntime& runtime =
            entry.second;

        if (!generator
            || !runtime.captured) {
            continue;
        }

        generator->field200 =
            runtime.originalTotalBudget;

        generator->field292 =
            runtime.originalSpawnBurst;

        generator->field288 =
            runtime.originalZoneThreshold;
    }

    g_generators.clear();
}

}