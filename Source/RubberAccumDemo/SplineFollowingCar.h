#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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
		double InCorneringLineCommitment);

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

	UPROPERTY(VisibleAnywhere, Category = "Spline Following Car")
	TObjectPtr<UStaticMeshComponent> CarMesh;

	double DistanceAlongTrack = 0.0;

	double LateralOffsetAlpha = 0.0;

	double SpeedCentimetresPerSecond = 0.0;
};
