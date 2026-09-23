#include "RacetrackCircuit.h"

#include "Components/InputComponent.h"
#include "HAL/IConsoleManager.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "SplineFollowingCar.h"
#include "SurfaceAccumulationMapping.h"
#include "SurfaceAccumulationSubsystem.h"
#include "SurfaceTrackRibbon.h"
#include "UObject/ConstructorHelpers.h"

namespace RacetrackLayouts
{
	constexpr double CorneringSampleSpacing = 150.0;
	constexpr int32 MaximumCorneringSamples = 4096;
	constexpr double CorneringNormalisationPercentile = 0.75;

	const FVector SpaCentreline[] = {
		{258, 622, 17}, {215, 660, 18}, {160, 715, 19}, {110, 765, 19},
		{75, 795, 19}, {48, 800, 19}, {35, 775, 19}, {42, 745, 18},
		{75, 700, 16}, {110, 655, 13}, {150, 600, 9}, {185, 550, 5},
		{205, 490, 2}, {215, 440, 0}, {235, 400, 3}, {270, 370, 9},
		{310, 330, 17}, {350, 290, 23}, {420, 240, 28}, {520, 180, 34},
		{620, 130, 39}, {720, 90, 43}, {830, 55, 46}, {930, 30, 48},
		{1020, 22, 49}, {1075, 40, 49}, {1105, 75, 48}, {1145, 85, 48},
		{1190, 60, 47}, {1240, 40, 46}, {1300, 45, 45}, {1350, 75, 44},
		{1372, 120, 43}, {1360, 165, 42}, {1320, 190, 41}, {1270, 195, 40},
		{1180, 215, 37}, {1080, 255, 34}, {990, 290, 31}, {920, 320, 29},
		{890, 370, 27}, {905, 420, 26}, {960, 450, 25}, {1050, 470, 24},
		{1150, 490, 23}, {1225, 515, 22}, {1240, 560, 21}, {1200, 590, 20},
		{1160, 605, 20}, {1200, 640, 19}, {1270, 665, 18}, {1330, 685, 17},
		{1340, 730, 15}, {1310, 780, 14}, {1250, 815, 13}, {1180, 838, 12},
		{1100, 845, 11}, {1030, 835, 11}, {975, 800, 10}, {950, 740, 10},
		{945, 670, 10}, {940, 600, 10}, {900, 545, 11}, {830, 505, 11},
		{760, 478, 12}, {700, 468, 12}, {620, 480, 13}, {540, 502, 13},
		{470, 520, 14}, {430, 534, 15}, {405, 540, 15}, {388, 550, 15},
		{378, 568, 16}, {376, 590, 16}, {366, 608, 16}, {345, 618, 17},
		{310, 622, 17}
	};
}

using RacetrackLayouts::CorneringNormalisationPercentile;
using RacetrackLayouts::CorneringSampleSpacing;
using RacetrackLayouts::MaximumCorneringSamples;

ARacetrackCircuit::ARacetrackCircuit()
{
	PrimaryActorTick.bCanEverTick = false;

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	SetRootComponent(Spline);

	Road = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Road"));
	Road->SetupAttachment(Spline);
	Road->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Road->bUseAsyncCooking = false;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DebugSurfaceMaterial(
		TEXT("/Game/Materials/M_SurfaceAccumulationDebug.M_SurfaceAccumulationDebug"));

	if (DebugSurfaceMaterial.Succeeded())
	{
		RoadMaterial = DebugSurfaceMaterial.Object;
	}

	CarClass = ASplineFollowingCar::StaticClass();
}

void ARacetrackCircuit::BeginPlay()
{
	Super::BeginPlay();

	BindAccumulationTargetToRoad();
	SpawnField();
	BindTimeControls();
	PlaceObserverAboveTrack();
}

void ARacetrackCircuit::PlaceObserverAboveTrack()
{
	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);

	if (Controller == nullptr || Controller->GetPawn() == nullptr)
	{
		return;
	}

	const FBox Bounds = GetComponentsBoundingBox(true);

	if (!Bounds.IsValid)
	{
		return;
	}

	const FVector Centre = Bounds.GetCenter();
	const FVector Extent = Bounds.GetExtent();

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	Controller->GetViewportSize(ViewportWidth, ViewportHeight);

	const double Aspect = ViewportHeight > 0
		? static_cast<double>(ViewportWidth) / ViewportHeight
		: 16.0 / 9.0;

	const double FieldOfView = Controller->PlayerCameraManager != nullptr
		? Controller->PlayerCameraManager->GetFOVAngle()
		: 90.0f;

	const double HalfHorizontal = FMath::DegreesToRadians(FieldOfView * 0.5);
	const double HalfVertical = FMath::Atan(FMath::Tan(HalfHorizontal) / Aspect);

	const double Pitch = FMath::DegreesToRadians(ObserverPitchDegrees);

	const double AcrossScreen = Extent.Y;
	const double UpScreen = Extent.X * FMath::Sin(Pitch) + Extent.Z * FMath::Cos(Pitch);

	const double Framing = ObserverFramingScale * FMath::Max(
		AcrossScreen / FMath::Tan(HalfHorizontal),
		UpScreen / FMath::Tan(HalfVertical));

	const FVector Eye(
		Centre.X - Framing * FMath::Cos(Pitch),
		Centre.Y,
		Centre.Z + Framing * FMath::Sin(Pitch));

	const FRotator Look(-ObserverPitchDegrees, 0.0, 0.0);

	Controller->GetPawn()->SetActorLocationAndRotation(Eye, Look);
	Controller->SetControlRotation(Look);

	UE_LOG(LogRacetrackSurface, Log,
		TEXT("%s placed the observer %.0f cm out at %.0f degrees over a %.0f by %.0f cm circuit, "
			"framed for a %.0f degree view at %.2f aspect"),
		*GetName(),
		Framing,
		ObserverPitchDegrees,
		Extent.X * 2.0,
		Extent.Y * 2.0,
		FieldOfView,
		Aspect);
}

void ARacetrackCircuit::BindTimeControls()
{
	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);

	if (Controller == nullptr)
	{
		return;
	}

	EnableInput(Controller);

	if (InputComponent == nullptr)
	{
		return;
	}

	InputComponent->BindKey(EKeys::Period, IE_Pressed, this, &ARacetrackCircuit::SpeedUpTime);
	InputComponent->BindKey(EKeys::Comma, IE_Pressed, this, &ARacetrackCircuit::SlowDownTime);
	InputComponent->BindKey(EKeys::RightBracket, IE_Pressed, this, &ARacetrackCircuit::DepositFaster);
	InputComponent->BindKey(EKeys::LeftBracket, IE_Pressed, this, &ARacetrackCircuit::DepositSlower);

	UE_LOG(LogRacetrackSurface, Log,
		TEXT("Press . and , for time, between %.2fx and %.0fx. Press ] and [ for how much rubber a "
			"contact leaves, between %.2fx and %.0fx of the derived rate"),
		SlowestTimeDilation,
		FastestTimeDilation,
		SlowestDepositionScale,
		FastestDepositionScale);
}

void ARacetrackCircuit::DepositFaster()
{
	ApplyDepositionScale(USurfaceAccumulationSubsystem::GetDepositionScale() * DepositionScaleStep);
}

void ARacetrackCircuit::DepositSlower()
{
	ApplyDepositionScale(USurfaceAccumulationSubsystem::GetDepositionScale() / DepositionScaleStep);
}

void ARacetrackCircuit::ApplyDepositionScale(float Scale)
{
	IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(
		RacetrackSurfaceMapping::DepositionScaleVariableName);

	if (Variable == nullptr)
	{
		return;
	}

	const float Clamped = FMath::Clamp(Scale, SlowestDepositionScale, FastestDepositionScale);

	Variable->Set(Clamped, ECVF_SetByCode);

	UE_LOG(LogRacetrackSurface, Log, TEXT("Deposition now %.2fx the derived rate"), Clamped);

	ReportLapsToSaturation();
}

void ARacetrackCircuit::SpeedUpTime()
{
	ApplyTimeDilation(UGameplayStatics::GetGlobalTimeDilation(this) * TimeDilationStep);
}

void ARacetrackCircuit::SlowDownTime()
{
	ApplyTimeDilation(UGameplayStatics::GetGlobalTimeDilation(this) / TimeDilationStep);
}

void ARacetrackCircuit::ApplyTimeDilation(float Dilation)
{
	const float Clamped = FMath::Clamp(Dilation, SlowestTimeDilation, FastestTimeDilation);

	UGameplayStatics::SetGlobalTimeDilation(this, Clamped);

	UE_LOG(LogRacetrackSurface, Log, TEXT("Time dilation now %.2fx"), Clamped);
}

void ARacetrackCircuit::SpawnField()
{
	UWorld* World = GetWorld();

	if (World == nullptr || CarClass == nullptr || CarCount <= 0)
	{
		return;
	}

	const double Length = GetTrackLength();

	if (Length <= UE_DOUBLE_SMALL_NUMBER)
	{
		return;
	}

	for (int32 Index = 0; Index < CarCount; ++Index)
	{
		const double GridFraction = CarCount > 1 ? static_cast<double>(Index) / (CarCount - 1) : 0.5;
		const double BaselineAlpha = FMath::Lerp(-DriverLateralSpread, DriverLateralSpread, GridFraction);
		const double DistanceFromCentre = FMath::Abs(GridFraction * 2.0 - 1.0);

		const double LineCommitment = FMath::Lerp(
			DriverLineCommitmentAtCentre,
			DriverLineCommitmentAtEdge,
			DistanceFromCentre);

		const double PackedSpacing = FMath::Min(GridSpacingCentimetres, Length / CarCount);
		const double StartingDistance = Length - PackedSpacing * Index;
		const FTransform SpawnTransform(
			GetWorldDirectionAtDistance(StartingDistance).Rotation(),
			GetWorldPositionAtDistance(StartingDistance, BaselineAlpha));

		ASplineFollowingCar* Car = World->SpawnActorDeferred<ASplineFollowingCar>(
			CarClass,
			SpawnTransform,
			this,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

		if (Car == nullptr)
		{
			continue;
		}

		Car->ConfigureDriver(this, StartingDistance, BaselineAlpha, LineCommitment);
		Car->FinishSpawning(SpawnTransform);
	}

	UE_LOG(LogRacetrackSurface, Log,
		TEXT("%s spawned a field of %d cars spread across %.2f of the track width"),
		*GetName(),
		CarCount,
		DriverLateralSpread * 2.0);

	ReportLapsToSaturation();
}

void ARacetrackCircuit::ReportLapsToSaturation()
{
	const ASplineFollowingCar* Reference = CarClass.GetDefaultObject();

	if (Reference == nullptr || CarCount <= 0)
	{
		return;
	}

	const float PerContact = RacetrackSurfaceMapping::SlipToExposure(
		Reference->GetCorneringSlipAmount(), USurfaceAccumulationSubsystem::GetDepositionScale());

	if (PerContact <= UE_SMALL_NUMBER)
	{
		return;
	}

	const double LapsToSaturation = 1.0 / (CarCount * PerContact);

	UE_LOG(LogRacetrackSurface, Log,
		TEXT("One cornering contact deposits %.6f exposure. With %d cars over one apex texel that is "
			"%.4f per lap, so the apex reaches 63 %% coverage in about %.0f laps and 95 %% in about %.0f"),
		PerContact,
		CarCount,
		CarCount * PerContact,
		LapsToSaturation / 3.0,
		LapsToSaturation);
}

void ARacetrackCircuit::BindAccumulationTargetToRoad()
{
	UWorld* World = GetWorld();

	if (World == nullptr || Road == nullptr || RoadMaterial == nullptr)
	{
		return;
	}

	USurfaceAccumulationSubsystem* Subsystem = World->GetSubsystem<USurfaceAccumulationSubsystem>();

	if (Subsystem == nullptr)
	{
		return;
	}

	UTextureRenderTarget2D* Target = Subsystem->GetOrCreateAccumulationTarget();

	if (Target == nullptr)
	{
		UE_LOG(LogRacetrackSurface, Warning,
			TEXT("%s could not obtain an accumulation target, so the road will not show the surface"),
			*GetName());
		return;
	}

	UMaterialInstanceDynamic* Dynamic = Road->CreateDynamicMaterialInstance(0, RoadMaterial);

	if (Dynamic == nullptr)
	{
		return;
	}

	Dynamic->SetTextureParameterValue(
		RacetrackSurfaceMapping::AccumulationTargetParameterName,
		Target);

	UE_LOG(LogRacetrackSurface, Log,
		TEXT("%s bound a %d x %d accumulation target into the road material"),
		*GetName(),
		Target->SizeX,
		Target->SizeY);
}

void ARacetrackCircuit::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	GenerateTrackSpline();
	BuildCorneringProfile();
	GenerateRoadMesh();
}

void ARacetrackCircuit::GenerateRoadMesh()
{
	if (Spline == nullptr || Road == nullptr)
	{
		return;
	}

	const double Length = GetTrackLength();

	if (Length <= UE_DOUBLE_SMALL_NUMBER)
	{
		return;
	}

	const int32 SegmentCount = FMath::Max(
		FMath::RoundToInt32(Length / FMath::Max(RoadSegmentLength, 1.0)), 3);

	const int32 RingCount = SegmentCount + 1;
	const double HalfWidth = TrackWidth * 0.5;

	TArray<FVector> Vertices;
	TArray<FVector2D> UVs;
	TArray<FVector> Normals;
	TArray<int32> Triangles;
	TArray<FLinearColor> VertexColours;
	TArray<FProcMeshTangent> Tangents;

	Vertices.Reserve(RingCount * 2);
	UVs.Reserve(RingCount * 2);
	Normals.Reserve(RingCount * 2);
	Triangles.Reserve(SegmentCount * 6);
	Tangents.Reserve(RingCount * 2);

	const FTransform WorldToLocal = Road->GetComponentTransform().Inverse();

	for (int32 Ring = 0; Ring < RingCount; ++Ring)
	{
		const double Distance = Length * Ring / SegmentCount;
		const float DistanceFloat = static_cast<float>(FMath::Min(Distance, Length));

		const FVector Centre = Spline->GetLocationAtDistanceAlongSpline(DistanceFloat, ESplineCoordinateSpace::World);
		const FVector Right = Spline->GetRightVectorAtDistanceAlongSpline(DistanceFloat, ESplineCoordinateSpace::World);
		const FVector Up = Spline->GetUpVectorAtDistanceAlongSpline(DistanceFloat, ESplineCoordinateSpace::World);
		const FVector Forward = Spline->GetDirectionAtDistanceAlongSpline(DistanceFloat, ESplineCoordinateSpace::World);

		const FVector Raised = Centre + Up * RoadHeightAboveSpline;
		const double AlongTrack = Distance / Length;

		Vertices.Add(WorldToLocal.TransformPosition(Raised - Right * HalfWidth));
		Vertices.Add(WorldToLocal.TransformPosition(Raised + Right * HalfWidth));

		UVs.Add(FVector2D(AlongTrack, 0.0));
		UVs.Add(FVector2D(AlongTrack, 1.0));

		const FVector LocalUp = WorldToLocal.TransformVectorNoScale(Up);
		Normals.Add(LocalUp);
		Normals.Add(LocalUp);

		const FProcMeshTangent Tangent(WorldToLocal.TransformVectorNoScale(Forward), false);
		Tangents.Add(Tangent);
		Tangents.Add(Tangent);
	}

	for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
	{
		const int32 Base = Segment * 2;

		Triangles.Add(Base);
		Triangles.Add(Base + 1);
		Triangles.Add(Base + 2);

		Triangles.Add(Base + 1);
		Triangles.Add(Base + 3);
		Triangles.Add(Base + 2);
	}

	Road->ClearAllMeshSections();
	Road->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, VertexColours, Tangents, false);

	if (RoadMaterial != nullptr)
	{
		Road->SetMaterial(0, RoadMaterial);
	}
}

void ARacetrackCircuit::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	UWorld* World = GetWorld();

	if (World == nullptr || Spline == nullptr)
	{
		return;
	}

	USurfaceAccumulationSubsystem* Subsystem = World->GetSubsystem<USurfaceAccumulationSubsystem>();

	if (Subsystem == nullptr)
	{
		return;
	}

	FSurfaceTrackRibbon Ribbon;
	Ribbon.TrackLength = GetTrackLength();
	Ribbon.TrackWidth = TrackWidth;
	Ribbon.CentimetresPerTexel = SurfaceCentimetresPerTexel;
	Ribbon.bClosedLoop = Spline->IsClosedLoop();

	Subsystem->ConfigureTrackRibbon(Spline, Ribbon);
}

void ARacetrackCircuit::BuildStadiumPoints(TArray<FVector>& OutPoints) const
{
	const int32 PointsPerHalfCorner = FMath::Max(PointsPerCorner, 1);
	const double HalfLength = StraightLength * 0.5;

	OutPoints.Reserve(2 * PointsPerHalfCorner + 3);

	OutPoints.Add(FVector(-HalfLength, CornerRadius, 0.0));
	OutPoints.Add(FVector(HalfLength, CornerRadius, 0.0));

	const FVector RightCornerCentre(HalfLength, 0.0, 0.0);
	for (int32 Step = 1; Step <= PointsPerHalfCorner; ++Step)
	{
		const double Angle = FMath::DegreesToRadians(90.0 - 180.0 * Step / PointsPerHalfCorner);
		OutPoints.Add(RightCornerCentre + FVector(CornerRadius * FMath::Cos(Angle), CornerRadius * FMath::Sin(Angle), 0.0));
	}

	OutPoints.Add(FVector(-HalfLength, -CornerRadius, 0.0));

	const FVector LeftCornerCentre(-HalfLength, 0.0, 0.0);
	for (int32 Step = 1; Step < PointsPerHalfCorner; ++Step)
	{
		const double Angle = FMath::DegreesToRadians(-90.0 - 180.0 * Step / PointsPerHalfCorner);
		OutPoints.Add(LeftCornerCentre + FVector(CornerRadius * FMath::Cos(Angle), CornerRadius * FMath::Sin(Angle), 0.0));
	}
}

void ARacetrackCircuit::BuildAuthoredLayoutPoints(TArray<FVector>& OutPoints) const
{
	OutPoints.Reserve(UE_ARRAY_COUNT(RacetrackLayouts::SpaCentreline));

	for (const FVector& Point : RacetrackLayouts::SpaCentreline)
	{
		OutPoints.Add(FVector(-Point.Y, Point.X, Point.Z * ElevationExaggeration));
	}
}

void ARacetrackCircuit::ApplySplinePoints(const TArray<FVector>& Points)
{
	Spline->ClearSplinePoints(false);

	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		Spline->AddSplinePoint(Points[Index], ESplineCoordinateSpace::Local, false);
		Spline->SetSplinePointType(Index, ESplinePointType::Curve, false);
	}

	Spline->SetClosedLoop(true, true);
}

void ARacetrackCircuit::GenerateTrackSpline()
{
	if (Spline == nullptr)
	{
		return;
	}

	TArray<FVector> Points;

	if (Layout == ETrackLayout::Stadium)
	{
		BuildStadiumPoints(Points);
		ApplySplinePoints(Points);
		return;
	}

	BuildAuthoredLayoutPoints(Points);
	ApplySplinePoints(Points);

	const double AuthoredLength = GetTrackLength();

	if (AuthoredLength <= UE_DOUBLE_SMALL_NUMBER)
	{
		return;
	}

	const double Scale = AuthoredLayoutLapLength / AuthoredLength;
	FVector Centre = FVector::ZeroVector;

	for (const FVector& Point : Points)
	{
		Centre += Point;
	}

	Centre /= Points.Num();
	Centre.Z = 0.0;

	for (FVector& Point : Points)
	{
		Point = (Point - Centre) * Scale;
	}

	ApplySplinePoints(Points);
}

void ARacetrackCircuit::SetLayout(ETrackLayout NewLayout, double NewLapLength)
{
	Layout = NewLayout;
	AuthoredLayoutLapLength = NewLapLength;

	GenerateTrackSpline();
	BuildCorneringProfile();
	GenerateRoadMesh();
}

void ARacetrackCircuit::BuildCorneringProfile()
{
	CorneringProfile.Reset();

	const double Length = GetTrackLength();

	if (Spline == nullptr || Length <= UE_DOUBLE_SMALL_NUMBER)
	{
		return;
	}

	const int32 SampleCount = FMath::Clamp(
		FMath::RoundToInt32(Length / CorneringSampleSpacing), 16, MaximumCorneringSamples);

	const double Spacing = Length / SampleCount;

	CorneringProfile.Reserve(SampleCount);

	TightestCurvature = 0.0;
	TotalTurningRadians = 0.0;

	for (int32 Sample = 0; Sample < SampleCount; ++Sample)
	{
		const double Distance = Spacing * Sample;

		const FVector Behind = Spline->GetDirectionAtDistanceAlongSpline(
			static_cast<float>(WrapDistance(Distance - Spacing)), ESplineCoordinateSpace::Local);

		const FVector Ahead = Spline->GetDirectionAtDistanceAlongSpline(
			static_cast<float>(WrapDistance(Distance + Spacing)), ESplineCoordinateSpace::Local);

		const double TurnAngle = FMath::Atan2(
			FVector::CrossProduct(Behind, Ahead).Z, FVector::DotProduct(Behind, Ahead));

		const double SignedCurvature = TurnAngle / (2.0 * Spacing);

		CorneringProfile.Add(SignedCurvature);
		TightestCurvature = FMath::Max(TightestCurvature, FMath::Abs(SignedCurvature));
		TotalTurningRadians += TurnAngle * 0.5;
	}

	if (TightestCurvature <= UE_DOUBLE_SMALL_NUMBER)
	{
		return;
	}

	TArray<double> Ranked;
	Ranked.Reserve(CorneringProfile.Num());

	for (const double Entry : CorneringProfile)
	{
		Ranked.Add(FMath::Abs(Entry));
	}

	Ranked.Sort();

	const int32 PercentileIndex = FMath::Clamp(
		FMath::FloorToInt32(CorneringNormalisationPercentile * Ranked.Num()), 0, Ranked.Num() - 1);

	const double SustainedCurvature = Ranked[PercentileIndex] > UE_DOUBLE_SMALL_NUMBER
		? Ranked[PercentileIndex]
		: TightestCurvature;

	for (double& Entry : CorneringProfile)
	{
		Entry = FMath::Clamp(Entry / SustainedCurvature, -1.0, 1.0);
	}
}

double ARacetrackCircuit::GetTrackLength() const
{
	return Spline != nullptr ? static_cast<double>(Spline->GetSplineLength()) : 0.0;
}

double ARacetrackCircuit::WrapDistance(double DistanceAlongTrack) const
{
	const double Length = GetTrackLength();

	if (Length <= UE_DOUBLE_SMALL_NUMBER)
	{
		return 0.0;
	}

	const double Wrapped = FMath::Fmod(DistanceAlongTrack, Length);

	return Wrapped < 0.0 ? Wrapped + Length : Wrapped;
}

FVector ARacetrackCircuit::GetWorldPositionAtDistance(double DistanceAlongTrack, double LateralOffsetAlpha) const
{
	const double Length = GetTrackLength();

	if (Spline == nullptr || Length <= UE_DOUBLE_SMALL_NUMBER)
	{
		return GetActorLocation();
	}

	const float DistanceFloat = static_cast<float>(WrapDistance(DistanceAlongTrack));
	const FVector CentreLocation = Spline->GetLocationAtDistanceAlongSpline(DistanceFloat, ESplineCoordinateSpace::World);
	const FVector RightVector = Spline->GetRightVectorAtDistanceAlongSpline(DistanceFloat, ESplineCoordinateSpace::World);
	const double ClampedAlpha = FMath::Clamp(LateralOffsetAlpha, -1.0, 1.0);

	return CentreLocation + RightVector * (ClampedAlpha * TrackWidth * 0.5);
}

FVector ARacetrackCircuit::GetWorldDirectionAtDistance(double DistanceAlongTrack) const
{
	const double Length = GetTrackLength();

	if (Spline == nullptr || Length <= UE_DOUBLE_SMALL_NUMBER)
	{
		return GetActorForwardVector();
	}

	return Spline->GetDirectionAtDistanceAlongSpline(
		static_cast<float>(WrapDistance(DistanceAlongTrack)), ESplineCoordinateSpace::World);
}

double ARacetrackCircuit::GetSignedCorneringAtDistance(double DistanceAlongTrack) const
{
	const double Length = GetTrackLength();

	if (CorneringProfile.Num() == 0 || Length <= UE_DOUBLE_SMALL_NUMBER)
	{
		return 0.0;
	}

	const double Position = WrapDistance(DistanceAlongTrack) / Length * CorneringProfile.Num();
	const int32 Index = FMath::Clamp(FMath::FloorToInt32(Position), 0, CorneringProfile.Num() - 1);
	const int32 NextIndex = (Index + 1) % CorneringProfile.Num();

	return FMath::Lerp(CorneringProfile[Index], CorneringProfile[NextIndex], Position - Index);
}

double ARacetrackCircuit::GetCorneringAmountAtDistance(double DistanceAlongTrack) const
{
	return FMath::Abs(GetSignedCorneringAtDistance(DistanceAlongTrack));
}
