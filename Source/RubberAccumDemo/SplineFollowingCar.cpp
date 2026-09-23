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
	double InCorneringLineCommitment)
{
	Track = InTrack;
	StartingDistanceAlongTrack = InStartingDistanceAlongTrack;
	BaselineLateralOffsetAlpha = InBaselineLateralOffsetAlpha;
	CorneringLineCommitment = InCorneringLineCommitment;

	PlaceOnTrack();
}

void ASplineFollowingCar::PlaceOnTrack()
{
	DistanceAlongTrack = StartingDistanceAlongTrack;
	LateralOffsetAlpha = BaselineLateralOffsetAlpha;
	SpeedCentimetresPerSecond = StraightLineSpeedCentimetresPerSecond;
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

	const double ApexTarget = ApexLateralOffsetAlpha * FMath::Sign(SignedCornering);

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
