#include "SurfaceVerificationRig.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "SurfaceAccumulationMapping.h"
#include "SurfaceAccumulationSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ASurfaceVerificationRig::ASurfaceVerificationRig()
{
	PrimaryActorTick.bCanEverTick = true;

	Origin = CreateDefaultSubobject<USceneComponent>(TEXT("Origin"));
	SetRootComponent(Origin);

	Surface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Surface"));
	Surface->SetupAttachment(Origin);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(
		TEXT("/Engine/BasicShapes/Plane.Plane"));

	if (PlaneMesh.Succeeded())
	{
		Surface->SetStaticMesh(PlaneMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DebugMaterial(
		TEXT("/Game/Materials/M_SurfaceAccumulationDebug.M_SurfaceAccumulationDebug"));

	if (DebugMaterial.Succeeded())
	{
		SurfaceMaterial = DebugMaterial.Object;
	}
}

void ASurfaceVerificationRig::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ResizeSurfaceToRegion();

	if (SurfaceMaterial != nullptr)
	{
		Surface->SetMaterial(0, SurfaceMaterial);
	}
}

void ASurfaceVerificationRig::ResizeSurfaceToRegion()
{
	if (Surface == nullptr || Surface->GetStaticMesh() == nullptr)
	{
		return;
	}

	const FVector MeshSize = Surface->GetStaticMesh()->GetBoundingBox().GetSize();

	if (MeshSize.X <= UE_DOUBLE_SMALL_NUMBER || MeshSize.Y <= UE_DOUBLE_SMALL_NUMBER)
	{
		return;
	}

	Surface->SetRelativeScale3D(FVector(Region.WorldSize / MeshSize.X, Region.WorldSize / MeshSize.Y, 1.0));
	Surface->SetWorldLocation(FVector(Region.WorldCentre.X, Region.WorldCentre.Y, GetActorLocation().Z));
}

void ASurfaceVerificationRig::BeginPlay()
{
	Super::BeginPlay();

	USurfaceAccumulationSubsystem* Subsystem = GetWorld()->GetSubsystem<USurfaceAccumulationSubsystem>();

	if (Subsystem == nullptr)
	{
		return;
	}

	Subsystem->ConfigureRegion(Region);

	SurfaceMaterialInstance = Surface->CreateDynamicMaterialInstance(0, SurfaceMaterial);

	if (SurfaceMaterialInstance != nullptr)
	{
		SurfaceMaterialInstance->SetTextureParameterValue(
			RacetrackSurfaceMapping::AccumulationTargetParameterName,
			Subsystem->GetOrCreateAccumulationTarget());
	}
}

void ASurfaceVerificationRig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	USurfaceAccumulationSubsystem* Subsystem = GetWorld()->GetSubsystem<USurfaceAccumulationSubsystem>();

	if (Subsystem == nullptr)
	{
		return;
	}

	const double SweepLength = Region.WorldSize * 0.8;

	DistanceSwept = FMath::Fmod(DistanceSwept + ContactSpeed * DeltaSeconds, SweepLength);

	const FVector Position(
		Region.WorldCentre.X - SweepLength * 0.5 + DistanceSwept,
		Region.WorldCentre.Y,
		GetActorLocation().Z);

	Subsystem->RegisterSurfaceContact(
		Position,
		FVector(ContactSpeed, 0.0, 0.0),
		SlipAmount,
		ContactWidth,
		Channel);
}
