#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "SurfaceAccumulationSubsystem.h"
#include "../RacetrackCircuit.h"
#include "../SplineFollowingCar.h"

namespace
{
	UWorld* FindRenderableWorld()
	{
		if (GEngine == nullptr)
		{
			return nullptr;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			const bool bUsableType = Context.WorldType == EWorldType::Editor
				|| Context.WorldType == EWorldType::Game
				|| Context.WorldType == EWorldType::PIE;

			if (bUsableType && Context.World() != nullptr)
			{
				return Context.World();
			}
		}

		return nullptr;
	}

	constexpr float TickSeconds = 1.0f / 60.0f;
	constexpr double StraightLapFraction = 0.02;
	constexpr double CornerLapFraction = 0.25;

	void TickCar(ASplineFollowingCar& Car, int32 TickCount)
	{
		for (int32 Tick = 0; Tick < TickCount; ++Tick)
		{
			Car.Tick(TickSeconds);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRacetrackCircuitIsClosedLoopWithPositiveLength,
	"RubberAccumDemo.Track.IsClosedLoopWithPositiveLength",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRacetrackCircuitIsClosedLoopWithPositiveLength::RunTest(const FString& Parameters)
{
	UWorld* World = FindRenderableWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No renderable world available."));
		return false;
	}

	ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>();

	if (Track == nullptr)
	{
		AddError(TEXT("Failed to spawn ARacetrackCircuit."));
		return false;
	}

	TestTrue(TEXT("Generated circuit has positive length"), Track->GetTrackLength() > 0.0);

	const FVector Start = Track->GetWorldPositionAtDistance(0.0, 0.0);
	const FVector WrappedAround = Track->GetWorldPositionAtDistance(Track->GetTrackLength(), 0.0);

	TestTrue(
		TEXT("A full lap returns to the starting position, proving the loop is closed"),
		Start.Equals(WrappedAround, 1.0));

	const FVector DirectionAtStart = Track->GetWorldDirectionAtDistance(0.0);
	const FVector DirectionAfterFullLap = Track->GetWorldDirectionAtDistance(Track->GetTrackLength());

	TestTrue(
		TEXT("The heading is continuous across the wrap point, proving the loop has no seam"),
		DirectionAtStart.Equals(DirectionAfterFullLap, 0.01));

	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRacetrackCircuitWrapsDistanceBeyondTrackLength,
	"RubberAccumDemo.Track.WrapsDistanceBeyondTrackLength",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRacetrackCircuitWrapsDistanceBeyondTrackLength::RunTest(const FString& Parameters)
{
	UWorld* World = FindRenderableWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No renderable world available."));
		return false;
	}

	ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>();

	if (Track == nullptr)
	{
		AddError(TEXT("Failed to spawn ARacetrackCircuit."));
		return false;
	}

	const double Length = Track->GetTrackLength();
	const FVector InRange = Track->GetWorldPositionAtDistance(Length * 0.25, 0.0);
	const FVector BeyondOneLap = Track->GetWorldPositionAtDistance(Length * 1.25, 0.0);
	const FVector NegativeDistance = Track->GetWorldPositionAtDistance(-Length * 0.75, 0.0);

	TestTrue(TEXT("A distance beyond one lap wraps to the same position"), InRange.Equals(BeyondOneLap, 1.0));
	TestTrue(TEXT("A negative distance wraps forwards rather than staying negative"), InRange.Equals(NegativeDistance, 1.0));

	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRacetrackCircuitLateralOffsetMovesAcrossTrackWidth,
	"RubberAccumDemo.Track.LateralOffsetMovesAcrossTrackWidth",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRacetrackCircuitLateralOffsetMovesAcrossTrackWidth::RunTest(const FString& Parameters)
{
	UWorld* World = FindRenderableWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No renderable world available."));
		return false;
	}

	ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>();

	if (Track == nullptr)
	{
		AddError(TEXT("Failed to spawn ARacetrackCircuit."));
		return false;
	}

	const double Distance = Track->GetTrackLength() * 0.1;
	const FVector Centreline = Track->GetWorldPositionAtDistance(Distance, 0.0);
	const FVector RightEdge = Track->GetWorldPositionAtDistance(Distance, 1.0);
	const FVector LeftEdge = Track->GetWorldPositionAtDistance(Distance, -1.0);
	const FVector ClampedBeyondRightEdge = Track->GetWorldPositionAtDistance(Distance, 4.0);

	TestTrue(
		TEXT("Full positive offset sits half the track width from the centreline"),
		FMath::IsNearlyEqual((RightEdge - Centreline).Size(), Track->GetTrackWidth() * 0.5, 1.0));

	TestTrue(
		TEXT("Positive and negative offsets land on opposite sides"),
		FMath::IsNearlyEqual((RightEdge - LeftEdge).Size(), Track->GetTrackWidth(), 1.0));

	TestTrue(
		TEXT("An offset beyond one is clamped to the edge rather than driving off the track"),
		ClampedBeyondRightEdge.Equals(RightEdge, 1.0));

	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRacetrackCircuitCorneringAmountSeparatesStraightsFromCorners,
	"RubberAccumDemo.Track.CorneringAmountSeparatesStraightsFromCorners",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRacetrackCircuitCorneringAmountSeparatesStraightsFromCorners::RunTest(const FString& Parameters)
{
	UWorld* World = FindRenderableWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No renderable world available."));
		return false;
	}

	ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>();

	if (Track == nullptr)
	{
		AddError(TEXT("Failed to spawn ARacetrackCircuit."));
		return false;
	}

	const double Length = Track->GetTrackLength();

	TestEqual(TEXT("The lap starts on a straight"), Track->GetCorneringAmountAtDistance(0.0), 0.0);
	TestEqual(TEXT("The middle of the first straight is not cornering"), Track->GetCorneringAmountAtDistance(Length * 0.097), 0.0);
	TestEqual(TEXT("The middle of the first corner is cornering"), Track->GetCorneringAmountAtDistance(Length * 0.347), 1.0);
	TestEqual(TEXT("The middle of the second straight is not cornering"), Track->GetCorneringAmountAtDistance(Length * 0.597), 0.0);
	TestEqual(TEXT("The middle of the second corner is cornering"), Track->GetCorneringAmountAtDistance(Length * 0.847), 1.0);

	TestEqual(
		TEXT("Cornering amount wraps with distance"),
		Track->GetCorneringAmountAtDistance(Length * 1.347),
		Track->GetCorneringAmountAtDistance(Length * 0.347));

	constexpr int32 SampleCount = 2000;
	int32 CorneringSamples = 0;

	for (int32 Sample = 0; Sample < SampleCount; ++Sample)
	{
		CorneringSamples += Track->GetCorneringAmountAtDistance(Length * Sample / SampleCount) > 0.5 ? 1 : 0;
	}

	const double MeasuredCorneringFraction = static_cast<double>(CorneringSamples) / SampleCount;
	const double StadiumPerimeter = 2.0 * Track->GetStraightLength() + 2.0 * UE_DOUBLE_PI * Track->GetCornerRadius();
	const double ExpectedCorneringFraction = 2.0 * UE_DOUBLE_PI * Track->GetCornerRadius() / StadiumPerimeter;

	AddInfo(FString::Printf(
		TEXT("Cornering fraction of the lap: %.4f measured against %.4f from the stadium geometry"),
		MeasuredCorneringFraction,
		ExpectedCorneringFraction));

	TestTrue(
		TEXT("The share of the lap reported as cornering matches the stadium's own geometry"),
		FMath::IsNearlyEqual(MeasuredCorneringFraction, ExpectedCorneringFraction, 0.02));

	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSplineFollowingCarSlowsForCornersAndRunsFastOnStraights,
	"RubberAccumDemo.Car.SlowsForCornersAndRunsFastOnStraights",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSplineFollowingCarSlowsForCornersAndRunsFastOnStraights::RunTest(const FString& Parameters)
{
	UWorld* World = FindRenderableWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No renderable world available."));
		return false;
	}

	ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>();
	ASplineFollowingCar* Car = World->SpawnActor<ASplineFollowingCar>();

	if (Track == nullptr || Car == nullptr)
	{
		AddError(TEXT("Failed to spawn the circuit or the car."));
		return false;
	}

	const double Length = Track->GetTrackLength();

	Car->ConfigureDriver(Track, Length * StraightLapFraction, 0.0, 0.0);
	TickCar(*Car, 1);
	const double StraightSpeed = Car->GetSpeedCentimetresPerSecond();

	Car->ConfigureDriver(Track, Length * CornerLapFraction, 0.0, 0.0);
	TickCar(*Car, 60);
	const double CornerSpeed = Car->GetSpeedCentimetresPerSecond();

	AddInfo(FString::Printf(
		TEXT("Speed on a straight %.1f cm/s, speed in a corner %.1f cm/s"), StraightSpeed, CornerSpeed));

	TestTrue(TEXT("The car holds its straight line speed on a straight"), StraightSpeed > 0.0);
	TestTrue(TEXT("The car is materially slower in a corner than on a straight"), CornerSpeed < StraightSpeed * 0.7);

	Car->Destroy();
	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSplineFollowingCarBrakesBeforeTheCornerRatherThanInIt,
	"RubberAccumDemo.Car.BrakesBeforeTheCornerRatherThanInIt",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSplineFollowingCarBrakesBeforeTheCornerRatherThanInIt::RunTest(const FString& Parameters)
{
	UWorld* World = FindRenderableWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No renderable world available."));
		return false;
	}

	ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>();
	ASplineFollowingCar* Car = World->SpawnActor<ASplineFollowingCar>();

	if (Track == nullptr || Car == nullptr)
	{
		AddError(TEXT("Failed to spawn the circuit or the car."));
		return false;
	}

	const double CornerEntry = Track->GetStraightLength();
	const double BeforeEntry = CornerEntry - 1000.0;

	TestEqual(
		TEXT("The sample point just before the corner is still on a straight"),
		Track->GetCorneringAmountAtDistance(BeforeEntry),
		0.0);

	Car->ConfigureDriver(Track, BeforeEntry, 0.0, 0.0);
	TickCar(*Car, 10);

	AddInfo(FString::Printf(
		TEXT("Speed %.1f cm/s at %.0f cm, which is %.0f cm before the corner entry"),
		Car->GetSpeedCentimetresPerSecond(),
		Car->GetDistanceAlongTrack(),
		CornerEntry - Car->GetDistanceAlongTrack()));

	TestTrue(
		TEXT("The car is still on the straight when it has already started braking"),
		Car->GetDistanceAlongTrack() < CornerEntry);

	TestTrue(
		TEXT("The car brakes before reaching the corner rather than on entering it"),
		Car->GetSpeedCentimetresPerSecond() < 3000.0);

	Car->Destroy();
	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSplineFollowingCarDriversTakeDifferentLinesThroughACorner,
	"RubberAccumDemo.Car.DriversTakeDifferentLinesThroughACorner",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSplineFollowingCarDriversTakeDifferentLinesThroughACorner::RunTest(const FString& Parameters)
{
	UWorld* World = FindRenderableWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No renderable world available."));
		return false;
	}

	ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>();
	ASplineFollowingCar* Uncommitted = World->SpawnActor<ASplineFollowingCar>();
	ASplineFollowingCar* Committed = World->SpawnActor<ASplineFollowingCar>();

	if (Track == nullptr || Uncommitted == nullptr || Committed == nullptr)
	{
		AddError(TEXT("Failed to spawn the circuit or the cars."));
		return false;
	}

	const double CornerDistance = Track->GetTrackLength() * CornerLapFraction;
	constexpr double SharedBaselineAlpha = 0.6;

	Uncommitted->ConfigureDriver(Track, CornerDistance, SharedBaselineAlpha, 0.0);
	Committed->ConfigureDriver(Track, CornerDistance, SharedBaselineAlpha, 0.9);

	TickCar(*Uncommitted, 60);
	TickCar(*Committed, 60);

	const double UncommittedAlpha = Uncommitted->GetLateralOffsetAlpha();
	const double CommittedAlpha = Committed->GetLateralOffsetAlpha();

	AddInfo(FString::Printf(
		TEXT("From the same baseline offset %.2f, the uncommitted driver holds %.3f and the committed driver holds %.3f"),
		SharedBaselineAlpha,
		UncommittedAlpha,
		CommittedAlpha));

	TestTrue(
		TEXT("A driver with no line commitment keeps its baseline offset through the corner"),
		FMath::IsNearlyEqual(UncommittedAlpha, SharedBaselineAlpha, 0.01));

	TestTrue(
		TEXT("A committed driver gives up most of its baseline offset through the corner"),
		CommittedAlpha < SharedBaselineAlpha * 0.25);

	TestTrue(
		TEXT("Two drivers on the same baseline take measurably different lines, which is what spreads traffic"),
		FMath::Abs(UncommittedAlpha - CommittedAlpha) > 0.2);

	Committed->Destroy();
	Uncommitted->Destroy();
	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSplineFollowingCarRegistersMoreSlipInACornerThanOnAStraight,
	"RubberAccumDemo.Car.RegistersMoreSlipInACornerThanOnAStraight",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSplineFollowingCarRegistersMoreSlipInACornerThanOnAStraight::RunTest(const FString& Parameters)
{
	UWorld* World = FindRenderableWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No renderable world available."));
		return false;
	}

	USurfaceAccumulationSubsystem* Subsystem = World->GetSubsystem<USurfaceAccumulationSubsystem>();

	if (Subsystem == nullptr)
	{
		AddError(TEXT("No USurfaceAccumulationSubsystem available."));
		return false;
	}

	ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>();
	ASplineFollowingCar* Car = World->SpawnActor<ASplineFollowingCar>();

	if (Track == nullptr || Car == nullptr)
	{
		AddError(TEXT("Failed to spawn the circuit or the car."));
		return false;
	}

	const double Length = Track->GetTrackLength();

	Car->ConfigureDriver(Track, Length * StraightLapFraction, 0.0, 0.0);
	Subsystem->ResetSurface();
	Car->Tick(TickSeconds);

	if (Subsystem->GetPendingContactCount() != 1)
	{
		AddError(FString::Printf(
			TEXT("Expected exactly one pending contact from one car tick, got %d"),
			Subsystem->GetPendingContactCount()));
		return false;
	}

	const float StraightSlip = Subsystem->GetPendingContacts()[0].SlipAmount;

	Car->ConfigureDriver(Track, Length * CornerLapFraction, 0.0, 0.0);
	TickCar(*Car, 30);
	Subsystem->ResetSurface();
	Car->Tick(TickSeconds);

	if (Subsystem->GetPendingContactCount() != 1)
	{
		AddError(FString::Printf(
			TEXT("Expected exactly one pending contact from one car tick, got %d"),
			Subsystem->GetPendingContactCount()));
		return false;
	}

	const float CornerSlip = Subsystem->GetPendingContacts()[0].SlipAmount;

	AddInfo(FString::Printf(
		TEXT("Registered slip %.3f on a straight against %.3f in a corner"), StraightSlip, CornerSlip));

	TestTrue(TEXT("The contact registered in a corner carries more slip than one on a straight"), CornerSlip > StraightSlip);

	Subsystem->ResetSurface();

	Car->Destroy();
	Track->Destroy();

	return true;
}

#endif
