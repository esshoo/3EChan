#include "extra/threechan_camera.h"

#include "extra/threechan_tuning.h"

#include "ai/player.h"

#include "gen/camera.h"
#include "gen/colsect.h"
#include "gen/director.h"
#include "gen/display.h"
#include "gen/game.h"
#include "gen/time.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

struct ThreeChanCameraRuntimeState {
    bool overrideWasActive = false;
    bool savedOriginalMode = false;
    CameraMode originalMode = CAM_MODE_FOLLOW;

    int lastConfiguredMode = -1;

    bool fixedInitialized = false;
    bool fixedTransitionActive = false;
    bool fixedFallbackOffsetValid = false;

    LVector fixedEye = {};
    LVector fixedTarget = {};

    LVector fixedTransitionStartEye = {};
    LVector fixedTransitionStartTarget = {};

    LVector fixedTransitionEndEye = {};
    LVector fixedTransitionEndTarget = {};

    LVector fixedFallbackOffset = {};

    f64 fixedTransitionStartTime = 0.0;
    f64 fixedLastRepositionTime = -1000.0;

    bool thirdPersonInitialized = false;

    LVector thirdPersonEye = {};
    LVector thirdPersonTarget = {};

    s32 thirdPersonYaw = 0;

    f64 thirdPersonLastMovementTime = -1000.0;

    bool thirdPersonCollisionLastFrame = false;

    char status[128] = "Original camera behaviour";
};

ThreeChanCameraRuntimeState g_runtime;

static constexpr f64 PI_D = 3.14159265358979323846;
static constexpr f64 THREECHAN_ANGLE_TO_RAD =
    (2.0 * PI_D) / 65536.0;
static constexpr f64 THREECHAN_RAD_TO_ANGLE =
    65536.0 / (2.0 * PI_D);

void SetStatus(const char* text) {
    std::snprintf(
        g_runtime.status,
        sizeof(g_runtime.status),
        "%s",
        text ? text : "");
}

s32 ClampAngle16(s32 angle) {
    return angle & 0xFFFF;
}

s32 AngleDelta16(s32 target, s32 current) {
    s32 delta =
        (ClampAngle16(target) - ClampAngle16(current)) & 0xFFFF;

    if (delta > 32767) {
        delta -= 65536;
    }

    return delta;
}

f32 GetRuntimeDeltaTime() {
    if (!g_time) {
        return 1.0f / 30.0f;
    }

    return std::clamp(
        g_time->GetDeltaTime(),
        0.0f,
        0.1f);
}

f32 ResponseAlpha(f32 response, f32 dt) {
    if (response <= 0.0f) {
        return 1.0f;
    }

    return 1.0f - std::exp(-response * dt);
}

s32 LerpS32(s32 a, s32 b, f32 alpha) {
    return a + static_cast<s32>(
        static_cast<f32>(b - a) * alpha);
}

LVector LerpVector(
    const LVector& a,
    const LVector& b,
    f32 alpha) {

    return {
        LerpS32(a.x, b.x, alpha),
        LerpS32(a.y, b.y, alpha),
        LerpS32(a.z, b.z, alpha)
    };
}

f32 SmoothStep01(f32 value) {
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

bool OverridesEnabled() {
    const ThreeChanSettings& settings =
        ThreeChanTuning::GetConst();

    return settings.masterEnabled
        && settings.camera.enabled;
}

bool IsCameraSuspended(const Camera& camera) {
    if (!g_game) {
        return true;
    }

    if (g_game->GetState() != GameState::Play) {
        return true;
    }

    if (g_directorActive != 0) {
        return true;
    }

    if (g_director && g_director->scriptState != 0) {
        return true;
    }

    if (camera.HasActiveCameraAnim()) {
        return true;
    }

    if (!Player::s_player) {
        return true;
    }

    return false;
}

void ResetExperimentalState() {
    g_runtime.fixedInitialized = false;
    g_runtime.fixedTransitionActive = false;
    g_runtime.fixedFallbackOffsetValid = false;

    g_runtime.thirdPersonInitialized = false;
    g_runtime.thirdPersonCollisionLastFrame = false;
}

void HandleConfiguredModeChange(int mode) {
    if (g_runtime.lastConfiguredMode == mode) {
        return;
    }

    g_runtime.lastConfiguredMode = mode;
    ResetExperimentalState();
}

void SetCameraTiming(
    Camera& camera,
    const LVector& baseMovement,
    const LVector& baseTracking,
    f32 movementMultiplier,
    f32 trackingMultiplier) {

    movementMultiplier =
        std::clamp(movementMultiplier, 0.10f, 4.0f);

    trackingMultiplier =
        std::clamp(trackingMultiplier, 0.10f, 4.0f);

    LVector movement = {
        std::max(
            1,
            static_cast<s32>(
                std::lround(
                    static_cast<f64>(baseMovement.x)
                    * movementMultiplier))),
        std::max(
            1,
            static_cast<s32>(
                std::lround(
                    static_cast<f64>(baseMovement.y)
                    * movementMultiplier))),
        std::max(
            1,
            static_cast<s32>(
                std::lround(
                    static_cast<f64>(baseMovement.z)
                    * movementMultiplier)))
    };

    LVector tracking = {
        std::max(
            1,
            static_cast<s32>(
                std::lround(
                    static_cast<f64>(baseTracking.x)
                    * trackingMultiplier))),
        std::max(
            1,
            static_cast<s32>(
                std::lround(
                    static_cast<f64>(baseTracking.y)
                    * trackingMultiplier))),
        std::max(
            1,
            static_cast<s32>(
                std::lround(
                    static_cast<f64>(baseTracking.z)
                    * trackingMultiplier)))
    };

    camera.SetMovementTime(&movement);
    camera.SetTrackingTime(&tracking);
}

LVector PlayerFocusPoint(s32 heightOffset) {
    Player* player = Player::s_player;

    if (!player) {
        return {};
    }

    LVector result = player->pos;
    result.y += heightOffset;

    return result;
}

bool IsPlayerInsideFixedSafeFrame(
    Camera& camera,
    const ThreeChanCameraTuning& cfg) {

    if (!Player::s_player || !g_display) {
        return false;
    }

    LVector focus =
        PlayerFocusPoint(cfg.fixedTargetHeightOffset);

    f32 x = 0.0f;
    f32 y = 0.0f;

    if (!camera.WorldToScreen(
            focus,
            &x,
            &y,
            nullptr)) {
        return false;
    }

    const f32 width =
        static_cast<f32>(
            g_display->GetScreenWidth());

    const f32 height =
        static_cast<f32>(
            g_display->GetScreenHeight());

    if (width <= 1.0f || height <= 1.0f) {
        return false;
    }

    const f32 marginX =
        width
        * std::clamp(
            cfg.fixedSafeMarginXPercent,
            0.0f,
            49.0f)
        * 0.01f;

    const f32 marginY =
        height
        * std::clamp(
            cfg.fixedSafeMarginYPercent,
            0.0f,
            49.0f)
        * 0.01f;

    return
        x >= marginX
        && x <= (width - marginX)
        && y >= marginY
        && y <= (height - marginY);
}

bool CaptureFixedDestination(
    Camera& camera,
    const ThreeChanCameraTuning& cfg,
    LVector& outEye,
    LVector& outTarget) {

    LVector focus =
        PlayerFocusPoint(cfg.fixedTargetHeightOffset);

    if (cfg.fixedUseOriginalRails) {
        s32 solverFov = cfg.fixedFov;

        if (camera.ThreeChanComputeFollowSolverPose(
                outEye,
                outTarget,
                solverFov)) {

            outTarget.y +=
                cfg.fixedTargetHeightOffset;

            return true;
        }
    }

    if (!g_runtime.fixedFallbackOffsetValid) {
        const LVector& current =
            camera.GetPosition();

        g_runtime.fixedFallbackOffset = {
            current.x - focus.x,
            current.y - focus.y,
            current.z - focus.z
        };

        g_runtime.fixedFallbackOffsetValid = true;
    }

    outEye = {
        focus.x + g_runtime.fixedFallbackOffset.x,
        focus.y + g_runtime.fixedFallbackOffset.y,
        focus.z + g_runtime.fixedFallbackOffset.z
    };

    outTarget = focus;

    return true;
}

bool HandleFixedFrame(
    Camera& camera,
    const ThreeChanCameraTuning& cfg) {

    if (!Player::s_player) {
        return false;
    }

    const f64 now =
        Time::GetTimeInSeconds();

    bool snapPresentation = false;

    if (!g_runtime.fixedInitialized) {
        LVector eye = {};
        LVector target = {};

        if (!CaptureFixedDestination(
                camera,
                cfg,
                eye,
                target)) {
            return false;
        }

        g_runtime.fixedEye = eye;
        g_runtime.fixedTarget = target;
        g_runtime.fixedTransitionEndEye = eye;
        g_runtime.fixedTransitionEndTarget = target;

        g_runtime.fixedInitialized = true;
        g_runtime.fixedTransitionActive = false;
        g_runtime.fixedLastRepositionTime = now;

        snapPresentation = true;
    }

    const f64 minimumWait =
        std::max(
            static_cast<f64>(
                cfg.fixedMinimumHoldTime),
            static_cast<f64>(
                cfg.fixedRepositionCooldown));

    const bool canReposition =
        (now - g_runtime.fixedLastRepositionTime)
        >= minimumWait;

    if (!g_runtime.fixedTransitionActive
        && canReposition
        && !IsPlayerInsideFixedSafeFrame(
            camera,
            cfg)) {

        LVector destinationEye = {};
        LVector destinationTarget = {};

        if (CaptureFixedDestination(
                camera,
                cfg,
                destinationEye,
                destinationTarget)) {

            if (cfg.fixedSmoothTransition
                && cfg.fixedTransitionTime > 0.001f) {

                g_runtime.fixedTransitionStartEye =
                    g_runtime.fixedEye;

                g_runtime.fixedTransitionStartTarget =
                    g_runtime.fixedTarget;

                g_runtime.fixedTransitionEndEye =
                    destinationEye;

                g_runtime.fixedTransitionEndTarget =
                    destinationTarget;

                g_runtime.fixedTransitionStartTime =
                    now;

                g_runtime.fixedTransitionActive =
                    true;
            }
            else {
                g_runtime.fixedEye =
                    destinationEye;

                g_runtime.fixedTarget =
                    destinationTarget;

                snapPresentation = true;
            }

            g_runtime.fixedLastRepositionTime =
                now;
        }
    }

    if (g_runtime.fixedTransitionActive) {
        const f64 duration =
            std::max(
                0.001,
                static_cast<f64>(
                    cfg.fixedTransitionTime));

        f32 t =
            static_cast<f32>(
                (now - g_runtime.fixedTransitionStartTime)
                / duration);

        if (t >= 1.0f) {
            g_runtime.fixedEye =
                g_runtime.fixedTransitionEndEye;

            g_runtime.fixedTarget =
                g_runtime.fixedTransitionEndTarget;

            g_runtime.fixedTransitionActive =
                false;
        }
        else {
            t = SmoothStep01(t);

            g_runtime.fixedEye =
                LerpVector(
                    g_runtime.fixedTransitionStartEye,
                    g_runtime.fixedTransitionEndEye,
                    t);

            g_runtime.fixedTarget =
                LerpVector(
                    g_runtime.fixedTransitionStartTarget,
                    g_runtime.fixedTransitionEndTarget,
                    t);
        }
    }

    LVector lookTarget =
        g_runtime.fixedTarget;

    if (cfg.fixedTrackJackie) {
        lookTarget =
            PlayerFocusPoint(
                cfg.fixedTargetHeightOffset);
    }

    camera.ThreeChanApplyExternalPose(
        g_runtime.fixedEye,
        lookTarget,
        cfg.fixedFov,
        snapPresentation);

    SetStatus(
        g_runtime.fixedTransitionActive
            ? "Fixed Frame: moving to next camera"
            : "Fixed Frame: camera locked");

    return true;
}

bool ResolveMovementYaw(
    const Player& player,
    s32& outYaw) {

    const f64 vx =
        static_cast<f64>(
            player.velocity.x);

    const f64 vz =
        static_cast<f64>(
            player.velocity.z);

    const f64 speedSquared =
        vx * vx + vz * vz;

    if (speedSquared < 4.0) {
        return false;
    }

    const f64 radians =
        std::atan2(vx, vz);

    outYaw =
        ClampAngle16(
            static_cast<s32>(
                std::lround(
                    radians * THREECHAN_RAD_TO_ANGLE)));

    return true;
}

s32 ResolveThirdPersonDesiredYaw(
    Player& player,
    const ThreeChanCameraTuning& cfg,
    f64 now,
    bool& outMoving) {

    const s32 facingYaw =
        ClampAngle16(
            player.orientation.y);

    s32 movementYaw = facingYaw;

    outMoving =
        ResolveMovementYaw(
            player,
            movementYaw);

    if (outMoving) {
        g_runtime.thirdPersonLastMovementTime =
            now;
    }

    if (cfg.thirdPersonFollowDirection == 0) {
        return facingYaw;
    }

    if (outMoving) {
        if (cfg.thirdPersonFollowDirection == 1) {
            return movementYaw;
        }

        const s32 delta =
            AngleDelta16(
                movementYaw,
                facingYaw);

        return ClampAngle16(
            facingYaw + delta / 2);
    }

    const f64 sinceMovement =
        now
        - g_runtime.thirdPersonLastMovementTime;

    if (sinceMovement
        < static_cast<f64>(
            cfg.thirdPersonAutoCenterDelay)) {

        return g_runtime.thirdPersonYaw;
    }

    return facingYaw;
}

LVector BuildThirdPersonDesiredEye(
    const Player& player,
    const ThreeChanCameraTuning& cfg,
    s32 yaw) {

    const f64 radians =
        static_cast<f64>(yaw)
        * THREECHAN_ANGLE_TO_RAD;

    const f64 forwardX =
        std::sin(radians);

    const f64 forwardZ =
        std::cos(radians);

    const f64 rightX =
        forwardZ;

    const f64 rightZ =
        -forwardX;

    LVector result = {};

    result.x =
        player.pos.x
        - static_cast<s32>(
            std::lround(
                forwardX
                * cfg.thirdPersonDistance))
        + static_cast<s32>(
            std::lround(
                rightX
                * cfg.thirdPersonShoulderOffset));

    result.y =
        player.pos.y
        + cfg.thirdPersonHeight;

    result.z =
        player.pos.z
        - static_cast<s32>(
            std::lround(
                forwardZ
                * cfg.thirdPersonDistance))
        + static_cast<s32>(
            std::lround(
                rightZ
                * cfg.thirdPersonShoulderOffset));

    return result;
}

bool ApplyThirdPersonCollision(
    const ThreeChanCameraTuning& cfg,
    const LVector& origin,
    LVector& inOutEye) {

    if (!cfg.thirdPersonCollision) {
        return false;
    }

    s32 ratio = 0x10000;
    LVector normal = {};
    LVector hitPoint = {};
    s32 wallHorizontal = 0;

    const s32 hit =
        CollisionSector::CheckWorldWallCollision(
            origin,
            inOutEye,
            32,
            64,
            0,
            ratio,
            normal,
            hitPoint,
            wallHorizontal);

    if (!hit) {
        return false;
    }

    ratio =
        std::clamp(
            ratio,
            0,
            0x10000);

    const f64 dx =
        static_cast<f64>(
            inOutEye.x - origin.x);

    const f64 dy =
        static_cast<f64>(
            inOutEye.y - origin.y);

    const f64 dz =
        static_cast<f64>(
            inOutEye.z - origin.z);

    const f64 length =
        std::sqrt(
            dx * dx
            + dy * dy
            + dz * dz);

    if (length > 1.0
        && cfg.thirdPersonCollisionPadding > 0) {

        const s32 paddingRatio =
            static_cast<s32>(
                std::lround(
                    (
                        static_cast<f64>(
                            cfg.thirdPersonCollisionPadding)
                        / length
                    )
                    * 65536.0));

        ratio =
            std::max(
                0,
                ratio - paddingRatio);
    }

    const LVector desired =
        inOutEye;

    inOutEye.x =
        origin.x
        + static_cast<s32>(
            (
                static_cast<s64>(
                    desired.x - origin.x)
                * ratio
            )
            >> 16);

    inOutEye.y =
        origin.y
        + static_cast<s32>(
            (
                static_cast<s64>(
                    desired.y - origin.y)
                * ratio
            )
            >> 16);

    inOutEye.z =
        origin.z
        + static_cast<s32>(
            (
                static_cast<s64>(
                    desired.z - origin.z)
                * ratio
            )
            >> 16);

    return true;
}

bool HandleThirdPerson(
    Camera& camera,
    const ThreeChanCameraTuning& cfg) {

    Player* player =
        Player::s_player;

    if (!player) {
        return false;
    }

    const f64 now =
        Time::GetTimeInSeconds();

    const f32 dt =
        GetRuntimeDeltaTime();

    bool moving = false;

    const s32 desiredYaw =
        ResolveThirdPersonDesiredYaw(
            *player,
            cfg,
            now,
            moving);

    bool snapPresentation = false;

    if (!g_runtime.thirdPersonInitialized) {
        g_runtime.thirdPersonYaw =
            desiredYaw;
    }
    else if (!cfg.thirdPersonSmoothTurns) {
        g_runtime.thirdPersonYaw =
            desiredYaw;
    }
    else {
        f32 response =
            cfg.thirdPersonRotationLag;

        if (!moving
            && now - g_runtime.thirdPersonLastMovementTime
                >= cfg.thirdPersonAutoCenterDelay) {

            response *=
                std::max(
                    0.0f,
                    cfg.thirdPersonAutoCenterStrength);
        }

        const f32 alpha =
            ResponseAlpha(
                response,
                dt);

        const s32 delta =
            AngleDelta16(
                desiredYaw,
                g_runtime.thirdPersonYaw);

        g_runtime.thirdPersonYaw =
            ClampAngle16(
                g_runtime.thirdPersonYaw
                + static_cast<s32>(
                    static_cast<f32>(delta)
                    * alpha));
    }

    LVector desiredTarget =
        PlayerFocusPoint(
            cfg.thirdPersonTargetHeight);

    LVector desiredEye =
        BuildThirdPersonDesiredEye(
            *player,
            cfg,
            g_runtime.thirdPersonYaw);

    g_runtime.thirdPersonCollisionLastFrame =
        ApplyThirdPersonCollision(
            cfg,
            desiredTarget,
            desiredEye);

    if (!g_runtime.thirdPersonInitialized) {
        g_runtime.thirdPersonEye =
            desiredEye;

        g_runtime.thirdPersonTarget =
            desiredTarget;

        g_runtime.thirdPersonInitialized =
            true;

        snapPresentation = true;
    }
    else {
        const f32 positionAlpha =
            ResponseAlpha(
                cfg.thirdPersonPositionLag,
                dt);

        const f32 targetAlpha =
            ResponseAlpha(
                cfg.thirdPersonTargetLag,
                dt);

        g_runtime.thirdPersonEye.x =
            LerpS32(
                g_runtime.thirdPersonEye.x,
                desiredEye.x,
                positionAlpha);

        g_runtime.thirdPersonEye.z =
            LerpS32(
                g_runtime.thirdPersonEye.z,
                desiredEye.z,
                positionAlpha);

        g_runtime.thirdPersonTarget.x =
            LerpS32(
                g_runtime.thirdPersonTarget.x,
                desiredTarget.x,
                targetAlpha);

        g_runtime.thirdPersonTarget.z =
            LerpS32(
                g_runtime.thirdPersonTarget.z,
                desiredTarget.z,
                targetAlpha);

        if (cfg.thirdPersonSmoothJumps) {
            g_runtime.thirdPersonEye.y =
                LerpS32(
                    g_runtime.thirdPersonEye.y,
                    desiredEye.y,
                    positionAlpha);

            g_runtime.thirdPersonTarget.y =
                LerpS32(
                    g_runtime.thirdPersonTarget.y,
                    desiredTarget.y,
                    targetAlpha);
        }
        else {
            g_runtime.thirdPersonEye.y =
                desiredEye.y;

            g_runtime.thirdPersonTarget.y =
                desiredTarget.y;
        }
    }

    if (cfg.thirdPersonAutoRecoverBehindJackie) {
        const f64 radians =
            static_cast<f64>(
                g_runtime.thirdPersonYaw)
            * THREECHAN_ANGLE_TO_RAD;

        const f64 fx =
            std::sin(radians);

        const f64 fz =
            std::cos(radians);

        const f64 camDx =
            static_cast<f64>(
                g_runtime.thirdPersonEye.x
                - player->pos.x);

        const f64 camDz =
            static_cast<f64>(
                g_runtime.thirdPersonEye.z
                - player->pos.z);

        const f64 forwardDot =
            camDx * fx
            + camDz * fz;

        if (forwardDot > 0.0) {
            g_runtime.thirdPersonEye.x =
                desiredEye.x;

            g_runtime.thirdPersonEye.z =
                desiredEye.z;
        }
    }

    const s32 fov =
        std::clamp(
            cfg.thirdPersonFov,
            cfg.thirdPersonMinFov,
            cfg.thirdPersonMaxFov);

    camera.ThreeChanApplyExternalPose(
        g_runtime.thirdPersonEye,
        g_runtime.thirdPersonTarget,
        fov,
        snapPresentation);

    SetStatus(
        g_runtime.thirdPersonCollisionLastFrame
            ? "Close Third Person: wall collision active"
            : "Close Third Person: following Jackie");

    return true;
}

void RestoreOriginalModeIfNeeded(
    Camera& camera) {

    if (!g_runtime.overrideWasActive) {
        return;
    }

    if (g_runtime.savedOriginalMode) {
        camera.SetMode(
            g_runtime.originalMode);
    }

    ResetExperimentalState();

    g_runtime.overrideWasActive =
        false;

    g_runtime.savedOriginalMode =
        false;

    g_runtime.lastConfiguredMode =
        -1;

    SetStatus(
        "Original camera behaviour");
}

} // namespace

namespace ThreeChanCamera {

bool HandleGameplayCamera(
    Camera& camera) {

    if (!OverridesEnabled()) {
        RestoreOriginalModeIfNeeded(
            camera);

        return false;
    }

    if (!g_runtime.overrideWasActive) {
        g_runtime.overrideWasActive =
            true;

        g_runtime.originalMode =
            camera.GetMode();

        g_runtime.savedOriginalMode =
            true;
    }

    if (IsCameraSuspended(camera)) {
        ResetExperimentalState();

        SetStatus(
            "Suspended: Director / NIS / non-gameplay camera");

        return false;
    }

    ThreeChanSettings& settings =
        ThreeChanTuning::Get();

    ThreeChanCameraTuning& cfg =
        settings.camera;

    const int configuredMode =
        static_cast<int>(cfg.mode);

    HandleConfiguredModeChange(
        configuredMode);

    switch (cfg.mode) {
        case ThreeChanCameraMode::FollowOriginal:
        {
            if (camera.GetMode()
                != CAM_MODE_FOLLOW) {

                camera.SetMode(
                    CAM_MODE_FOLLOW);
            }

            const LVector moveBase = {
                8,
                10,
                8
            };

            const LVector trackBase = {
                6,
                6,
                6
            };

            SetCameraTiming(
                camera,
                moveBase,
                trackBase,
                cfg.followMovementSmoothingMultiplier,
                cfg.followTrackingSmoothingMultiplier);

            SetStatus(
                "Original Follow");

            return false;
        }

        case ThreeChanCameraMode::RigidOriginal:
        {
            if (camera.GetMode()
                != CAM_MODE_RIGID) {

                camera.SetMode(
                    CAM_MODE_RIGID);
            }

            const LVector base = {
                6,
                6,
                6
            };

            SetCameraTiming(
                camera,
                base,
                base,
                cfg.rigidMovementSmoothingMultiplier,
                cfg.rigidTrackingSmoothingMultiplier);

            camera.SetFOV(
                cfg.rigidFov);

            SetStatus(
                "Original Rigid with live 3EChan parameters");

            return false;
        }

        case ThreeChanCameraMode::FixedFrameExperimental:
        {
            if (camera.GetMode()
                != CAM_MODE_FOLLOW) {

                camera.SetMode(
                    CAM_MODE_FOLLOW);
            }

            return HandleFixedFrame(
                camera,
                cfg);
        }

        case ThreeChanCameraMode::CloseThirdPersonExperimental:
        {
            if (camera.GetMode()
                != CAM_MODE_FOLLOW) {

                camera.SetMode(
                    CAM_MODE_FOLLOW);
            }

            return HandleThirdPerson(
                camera,
                cfg);
        }
    }

    return false;
}

void ResolveFollowFov(
    s32& desiredFov) {

    if (!OverridesEnabled()) {
        return;
    }

    if (!g_game
        || g_game->GetState()
            != GameState::Play) {
        return;
    }

    if (g_directorActive != 0
        || (g_director
            && g_director->scriptState != 0)) {
        return;
    }

    const ThreeChanCameraTuning& cfg =
        ThreeChanTuning::GetConst().camera;

    if (cfg.mode
        == ThreeChanCameraMode::FollowOriginal) {

        desiredFov =
            cfg.followFov;
    }
}

void ResolveRigidParameters(
    s32& distance,
    s32& height,
    s32& spinRate) {

    if (!OverridesEnabled()) {
        return;
    }

    if (!g_game
        || g_game->GetState()
            != GameState::Play) {
        return;
    }

    if (g_directorActive != 0
        || (g_director
            && g_director->scriptState != 0)) {
        return;
    }

    const ThreeChanCameraTuning& cfg =
        ThreeChanTuning::GetConst().camera;

    if (cfg.mode
        != ThreeChanCameraMode::RigidOriginal) {
        return;
    }

    distance =
        cfg.rigidDistance;

    height =
        cfg.rigidHeight;

    spinRate =
        cfg.rigidSpinRate;
}

void ResetRuntime() {
    g_runtime =
        ThreeChanCameraRuntimeState{};
}

bool IsExperimentalCameraActive() {
    if (!OverridesEnabled()) {
        return false;
    }

    const ThreeChanCameraMode mode =
        ThreeChanTuning::GetConst().camera.mode;

    return
        mode
            == ThreeChanCameraMode::FixedFrameExperimental
        || mode
            == ThreeChanCameraMode::CloseThirdPersonExperimental;
}

const char* GetRuntimeStatus() {
    return g_runtime.status;
}

} // namespace ThreeChanCamera