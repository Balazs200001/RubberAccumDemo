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

void ASplineFollowingCar::BeginPlay()
{
	Super::BeginPlay();

	DistanceAlongTrack = StartingDistanceAlongTrack;

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
}

void ASplineFollowingCar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Track == nullptr)
	{
		return;
	}

	USurfaceAccumulationSubsystem* Subsystem = GetWorld()->GetSubsystem<USurfaceAccumulationSubsystem>();

	if (Subsystem == nullptr)
	{
		return;
	}

	DistanceAlongTrack += SpeedCentimetresPerSecond * DeltaSeconds;

	const FVector Position = Track->GetWorldPositionAtDistance(DistanceAlongTrack, LateralOffsetAlpha);
	const FVector Direction = Track->GetWorldDirectionAtDistance(DistanceAlongTrack);

	SetActorLocation(Position);
	SetActorRotation(Direction.Rotation());

	Subsystem->RegisterSurfaceContactInTrackSpace(
		DistanceAlongTrack,
		LateralOffsetAlpha,
		SpeedCentimetresPerSecond,
		SlipAmount,
		ContactWidth,
		Channel);
}
