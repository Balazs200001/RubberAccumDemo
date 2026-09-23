#include "RacetrackCircuit.h"

#include "Components/SplineComponent.h"
#include "Engine/World.h"
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

	const FVector2D SpaCentreline[] = {
		{258, 622}, {215, 660}, {160, 715}, {110, 765}, {75, 795},
		{48, 800}, {35, 775}, {42, 745}, {75, 700}, {110, 655},
		{150, 600}, {185, 550}, {205, 490}, {215, 440}, {235, 400},
		{270, 370}, {310, 330}, {350, 290}, {420, 240}, {520, 180},
		{620, 130}, {720, 90}, {830, 55}, {930, 30}, {1020, 22},
		{1075, 40}, {1105, 75}, {1145, 85}, {1190, 60}, {1240, 40},
		{1300, 45}, {1350, 75}, {1372, 120}, {1360, 165}, {1320, 190},
		{1270, 195}, {1180, 215}, {1080, 255}, {990, 290}, {920, 320},
		{890, 370}, {905, 420}, {960, 450}, {1050, 470}, {1150, 490},
		{1225, 515}, {1240, 560}, {1200, 590}, {1160, 605}, {1200, 640},
		{1270, 665}, {1330, 685}, {1340, 730}, {1310, 780}, {1250, 815},
		{1180, 838}, {1100, 845}, {1030, 835}, {975, 800}, {950, 740},
		{945, 670}, {940, 600}, {900, 545}, {830, 505}, {760, 478},
		{700, 468}, {620, 480}, {540, 505}, {470, 530}, {420, 545},
		{385, 552}, {370, 585}, {360, 620}, {352, 650}, {320, 645},
		{290, 632}
	};
}

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

		const double StartingDistance = Length * Index / CarCount;
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

	for (const FVector2D& Point : RacetrackLayouts::SpaCentreline)
	{
		OutPoints.Add(FVector(-Point.Y, Point.X, 0.0));
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

	double TightestCurvature = 0.0;

	for (int32 Sample = 0; Sample < SampleCount; ++Sample)
	{
		const double Distance = Spacing * Sample;

		const FVector Behind = Spline->GetDirectionAtDistanceAlongSpline(
			static_cast<float>(WrapDistance(Distance - Spacing)), ESplineCoordinateSpace::Local);

		const FVector Ahead = Spline->GetDirectionAtDistanceAlongSpline(
			static_cast<float>(WrapDistance(Distance + Spacing)), ESplineCoordinateSpace::Local);

		const double Curvature = (Ahead - Behind).Size() / (2.0 * Spacing);

		CorneringProfile.Add(Curvature);
		TightestCurvature = FMath::Max(TightestCurvature, Curvature);
	}

	if (TightestCurvature <= UE_DOUBLE_SMALL_NUMBER)
	{
		return;
	}

	for (double& Entry : CorneringProfile)
	{
		Entry = FMath::Clamp(Entry / TightestCurvature, 0.0, 1.0);
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

double ARacetrackCircuit::GetCorneringAmountAtDistance(double DistanceAlongTrack) const
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
