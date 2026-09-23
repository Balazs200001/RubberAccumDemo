#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RacetrackCircuit.generated.h"

class ASplineFollowingCar;
class UProceduralMeshComponent;
class USplineComponent;

UENUM()
enum class ETrackLayout : uint8
{
	Stadium,
	Spa
};

UCLASS()
class ARacetrackCircuit : public AActor
{
	GENERATED_BODY()

public:
	ARacetrackCircuit();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void PostInitializeComponents() override;

	virtual void BeginPlay() override;

	const USplineComponent* GetCentreline() const
	{
		return Spline;
	}

	double GetTrackLength() const;

	double GetTrackWidth() const
	{
		return TrackWidth;
	}

	FVector GetWorldPositionAtDistance(double DistanceAlongTrack, double LateralOffsetAlpha) const;

	FVector GetWorldDirectionAtDistance(double DistanceAlongTrack) const;

	double GetCorneringAmountAtDistance(double DistanceAlongTrack) const;

	double GetSignedCorneringAtDistance(double DistanceAlongTrack) const;

	void SetLayout(ETrackLayout NewLayout, double NewLapLength);

	double GetTightestCurvature() const
	{
		return TightestCurvature;
	}

	double GetTotalTurningRadians() const
	{
		return TotalTurningRadians;
	}

	double GetCornerRadius() const
	{
		return CornerRadius;
	}

	double GetStraightLength() const
	{
		return StraightLength;
	}

protected:
	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit")
	ETrackLayout Layout = ETrackLayout::Stadium;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit", meta = (ClampMin = "10000.0"))
	double AuthoredLayoutLapLength = 150000.0;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit", meta = (ClampMin = "0.0"))
	double ElevationExaggeration = 2.5;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Time", meta = (ClampMin = "1.01"))
	float TimeDilationStep = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Time", meta = (ClampMin = "0.01"))
	float SlowestTimeDilation = 0.1f;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Time", meta = (ClampMin = "1.0"))
	float FastestTimeDilation = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Deposition", meta = (ClampMin = "1.01"))
	float DepositionScaleStep = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Deposition", meta = (ClampMin = "0.01"))
	float SlowestDepositionScale = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Deposition", meta = (ClampMin = "1.0"))
	float FastestDepositionScale = 64.0f;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Observer", meta = (ClampMin = "10.0", ClampMax = "89.0"))
	double ObserverPitchDegrees = 62.0;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Observer", meta = (ClampMin = "1.0"))
	double ObserverFramingScale = 1.08;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit", meta = (ClampMin = "500.0"))
	double StraightLength = 6000.0;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit", meta = (ClampMin = "500.0"))
	double CornerRadius = 3000.0;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit", meta = (ClampMin = "100.0"))
	double TrackWidth = 1200.0;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit", meta = (ClampMin = "2", ClampMax = "64"))
	int32 PointsPerCorner = 8;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit", meta = (ClampMin = "0.5"))
	double SurfaceCentimetresPerTexel = 9.765625;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit", meta = (ClampMin = "50.0"))
	double RoadSegmentLength = 200.0;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit")
	double RoadHeightAboveSpline = 1.0;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit")
	TObjectPtr<UMaterialInterface> RoadMaterial;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Field", meta = (ClampMin = "0", ClampMax = "64"))
	int32 CarCount = 5;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Field")
	TSubclassOf<ASplineFollowingCar> CarClass;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Field", meta = (ClampMin = "300.0"))
	double GridSpacingCentimetres = 1400.0;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Field", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double DriverLateralSpread = 0.4;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Field", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double DriverLineCommitmentAtCentre = 0.2;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Field", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double DriverLineCommitmentAtEdge = 0.8;

private:
	double WrapDistance(double DistanceAlongTrack) const;

	void SpawnField();

	void ReportLapsToSaturation();

	void BindTimeControls();

	void PlaceObserverAboveTrack();

	void SpeedUpTime();

	void SlowDownTime();

	void ApplyTimeDilation(float Dilation);

	void DepositFaster();

	void DepositSlower();

	void ApplyDepositionScale(float Scale);

	void GenerateTrackSpline();

	void BuildStadiumPoints(TArray<FVector>& OutPoints) const;

	void BuildAuthoredLayoutPoints(TArray<FVector>& OutPoints) const;

	void ApplySplinePoints(const TArray<FVector>& Points);

	void BuildCorneringProfile();

	void GenerateRoadMesh();

	void BindAccumulationTargetToRoad();

	UPROPERTY(VisibleAnywhere, Category = "Racetrack Circuit")
	TObjectPtr<USplineComponent> Spline;

	UPROPERTY(VisibleAnywhere, Category = "Racetrack Circuit")
	TObjectPtr<UProceduralMeshComponent> Road;

	TArray<double> CorneringProfile;

	double TightestCurvature = 0.0;

	double TotalTurningRadians = 0.0;
};
