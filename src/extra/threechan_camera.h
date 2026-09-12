#pragma once

#include "core.h"

class Camera;

namespace ThreeChanCamera {

bool HandleGameplayCamera(Camera& camera);
void ApplyManualLookPostUpdate(Camera& camera);

void ResolveFollowFov(s32& desiredFov);

void ResolveRigidParameters(
    s32& distance,
    s32& height,
    s32& spinRate);

void ResetRuntime();

bool IsFreeCameraActive();
bool IsExperimentalCameraActive();

const char* GetRuntimeStatus();

}
