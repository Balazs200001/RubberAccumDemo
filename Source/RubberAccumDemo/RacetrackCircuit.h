// Copyright 2026 Balázs Fábián. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RacetrackSurfaceTypes.h"
#include "RacetrackCircuit.generated.h"

class ADirectionalLight;
class APlayerController;
class ASkyLight;
class ASplineFollowingCar;
class AWheeledVehiclePawn;
class SDemoWindow;
class SWidget;
class UInputMappingContext;
class UMaterialInstanceDynamic;
class UPostProcessComponent;
class UProceduralMeshComponent;
class USplineComponent;

UENUM()
enum class ETrackLayout : uint8
{
	Stadium,
	Spa
};

UENUM()
enum class ESurfaceDebugView : uint8
{
	AllChannels,
	RubberBuildup,
	SkidMarks,
	Marbles
};

UENUM()
enum class ESurfaceLook : uint8
{
	Debug,
	Asphalt
};

UENUM()
enum class ESceneLighting : uint8
{
	MapSun,
	LowSun,
	Overcast
};

struct FMapLighting
{
	FRotator SunRotation = FRotator::ZeroRotator;

	float SunIntensity = 0.0f;

	float SkyLightIntensity = 0.0f;
};

struct FSkidMarkPairPlacement
{
	double DistanceAlongCentreline = 0.0;

	double OnTheLineAlpha = 0.0;

	double OffTheLineAlpha = 0.0;

	float OnTheLineRubber = 0.0f;

	float OffTheLineRubber = 0.0f;

	double MarkLengthCentimetres = 0.0;

	float ContactWidth = 0.0f;
};

struct FLapsToSaturation
{
	float ApexExposurePerContact = 0.0f;

	float StraightExposurePerContact = 0.0f;

	double AdjacentBaselineSpacing = 0.0;

	double MarkWidth = 0.0;

	double WheelsOverATexel = 0.0;

	double CarsOverAStraightTexel = 0.0;

	double ApexExposurePerLap = 0.0;

	double StraightExposurePerLap = 0.0;

	double ApexLapsToSixtyThreePercent() const
	{
		return 1.0 / (3.0 * ApexExposurePerLap);
	}

	double ApexLapsToNinetyFivePercent() const
	{
		return 1.0 / ApexExposurePerLap;
	}

	double StraightLapsToSixtyThreePercent() const
	{
		return 1.0 / (3.0 * StraightExposurePerLap);
	}

	double StraightLapsToNinetyFivePercent() const
	{
		return 1.0 / StraightExposurePerLap;
	}
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

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaSeconds) override;

	ASplineFollowingCar* FindLeadingCar() const;

	const TArray<FSurfaceQueryHandle>& GetLeaderWheelQueries() const
	{
		return LeaderWheelQueries;
	}

	const TArray<FSurfaceStateSample>& GetLeaderWheelSamples() const
	{
		return LeaderWheelSamples;
	}

	const ASplineFollowingCar* GetLeadingCarLastRead() const
	{
		return Leader.Get();
	}

	const USplineComponent* GetCentreline() const
	{
		return Spline;
	}

	UProceduralMeshComponent* GetRoad() const
	{
		return Road;
	}

	UProceduralMeshComponent* GetEdgeWalls() const
	{
		return EdgeWalls;
	}

	double GetEdgeWallHeight() const
	{
		return EdgeWallHeight;
	}

	double GetRoadHeightAboveSpline() const
	{
		return RoadHeightAboveSpline;
	}

	double GetTrackLength() const;

	double WrapDistance(double DistanceAlongTrack) const;

	double GetTrackWidth() const
	{
		return TrackWidth;
	}

	double GetSurfaceCentimetresPerTexel() const
	{
		return SurfaceCentimetresPerTexel;
	}

	double GetAuthoredLayoutLapLength() const
	{
		return AuthoredLayoutLapLength;
	}

	double GetRoadSegmentLength() const
	{
		return RoadSegmentLength;
	}

	int32 GetRoadSegmentCount() const;

	FVector GetWorldPositionAtDistance(double DistanceAlongTrack, double LateralOffsetAlpha) const;

	FVector GetWorldDirectionAtDistance(double DistanceAlongTrack) const;

	FVector GetWorldRightVectorAtDistance(double DistanceAlongTrack) const;

	FVector GetWorldUpVectorAtDistance(double DistanceAlongTrack) const;

	double GetCorneringAmountAtDistance(double DistanceAlongTrack) const;

	double GetSignedCorneringAtDistance(double DistanceAlongTrack) const;

	void SetLayout(ETrackLayout NewLayout, double NewLapLength);

	ETrackLayout GetLayout() const
	{
		return Layout;
	}

	double GetTightestCurvature() const
	{
		return TightestCurvature;
	}

	double GetTotalTurningRadians() const
	{
		return TotalTurningRadians;
	}

	int32 GetCorneringSampleCount() const
	{
		return CorneringProfile.Num();
	}

	double GetCornerRadius() const
	{
		return CornerRadius;
	}

	double GetStraightLength() const
	{
		return StraightLength;
	}

	int32 GetCarCount() const
	{
		return CarCount;
	}

	double GetDriverLateralSpread() const
	{
		return DriverLateralSpread;
	}

	double GetAdjacentDriverSpacingCentimetres() const;

	double GetDriverBaselineLateralOffsetAlpha(int32 GridIndex) const;

	double GetDriverLineCommitment(int32 GridIndex) const;

	int32 GetDriverSeedForGridIndex(int32 GridIndex) const
	{
		return FieldRandomSeed + GridIndex;
	}

	bool IsFieldPaused() const
	{
		return bFieldPaused;
	}

	int32 GetSpawnedCarCount() const
	{
		return SpawnedField.Num();
	}

	const TArray<TObjectPtr<ASplineFollowingCar>>& GetSpawnedField() const
	{
		return SpawnedField;
	}

	void SetFieldPaused(bool bPaused);

	bool StartsPausedAfterCommandLine() const;

	ESurfaceDebugView GetDebugView() const
	{
		return DebugView;
	}

	void SetDebugView(ESurfaceDebugView View);

	void ReportSurfaceCoverage();

	bool SaveSurfaceStateTo(const FString& FilePath);

	bool LoadSurfaceStateFrom(const FString& FilePath);

	void SaveSurfaceStateWithDialog();

	void LoadSurfaceStateWithDialog();

	void ApplyTimeDilation(float Dilation);

	void SpeedUpTime();

	void SlowDownTime();

	void DepositFaster();

	void DepositSlower();

	void ApplyDepositionScale(float Scale);

	void DecayFaster();

	void DecaySlower();

	float GetDecayRate() const;

	void ApplyDecayRate(float Rate);

	TOptional<FLapsToSaturation> EstimateLapsToSaturation() const;

	bool IsFollowingTheLeader() const;

	void SetFollowTheLeader(bool bFollow);

	float GetFollowDistance() const;

	void SetFollowDistance(float Centimetres);

	static FLinearColor DebugViewChannelMask(ESurfaceDebugView View);

	static const TCHAR* DebugViewName(ESurfaceDebugView View);

	ESurfaceLook GetSurfaceLook() const
	{
		return SurfaceLook;
	}

	void SetSurfaceLook(ESurfaceLook Look);

	static const TCHAR* SurfaceLookName(ESurfaceLook Look);

	ESceneLighting GetSceneLighting() const
	{
		return SceneLighting;
	}

	void SetSceneLighting(ESceneLighting Lighting);

	static const TCHAR* SceneLightingName(ESceneLighting Lighting);

	float NextDecayRate(float CurrentRate, bool bFaster) const;

	double GetLapSeconds() const;

	double GetDecayHalfLifeInLaps() const
	{
		return DecayHalfLifeInLaps;
	}

	double FindLongestApexDistance() const;

	void ArmSkidMarkPair(double DelaySeconds, double ReportIntervalSeconds);

	const TOptional<FSkidMarkPairPlacement>& GetSkidMarkPair() const
	{
		return SkidMarkPair;
	}

	bool ReadSkidMarkPair(FLinearColor& OutOnTheLine, FLinearColor& OutOffTheLine);

	void ToggleDriving();

	bool IsDriving() const
	{
		return bDriving;
	}

	void SetDemoWindowOpen(bool bOpen);

protected:
	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit")
	ETrackLayout Layout = ETrackLayout::Stadium;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit", meta = (ClampMin = "10000.0"))
	double AuthoredLayoutLapLength = 180000.0;

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

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Decay", meta = (ClampMin = "1.01"))
	float DecayRateStep = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Decay", meta = (ClampMin = "0.01"))
	float SlowestDecayRate = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Decay", meta = (ClampMin = "1.0"))
	float FastestDecayRate = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Decay", meta = (ClampMin = "0.1"))
	double DecayHalfLifeInLaps = 56.0;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Observer", meta = (ClampMin = "10.0", ClampMax = "89.0"))
	double ObserverPitchDegrees = 62.0;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Observer", meta = (ClampMin = "1.0"))
	double ObserverFramingScale = 1.08;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit", meta = (ClampMin = "500.0"))
	double StraightLength = 6000.0;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit", meta = (ClampMin = "500.0"))
	double CornerRadius = 3000.0;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit", meta = (ClampMin = "100.0"))
	double TrackWidth = 800.0;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit", meta = (ClampMin = "2", ClampMax = "64"))
	int32 PointsPerCorner = 8;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit", meta = (ClampMin = "0.5"))
	double SurfaceCentimetresPerTexel = 3.125;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit", meta = (ClampMin = "50.0"))
	double RoadSegmentLength = 128.0;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit")
	double RoadHeightAboveSpline = 1.0;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit", meta = (ClampMin = "10.0"))
	double EdgeWallHeight = 150.0;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit")
	TObjectPtr<UMaterialInterface> RoadMaterial;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit")
	TObjectPtr<UMaterialInterface> AsphaltMaterial;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Field", meta = (ClampMin = "0", ClampMax = "64"))
	int32 CarCount = 20;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Field")
	TSubclassOf<ASplineFollowingCar> CarClass;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Field", meta = (ClampMin = "300.0"))
	double GridSpacingCentimetres = 1400.0;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Field", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double DriverLateralSpread = 0.1125;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Field")
	int32 FieldRandomSeed = 20260923;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Field")
	bool bStartPaused = true;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Field", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double DriverLineCommitmentAtCentre = 0.2;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Field", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double DriverLineCommitmentAtEdge = 0.8;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Drive")
	TSubclassOf<AWheeledVehiclePawn> DriveVehicleClass;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Drive")
	TObjectPtr<UInputMappingContext> DriveMappingContext;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Drive", meta = (ClampMin = "0.0"))
	double DriveSpawnHeight = 50.0;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Drive", meta = (ClampMin = "0.001"))
	float DriveFullRubberSlipRatio = 0.1f;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Drive", meta = (ClampMin = "0.001"))
	float DriveFullRubberSlipAngle = 0.1f;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Drive", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DriveLockedWheelSpeedFraction = 0.1f;

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit|Drive", meta = (ClampMin = "0.0"))
	float DriveSlowestSkiddingGroundSpeed = 200.0f;

private:
	void SpawnField();

	void ReportLapsToSaturation();

	void BindCameraGrab();

	void GrabTheCamera();

	void ReleaseTheCamera();

	void ApplyCameraGate(APlayerController& Controller) const;

	void PlaceObserverAboveTrack();

	void PlaceObserverBehindTheLeader(const ASplineFollowingCar& Leading);

	void AcquireLeaderWheelQueries();

	void ReleaseLeaderWheelQueries();

	void ReadTheLeadersWheels();

	void ApplyDebugViewToRoad();

	void ApplySurfaceLookToRoad();

	void CaptureMapLighting();

	FRotator LowSunAheadOfTheCamera(const FRotator& MapSunRotation) const;

	void AddTimeReadout();

	void RemoveTimeReadout();

	void AddDemoWindow();

	void RemoveDemoWindow();

	FText TimeReadoutText() const;

	FText TimeReadoutDrivingText() const;

	void PublishTrackRibbon();

	void GenerateTrackSpline();

	void BuildStadiumPoints(TArray<FVector>& OutPoints) const;

	void BuildAuthoredLayoutPoints(TArray<FVector>& OutPoints) const;

	void ApplySplinePoints(const TArray<FVector>& Points);

	void BuildCorneringProfile();

	void GenerateRoadMesh();

	void GenerateEdgeWalls(
		const TArray<FVector>& LeftEdges,
		const TArray<FVector>& RightEdges,
		const TArray<FVector>& EdgeUps);

	void BindAccumulationTargetToRoad();

	void AdvanceSkidMarkPair(float DeltaSeconds);

	void LaySkidMarkPair(float DeltaSeconds);

	void ReportSkidMarkPair();

	void PlaceObserverAboveSkidMarkPair();

	void StartDriving();

	void StopDriving();

	AWheeledVehiclePawn* SpawnVehicleNearTheCamera(const APlayerController& Controller);

	void SetDriveMappingContext(const APlayerController& Controller, bool bAdd) const;

	void ParkTheDrivenVehicle() const;

	void ReadWriteAndDrawTheDrivenWheels(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, Category = "Racetrack Circuit")
	TObjectPtr<USplineComponent> Spline;

	UPROPERTY(VisibleAnywhere, Category = "Racetrack Circuit")
	TObjectPtr<UProceduralMeshComponent> Road;

	UPROPERTY(VisibleAnywhere, Category = "Racetrack Circuit")
	TObjectPtr<UProceduralMeshComponent> EdgeWalls;

	UPROPERTY(VisibleAnywhere, Category = "Racetrack Circuit")
	TObjectPtr<UPostProcessComponent> PerTexelDebugView;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RoadSurfaceMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> AsphaltSurfaceMaterial;

	UPROPERTY()
	TArray<TObjectPtr<ASplineFollowingCar>> SpawnedField;

	TArray<FSurfaceQueryHandle> LeaderWheelQueries;

	TArray<FSurfaceStateSample> LeaderWheelSamples;

	TWeakObjectPtr<ASplineFollowingCar> Leader;

	TWeakObjectPtr<AWheeledVehiclePawn> DrivenVehicle;

	TWeakObjectPtr<APawn> FlyPawn;

	bool bDriving = false;

	bool bCameraGrabbed = false;

	TArray<FVector> DrivenWheelPreviousContacts;

	TArray<bool> DrivenWheelHadContact;

	bool bFieldPaused = false;

	ESurfaceDebugView DebugView = ESurfaceDebugView::AllChannels;

	ESurfaceLook SurfaceLook = ESurfaceLook::Debug;

	ESceneLighting SceneLighting = ESceneLighting::MapSun;

	TWeakObjectPtr<ADirectionalLight> Sun;

	TWeakObjectPtr<ASkyLight> SkyLight;

	TOptional<FMapLighting> MapLighting;

	TSharedPtr<SWidget> TimeReadout;

	TSharedPtr<SDemoWindow> DemoWindow;

	TArray<double> CorneringProfile;

	double TightestCurvature = 0.0;

	double TotalTurningRadians = 0.0;

	TOptional<double> SecondsUntilSkidMarkPair;

	double SkidMarkPairReportInterval = 0.0;

	double SecondsUntilSkidMarkPairReport = 0.0;

	double SecondsSinceSkidMarkPairLaid = 0.0;

	TOptional<FSkidMarkPairPlacement> SkidMarkPair;

	TOptional<FVector2D> SkidMarkPairLaidSkid;
};
