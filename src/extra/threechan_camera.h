#pragma once

#include "core.h"

class Camera;

namespace ThreeChanCamera {

bool HandleGameplayCamera(Camera& camera);

void ResolveFollowFov(s32& desiredFov);

void ResolveRigidParameters(
    s32& distance,
    s32& height,
    s32& spinRate);

void ResetRuntime();

bool IsExperimentalCameraActive();

const char* GetRuntimeStatus();

}