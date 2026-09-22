#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RacetrackCircuit.generated.h"

class USplineComponent;

UCLASS()
class ARacetrackCircuit : public AActor
{
	GENERATED_BODY()

public:
	ARacetrackCircuit();

	virtual void OnConstruction(const FTransform& Transform) override;

	double GetTrackLength() const;

	double GetTrackWidth() const
	{
		return TrackWidth;
	}

	FVector GetWorldPositionAtDistance(double DistanceAlongTrack, double LateralOffsetAlpha) const;

	FVector GetWorldDirectionAtDistance(double DistanceAlongTrack) const;

protected:
	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit", meta = (ClampMin = "500.0"))
	double StraightLength = 6000.0;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit", meta = (ClampMin = "500.0"))
	double CornerRadius = 3000.0;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit", meta = (ClampMin = "100.0"))
	double TrackWidth = 1200.0;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit", meta = (ClampMin = "2", ClampMax = "64"))
	int32 PointsPerCorner = 8;

private:
	void GenerateTrackSpline();

	UPROPERTY(VisibleAnywhere, Category = "Racetrack Circuit")
	TObjectPtr<USplineComponent> Spline;
};
