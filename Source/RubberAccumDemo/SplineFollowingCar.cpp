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

	constexpr int32 BrakingSeedOffset = 1000003;
	constexpr int32 MarbleSeedOffset = 2000003;

	constexpr float LongestDrivingStepSeconds = 1.0f / 60.0f;
}

ASplineFollowingCar::ASplineFollowingCar()
{
	PrimaryActorTick.bCanEverTick = true;

	WheelPositions.SetNumZeroed(WheelCount);
	PreviousWheelPositions.SetNumZeroed(WheelCount);

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

	BrakingRandomStream.Initialize(DriverSeed + BrakingSeedOffset);
	MarbleRandomStream.Initialize(DriverSeed + MarbleSeedOffset);
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

double ASplineFollowingCar::DrawMarbleThrowFallingAwayFromTheLine()
{
	const double FirstUniformDraw = MarbleRandomStream.FRand();
	const double SecondUniformDraw = MarbleRandomStream.FRand();

	return FMath::Min(FirstUniformDraw, SecondUniformDraw) * MarbleThrowCentimetres;
}

void ASplineFollowingCar::DepositMarblesOutsideTheLine(
	USurfaceAccumulationSubsystem& Subsystem,
	double SignedCornering,
	float RollingSlipAmount)
{
	const double CorneringHere = FMath::Abs(SignedCornering);

	if (Track == nullptr || CorneringHere < CornerExitCorneringAmount)
	{
		return;
	}

	const double HalfTrackWidth = Track->GetTrackWidth() * 0.5;

	if (HalfTrackWidth <= UE_DOUBLE_SMALL_NUMBER)
	{
		return;
	}

	const double AwayFromTheApex = -FMath::Sign(SignedCornering);
	const double ThrowAlpha = DrawMarbleThrowFallingAwayFromTheLine() / HalfTrackWidth;

	Subsystem.RegisterSurfaceContactInTrackSpace(
		DistanceAlongTrack,
		LateralOffsetAlpha + AwayFromTheApex * ThrowAlpha,
		SpeedCentimetresPerSecond,
		RollingSlipAmount * static_cast<float>(CorneringHere),
		ContactWidth,
		EAccumulationChannel::Marbles);
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

EAccumulationChannel ASplineFollowingCar::ChannelAtWheel(int32 WheelIndex) const
{
	const bool bLocked = LockedWheel.IsSet() && static_cast<int32>(LockedWheel.GetValue()) == WheelIndex;

	return bLocked ? EAccumulationChannel::SkidMark : Channel;
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
			ContactWidth,
			ChannelAtWheel(Wheel));

		PreviousWheelPositions[Wheel] = WheelPositions[Wheel];
	}

	DepositMarblesOutsideTheLine(*Subsystem, SignedCornering, RollingSlipAmount);

	bWheelHistoryValid = true;
}
