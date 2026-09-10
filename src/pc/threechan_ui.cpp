#include "pc/threechan_ui.h"

#include "extra/threechan_tuning.h"
#include "extra/threechan_camera.h"
#include "imgui.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

char g_profileNameBuffer[64] = "Test";

void Multiplier(
    const char* label,
    float& value,
    float minValue = 0.25f,
    float maxValue = 5.0f) {

    ImGui::SliderFloat(label, &value, minValue, maxValue, "%.2fx");
}

void DrawProfilesTab(ThreeChanSettings& settings) {
    ImGui::Checkbox("3EChan Overrides", &settings.masterEnabled);
    ImGui::Checkbox("Apply Changes Live", &settings.applyChangesLive);

    ImGui::Separator();

    const std::vector<std::string> profiles =
        ThreeChanTuning::ListProfiles();

    const std::string& active =
        ThreeChanTuning::GetActiveProfileName();

    if (ImGui::BeginCombo("Active Profile", active.c_str())) {
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
        "Profile Name",
        g_profileNameBuffer,
        sizeof(g_profileNameBuffer));

    if (ImGui::Button("Save / Save As")) {
        ThreeChanTuning::SaveProfile(g_profileNameBuffer);
    }

    ImGui::SameLine();

    if (ImGui::Button("Reload")) {
        ThreeChanTuning::LoadProfile(
            ThreeChanTuning::GetActiveProfileName().c_str());
    }

    ImGui::SameLine();

    if (ImGui::Button("Restore Original")) {
        ThreeChanTuning::ResetToOriginal();

        std::snprintf(
            g_profileNameBuffer,
            sizeof(g_profileNameBuffer),
            "Test");
    }

    ImGui::Separator();
    ImGui::TextWrapped("%s", ThreeChanTuning::GetLastMessage());

    ImGui::SeparatorText("Purpose");
    ImGui::TextWrapped(
        "Profiles are stored outside the executable so gameplay and camera "
        "tuning can be changed without rebuilding 3EChan.");
}

void DrawEnemiesTab(ThreeChanSettings& settings) {
    auto& e = settings.enemies;

    ImGui::Checkbox("Enable Regular Enemy Tuning", &e.enabled);

    ImGui::SeparatorText("Survivability / Damage");
    Multiplier("Health", e.healthMultiplier);
    Multiplier("Damage", e.damageMultiplier);

    ImGui::SeparatorText("Decision Making");
    Multiplier("Attack Frequency", e.attackFrequencyMultiplier);
    Multiplier("Aggression", e.aggressionMultiplier);
    Multiplier("Distancing", e.distancingMultiplier);
    Multiplier("Circling", e.circlingMultiplier);
    Multiplier("Decision Speed", e.decisionSpeedMultiplier);

    ImGui::Checkbox("Use Custom Think Range", &e.useCustomThinkRange);

    if (e.useCustomThinkRange) {
        ImGui::SliderInt("Minimum Think Frames", &e.minThinkFrames, 0, 120);
        ImGui::SliderInt("Maximum Think Frames", &e.maxThinkFrames, 0, 120);

        if (e.maxThinkFrames < e.minThinkFrames) {
            e.maxThinkFrames = e.minThinkFrames;
        }
    }

    ImGui::SeparatorText("Movement");
    Multiplier("Running Speed", e.runningSpeedMultiplier);
    Multiplier("Strafing Speed", e.strafingSpeedMultiplier);
    Multiplier("Turn Speed", e.turnSpeedMultiplier);
    Multiplier("Attack Animation Speed", e.attackAnimationSpeedMultiplier);

    ImGui::SeparatorText("Attack Selection");
    Multiplier("Punch Chance", e.punchChanceMultiplier, 0.0f, 3.0f);
    Multiplier("Kick Chance", e.kickChanceMultiplier, 0.0f, 3.0f);
    Multiplier("Throw Chance", e.throwChanceMultiplier, 0.0f, 3.0f);
    Multiplier("Combo Chance", e.comboChanceMultiplier, 0.0f, 3.0f);

    ImGui::SeparatorText("Reactions / Recovery");
    Multiplier("Stun Duration", e.stunDurationMultiplier);
    Multiplier("Knockdown Recovery", e.knockdownRecoveryMultiplier);
    Multiplier("Get-Up Speed", e.getUpSpeedMultiplier);
}

void DrawGroupCombatTab(ThreeChanSettings& settings) {
    auto& g = settings.groupCombat;

    ImGui::Checkbox("Enable Extended Group Combat", &g.enabled);

    ImGui::TextDisabled("0 keeps the original game value.");

    ImGui::SliderInt(
        "Max Engaged Enemies",
        &g.maxEngagedEnemies,
        0,
        settings.advanced.hardEnemySafetyCap);

    ImGui::SliderInt(
        "Max Simultaneous Attacks",
        &g.maxSimultaneousAttacks,
        0,
        settings.advanced.hardSimultaneousAttackSafetyCap);

    ImGui::SliderInt(
        "Minimum Gap Between Attacks (ms)",
        &g.minimumAttackGapMs,
        0,
        3000);

    ImGui::Checkbox("Attacker Rotation", &g.attackerRotation);
    ImGui::Checkbox("Allow Chain Pressure", &g.chainPressure);
    ImGui::Checkbox("Immediate Replacement", &g.immediateReplacement);

    ImGui::Separator();
    ImGui::TextWrapped(
        "The original ActiveZone system is limited to three overlord members. "
        "The integration stage will keep the original layout intact and add a "
        "3EChan sidecar coordinator for expanded encounters.");
}

void DrawSpawningTab(ThreeChanSettings& settings) {
    auto& s = settings.spawning;

    ImGui::Checkbox("Enable Spawn Tuning", &s.enabled);

    Multiplier("Enemy Count", s.enemyCountMultiplier, 0.25f, 4.0f);
    Multiplier("Respawn Delay", s.respawnDelayMultiplier, 0.10f, 5.0f);

    ImGui::TextDisabled("0 keeps the original level value.");

    ImGui::SliderInt(
        "Spawn Burst Override",
        &s.spawnBurstOverride,
        0,
        16);

    ImGui::SliderInt(
        "Maximum Alive Override",
        &s.maxAliveOverride,
        0,
        settings.advanced.hardEnemySafetyCap);

    ImGui::SliderInt(
        "Active Zone Threshold Offset",
        &s.activeZoneThresholdOffset,
        -8,
        8);

    ImGui::Checkbox("Pause Enemy Generators", &s.pauseGenerators);
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
        "Enable Boss Tuning",
        &bosses.global.enabled);

    ImGui::SeparatorText("Global Boss Multipliers");

    Multiplier("Boss Health", bosses.global.healthMultiplier);
    Multiplier("Boss Damage", bosses.global.damageMultiplier);
    Multiplier("Boss Movement Speed", bosses.global.movementSpeedMultiplier);
    Multiplier("Boss Decision Speed", bosses.global.decisionSpeedMultiplier);
    Multiplier("Boss Attack Speed", bosses.global.attackSpeedMultiplier);
    Multiplier("Boss Recovery Speed", bosses.global.recoverySpeedMultiplier);

    ImGui::SeparatorText("Per-Boss Advanced Tuning");

    if (ImGui::CollapsingHeader("Butch")) {
        Multiplier("Stomp Damage##Butch", bosses.butch.stompDamageMultiplier);
        Multiplier("Attack Distance##Butch", bosses.butch.attackDistanceMultiplier);
        Multiplier("Stomp Frequency##Butch", bosses.butch.stompFrequencyMultiplier);
        Multiplier("Charge Pressure##Butch", bosses.butch.chargePressureMultiplier);
        Multiplier("Pot Frequency##Butch", bosses.butch.potFrequencyMultiplier);
    }

    if (ImGui::CollapsingHeader("Grontar")) {
        Multiplier("Close Attack Range##Grontar", bosses.grontar.closeAttackRangeMultiplier);
        Multiplier("Far Attack Range##Grontar", bosses.grontar.farAttackRangeMultiplier);
        Multiplier("Throw Range##Grontar", bosses.grontar.throwRangeMultiplier);
        Multiplier("Target Tracking##Grontar", bosses.grontar.targetTrackingMultiplier);
        Multiplier("Dive Roll Pressure##Grontar", bosses.grontar.diveRollPressureMultiplier);
    }

    if (ImGui::CollapsingHeader("Dante")) {
        ImGui::SliderInt(
            "Phase 2 Health % (-1 = Original)",
            &bosses.dante.phase2HealthPercent,
            -1,
            100);

        ImGui::SliderInt(
            "Phase 3 Health % (-1 = Original)",
            &bosses.dante.phase3HealthPercent,
            -1,
            100);

        Multiplier("Missile Damage##Dante", bosses.dante.missileDamageMultiplier);
        Multiplier("Missile Radius##Dante", bosses.dante.missileRadiusMultiplier);
        Multiplier("Missile Frequency##Dante", bosses.dante.missileFrequencyMultiplier);
        Multiplier("Missile Recovery##Dante", bosses.dante.missileRecoveryMultiplier);
        Multiplier("Target Missile Timing##Dante", bosses.dante.targetedMissileTimingMultiplier);
    }

    DrawBossMultiplierSet(
        "Paul",
        bosses.paul.closeAttackRangeMultiplier,
        "Close Attack Range##Paul",
        bosses.paul.danceTimeMultiplier,
        "Dance Time##Paul",
        bosses.paul.recoveryTimeMultiplier,
        "Recovery Time##Paul",
        bosses.paul.aggressionMultiplier,
        "Aggression##Paul");

    if (ImGui::CollapsingHeader("Oscar")) {
        Multiplier("Close Range##Oscar", bosses.oscar.closeRangeMultiplier);
        Multiplier("Mid Range##Oscar", bosses.oscar.midRangeMultiplier);
        Multiplier("Henchman Coordination##Oscar", bosses.oscar.henchmanCoordinationMultiplier);
        Multiplier("Henchman Sync Delay##Oscar", bosses.oscar.henchmanSyncDelayMultiplier);
        Multiplier("Aggression##Oscar", bosses.oscar.aggressionMultiplier);
    }
}

void DrawCameraTab(ThreeChanSettings& settings) {
    auto& c = settings.camera;

    ImGui::Checkbox("Enable 3EChan Camera Override", &c.enabled);

    ImGui::TextDisabled(
        "Runtime: %s",
        ThreeChanCamera::GetRuntimeStatus());

    static const char* cameraModes[] = {
        "Follow (Original)",
        "Rigid (Original)",
        "Fixed Frame (Experimental)",
        "Close Third Person (Experimental)"
    };

    int mode = static_cast<int>(c.mode);

    if (ImGui::Combo(
        "Camera Mode",
        &mode,
        cameraModes,
        4)) {

        c.mode = static_cast<ThreeChanCameraMode>(mode);
    }

    ImGui::Separator();

    switch (c.mode) {
        case ThreeChanCameraMode::FollowOriginal:
            ImGui::SeparatorText("Original Follow");

            ImGui::SliderInt("FOV##Follow", &c.followFov, 1, 20);
            Multiplier(
                "Movement Smoothing##Follow",
                c.followMovementSmoothingMultiplier,
                0.10f,
                4.0f);
            Multiplier(
                "Tracking Smoothing##Follow",
                c.followTrackingSmoothingMultiplier,
                0.10f,
                4.0f);
            break;

        case ThreeChanCameraMode::RigidOriginal:
            ImGui::SeparatorText("Original Rigid");

            ImGui::SliderInt("Distance##Rigid", &c.rigidDistance, 300, 6000);
            ImGui::SliderInt("Height##Rigid", &c.rigidHeight, -500, 4000);
            ImGui::SliderInt("Spin Rate##Rigid", &c.rigidSpinRate, 0, 20000);
            ImGui::SliderInt("FOV##Rigid", &c.rigidFov, 1, 20);

            Multiplier(
                "Movement Smoothing##Rigid",
                c.rigidMovementSmoothingMultiplier,
                0.10f,
                4.0f);

            Multiplier(
                "Tracking Smoothing##Rigid",
                c.rigidTrackingSmoothingMultiplier,
                0.10f,
                4.0f);
            break;

        case ThreeChanCameraMode::FixedFrameExperimental:
            ImGui::SeparatorText("Fixed Frame - Experimental");

            ImGui::SliderFloat(
                "Safe Frame X Margin",
                &c.fixedSafeMarginXPercent,
                0.0f,
                45.0f,
                "%.0f%%");

            ImGui::SliderFloat(
                "Safe Frame Y Margin",
                &c.fixedSafeMarginYPercent,
                0.0f,
                45.0f,
                "%.0f%%");

            ImGui::Checkbox(
                "Smooth Camera Transition",
                &c.fixedSmoothTransition);

            if (c.fixedSmoothTransition) {
                ImGui::SliderFloat(
                    "Transition Time",
                    &c.fixedTransitionTime,
                    0.0f,
                    2.0f,
                    "%.2f sec");
            }

            ImGui::SliderFloat(
                "Minimum Camera Hold",
                &c.fixedMinimumHoldTime,
                0.0f,
                3.0f,
                "%.2f sec");

            ImGui::SliderFloat(
                "Reposition Cooldown",
                &c.fixedRepositionCooldown,
                0.0f,
                3.0f,
                "%.2f sec");

            ImGui::SliderInt(
                "Target Height Offset##Fixed",
                &c.fixedTargetHeightOffset,
                -1500,
                2500);

            ImGui::SliderInt(
                "FOV##Fixed",
                &c.fixedFov,
                1,
                20);

            ImGui::Checkbox(
                "Use Original Camera Rails",
                &c.fixedUseOriginalRails);

            ImGui::Checkbox(
                "Track Jackie While Camera Is Fixed",
                &c.fixedTrackJackie);

            ImGui::SeparatorText("Debug");

            ImGui::Checkbox(
                "Show Safe Frame",
                &c.fixedShowSafeFrame);

            ImGui::Checkbox(
                "Show Camera Anchor",
                &c.fixedShowCameraAnchor);
            break;

        case ThreeChanCameraMode::CloseThirdPersonExperimental:
            ImGui::SeparatorText("Close Third Person - Experimental");

            ImGui::SliderInt(
                "Distance##ThirdPerson",
                &c.thirdPersonDistance,
                300,
                4000);

            ImGui::SliderInt(
                "Camera Height##ThirdPerson",
                &c.thirdPersonHeight,
                -500,
                3000);

            ImGui::SliderInt(
                "Target Height##ThirdPerson",
                &c.thirdPersonTargetHeight,
                -500,
                2500);

            ImGui::SliderInt(
                "Shoulder Offset",
                &c.thirdPersonShoulderOffset,
                -1500,
                1500);

            ImGui::SliderInt(
                "FOV##ThirdPerson",
                &c.thirdPersonFov,
                1,
                20);

            ImGui::SliderInt(
                "Minimum FOV",
                &c.thirdPersonMinFov,
                1,
                20);

            ImGui::SliderInt(
                "Maximum FOV",
                &c.thirdPersonMaxFov,
                1,
                25);

            ImGui::SliderFloat(
                "Position Lag",
                &c.thirdPersonPositionLag,
                0.0f,
                30.0f);

            ImGui::SliderFloat(
                "Rotation Lag",
                &c.thirdPersonRotationLag,
                0.0f,
                30.0f);

            ImGui::SliderFloat(
                "Target Lag",
                &c.thirdPersonTargetLag,
                0.0f,
                30.0f);

            ImGui::SliderFloat(
                "Auto Center Strength",
                &c.thirdPersonAutoCenterStrength,
                0.0f,
                3.0f);

            ImGui::SliderFloat(
                "Auto Center Delay",
                &c.thirdPersonAutoCenterDelay,
                0.0f,
                3.0f,
                "%.2f sec");

            static const char* directionModes[] = {
                "Jackie Facing Direction",
                "Movement Direction",
                "Blended"
            };

            ImGui::Combo(
                "Follow Direction",
                &c.thirdPersonFollowDirection,
                directionModes,
                3);

            ImGui::SeparatorText("Collision");

            ImGui::Checkbox(
                "Camera Collision",
                &c.thirdPersonCollision);

            ImGui::SliderInt(
                "Collision Padding",
                &c.thirdPersonCollisionPadding,
                0,
                500);

            ImGui::SliderInt(
                "Minimum Camera Distance",
                &c.thirdPersonMinimumDistance,
                100,
                1500);

            ImGui::SeparatorText("Behaviour");

            ImGui::Checkbox(
                "Smooth Sudden Turns",
                &c.thirdPersonSmoothTurns);

            ImGui::Checkbox(
                "Smooth Jumps / Falls",
                &c.thirdPersonSmoothJumps);

            ImGui::Checkbox(
                "Auto Recover Behind Jackie",
                &c.thirdPersonAutoRecoverBehindJackie);

            ImGui::SeparatorText("Debug");

            ImGui::Checkbox(
                "Show Desired Camera",
                &c.thirdPersonShowDesiredCamera);

            ImGui::Checkbox(
                "Show Collision Line",
                &c.thirdPersonShowCollisionLine);

            ImGui::Checkbox(
                "Show Camera Target",
                &c.thirdPersonShowTarget);
            break;
    }

    ImGui::Separator();
    ImGui::TextDisabled(
        "Experimental camera modes will automatically suspend during "
        "Director/NIS cutscenes.");
}

void DrawInspectorTab(ThreeChanSettings& settings) {
    ImGui::TextWrapped(
        "This tab will extend the existing 3D AI inspector instead of "
        "creating a duplicate inspector.");

    ImGui::Checkbox(
        "Selected Enemy Runtime",
        &settings.inspector.showSelectedEnemyRuntime);

    ImGui::Checkbox(
        "Behaviour Runtime",
        &settings.inspector.showBehaviourRuntime);

    ImGui::Checkbox(
        "Combat Slot Runtime",
        &settings.inspector.showCombatSlotRuntime);

    ImGui::Checkbox(
        "Damage Runtime",
        &settings.inspector.showDamageRuntime);
}

void DrawTelemetryTab(ThreeChanSettings& settings) {
    auto& t = settings.telemetry;

    ImGui::Checkbox("Enable Telemetry", &t.enabled);

    ImGui::Checkbox("Enemy Counts", &t.showEnemyCounts);
    ImGui::Checkbox("Combat Slots", &t.showCombatSlots);
    ImGui::Checkbox("Attack Rate", &t.showAttackRate);
    ImGui::Checkbox("Damage Rate", &t.showDamageRate);
    ImGui::Checkbox("Selected AI", &t.showSelectedAI);
    ImGui::Checkbox("Boss Runtime", &t.showBossRuntime);

    ImGui::Separator();
    ImGui::TextDisabled(
        "Runtime values will be connected during combat integration.");
}

void DrawAdvancedTab(ThreeChanSettings& settings) {
    auto& a = settings.advanced;

    ImGui::Checkbox("Safe Mode", &a.safeMode);

    ImGui::SliderInt(
        "Intended Fighting Collision Capacity",
        &a.intendedFightingCollisionCapacity,
        12,
        64);

    ImGui::SliderInt(
        "Hard Enemy Safety Cap",
        &a.hardEnemySafetyCap,
        3,
        64);

    ImGui::SliderInt(
        "Hard Simultaneous Attack Cap",
        &a.hardSimultaneousAttackSafetyCap,
        1,
        16);

    ImGui::Checkbox(
        "Log Runtime Changes",
        &a.logRuntimeChanges);

    ImGui::Separator();
    ImGui::TextWrapped(
        "Safe Mode will clamp experimental values and preserve the original "
        "behaviour whenever a 3EChan subsystem is disabled.");
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
        "Profile: %s",
        ThreeChanTuning::GetActiveProfileName().c_str());

    ImGui::SameLine();

    if (settings.masterEnabled) {
        ImGui::TextDisabled("| Overrides Active");
    }
    else {
        ImGui::TextDisabled("| Original Behaviour");
    }

    ImGui::Separator();

    if (ImGui::BeginTabBar("3EChanTabs")) {
        if (ImGui::BeginTabItem("Profiles")) {
            DrawProfilesTab(settings);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Enemies")) {
            DrawEnemiesTab(settings);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Group Combat")) {
            DrawGroupCombatTab(settings);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Spawning")) {
            DrawSpawningTab(settings);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Bosses")) {
            DrawBossesTab(settings);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Camera")) {
            DrawCameraTab(settings);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Inspector")) {
            DrawInspectorTab(settings);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Telemetry")) {
            DrawTelemetryTab(settings);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Advanced")) {
            DrawAdvancedTab(settings);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

} // namespace ThreeChanUI