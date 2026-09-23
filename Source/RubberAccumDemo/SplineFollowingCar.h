#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/RandomStream.h"
#include "RacetrackSurfaceTypes.h"
#include "SplineFollowingCar.generated.h"

class ARacetrackCircuit;
class UStaticMeshComponent;

UCLASS()
class ASplineFollowingCar : public AActor
{
	GENERATED_BODY()

public:
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
	double ApexLateralOffsetAlpha = 0.7;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double MaxCornerDeviationAlpha = 0.20;

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
	float StraightSlipAmount = 0.10f;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CorneringSlipAmount = 0.50f;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "0.0"))
	float ContactWidth = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car")
	EAccumulationChannel Channel = EAccumulationChannel::RubberBuildup;

private:
	void ResizeCarMesh();

	void PlaceOnTrack();

	void UpdateCornerCommitment(double CorneringAmount);

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
};
