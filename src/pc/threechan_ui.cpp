#include "pc/threechan_ui.h"
#include "pc/imgui_localization.h"

#include "extra/threechan_tuning.h"
#include "extra/threechan_camera.h"
#include "extra/threechan_group_combat.h"
#include "extra/threechan_spawning.h"
#include "extra/threechan_boss.h"
#include "imgui.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

char g_profileNameBuffer[64] = "Test";
std::string LocalizeTuningMessage(const char* message) {
    const char* source = message ? message : "";

    if (!ImGuiLocalization::IsArabic()) {
        return source;
    }

    struct MessageEntry {
        const char* english;
        const char* token;
    };

    static const MessageEntry messages[] = {
        { "Ready.", "3E_M_READY" },
        { "3EChan tuning system initialized.", "3E_M_INIT" },
        { "Warning: could not create userfiles/3EChan/profiles.", "3E_M_MKDIRWARN" },
        { "Restored original 3EChan tuning values.", "3E_M_RESTORED" },
        { "Profile name is empty.", "3E_M_EMPTY" },
        { "Original is reserved and cannot be overwritten.", "3E_M_RESERVED" },
        { "Could not create profile directory.", "3E_M_DIRFAIL" },
        { "Could not open profile for writing.", "3E_M_OPENFAIL" },
        { "Failed while writing profile.", "3E_M_WRITEFAIL" }
    };

    for (const MessageEntry& entry : messages) {
        if (std::strcmp(source, entry.english) == 0) {
            return ImGuiLocalization::Text(source, entry.token);
        }
    }

    static constexpr const char* savedPrefix =
        "Profile saved: ";

    static constexpr const char* loadFailedPrefix =
        "Could not load profile: ";

    static constexpr const char* loadedPrefix =
        "Profile loaded: ";

    if (std::strncmp(source, savedPrefix, std::strlen(savedPrefix)) == 0) {
        return ImGuiLocalization::Format(
            "Profile saved: %s",
            "3E_M_SAVED",
            source + std::strlen(savedPrefix));
    }

    if (std::strncmp(
            source,
            loadFailedPrefix,
            std::strlen(loadFailedPrefix)) == 0) {
        return ImGuiLocalization::Format(
            "Could not load profile: %s",
            "3E_M_LOADFAIL",
            source + std::strlen(loadFailedPrefix));
    }

    if (std::strncmp(source, loadedPrefix, std::strlen(loadedPrefix)) == 0) {
        return ImGuiLocalization::Format(
            "Profile loaded: %s",
            "3E_M_LOADED",
            source + std::strlen(loadedPrefix));
    }

    return ImGuiLocalization::Text(
        "3EChan status updated.",
        "3E_M_STATUS");
}

void Multiplier(
    const char* label,
    float& value,
    float minValue = 0.25f,
    float maxValue = 5.0f) {

    ImGui::SliderFloat(label, &value, minValue, maxValue, "%.2fx");
}

void DrawProfilesTab(ThreeChanSettings& settings) {
    ImGui::Checkbox(ImGuiLocalization::Label("3EChan Overrides", "3E_OVR", "3E_Overrides").c_str(), &settings.masterEnabled);
    ImGui::TextDisabled("%s", ImGuiLocalization::Text("Changes apply live while 3EChan Overrides is enabled.", "3E_LIVE").c_str());

    ImGui::Separator();

    const std::vector<std::string> profiles =
        ThreeChanTuning::ListProfiles();

    const std::string& active =
        ThreeChanTuning::GetActiveProfileName();

    if (ImGui::BeginCombo(ImGuiLocalization::Label("Active Profile", "3E_ACTPROF", "3E_ActiveProfile").c_str(), active.c_str())) {
        for (const std::string& profile : profiles) {
            const bool selected = (profile == active);

            if (ImGui::Selectable(profile.c_str(), selected)) {
                ThreeChanTuning::LoadProfile(profile.c_str());

                std::snprintf(
                    g_profileNameBuffer,
                    sizeof(g_profileNameBuffer),
                    "%s",
                    profile.c_str());
            }

            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    ImGui::InputText(
        ImGuiLocalization::Label("Profile Name", "3E_PROFNAME", "3E_ProfileName").c_str(),
        g_profileNameBuffer,
        sizeof(g_profileNameBuffer));

    if (ImGui::Button(ImGuiLocalization::Label("Save / Save As", "3E_SAVEAS", "3E_SaveAs").c_str())) {
        ThreeChanTuning::SaveProfile(g_profileNameBuffer);
    }

    ImGui::SameLine();

    if (ImGui::Button(ImGuiLocalization::Label("Reload", "3E_RELOAD", "3E_Reload").c_str())) {
        ThreeChanTuning::LoadProfile(

    }

    ImGui::SameLine();

    if (ImGui::Button(ImGuiLocalization::Label("Restore Original", "3E_RESTORE", "3E_Restore").c_str())) {
        ThreeChanTuning::ResetToOriginal();

        std::snprintf(
            g_profileNameBuffer,
            sizeof(g_profileNameBuffer),
            "Test");
    }

    ImGui::Separator();
    ImGui::TextWrapped("%s", LocalizeTuningMessage(ThreeChanTuning::GetLastMessage()).c_str());

    ImGui::SeparatorText(ImGuiLocalization::Label("Purpose", "3E_PURPOSE", "3E_Purpose").c_str());
    ImGui::TextWrapped(
        "%s", ImGuiLocalization::Text("Profiles are stored outside the executable so gameplay and camera tuning can be changed without rebuilding 3EChan.", "3E_PROFDESC").c_str());
}

void DrawEnemiesTab(ThreeChanSettings& settings) {
    auto& e = settings.enemies;

    ImGui::Checkbox(ImGuiLocalization::Label("Enable Regular Enemy Tuning", "3E_EN_ENABLE", "3E_EnemyEnable").c_str(), &e.enabled);

    ImGui::SeparatorText(ImGuiLocalization::Label("Survivability / Damage", "3E_SURV", "3E_EnemySurv").c_str());
    Multiplier(ImGuiLocalization::Label("Health", "3E_HEALTH", "3E_EnemyHealth").c_str(), e.healthMultiplier);
    Multiplier(ImGuiLocalization::Label("Damage", "3E_DAMAGE", "3E_EnemyDamage").c_str(), e.damageMultiplier);

    ImGui::SeparatorText(ImGuiLocalization::Label("Decision Making", "3E_DECISION", "3E_EnemyDecision").c_str());
    Multiplier(ImGuiLocalization::Label("Attack Frequency", "3E_ATKFREQ", "3E_EnemyAtkFreq").c_str(), e.attackFrequencyMultiplier);
    Multiplier(ImGuiLocalization::Label("Aggression", "3E_AGGR", "3E_EnemyAggression").c_str(), e.aggressionMultiplier);
    Multiplier(ImGuiLocalization::Label("Distancing", "3E_DIST", "3E_EnemyDistancing").c_str(), e.distancingMultiplier);
    Multiplier(ImGuiLocalization::Label("Circling", "3E_CIRCLE", "3E_EnemyCircling").c_str(), e.circlingMultiplier);
    Multiplier(ImGuiLocalization::Label("Decision Speed", "3E_DECSPEED", "3E_EnemyDecisionSpeed").c_str(), e.decisionSpeedMultiplier);

    ImGui::Checkbox(ImGuiLocalization::Label("Use Custom Think Range", "3E_THINKUSE", "3E_EnemyThinkRange").c_str(), &e.useCustomThinkRange);

    if (e.useCustomThinkRange) {
        ImGui::SliderInt(ImGuiLocalization::Label("Minimum Think Frames", "3E_MINTHINK", "3E_EnemyMinThink").c_str(), &e.minThinkFrames, 0, 120);
        ImGui::SliderInt(ImGuiLocalization::Label("Maximum Think Frames", "3E_MAXTHINK", "3E_EnemyMaxThink").c_str(), &e.maxThinkFrames, 0, 120);

        if (e.maxThinkFrames < e.minThinkFrames) {
            e.maxThinkFrames = e.minThinkFrames;
        }
    }

    ImGui::SeparatorText(ImGuiLocalization::Label("Movement", "3E_MOVE", "3E_EnemyMovement").c_str());
    Multiplier(ImGuiLocalization::Label("Running Speed", "3E_RUNSPD", "3E_EnemyRunSpeed").c_str(), e.runningSpeedMultiplier);
    Multiplier(ImGuiLocalization::Label("Strafing Speed", "3E_STRAFSPD", "3E_EnemyStrafeSpeed").c_str(), e.strafingSpeedMultiplier);
    Multiplier(ImGuiLocalization::Label("Turn Speed", "3E_TURNSPD", "3E_EnemyTurnSpeed").c_str(), e.turnSpeedMultiplier);
    Multiplier(ImGuiLocalization::Label("Attack Animation Speed", "3E_ATKANIM", "3E_EnemyAttackAnim").c_str(), e.attackAnimationSpeedMultiplier);

    ImGui::SeparatorText(ImGuiLocalization::Label("Attack Selection", "3E_ATKSEL", "3E_EnemyAttackSelection").c_str());
    Multiplier(ImGuiLocalization::Label("Punch Chance", "3E_PUNCH", "3E_EnemyPunch").c_str(), e.punchChanceMultiplier, 0.0f, 3.0f);
    Multiplier(ImGuiLocalization::Label("Kick Chance", "3E_KICK", "3E_EnemyKick").c_str(), e.kickChanceMultiplier, 0.0f, 3.0f);
    Multiplier(ImGuiLocalization::Label("Throw Chance", "3E_THROW", "3E_EnemyThrow").c_str(), e.throwChanceMultiplier, 0.0f, 3.0f);
    Multiplier(ImGuiLocalization::Label("Combo Chance", "3E_COMBO", "3E_EnemyCombo").c_str(), e.comboChanceMultiplier, 0.0f, 3.0f);

    ImGui::SeparatorText(ImGuiLocalization::Label("Reactions / Recovery", "3E_REACT", "3E_EnemyReactions").c_str());
    Multiplier(ImGuiLocalization::Label("Stun Duration", "3E_STUN", "3E_EnemyStun").c_str(), e.stunDurationMultiplier);
    Multiplier(ImGuiLocalization::Label("Knockdown Recovery", "3E_KDREC", "3E_EnemyKnockdownRecovery").c_str(), e.knockdownRecoveryMultiplier);
    Multiplier(ImGuiLocalization::Label("Get-Up Speed", "3E_GETUP", "3E_EnemyGetUp").c_str(), e.getUpSpeedMultiplier);
}

void DrawGroupCombatTab(ThreeChanSettings& settings) {
    auto& g = settings.groupCombat;

    ImGui::Checkbox(ImGuiLocalization::Label("Enable Extended Group Combat", "3E_GRP_ENABLE", "3E_GroupEnable").c_str(), &g.enabled);

    ImGui::TextDisabled("%s", ImGuiLocalization::Text("0 keeps the original game value.", "3E_ZERO_ORIG").c_str());

    ImGui::SliderInt(
        ImGuiLocalization::Label("Max Engaged Enemies", "3E_MAXENG", "3E_MaxEngaged").c_str(),
        &g.maxEngagedEnemies,
        0,
        settings.advanced.hardEnemySafetyCap);

    ImGui::SliderInt(
        ImGuiLocalization::Label("Max Simultaneous Attacks", "3E_MAXATK", "3E_MaxAttacks").c_str(),
        &g.maxSimultaneousAttacks,
        0,
        settings.advanced.hardSimultaneousAttackSafetyCap);

    ImGui::SliderInt(
        ImGuiLocalization::Label("Minimum Gap Between Attacks (ms)", "3E_MINGAP", "3E_MinAttackGap").c_str(),
        &g.minimumAttackGapMs,
        0,
        3000);

    ImGui::Checkbox(ImGuiLocalization::Label("Attacker Rotation", "3E_ROTATE", "3E_AttackerRotation").c_str(), &g.attackerRotation);
    ImGui::Checkbox(ImGuiLocalization::Label("Allow Chain Pressure", "3E_CHAIN", "3E_ChainPressure").c_str(), &g.chainPressure);
    ImGui::Checkbox(ImGuiLocalization::Label("Immediate Replacement", "3E_REPLACE", "3E_ImmediateReplacement").c_str(), &g.immediateReplacement);
    const ThreeChanGroupCombatStats runtime =
        ThreeChanGroupCombat::GetStats();

    ImGui::SeparatorText(ImGuiLocalization::Label("Live Runtime", "3E_LIVERUN", "3E_GroupLiveRuntime").c_str());

    ImGui::Text("%s", ImGuiLocalization::Format("Tracked Zones: %d", "3E_TRKZONE", runtime.trackedZones).c_str());
    ImGui::Text("%s", ImGuiLocalization::Format("Tracked Humanoids: %d", "3E_TRKHUM", runtime.trackedHumanoids).c_str());
    ImGui::Text("%s", ImGuiLocalization::Format("Engaged Enemies: %d", "3E_ENGAGED", runtime.engagedHumanoids).c_str());
    ImGui::Text("%s", ImGuiLocalization::Format("Active Attack Slots: %d", "3E_ATKSLOTS", runtime.activeAttackers).c_str());

    ImGui::Text("%s", ImGuiLocalization::Text("Physical Fight Capacity: 32", "3E_PHYSCAP").c_str());

    ImGui::Text(
        "%s",
        ImGuiLocalization::Format(
            "Current Logical Capacity: %d",
            "3E_LOGCAP",
            ThreeChanGroupCombat::GetFightingCollisionCapacity()).c_str());

    ImGui::Separator();
    ImGui::TextWrapped(
        "%s",
        ImGuiLocalization::Text(
            "The original ActiveZone system is limited to three overlord members. The integration stage will keep the original layout intact and add a 3EChan sidecar coordinator for expanded encounters.",
            "3E_GRPDESC").c_str());
}

void DrawSpawningTab(ThreeChanSettings& settings) {
    auto& s = settings.spawning;

    ImGui::Checkbox(ImGuiLocalization::Label("Enable Spawn Tuning", "3E_SPN_ENABLE", "3E_SpawnEnable").c_str(), &s.enabled);

    Multiplier(ImGuiLocalization::Label("Enemy Count", "3E_ENCOUNT", "3E_EnemyCount").c_str(), s.enemyCountMultiplier, 0.25f, 4.0f);
    ImGui::SliderFloat(
        ImGuiLocalization::Label("Wave Delay (seconds)", "3E_WAVEDELAY", "3E_WaveDelay").c_str(),
        &s.waveDelaySeconds,
        0.0f,
        10.0f,
        "%.2f sec");

    ImGui::TextDisabled(
        "%s", ImGuiLocalization::Text("0 seconds keeps the original immediate wave timing.", "3E_WAVEZERO").c_str());

    ImGui::TextDisabled("%s", ImGuiLocalization::Text("0 keeps the original level value.", "3E_ZEROLEVEL").c_str());

    ImGui::SliderInt(
        ImGuiLocalization::Label("Spawn Burst Override", "3E_BURST", "3E_SpawnBurst").c_str(),
        &s.spawnBurstOverride,
        0,
        16);

    ImGui::SliderInt(
        ImGuiLocalization::Label("Maximum Alive Override", "3E_MAXALIVE", "3E_MaxAlive").c_str(),
        &s.maxAliveOverride,
        0,
        settings.advanced.hardEnemySafetyCap);

    ImGui::SliderInt(
        ImGuiLocalization::Label("Active Zone Threshold Offset", "3E_ZONEOFF", "3E_ZoneOffset").c_str(),
        &s.activeZoneThresholdOffset,
        -8,
        8);

    ImGui::Checkbox(ImGuiLocalization::Label("Pause Enemy Generators", "3E_PAUSEGEN", "3E_PauseGenerators").c_str(), &s.pauseGenerators);
    const ThreeChanSpawningStats runtime =
        ThreeChanSpawning::GetStats();

    ImGui::SeparatorText(ImGuiLocalization::Label("Live Runtime", "3E_LIVERUN", "3E_SpawnLiveRuntime").c_str());

    ImGui::Text("%s", ImGuiLocalization::Format("Tracked Enemy Generators: %d", "3E_TRKGEN", runtime.trackedGenerators).c_str());

    ImGui::Text("%s", ImGuiLocalization::Format("Live Generated Enemies: %d", "3E_LIVEGEN", runtime.liveGeneratedEnemies).c_str());

    ImGui::Text("%s", ImGuiLocalization::Format("Pending Waves: %d", "3E_PENDING", runtime.pendingWaves).c_str());

    ImGui::Text("%s", ImGuiLocalization::Format("Paused Generators: %d", "3E_PAUSED", runtime.pausedGenerators).c_str());
}

void DrawBossMultiplierSet(
    const char* title,
    float& a,
    const char* aName,
    float& b,
    const char* bName,
    float& c,
    const char* cName,
    float& d,
    const char* dName) {

    if (!ImGui::CollapsingHeader(title)) {
        return;
    }

    Multiplier(aName, a);
    Multiplier(bName, b);
    Multiplier(cName, c);
    Multiplier(dName, d);
}

void DrawBossesTab(ThreeChanSettings& settings) {
    auto& bosses = settings.bosses;

    ImGui::Checkbox(
        ImGuiLocalization::Label("Enable Boss Tuning", "3E_BOS_EN", "3E_BossEnable").c_str(),
        &bosses.global.enabled);

    ImGui::SeparatorText(ImGuiLocalization::Label("Global Boss Multipliers", "3E_BOS_GLOBAL", "3E_BossGlobal").c_str());

    Multiplier(ImGuiLocalization::Label("Boss Health", "3E_BOS_HEALTH", "3E_BossHealth").c_str(), bosses.global.healthMultiplier);
    Multiplier(ImGuiLocalization::Label("Boss Damage", "3E_BOS_DMG", "3E_BossDamage").c_str(), bosses.global.damageMultiplier);
    Multiplier(ImGuiLocalization::Label("Boss Movement Speed", "3E_BOS_MOVE", "3E_BossMove").c_str(), bosses.global.movementSpeedMultiplier);
    Multiplier(ImGuiLocalization::Label("Boss Decision Speed", "3E_BOS_DEC", "3E_BossDecision").c_str(), bosses.global.decisionSpeedMultiplier);
    Multiplier(ImGuiLocalization::Label("Boss Attack Speed", "3E_BOS_ATK", "3E_BossAttack").c_str(), bosses.global.attackSpeedMultiplier);
    Multiplier(ImGuiLocalization::Label("Boss Recovery Speed", "3E_BOS_REC", "3E_BossRecovery").c_str(), bosses.global.recoverySpeedMultiplier);
    ImGui::TextDisabled(
        "%s", ImGuiLocalization::Text("Rate/Speed > 1 = faster. Time/Delay > 1 = longer.", "3E_BOS_RATE").c_str());

    ImGui::TextDisabled(
        "%s", ImGuiLocalization::Text("Boss Decision Speed applies to stock AI paths with explicit decision counters.", "3E_BOS_DECNOTE").c_str());

    ImGui::SeparatorText(ImGuiLocalization::Label("Per-Boss Advanced Tuning", "3E_BOS_ADV", "3E_BossAdvanced").c_str());

    if (ImGui::CollapsingHeader("Butch")) {
        Multiplier(ImGuiLocalization::Label("Stomp Damage", "3E_BUT_STOMP", "3E_ButchStompDamage").c_str(), bosses.butch.stompDamageMultiplier);
        Multiplier(ImGuiLocalization::Label("Attack Distance", "3E_BUT_DIST", "3E_ButchAttackDistance").c_str(), bosses.butch.attackDistanceMultiplier);
        Multiplier(ImGuiLocalization::Label("Stomp Frequency", "3E_BUT_FREQ", "3E_ButchStompFrequency").c_str(), bosses.butch.stompFrequencyMultiplier);
        Multiplier(ImGuiLocalization::Label("Charge Pressure", "3E_BUT_CHARGE", "3E_ButchChargePressure").c_str(), bosses.butch.chargePressureMultiplier);
        ImGui::TextDisabled(
            "%s", ImGuiLocalization::Text("Pot Frequency: reserved - stock pot behaviour is pickup-availability driven.", "3E_BUT_POT").c_str());
    }

    if (ImGui::CollapsingHeader("Grontar")) {
        Multiplier(ImGuiLocalization::Label("Close Attack Range", "3E_GRO_CLOSE", "3E_GrontarCloseRange").c_str(), bosses.grontar.closeAttackRangeMultiplier);
        Multiplier(ImGuiLocalization::Label("Far Attack Range", "3E_GRO_FAR", "3E_GrontarFarRange").c_str(), bosses.grontar.farAttackRangeMultiplier);
        Multiplier(ImGuiLocalization::Label("Throw Range", "3E_GRO_THROW", "3E_GrontarThrowRange").c_str(), bosses.grontar.throwRangeMultiplier);
        Multiplier(ImGuiLocalization::Label("Target Tracking", "3E_GRO_TRACK", "3E_GrontarTracking").c_str(), bosses.grontar.targetTrackingMultiplier);
        Multiplier(ImGuiLocalization::Label("Dive Roll Pressure", "3E_GRO_DIVE", "3E_GrontarDiveRoll").c_str(), bosses.grontar.diveRollPressureMultiplier);
    }

    if (ImGui::CollapsingHeader("Dante")) {
        ImGui::SliderInt(
            ImGuiLocalization::Label("Phase 2 Health % (-1 = Original)", "3E_DAN_P2", "3E_DantePhase2").c_str(),
            &bosses.dante.phase2HealthPercent,
            -1,
            100);

        ImGui::SliderInt(
            ImGuiLocalization::Label("Phase 3 Health % (-1 = Original)", "3E_DAN_P3", "3E_DantePhase3").c_str(),
            &bosses.dante.phase3HealthPercent,
            -1,
            100);

        Multiplier(ImGuiLocalization::Label("Missile Damage", "3E_DAN_MDAMAGE", "3E_DanteMissileDamage").c_str(), bosses.dante.missileDamageMultiplier);
        Multiplier(ImGuiLocalization::Label("Missile Radius", "3E_DAN_MRAD", "3E_DanteMissileRadius").c_str(), bosses.dante.missileRadiusMultiplier);
        Multiplier(ImGuiLocalization::Label("Missile Volley Rate", "3E_DAN_MRATE", "3E_DanteMissileRate").c_str(), bosses.dante.missileFrequencyMultiplier);
        Multiplier(ImGuiLocalization::Label("Missile Recovery Time", "3E_DAN_MREC", "3E_DanteMissileRecovery").c_str(), bosses.dante.missileRecoveryMultiplier);
        Multiplier(ImGuiLocalization::Label("Target Missile Trigger Time", "3E_DAN_TARGET", "3E_DanteTargetMissile").c_str(), bosses.dante.targetedMissileTimingMultiplier);
    }

    DrawBossMultiplierSet(
        "Paul",
        bosses.paul.closeAttackRangeMultiplier,
        ImGuiLocalization::Label("Close Attack Range", "3E_PAU_CLOSE", "3E_PaulCloseRange").c_str(),
        bosses.paul.danceTimeMultiplier,
        ImGuiLocalization::Label("Dance Time", "3E_PAU_DANCE", "3E_PaulDanceTime").c_str(),
        bosses.paul.recoveryTimeMultiplier,
        ImGuiLocalization::Label("Recovery Time", "3E_PAU_REC", "3E_PaulRecovery").c_str(),
        bosses.paul.aggressionMultiplier,
        ImGuiLocalization::Label("Aggression", "3E_PAU_AGGR", "3E_PaulAggression").c_str());

    if (ImGui::CollapsingHeader("Oscar")) {
        Multiplier(ImGuiLocalization::Label("Close Range", "3E_OSC_CLOSE", "3E_OscarCloseRange").c_str(), bosses.oscar.closeRangeMultiplier);
        Multiplier(ImGuiLocalization::Label("Mid Range", "3E_OSC_MID", "3E_OscarMidRange").c_str(), bosses.oscar.midRangeMultiplier);
        Multiplier(ImGuiLocalization::Label("Henchman Coordination", "3E_OSC_COORD", "3E_OscarCoordination").c_str(), bosses.oscar.henchmanCoordinationMultiplier);
        Multiplier(ImGuiLocalization::Label("Henchman Sync Delay", "3E_OSC_SYNC", "3E_OscarSyncDelay").c_str(), bosses.oscar.henchmanSyncDelayMultiplier);
        ImGui::TextDisabled(
            "%s", ImGuiLocalization::Text("Aggression: reserved - audited Oscar pressure path is deterministic.", "3E_OSC_NOTE").c_str());
    }
}

void DrawCameraTab(ThreeChanSettings& settings) {
    auto& c = settings.camera;

    ImGui::Checkbox(ImGuiLocalization::Label("Enable 3EChan Camera Override", "3E_CAM_ENABLE", "3E_CameraEnable").c_str(), &c.enabled);

    ImGui::TextDisabled(
        "%s",
        ImGuiLocalization::Format(
            "Runtime: %s",
            "3E_CAM_RUN",
            ThreeChanCamera::GetRuntimeStatus()).c_str());

    const std::string followMode =
        ImGuiLocalization::Text("Follow (Original)", "3E_CAM_FOLLOW");
    const std::string rigidMode =
        ImGuiLocalization::Text("Rigid (Original)", "3E_CAM_RIGID");
    const std::string fixedMode =
        ImGuiLocalization::Text("Fixed Frame (Experimental)", "3E_CAM_FIXED");
    const std::string thirdPersonMode =
        ImGuiLocalization::Text("Close Third Person (Experimental)", "3E_CAM_THIRD");

    const char* cameraModes[] = {
        followMode.c_str(),
        rigidMode.c_str(),
        fixedMode.c_str(),
        thirdPersonMode.c_str()
    };

    int mode = static_cast<int>(c.mode);

    if (ImGui::Combo(
        ImGuiLocalization::Label("Camera Mode", "3E_CAM_MODE", "3E_CameraMode").c_str(),
        &mode,
        cameraModes,
        4)) {

        c.mode = static_cast<ThreeChanCameraMode>(mode);
    }

    ImGui::Separator();

    switch (c.mode) {
        case ThreeChanCameraMode::FollowOriginal:
            ImGui::SeparatorText(ImGuiLocalization::Label("Original Follow", "3E_CAM_OFOLLOW", "3E_OriginalFollow").c_str());

            ImGui::SliderInt(ImGuiLocalization::Label("FOV", "3E_CAM_FOV", "3E_FollowFOV").c_str(), &c.followFov, 1, 20);
            Multiplier(
                ImGuiLocalization::Label("Movement Smoothing", "3E_CAM_MOVSM", "3E_FollowMoveSmooth").c_str(),
                c.followMovementSmoothingMultiplier,
                0.10f,
                4.0f);
            Multiplier(
                ImGuiLocalization::Label("Tracking Smoothing", "3E_CAM_TRKSM", "3E_FollowTrackSmooth").c_str(),
                c.followTrackingSmoothingMultiplier,
                0.10f,
                4.0f);
            break;

        case ThreeChanCameraMode::RigidOriginal:
            ImGui::SeparatorText(ImGuiLocalization::Label("Original Rigid", "3E_CAM_ORIGRIG", "3E_OriginalRigid").c_str());

            ImGui::SliderInt(ImGuiLocalization::Label("Distance", "3E_CAM_DIST", "3E_RigidDistance").c_str(), &c.rigidDistance, 300, 6000);
            ImGui::SliderInt(ImGuiLocalization::Label("Height", "3E_CAM_HEIGHT", "3E_RigidHeight").c_str(), &c.rigidHeight, -500, 4000);
            ImGui::SliderInt(ImGuiLocalization::Label("Spin Rate", "3E_CAM_SPIN", "3E_RigidSpin").c_str(), &c.rigidSpinRate, 0, 20000);
            ImGui::SliderInt(ImGuiLocalization::Label("FOV", "3E_CAM_FOV", "3E_RigidFOV").c_str(), &c.rigidFov, 1, 20);

            Multiplier(
                ImGuiLocalization::Label("Movement Smoothing", "3E_CAM_MOVSM", "3E_RigidMoveSmooth").c_str(),
                c.rigidMovementSmoothingMultiplier,
                0.10f,
                4.0f);

            Multiplier(
                ImGuiLocalization::Label("Tracking Smoothing", "3E_CAM_TRKSM", "3E_RigidTrackSmooth").c_str(),
                c.rigidTrackingSmoothingMultiplier,
                0.10f,
                4.0f);
            break;

        case ThreeChanCameraMode::FixedFrameExperimental:
            ImGui::SeparatorText(ImGuiLocalization::Label("Fixed Frame - Experimental", "3E_FIX_TITLE", "3E_FixedTitle").c_str());

            ImGui::SliderFloat(
                ImGuiLocalization::Label("Safe Frame X Margin", "3E_SAFE_X", "3E_FixedSafeX").c_str(),
                &c.fixedSafeMarginXPercent,
                0.0f,
                45.0f,
                "%.0f%%");

            ImGui::SliderFloat(
                ImGuiLocalization::Label("Safe Frame Y Margin", "3E_SAFE_Y", "3E_FixedSafeY").c_str(),
                &c.fixedSafeMarginYPercent,
                0.0f,
                45.0f,
                "%.0f%%");

            ImGui::Checkbox(
                ImGuiLocalization::Label("Smooth Camera Transition", "3E_SMOOTH_TR", "3E_FixedSmoothTransition").c_str(),
                &c.fixedSmoothTransition);

            if (c.fixedSmoothTransition) {
                ImGui::SliderFloat(
                    ImGuiLocalization::Label("Transition Time", "3E_TRANS_TIME", "3E_FixedTransitionTime").c_str(),
                    &c.fixedTransitionTime,
                    0.0f,
                    2.0f,
                    "%.2f sec");
            }

            ImGui::SliderFloat(
                ImGuiLocalization::Label("Minimum Camera Hold", "3E_MIN_HOLD", "3E_FixedMinHold").c_str(),
                &c.fixedMinimumHoldTime,
                0.0f,
                3.0f,
                "%.2f sec");

            ImGui::SliderFloat(
                ImGuiLocalization::Label("Reposition Cooldown", "3E_REPOS_CD", "3E_FixedRepositionCooldown").c_str(),
                &c.fixedRepositionCooldown,
                0.0f,
                3.0f,
                "%.2f sec");

            ImGui::SliderInt(
                ImGuiLocalization::Label("Target Height Offset", "3E_TGT_HEIGHT", "3E_FixedTargetHeight").c_str(),
                &c.fixedTargetHeightOffset,
                -1500,
                2500);

            ImGui::SliderInt(
                ImGuiLocalization::Label("FOV", "3E_CAM_FOV", "3E_FixedFOV").c_str(),
                &c.fixedFov,
                1,
                20);

            ImGui::Checkbox(
                ImGuiLocalization::Label("Use Original Camera Rails", "3E_USE_RAILS", "3E_FixedRails").c_str(),
                &c.fixedUseOriginalRails);

            ImGui::Checkbox(
                ImGuiLocalization::Label("Track Jackie While Camera Is Fixed", "3E_TRACK_JACKIE", "3E_FixedTrackJackie").c_str(),
                &c.fixedTrackJackie);

            ImGui::SeparatorText(ImGuiLocalization::Label("Debug", "IM_DEBUG", "3E_FixedDebug").c_str());

            ImGui::Checkbox(
                ImGuiLocalization::Label("Show Safe Frame", "3E_SHOW_SAFE", "3E_FixedShowSafe").c_str(),
                &c.fixedShowSafeFrame);

            ImGui::Checkbox(
                ImGuiLocalization::Label("Show Camera Anchor", "3E_SHOW_ANCHOR", "3E_FixedShowAnchor").c_str(),
                &c.fixedShowCameraAnchor);
            break;

        case ThreeChanCameraMode::CloseThirdPersonExperimental:
            ImGui::SeparatorText(ImGuiLocalization::Label("Close Third Person - Experimental", "3E_TP_TITLE", "3E_ThirdPersonTitle").c_str());

            ImGui::SliderInt(
                ImGuiLocalization::Label("Distance", "3E_TP_DIST", "3E_ThirdDistance").c_str(),
                &c.thirdPersonDistance,
                300,
                4000);

            ImGui::SliderInt(
                ImGuiLocalization::Label("Camera Height", "3E_TP_CHEIGHT", "3E_ThirdCameraHeight").c_str(),
                &c.thirdPersonHeight,
                -500,
                3000);

            ImGui::SliderInt(
                ImGuiLocalization::Label("Target Height", "3E_TP_THEIGHT", "3E_ThirdTargetHeight").c_str(),
                &c.thirdPersonTargetHeight,
                -500,
                2500);

            ImGui::SliderInt(
                ImGuiLocalization::Label("Shoulder Offset", "3E_TP_SHOULDER", "3E_ThirdShoulder").c_str(),
                &c.thirdPersonShoulderOffset,
                -1500,
                1500);

            ImGui::SliderInt(
                ImGuiLocalization::Label("FOV", "3E_CAM_FOV", "3E_ThirdFOV").c_str(),
                &c.thirdPersonFov,
                1,
                20);

            ImGui::SliderInt(
                ImGuiLocalization::Label("Minimum FOV", "3E_TP_MINFOV", "3E_ThirdMinFOV").c_str(),
                &c.thirdPersonMinFov,
                1,
                20);

            ImGui::SliderInt(
                ImGuiLocalization::Label("Maximum FOV", "3E_TP_MAXFOV", "3E_ThirdMaxFOV").c_str(),
                &c.thirdPersonMaxFov,
                1,
                25);

            ImGui::SliderFloat(
                ImGuiLocalization::Label("Position Lag", "3E_TP_POSLAG", "3E_ThirdPositionLag").c_str(),
                &c.thirdPersonPositionLag,
                0.0f,
                30.0f);

            ImGui::SliderFloat(
                ImGuiLocalization::Label("Rotation Lag", "3E_TP_ROTLAG", "3E_ThirdRotationLag").c_str(),
                &c.thirdPersonRotationLag,
                0.0f,
                30.0f);

            ImGui::SliderFloat(
                ImGuiLocalization::Label("Target Lag", "3E_TP_TGTLAG", "3E_ThirdTargetLag").c_str(),
                &c.thirdPersonTargetLag,
                0.0f,
                30.0f);

            ImGui::SliderFloat(
                ImGuiLocalization::Label("Auto Center Strength", "3E_TP_CENTERSTR", "3E_ThirdCenterStrength").c_str(),
                &c.thirdPersonAutoCenterStrength,
                0.0f,
                3.0f);

            ImGui::SliderFloat(
                ImGuiLocalization::Label("Auto Center Delay", "3E_TP_CENTERDEL", "3E_ThirdCenterDelay").c_str(),
                &c.thirdPersonAutoCenterDelay,
                0.0f,
                3.0f,
                "%.2f sec");

            const std::string facingDirection =
                ImGuiLocalization::Text("Jackie Facing Direction", "3E_TP_FACE");
            const std::string movementDirection =
                ImGuiLocalization::Text("Movement Direction", "3E_TP_MOVE");
            const std::string blendedDirection =
                ImGuiLocalization::Text("Blended", "3E_TP_BLEND");

            const char* directionModes[] = {
                facingDirection.c_str(),
                movementDirection.c_str(),
                blendedDirection.c_str()
            };

            ImGui::Combo(
                ImGuiLocalization::Label("Follow Direction", "3E_TP_FOLLOWDIR", "3E_ThirdFollowDirection").c_str(),
                &c.thirdPersonFollowDirection,
                directionModes,
                3);

            ImGui::SeparatorText(ImGuiLocalization::Label("Collision", "3E_COLLISION", "3E_ThirdCollisionSection").c_str());

            ImGui::Checkbox(
                ImGuiLocalization::Label("Camera Collision", "3E_TP_CAMCOL", "3E_ThirdCameraCollision").c_str(),
                &c.thirdPersonCollision);

            ImGui::SliderInt(
                ImGuiLocalization::Label("Collision Padding", "3E_TP_COLPAD", "3E_ThirdCollisionPadding").c_str(),
                &c.thirdPersonCollisionPadding,
                0,
                500);

            ImGui::SliderInt(
                ImGuiLocalization::Label("Minimum Camera Distance", "3E_TP_MINDIST", "3E_ThirdMinimumDistance").c_str(),
                &c.thirdPersonMinimumDistance,
                100,
                1500);

            ImGui::SeparatorText(ImGuiLocalization::Label("Behaviour", "3E_BEHAVIOUR", "3E_ThirdBehaviour").c_str());

            ImGui::Checkbox(
                ImGuiLocalization::Label("Smooth Sudden Turns", "3E_TP_SMOOTHT", "3E_ThirdSmoothTurns").c_str(),
                &c.thirdPersonSmoothTurns);

            ImGui::Checkbox(
                ImGuiLocalization::Label("Smooth Jumps / Falls", "3E_TP_SMOOTHJ", "3E_ThirdSmoothJumps").c_str(),
                &c.thirdPersonSmoothJumps);

            ImGui::Checkbox(
                ImGuiLocalization::Label("Auto Recover Behind Jackie", "3E_TP_RECOVER", "3E_ThirdAutoRecover").c_str(),
                &c.thirdPersonAutoRecoverBehindJackie);

            ImGui::SeparatorText(ImGuiLocalization::Label("Debug", "IM_DEBUG", "3E_ThirdDebug").c_str());

            ImGui::Checkbox(
                ImGuiLocalization::Label("Show Desired Camera", "3E_TP_SHOWDES", "3E_ThirdShowDesired").c_str(),
                &c.thirdPersonShowDesiredCamera);

            ImGui::Checkbox(
                ImGuiLocalization::Label("Show Collision Line", "3E_TP_SHOWCOL", "3E_ThirdShowCollision").c_str(),
                &c.thirdPersonShowCollisionLine);

            ImGui::Checkbox(
                ImGuiLocalization::Label("Show Camera Target", "3E_TP_SHOWTGT", "3E_ThirdShowTarget").c_str(),
                &c.thirdPersonShowTarget);
            break;
    }

    ImGui::Separator();
    ImGui::TextDisabled(
        "%s",
        ImGuiLocalization::Text(
            "Experimental camera modes will automatically suspend during Director/NIS cutscenes.",
            "3E_CAM_SUSP").c_str());
}

void DrawInspectorTab(ThreeChanSettings& settings) {
    (void)settings;

    ImGui::TextWrapped(
        "%s", ImGuiLocalization::Text("The existing 3D AI inspector remains the primary inspector.", "3E_INS_MAIN").c_str());

    ImGui::Separator();

    ImGui::TextDisabled("%s", ImGuiLocalization::Text("Selected Enemy Runtime - reserved", "3E_INS_SEL").c_str());
    ImGui::TextDisabled("%s", ImGuiLocalization::Text("Behaviour Runtime - reserved", "3E_INS_BEH").c_str());
    ImGui::TextDisabled("%s", ImGuiLocalization::Text("Combat Slot Runtime - reserved", "3E_INS_SLOT").c_str());
    ImGui::TextDisabled("%s", ImGuiLocalization::Text("Damage Runtime - reserved", "3E_INS_DMG").c_str());

    ImGui::TextWrapped(
        "%s", ImGuiLocalization::Text("These additional runtime overlays are not connected yet.", "3E_INS_NOTE").c_str());
}
void DrawTelemetryTab(ThreeChanSettings& settings) {
    (void)settings;

    ImGui::TextWrapped(
        "%s", ImGuiLocalization::Text("Extended telemetry views are reserved for a later runtime pass.", "3E_TEL_MAIN").c_str());

    ImGui::Separator();

    ImGui::TextDisabled("%s", ImGuiLocalization::Text("Enemy Counts - reserved", "3E_TEL_ENEMY").c_str());
    ImGui::TextDisabled("%s", ImGuiLocalization::Text("Combat Slots - reserved", "3E_TEL_SLOT").c_str());
    ImGui::TextDisabled("%s", ImGuiLocalization::Text("Attack Rate - reserved", "3E_TEL_ATK").c_str());
    ImGui::TextDisabled("%s", ImGuiLocalization::Text("Damage Rate - reserved", "3E_TEL_DMG").c_str());
    ImGui::TextDisabled("%s", ImGuiLocalization::Text("Selected AI - reserved", "3E_TEL_AI").c_str());
    ImGui::TextDisabled("%s", ImGuiLocalization::Text("Boss Runtime - reserved", "3E_TEL_BOSS").c_str());

    ImGui::Separator();

    ImGui::TextWrapped(
        "%s",
        ImGuiLocalization::Text(
            "Live Group Combat and Spawning statistics are already shown inside their respective tabs.",
            "3E_TEL_NOTE").c_str());
}

void DrawAdvancedTab(ThreeChanSettings& settings) {
    auto& a = settings.advanced;

    ImGui::TextWrapped(
        "%s",
        ImGuiLocalization::Text(
            "Runtime safety limits are always enforced by the active 3EChan subsystems.",
            "3E_ADV_SAFE").c_str());

    ImGui::SeparatorText(ImGuiLocalization::Label("Runtime Limits", "3E_ADV_LIMIT", "3E_AdvancedLimits").c_str());

    ImGui::SliderInt(
        ImGuiLocalization::Label("Intended Fighting Collision Capacity", "3E_ADV_FIGHT", "3E_AdvancedFightCapacity").c_str(),
        &a.intendedFightingCollisionCapacity,
        12,
        32);

    ImGui::SliderInt(
        ImGuiLocalization::Label("Hard Enemy Safety Cap", "3E_ADV_ENCAP", "3E_AdvancedEnemyCap").c_str(),
        &a.hardEnemySafetyCap,
        3,
        32);

    ImGui::SliderInt(
        ImGuiLocalization::Label("Hard Simultaneous Attack Cap", "3E_ADV_ATKCAP", "3E_AdvancedAttackCap").c_str(),
        &a.hardSimultaneousAttackSafetyCap,
        1,
        16);

    ImGui::Separator();

    ImGui::TextDisabled("%s", ImGuiLocalization::Text("Runtime change logging - reserved", "3E_ADV_LOG").c_str());

    ImGui::TextWrapped(
        "%s",
        ImGuiLocalization::Text(
            "Disabling a 3EChan subsystem restores its original runtime behaviour.",
            "3E_ADV_NOTE").c_str());
}
} // namespace

namespace ThreeChanUI {

void Draw(bool* open) {
    if (!open || !*open) {
        return;
    }

    ThreeChanSettings& settings = ThreeChanTuning::Get();

    ImGui::SetNextWindowSize(
        ImVec2(760.0f, 620.0f),
        ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("3EChan", open)) {
        ImGui::End();
        return;
    }

    ImGui::Text(
        "%s", ImGuiLocalization::Format("Profile: %s", "3E_PROFLABEL", ThreeChanTuning::GetActiveProfileName().c_str()).c_str());


    ImGui::SameLine();

    if (settings.masterEnabled) {
        ImGui::TextDisabled("%s", ImGuiLocalization::Text("| Overrides Active", "3E_OVRACT").c_str());
    }
    else {
        ImGui::TextDisabled("%s", ImGuiLocalization::Text("| Original Behaviour", "3E_ORIGBEH").c_str());
    }

    ImGui::Separator();

    if (ImGui::BeginTabBar("3EChanTabs")) {
        if (ImGui::BeginTabItem(ImGuiLocalization::Label("Profiles", "3E_PROFILES", "3E_TabProfiles").c_str())) {
            DrawProfilesTab(settings);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(ImGuiLocalization::Label("Enemies", "3E_ENEMIES", "3E_TabEnemies").c_str())) {
            DrawEnemiesTab(settings);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(ImGuiLocalization::Label("Group Combat", "3E_GROUP", "3E_TabGroupCombat").c_str())) {
            DrawGroupCombatTab(settings);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(ImGuiLocalization::Label("Spawning", "3E_SPAWN", "3E_TabSpawning").c_str())) {
            DrawSpawningTab(settings);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(ImGuiLocalization::Label("Bosses", "3E_BOSSES", "3E_TabBosses").c_str())) {
            DrawBossesTab(settings);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(ImGuiLocalization::Label("Camera", "IM_CAMERA", "3E_TabCamera").c_str())) {
            DrawCameraTab(settings);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(ImGuiLocalization::Label("Inspector", "3E_INSPECT", "3E_TabInspector").c_str())) {
            DrawInspectorTab(settings);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(ImGuiLocalization::Label("Telemetry", "3E_TELEM", "3E_TabTelemetry").c_str())) {
            DrawTelemetryTab(settings);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(ImGuiLocalization::Label("Advanced", "3E_ADV", "3E_TabAdvanced").c_str())) {
            DrawAdvancedTab(settings);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

} // namespace ThreeChanUI
