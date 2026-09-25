#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RacetrackSurfaceTypes.h"
#include "RacetrackCircuit.generated.h"

class ASplineFollowingCar;
class UMaterialInstanceDynamic;
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

struct FSkidMarkPairPlacement
{
	double DistanceAlongCentreline = 0.0;

	double OnTheLineAlpha = 0.0;

	double OffTheLineAlpha = 0.0;

	float OnTheLineRubber = 0.0f;

	float OffTheLineRubber = 0.0f;

	int32 ContactsPerMark = 0;
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

	double GetTrackLength() const;

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

	FVector GetWorldPositionAtDistance(double DistanceAlongTrack, double LateralOffsetAlpha) const;

	FVector GetWorldDirectionAtDistance(double DistanceAlongTrack) const;

	FVector GetWorldRightVectorAtDistance(double DistanceAlongTrack) const;

	FVector GetWorldUpVectorAtDistance(double DistanceAlongTrack) const;

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

	void ApplyMarbleThrow(double Centimetres);

	void ApplyTimeDilation(float Dilation);

	static ESurfaceDebugView NextDebugView(ESurfaceDebugView View);

	static FLinearColor DebugViewChannelMask(ESurfaceDebugView View);

	static const TCHAR* DebugViewName(ESurfaceDebugView View);

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

	UPROPERTY(EditAnywhere, Category = "Racetrack Circuit")
	TObjectPtr<UMaterialInterface> RoadMaterial;

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

private:
	double WrapDistance(double DistanceAlongTrack) const;

	void SpawnField();

	void ToggleFieldPaused();

	void ReportLapsToSaturation();

	void BindTimeControls();

	void PlaceObserverAboveTrack();

	void PlaceObserverBehindTheLeader(const ASplineFollowingCar& Leading);

	void AcquireLeaderWheelQueries();

	void ReleaseLeaderWheelQueries();

	void ReadAndDrawTheLeadersWheels();

	void SpeedUpTime();

	void SlowDownTime();

	void DepositFaster();

	void DepositSlower();

	void ApplyDepositionScale(float Scale);

	void DecayFaster();

	void DecaySlower();

	void ApplyDecayRate(float Rate);

	void CycleDebugView();

	void ApplyDebugViewToRoad();

	void PublishTrackRibbon();

	void GenerateTrackSpline();

	void BuildStadiumPoints(TArray<FVector>& OutPoints) const;

	void BuildAuthoredLayoutPoints(TArray<FVector>& OutPoints) const;

	void ApplySplinePoints(const TArray<FVector>& Points);

	void BuildCorneringProfile();

	void GenerateRoadMesh();

	void BindAccumulationTargetToRoad();

	void AdvanceSkidMarkPair(float DeltaSeconds);

	void LaySkidMarkPair(float DeltaSeconds);

	void ReportSkidMarkPair();

	void PlaceObserverAboveSkidMarkPair();

	UPROPERTY(VisibleAnywhere, Category = "Racetrack Circuit")
	TObjectPtr<USplineComponent> Spline;

	UPROPERTY(VisibleAnywhere, Category = "Racetrack Circuit")
	TObjectPtr<UProceduralMeshComponent> Road;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RoadSurfaceMaterial;

	UPROPERTY()
	TArray<TObjectPtr<ASplineFollowingCar>> SpawnedField;

	TArray<FSurfaceQueryHandle> LeaderWheelQueries;

	TArray<FSurfaceStateSample> LeaderWheelSamples;

	TWeakObjectPtr<ASplineFollowingCar> Leader;

	bool bFieldPaused = false;

	ESurfaceDebugView DebugView = ESurfaceDebugView::AllChannels;

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
