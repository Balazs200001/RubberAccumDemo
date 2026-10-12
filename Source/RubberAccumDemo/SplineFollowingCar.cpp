#include "SplineFollowingCar.h"

#include "AnimationRuntime.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "RacetrackCircuit.h"
#include "SurfaceAccumulationSubsystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr const TCHAR* WheelBoneNames[ASplineFollowingCar::WheelCount] = {
		TEXT("Phys_Wheel_FL"),
		TEXT("Phys_Wheel_FR"),
		TEXT("Phys_Wheel_BL"),
		TEXT("Phys_Wheel_BR")
	};

	constexpr const TCHAR* WheelComponentNames[ASplineFollowingCar::WheelCount] = {
		TEXT("FrontLeftWheel"),
		TEXT("FrontRightWheel"),
		TEXT("RearLeftWheel"),
		TEXT("RearRightWheel")
	};

	constexpr double LeftWheelYawOnItsBone = -90.0;

	constexpr double RightWheelYawOnItsBone = 90.0;

	bool IsLeftWheel(int32 Wheel)
	{
		return Wheel == static_cast<int32>(ECarWheel::FrontLeft) || Wheel == static_cast<int32>(ECarWheel::RearLeft);
	}

	constexpr int32 BrakingSeedOffset = 1000003;

	constexpr float LongestDrivingStepSeconds = 1.0f / 60.0f;
}

ASplineFollowingCar::ASplineFollowingCar()
{
	PrimaryActorTick.bCanEverTick = true;

	WheelPositions.SetNumZeroed(WheelCount);
	PreviousWheelPositions.SetNumZeroed(WheelCount);

	CarRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CarRoot"));
	SetRootComponent(CarRoot);

	CarBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarBody"));
	CarBody->SetupAttachment(CarRoot);
	CarBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CarGlass = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarGlass"));
	CarGlass->SetupAttachment(CarBody);
	CarGlass->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMesh(
		TEXT("/Game/Vehicles/SportsCar/SM_SportsCar.SM_SportsCar"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> GlassMesh(
		TEXT("/Game/Vehicles/SportsCar/SM_SportsCar_Glass.SM_SportsCar_Glass"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> WheelMesh(
		TEXT("/Game/Vehicles/SportsCar/SM_SportsCar_Wheel.SM_SportsCar_Wheel"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> WheelBones(
		TEXT("/Game/Vehicles/SportsCar/SKM_SportsCar.SKM_SportsCar"));

	if (BodyMesh.Succeeded())
	{
		CarBody->SetStaticMesh(BodyMesh.Object);
	}

	if (GlassMesh.Succeeded())
	{
		CarGlass->SetStaticMesh(GlassMesh.Object);
	}

	if (WheelBones.Succeeded())
	{
		WheelPlacement = WheelBones.Object;
	}

	CarWheels.SetNum(WheelCount);

	for (int32 Wheel = 0; Wheel < WheelCount; ++Wheel)
	{
		UStaticMeshComponent* WheelComponent = CreateDefaultSubobject<UStaticMeshComponent>(WheelComponentNames[Wheel]);
		WheelComponent->SetupAttachment(CarBody);
		WheelComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		if (WheelMesh.Succeeded())
		{
			WheelComponent->SetStaticMesh(WheelMesh.Object);
		}

		CarWheels[Wheel] = WheelComponent;
	}
}

void ASplineFollowingCar::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	FitCarBodyToTheWheels();
}

void ASplineFollowingCar::FitCarBodyToTheWheels()
{
	if (WheelPlacement == nullptr || CarBody == nullptr || CarWheels.Num() != WheelCount)
	{
		return;
	}

	const FReferenceSkeleton& Skeleton = WheelPlacement->GetRefSkeleton();
	FTransform WheelBoneTransforms[WheelCount];

	for (int32 Wheel = 0; Wheel < WheelCount; ++Wheel)
	{
		const int32 BoneIndex = Skeleton.FindBoneIndex(FName(WheelBoneNames[Wheel]));

		if (BoneIndex == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("%s's wheel placement has no %s bone, so the body is drawn at its own size"),
				*GetName(),
				WheelBoneNames[Wheel]);
			return;
		}

		WheelBoneTransforms[Wheel] = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, BoneIndex);
	}

	const FVector FrontLeft = WheelBoneTransforms[static_cast<int32>(ECarWheel::FrontLeft)].GetLocation();
	const FVector FrontRight = WheelBoneTransforms[static_cast<int32>(ECarWheel::FrontRight)].GetLocation();
	const FVector FrontAxle = (FrontLeft + FrontRight) * 0.5;
	const FVector RearAxle = (WheelBoneTransforms[static_cast<int32>(ECarWheel::RearLeft)].GetLocation()
		+ WheelBoneTransforms[static_cast<int32>(ECarWheel::RearRight)].GetLocation()) * 0.5;

	const double MeshWheelbase = FrontAxle.X - RearAxle.X;

	if (MeshWheelbase <= UE_DOUBLE_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s's body does not face along X, so it is drawn at its own size"),
			*GetName());
		return;
	}

	const UStaticMesh* WheelMesh = CarWheels[0] != nullptr ? CarWheels[0]->GetStaticMesh() : nullptr;
	const double WheelRadius = WheelMesh != nullptr ? WheelMesh->GetBoundingBox().GetExtent().Z : 0.0;

	double LowestWheelCentre = FrontLeft.Z;

	for (int32 Wheel = 0; Wheel < WheelCount; ++Wheel)
	{
		LowestWheelCentre = FMath::Min(LowestWheelCentre, WheelBoneTransforms[Wheel].GetLocation().Z);

		const bool bLeft = IsLeftWheel(Wheel);
		const FTransform OnItsBone(
			FRotator(0.0, bLeft ? LeftWheelYawOnItsBone : RightWheelYawOnItsBone, 0.0),
			FVector::ZeroVector,
			FVector(bLeft ? -1.0 : 1.0, 1.0, 1.0));

		if (CarWheels[Wheel] != nullptr)
		{
			CarWheels[Wheel]->SetRelativeTransform(OnItsBone * WheelBoneTransforms[Wheel]);
		}
	}

	const double Scale = Wheelbase / MeshWheelbase;
	const FVector BetweenTheAxles = (FrontAxle + RearAxle) * 0.5;
	const double GroundUnderTheTyres = LowestWheelCentre - WheelRadius;

	CarBody->SetRelativeScale3D(FVector(Scale));
	CarBody->SetRelativeLocation(FVector(-BetweenTheAxles.X, -BetweenTheAxles.Y, -GroundUnderTheTyres) * Scale);

	BodyWheelTrack = FMath::Abs(FrontRight.Y - FrontLeft.Y) * Scale;
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

	BrakingRandomStream.Initialize(DriverSeed + BrakingSeedOffset);
	bBraking = false;
	BrakingDrawCount = 0;
	LockedWheel.Reset();
	DistanceLastBraking.Reset();
	bWheelHistoryValid = false;
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

void ASplineFollowingCar::UpdateBrakingCommitment(double BrakingAmount)
{
	if (BrakingAmount > BrakingExitAmount)
	{
		DistanceLastBraking = DistanceAlongTrack;
	}

	if (bBraking)
	{
		const double CoastingDistance = DistanceLastBraking.IsSet()
			? DistanceAlongTrack - DistanceLastBraking.GetValue()
			: 0.0;

		if (CoastingDistance < SameBrakingEventSeparationCentimetres)
		{
			return;
		}

		bBraking = false;
		LockedWheel.Reset();
		return;
	}

	if (BrakingAmount < BrakingEntryAmount)
	{
		return;
	}

	bBraking = true;
	++BrakingDrawCount;

	const double LockupDraw = BrakingRandomStream.FRand();
	const double SideDraw = BrakingRandomStream.FRand();

	if (LockupDraw < LockupProbability)
	{
		LockedWheel = SideDraw < 0.5 ? ECarWheel::FrontLeft : ECarWheel::FrontRight;
	}
}

void ASplineFollowingCar::PlaceWheelsFromPose(
	const FVector& RearAxle,
	const FVector& Heading,
	const FVector& BodyRight)
{
	const FVector FrontAxle = RearAxle + Heading * Wheelbase;
	const FVector HalfTrack = BodyRight * (WheelTrack * 0.5);

	WheelPositions[static_cast<int32>(ECarWheel::FrontLeft)] = FrontAxle - HalfTrack;
	WheelPositions[static_cast<int32>(ECarWheel::FrontRight)] = FrontAxle + HalfTrack;
	WheelPositions[static_cast<int32>(ECarWheel::RearLeft)] = RearAxle - HalfTrack;
	WheelPositions[static_cast<int32>(ECarWheel::RearRight)] = RearAxle + HalfTrack;
}

float ASplineFollowingCar::SlipAtWheel(int32 WheelIndex, float RollingSlipAmount) const
{
	const bool bLocked = LockedWheel.IsSet() && static_cast<int32>(LockedWheel.GetValue()) == WheelIndex;

	return bLocked ? LockedWheelSlipAmount : RollingSlipAmount;
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

void ASplineFollowingCar::DriveAlongTrack(float StepSeconds)
{
	const double SignedCornering = Track->GetSignedCorneringAtDistance(DistanceAlongTrack);
	const double CorneringAmount = FMath::Max(
		FMath::Abs(SignedCornering),
		Track->GetCorneringAmountAtDistance(DistanceAlongTrack + BrakingLookaheadCentimetres));

	const double TargetSpeed = StraightLineSpeedCentimetresPerSecond
		* FMath::Lerp(1.0, CorneringSpeedFraction, CorneringAmount);

	const double LargestSpeedDeficit =
		StraightLineSpeedCentimetresPerSecond * (1.0 - CorneringSpeedFraction);

	const double BrakingAmount = LargestSpeedDeficit > UE_DOUBLE_SMALL_NUMBER
		? FMath::Clamp((SpeedCentimetresPerSecond - TargetSpeed) / LargestSpeedDeficit, 0.0, 1.0)
		: 0.0;

	SpeedCentimetresPerSecond = FMath::FInterpConstantTo(
		SpeedCentimetresPerSecond,
		TargetSpeed,
		StepSeconds,
		SpeedChangeCentimetresPerSecondSquared);

	UpdateCornerCommitment(CorneringAmount);
	UpdateBrakingCommitment(BrakingAmount);

	const double ApexTarget = FMath::Clamp(
		(ApexLateralOffsetAlpha + CornerDeviationAlpha) * FMath::Sign(SignedCornering),
		-MaxApexLateralOffsetAlpha,
		MaxApexLateralOffsetAlpha);

	const double TargetLateralOffsetAlpha = FMath::Lerp(
		BaselineLateralOffsetAlpha,
		ApexTarget,
		CorneringLineCommitment * FMath::Abs(SignedCornering));

	LateralOffsetAlpha = FMath::Clamp(
		FMath::FInterpTo(LateralOffsetAlpha, TargetLateralOffsetAlpha, StepSeconds, LineChangeResponsiveness),
		-1.0,
		1.0);

	DistanceAlongTrack += SpeedCentimetresPerSecond * StepSeconds;
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
	const double PreviousLateralOffsetAlpha = LateralOffsetAlpha;

	const int32 StepCount = FMath::Max(1, FMath::CeilToInt32(DeltaSeconds / LongestDrivingStepSeconds));
	const float StepSeconds = DeltaSeconds / StepCount;

	for (int32 Step = 0; Step < StepCount; ++Step)
	{
		DriveAlongTrack(StepSeconds);
	}

	const FVector RearAxle = Track->GetWorldPositionAtDistance(DistanceAlongTrack, LateralOffsetAlpha);
	const FVector Forward = Track->GetWorldDirectionAtDistance(DistanceAlongTrack);
	const FVector Right = Track->GetWorldRightVectorAtDistance(DistanceAlongTrack);
	const FVector Up = Track->GetWorldUpVectorAtDistance(DistanceAlongTrack);

	const double LateralCentimetresPerSecond =
		(LateralOffsetAlpha - PreviousLateralOffsetAlpha) / DeltaSeconds * Track->GetTrackWidth() * 0.5;

	const FVector Velocity = Forward * SpeedCentimetresPerSecond + Right * LateralCentimetresPerSecond;
	const FVector Heading = Velocity.GetSafeNormal(UE_DOUBLE_SMALL_NUMBER, Forward);
	const FVector BodyRight = FVector::CrossProduct(Up, Heading).GetSafeNormal(UE_DOUBLE_SMALL_NUMBER, Right);

	PlaceWheelsFromPose(RearAxle, Heading, BodyRight);

	SetActorLocation(RearAxle + Heading * (Wheelbase * 0.5));
	SetActorRotation(Heading.Rotation());

	const float RollingSlipAmount = FMath::Lerp(
		StraightSlipAmount,
		CorneringSlipAmount,
		static_cast<float>(FMath::Abs(SignedCornering)));

	for (int32 Wheel = 0; Wheel < WheelCount; ++Wheel)
	{
		const FVector WheelVelocity = bWheelHistoryValid
			? (WheelPositions[Wheel] - PreviousWheelPositions[Wheel]) / DeltaSeconds
			: Velocity;

		Subsystem->RegisterSurfaceContact(
			WheelPositions[Wheel],
			WheelVelocity,
			SlipAtWheel(Wheel, RollingSlipAmount),
			ContactWidth);

		PreviousWheelPositions[Wheel] = WheelPositions[Wheel];
	}

	bWheelHistoryValid = true;
}
