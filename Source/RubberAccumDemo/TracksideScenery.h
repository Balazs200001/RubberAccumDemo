// Copyright 2026 Balázs Fábián. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TracksideScenery.generated.h"

class ARacetrackCircuit;
class UMaterialInterface;
class UProceduralMeshComponent;

struct FTracksideRing
{
	FVector RoadCentre = FVector::ZeroVector;

	FVector Right = FVector::RightVector;

	FVector Up = FVector::UpVector;

	double Distance = 0.0;

	double SignedCurvature = 0.0;
};

struct FTracksideBands
{
	double Line = 0.0;

	double Kerb = 0.0;

	double Green = 0.0;

	double RunOff = 0.0;

	double Verge = 0.0;

	bool bGravel = false;

	double Total() const
	{
		return Line + Kerb + Green + RunOff + Verge;
	}
};

struct FTracksideTerrain
{
	FVector2D Origin = FVector2D::ZeroVector;

	double CellSize = 500.0;

	int32 CountX = 0;

	int32 CountY = 0;

	double BorderHeight = 0.0;

	TArray<double> Heights;

	double HeightAt(int32 IndexX, int32 IndexY) const
	{
		return Heights[IndexY * CountX + IndexX];
	}

	double LowestAround(const FVector& Location) const;

	FVector2D Size() const
	{
		return FVector2D((CountX - 1) * CellSize, (CountY - 1) * CellSize);
	}
};

UCLASS()
class ATracksideScenery : public AActor
{
	GENERATED_BODY()

public:
	ATracksideScenery();

	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(CallInEditor, Category = "Trackside Scenery")
	void Rebuild();

protected:
	UPROPERTY(EditAnywhere, Category = "Trackside Scenery")
	TObjectPtr<ARacetrackCircuit> Circuit;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery")
	TObjectPtr<UMaterialInterface> SceneryMaterial;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Bands", meta = (ClampMin = "0.0"))
	double WhiteLineWidth = 15.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Bands", meta = (ClampMin = "0.0"))
	double KerbWidth = 90.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Bands", meta = (ClampMin = "0.0"))
	double KerbCrownHeight = 3.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Bands", meta = (ClampMin = "0.0"))
	double GreenStripWidth = 80.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Bands", meta = (ClampMin = "0.0"))
	double MaxRunOffWidth = 1200.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Bands", meta = (ClampMin = "0.0"))
	double RunOffStripeInset = 100.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Bands", meta = (ClampMin = "0.0"))
	double RunOffStripeWidth = 30.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Bands", meta = (ClampMin = "0.0"))
	double RunOffStripePitch = 120.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Bands", meta = (ClampMin = "0.0"))
	double VergeWidth = 300.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Bands", meta = (ClampMin = "0.0"))
	double VergeBehindRunOffWidth = 100.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Bands", meta = (ClampMin = "0.0"))
	double VergeFall = 6.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Bands", meta = (ClampMin = "1.0"))
	double MowingStripeLength = 400.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Bands", meta = (ClampMin = "0.0"))
	double MinimumSkirtDepth = 50.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Corners", meta = (ClampMin = "100.0"))
	double InsideKerbRadius = 15000.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Corners", meta = (ClampMin = "100.0"))
	double OutsideKerbRadius = 6000.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Corners", meta = (ClampMin = "100.0"))
	double RunOffStartRadius = 12000.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Corners", meta = (ClampMin = "100.0"))
	double RunOffFullRadius = 3000.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Corners", meta = (ClampMin = "10.0"))
	double CurvatureSampleDistance = 500.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Corners", meta = (ClampMin = "0.0"))
	double CornerSmoothingDistance = 1500.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Corners", meta = (ClampMin = "0.0"))
	double RunOffSmoothingDistance = 2500.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Clearance", meta = (ClampMin = "0.0"))
	double ClearanceMargin = 150.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Clearance", meta = (ClampMin = "5.0"))
	double ClearanceStep = 50.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Clearance", meta = (ClampMin = "100.0"))
	double OwnTrackWindow = 3000.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Clearance", meta = (ClampMin = "0.0"))
	double WidthSmoothingDistance = 500.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Terrain", meta = (ClampMin = "100.0"))
	double TerrainCellSize = 500.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Terrain", meta = (ClampMin = "0.0"))
	double TerrainMargin = 25000.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Terrain", meta = (ClampMin = "100.0"))
	double TerrainNearSmoothing = 2000.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Terrain", meta = (ClampMin = "100.0"))
	double TerrainFarSmoothing = 12000.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Terrain", meta = (ClampMin = "100.0"))
	double TerrainSampleSpacing = 350.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Terrain", meta = (ClampMin = "1.0"))
	double TerrainBorderBlend = 10000.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Terrain", meta = (ClampMin = "0.0"))
	double TerrainFootprintMargin = 750.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Terrain", meta = (ClampMin = "0.0"))
	double TerrainDrop = 30.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Terrain", meta = (ClampMin = "0.0"))
	double FarPlaneExtent = 2000000.0;

private:
	void ClearScenery();

	bool CanDressTheCircuit() const;

	TArray<FTracksideRing> SampleRings() const;

	void LayOutBands(
		const TArray<FTracksideRing>& Rings,
		TArray<FTracksideBands>& OutLeft,
		TArray<FTracksideBands>& OutRight) const;

	FTracksideTerrain ShapeTerrain(
		const TArray<FTracksideRing>& Rings,
		const TArray<FTracksideBands>& Left,
		const TArray<FTracksideBands>& Right) const;

	void BuildGround(
		const TArray<FTracksideRing>& Rings,
		const TArray<FTracksideBands>& Left,
		const TArray<FTracksideBands>& Right,
		const FTracksideTerrain& Terrain);

	void BuildTerrain(const FTracksideTerrain& Terrain);

	UPROPERTY(VisibleAnywhere, Category = "Trackside Scenery")
	TObjectPtr<USceneComponent> SceneryRoot;

	UPROPERTY(VisibleAnywhere, Category = "Trackside Scenery")
	TObjectPtr<UProceduralMeshComponent> Ground;
};
