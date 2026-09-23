#include "SplineFollowingCar.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "RacetrackCircuit.h"
#include "SurfaceAccumulationSubsystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr double DesiredCarLength = 220.0;
	constexpr double DesiredCarWidth = 100.0;
	constexpr double DesiredCarHeight = 60.0;
}

ASplineFollowingCar::ASplineFollowingCar()
{
	PrimaryActorTick.bCanEverTick = true;

	CarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarMesh"));
	SetRootComponent(CarMesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	if (CubeMesh.Succeeded())
	{
		CarMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void ASplineFollowingCar::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ResizeCarMesh();
}

void ASplineFollowingCar::ResizeCarMesh()
{
	if (CarMesh == nullptr || CarMesh->GetStaticMesh() == nullptr)
	{
		return;
	}

	const FVector MeshSize = CarMesh->GetStaticMesh()->GetBoundingBox().GetSize();

	if (MeshSize.X <= UE_DOUBLE_SMALL_NUMBER || MeshSize.Y <= UE_DOUBLE_SMALL_NUMBER || MeshSize.Z <= UE_DOUBLE_SMALL_NUMBER)
	{
		return;
	}

	CarMesh->SetRelativeScale3D(FVector(
		DesiredCarLength / MeshSize.X,
		DesiredCarWidth / MeshSize.Y,
		DesiredCarHeight / MeshSize.Z));
}

void ASplineFollowingCar::ConfigureDriver(
	ARacetrackCircuit* InTrack,
	double InStartingDistanceAlongTrack,
	double InBaselineLateralOffsetAlpha,
	double InCorneringLineCommitment,
	int32 InDriverSeed)
{
	Track = InTrack;
	StartingDistanceAlongTrack = InStartingDistanceAlongTrack;
	BaselineLateralOffsetAlpha = InBaselineLateralOffsetAlpha;
	CorneringLineCommitment = InCorneringLineCommitment;
	DriverSeed = InDriverSeed;

	PlaceOnTrack();
}

void ASplineFollowingCar::PlaceOnTrack()
{
	DistanceAlongTrack = StartingDistanceAlongTrack;
	LateralOffsetAlpha = BaselineLateralOffsetAlpha;
	SpeedCentimetresPerSecond = StraightLineSpeedCentimetresPerSecond;

	DriverRandomStream.Initialize(DriverSeed);
	CornerDeviationAlpha = 0.0;
	bCommittedToACorner = false;
	CornerDrawCount = 0;
	DistanceLastCornering.Reset();
}

double ASplineFollowingCar::DrawTriangularCornerDeviationAlpha()
{
	const double FirstUniformDraw = DriverRandomStream.FRand();
	const double SecondUniformDraw = DriverRandomStream.FRand();

	return (FirstUniformDraw + SecondUniformDraw - 1.0) * MaxCornerDeviationAlpha;
}

void ASplineFollowingCar::UpdateCornerCommitment(double CorneringAmount)
{
	if (CorneringAmount > CornerExitCorneringAmount)
	{
		DistanceLastCornering = DistanceAlongTrack;
	}

	if (bCommittedToACorner)
	{
		const double StraightDistance = DistanceLastCornering.IsSet()
			? DistanceAlongTrack - DistanceLastCornering.GetValue()
			: 0.0;

		bCommittedToACorner = StraightDistance < SameCornerSeparationCentimetres;
		return;
	}

	if (CorneringAmount < CornerEntryCorneringAmount)
	{
		return;
	}

	bCommittedToACorner = true;
	CornerDeviationAlpha = DrawTriangularCornerDeviationAlpha();
	++CornerDrawCount;
}

void ASplineFollowingCar::BeginPlay()
{
	Super::BeginPlay();

	if (Track == nullptr)
	{
		for (TActorIterator<ARacetrackCircuit> It(GetWorld()); It; ++It)
		{
			Track = *It;
			break;
		}
	}

	if (Track == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s found no ARacetrackCircuit in the level to follow"), *GetName());
	}

	PlaceOnTrack();
}

void ASplineFollowingCar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Track == nullptr || DeltaSeconds <= UE_SMALL_NUMBER)
	{
		return;
	}

	USurfaceAccumulationSubsystem* Subsystem = GetWorld()->GetSubsystem<USurfaceAccumulationSubsystem>();

	if (Subsystem == nullptr)
	{
		return;
	}

	const double SignedCornering = Track->GetSignedCorneringAtDistance(DistanceAlongTrack);
	const double CorneringAmount = FMath::Max(
		FMath::Abs(SignedCornering),
		Track->GetCorneringAmountAtDistance(DistanceAlongTrack + BrakingLookaheadCentimetres));

	const double TargetSpeed = StraightLineSpeedCentimetresPerSecond
		* FMath::Lerp(1.0, CorneringSpeedFraction, CorneringAmount);

	SpeedCentimetresPerSecond = FMath::FInterpConstantTo(
		SpeedCentimetresPerSecond,
		TargetSpeed,
		DeltaSeconds,
		SpeedChangeCentimetresPerSecondSquared);

	UpdateCornerCommitment(CorneringAmount);

	const double ApexTarget = FMath::Clamp(
		(ApexLateralOffsetAlpha + CornerDeviationAlpha) * FMath::Sign(SignedCornering),
		-MaxApexLateralOffsetAlpha,
		MaxApexLateralOffsetAlpha);

	const double TargetLateralOffsetAlpha = FMath::Lerp(
		BaselineLateralOffsetAlpha,
		ApexTarget,
		CorneringLineCommitment * FMath::Abs(SignedCornering));

	const double PreviousLateralOffsetAlpha = LateralOffsetAlpha;

	LateralOffsetAlpha = FMath::Clamp(
		FMath::FInterpTo(LateralOffsetAlpha, TargetLateralOffsetAlpha, DeltaSeconds, LineChangeResponsiveness),
		-1.0,
		1.0);

	DistanceAlongTrack += SpeedCentimetresPerSecond * DeltaSeconds;

	const FVector Position = Track->GetWorldPositionAtDistance(DistanceAlongTrack, LateralOffsetAlpha);
	const FVector Direction = Track->GetWorldDirectionAtDistance(DistanceAlongTrack);

	SetActorLocation(Position);
	SetActorRotation(Direction.Rotation());

	FSurfaceTrackContact Contact;
	Contact.Coordinate.DistanceAlongCentreline = DistanceAlongTrack;
	Contact.Coordinate.LateralOffsetAlpha = LateralOffsetAlpha;
	Contact.CentimetresPerSecondAlongCentreline = SpeedCentimetresPerSecond;
	Contact.LateralOffsetAlphaPerSecond = (LateralOffsetAlpha - PreviousLateralOffsetAlpha) / DeltaSeconds;
	Contact.SlipAmount = FMath::Lerp(StraightSlipAmount, CorneringSlipAmount, static_cast<float>(CorneringAmount));
	Contact.ContactWidth = ContactWidth;
	Contact.Channel = Channel;

	Subsystem->RegisterSurfaceContactInTrackSpace(Contact);
}
