// Copyright 2026 Balázs Fábián. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TracksideScenery.generated.h"

class ARacetrackCircuit;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
class UStaticMesh;

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

struct FTracksideStands
{
	TArray<FTransform> Open;

	TArray<FTransform> Covered;
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

	virtual void BeginPlay() override;

	UFUNCTION(CallInEditor, Category = "Trackside Scenery")
	void Rebuild();

	void SetCircuit(ARacetrackCircuit* NewCircuit);

	UProceduralMeshComponent* GetGround() const
	{
		return Ground;
	}

	UProceduralMeshComponent* GetTerrainMesh() const
	{
		return TerrainMesh;
	}

	UProceduralMeshComponent* GetBarriers() const
	{
		return Barriers;
	}

protected:
	UPROPERTY(EditAnywhere, Category = "Trackside Scenery")
	TObjectPtr<ARacetrackCircuit> Circuit;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery")
	TObjectPtr<UMaterialInterface> SceneryMaterial;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery")
	TObjectPtr<UMaterialInterface> FenceMaterial;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery")
	TObjectPtr<UStaticMesh> FencePostMesh;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery")
	TObjectPtr<UStaticMesh> ArmcoPostMesh;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery")
	TObjectPtr<UStaticMesh> OpenGrandstandMesh;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery")
	TObjectPtr<UStaticMesh> CoveredGrandstandMesh;

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

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Barriers", meta = (ClampMin = "0.0"))
	double WallHeight = 80.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Barriers", meta = (ClampMin = "1.0"))
	double WallThickness = 25.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Barriers", meta = (ClampMin = "0.0"))
	double BarrierFootDepth = 40.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Barriers", meta = (ClampMin = "0.0"))
	double ArmcoSetback = 10.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Barriers", meta = (ClampMin = "0.0"))
	double ArmcoLowestRail = 18.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Barriers", meta = (ClampMin = "1.0"))
	double ArmcoRailHeight = 12.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Barriers", meta = (ClampMin = "1.0"))
	double ArmcoRailPitch = 16.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Barriers", meta = (ClampMin = "10.0"))
	double ArmcoPostSpacing = 200.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Barriers", meta = (ClampMin = "0.0"))
	double FenceSetback = 15.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Barriers", meta = (ClampMin = "0.0"))
	double FenceHeight = 220.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Barriers", meta = (ClampMin = "10.0"))
	double FencePostSpacing = 350.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Grandstands", meta = (ClampMin = "0.0"))
	double GrandstandSetback = 300.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Grandstands", meta = (ClampMin = "100.0"))
	double GrandstandModuleSpacing = 1250.0;

	UPROPERTY(EditAnywhere, Category = "Trackside Scenery|Grandstands", meta = (ClampMin = "0.0"))
	double GrandstandPadMargin = 300.0;

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
		const TArray<FTracksideBands>& Right,
		const FTracksideStands& Stands) const;

	void BuildGround(
		const TArray<FTracksideRing>& Rings,
		const TArray<FTracksideBands>& Left,
		const TArray<FTracksideBands>& Right,
		const FTracksideTerrain& Terrain);

	void BuildTerrain(const FTracksideTerrain& Terrain);

	void BuildBarriers(
		const TArray<FTracksideRing>& Rings,
		const TArray<FTracksideBands>& Left,
		const TArray<FTracksideBands>& Right);

	FTracksideStands PlanGrandstands(
		const TArray<FTracksideRing>& Rings,
		const TArray<FTracksideBands>& Left,
		const TArray<FTracksideBands>& Right) const;

	void PlaceGrandstands(const FTracksideStands& Stands);

	double BarrierDepth(bool bWall, bool bCatchFence) const;

	UPROPERTY(VisibleAnywhere, Category = "Trackside Scenery")
	TObjectPtr<USceneComponent> SceneryRoot;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Trackside Scenery")
	TObjectPtr<UProceduralMeshComponent> Ground;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Trackside Scenery")
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Trackside Scenery")
	TObjectPtr<UProceduralMeshComponent> Barriers;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Trackside Scenery")
	TObjectPtr<UInstancedStaticMeshComponent> FencePosts;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Trackside Scenery")
	TObjectPtr<UInstancedStaticMeshComponent> ArmcoPosts;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Trackside Scenery")
	TObjectPtr<UInstancedStaticMeshComponent> OpenGrandstands;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Trackside Scenery")
	TObjectPtr<UInstancedStaticMeshComponent> CoveredGrandstands;
};
