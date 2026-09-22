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

protected:
	UPROPERTY(EditInstanceOnly, Category = "Spline Following Car")
	TObjectPtr<ARacetrackCircuit> Track;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car")
	double StartingDistanceAlongTrack = 0.0;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	double LateralOffsetAlpha = 0.0;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "0.0"))
	double SpeedCentimetresPerSecond = 3000.0;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SlipAmount = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car", meta = (ClampMin = "0.0"))
	float ContactWidth = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Spline Following Car")
	EAccumulationChannel Channel = EAccumulationChannel::RubberBuildup;

private:
	void ResizeCarMesh();

	UPROPERTY(VisibleAnywhere, Category = "Spline Following Car")
	TObjectPtr<UStaticMeshComponent> CarMesh;

	double DistanceAlongTrack = 0.0;
};
