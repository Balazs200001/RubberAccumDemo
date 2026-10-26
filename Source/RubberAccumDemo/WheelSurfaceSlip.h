// Copyright 2026 Balázs Fábián. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FWheelSlipReading
{
	float BrakeTorque = 0.0f;

	float SlipMagnitude = 0.0f;

	float SlipAngle = 0.0f;

	float GroundSpeed = 0.0f;
};

struct FWheelSlipCalibration
{
	float FullRubberSlipRatio = 1.0f;

	float FullRubberSlipAngle = 1.0f;

	float LockedWheelSpeedFraction = 0.0f;

	float SlowestSkiddingGroundSpeed = 0.0f;
};

namespace WheelSurfaceSlip
{
	constexpr float HighestRollingSlip = 0.99f;

	float LongitudinalGroundSpeed(const FWheelSlipReading& Reading);

	bool IsWheelLocked(const FWheelSlipReading& Reading, const FWheelSlipCalibration& Calibration);

	float SurfaceSlipFromWheel(const FWheelSlipReading& Reading, const FWheelSlipCalibration& Calibration);
}
