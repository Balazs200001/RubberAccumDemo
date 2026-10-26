// Copyright 2026 Balázs Fábián. All Rights Reserved.

#include "WheelSurfaceSlip.h"

namespace WheelSurfaceSlip
{
	float LongitudinalGroundSpeed(const FWheelSlipReading& Reading)
	{
		return FMath::Abs(Reading.GroundSpeed * FMath::Cos(Reading.SlipAngle));
	}

	bool IsWheelLocked(const FWheelSlipReading& Reading, const FWheelSlipCalibration& Calibration)
	{
		const float RoadSpeed = LongitudinalGroundSpeed(Reading);

		return Reading.BrakeTorque > 0.0f
			&& RoadSpeed >= Calibration.SlowestSkiddingGroundSpeed
			&& FMath::Abs(Reading.SlipMagnitude) >= (1.0f - Calibration.LockedWheelSpeedFraction) * RoadSpeed;
	}

	float SurfaceSlipFromWheel(const FWheelSlipReading& Reading, const FWheelSlipCalibration& Calibration)
	{
		if (IsWheelLocked(Reading, Calibration))
		{
			return 1.0f;
		}

		const float RoadSpeed = FMath::Max(LongitudinalGroundSpeed(Reading), Calibration.SlowestSkiddingGroundSpeed);

		const float SlipRatio = RoadSpeed > UE_SMALL_NUMBER
			? FMath::Abs(Reading.SlipMagnitude) / RoadSpeed
			: 0.0f;

		const float RubberFromSlipRatio = SlipRatio / FMath::Max(Calibration.FullRubberSlipRatio, UE_SMALL_NUMBER);
		const float RubberFromSlipAngle =
			FMath::Abs(Reading.SlipAngle) / FMath::Max(Calibration.FullRubberSlipAngle, UE_SMALL_NUMBER);

		return FMath::Clamp(FMath::Max(RubberFromSlipRatio, RubberFromSlipAngle), 0.0f, HighestRollingSlip);
	}
}
