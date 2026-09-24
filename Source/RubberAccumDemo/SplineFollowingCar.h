#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/RandomStream.h"
#include "RacetrackSurfaceTypes.h"
#include "SplineFollowingCar.generated.h"

class ARacetrackCircuit;
class UStaticMeshComponent;

enum class ECarWheel : uint8
{
	FrontLeft,
	FrontRight,
	RearLeft,
	RearRight
};

UCLASS()
class ASplineFollowingCar : public AActor
{
	GENERATED_BODY()

public:
	static constexpr int32 WheelCount = 4;

	static constexpr int32 WheelsSharingATrack = 2;

	ASplineFollowingCar();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void ConfigureDriver(
		ARacetrackCircuit* InTrack,
		double InStartingDistanceAlongTrack,
		double InBaselineLateralOffsetAlpha,
		double InCorneringLineCommitment,
		int32 InDriverSeed);

	double GetLateralOffsetAlpha() const
	{
		return LateralOffsetAlpha;
	}

	double GetSpeedCentimetresPerSecond() const
	{
		return SpeedCentimetresPerSecond;
	}

	double GetDistanceAlongTrack() const
	{
		return DistanceAlongTrack;
	}

	float GetCorneringSlipAmount() const
	{
		return CorneringSlipAmount;
	}

	float GetStraightSlipAmount() const
	{
		return StraightSlipAmount;
	}

	float GetContactWidth() const
	{
		return ContactWidth;
	}

	double GetStraightLineSpeedCentimetresPerSecond() const
	{
		return StraightLineSpeedCentimetresPerSecond;
	}

	double GetCorneringSpeedFraction() const
	{
		return CorneringSpeedFraction;
	}

	double GetWheelbase() const
	{
		return Wheelbase;
	}

	double GetWheelTrack() const
	{
		return WheelTrack;
	}

	TOptional<ECarWheel> GetLockedWheel() const
	{
		return LockedWheel;
	}

	int32 GetBrakingDrawCount() const
	{
		return BrakingDrawCount;
	}

	bool IsBraking() const
	{
		return bBraking;
	}

	const TArray<FVector>& GetWheelPositions() const
	{
		return WheelPositions;
	}

	bool HasPlacedItsWheels() const
	{
		return bWheelHistoryValid;
	}

	double GetCornerDeviationAlpha() const
	{
		return CornerDeviationAlpha;
	}

	bool IsCommittedToACorner() const
	{
		return bCommittedToACorner;
	}

	int32 GetCornerDrawCount() const
	{
		return CornerDrawCount;
	}

	double GetMaxCornerDeviationAlpha() const
	{
		return MaxCornerDeviationAlpha;
	}

protected:
	UPROPERTY(EditInstanceOnly, Category = "Spline Following Car")
	TObjectPtr<ARacetrackCircuit> Track;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car")
	double StartingDistanceAlongTrack = 0.0;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	double BaselineLateralOffsetAlpha = 0.0;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double CorneringLineCommitment = 0.5;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double ApexLateralOffsetAlpha = 0.6;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double MaxCornerDeviationAlpha = 0.30;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double MaxApexLateralOffsetAlpha = 0.90;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double CornerEntryCorneringAmount = 0.15;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double CornerExitCorneringAmount = 0.05;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "0.0"))
	double SameCornerSeparationCentimetres = 600.0;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car")
	int32 DriverSeed = 0;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "0.0"))
	double StraightLineSpeedCentimetresPerSecond = 3000.0;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	double CorneringSpeedFraction = 0.55;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "0.0"))
	double SpeedChangeCentimetresPerSecondSquared = 2500.0;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "0.0"))
	double LineChangeResponsiveness = 2.0;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "0.0"))
	double BrakingLookaheadCentimetres = 2500.0;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StraightSlipAmount = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CorneringSlipAmount = 0.50f;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "0.0"))
	float ContactWidth = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car")
	EAccumulationChannel Channel = EAccumulationChannel::RubberBuildup;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car|Wheels", meta = (ClampMin = "1.0"))
	double Wheelbase = 140.0;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car|Wheels", meta = (ClampMin = "1.0"))
	double WheelTrack = 80.0;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car|Wheels", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LockedWheelSlipAmount = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car|Wheels", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double LockupProbability = 0.15;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car|Wheels", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double BrakingEntryAmount = 0.25;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car|Wheels", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double BrakingExitAmount = 0.10;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car|Wheels", meta = (ClampMin = "0.0"))
	double SameBrakingEventSeparationCentimetres = 600.0;

private:
	void ResizeCarMesh();

	void PlaceOnTrack();

	void UpdateCornerCommitment(double CorneringAmount);

	void UpdateBrakingCommitment(double BrakingAmount);

	void PlaceWheelsFromPose(const FVector& RearAxle, const FVector& Heading, const FVector& BodyRight);

	float SlipAtWheel(int32 WheelIndex, float RollingSlipAmount) const;

	EAccumulationChannel ChannelAtWheel(int32 WheelIndex) const;

	double DrawTriangularCornerDeviationAlpha();

	UPROPERTY(VisibleAnywhere, Category = "Spline Following Car")
	TObjectPtr<UStaticMeshComponent> CarMesh;

	double DistanceAlongTrack = 0.0;

	double LateralOffsetAlpha = 0.0;

	double SpeedCentimetresPerSecond = 0.0;

	double CornerDeviationAlpha = 0.0;

	bool bCommittedToACorner = false;

	int32 CornerDrawCount = 0;

	TOptional<double> DistanceLastCornering;

	FRandomStream DriverRandomStream;

	TArray<FVector> WheelPositions;

	TArray<FVector> PreviousWheelPositions;

	bool bWheelHistoryValid = false;

	bool bBraking = false;

	int32 BrakingDrawCount = 0;

	TOptional<ECarWheel> LockedWheel;

	TOptional<double> DistanceLastBraking;

	FRandomStream BrakingRandomStream;
};
