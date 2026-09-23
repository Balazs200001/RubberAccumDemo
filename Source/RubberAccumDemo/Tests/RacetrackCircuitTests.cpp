#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "SurfaceAccumulationMapping.h"
#include "SurfaceAccumulationSubsystem.h"
#include "SurfaceTrackRibbon.h"
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
	constexpr int32 UnusedDriverSeed = 0;

	void TickCar(ASplineFollowingCar& Car, int32 TickCount)
	{
		for (int32 Tick = 0; Tick < TickCount; ++Tick)
		{
			Car.Tick(TickSeconds);
		}
	}

	double AlphaPerContactWidth(const ARacetrackCircuit& Track, const ASplineFollowingCar& Car)
	{
		return Car.GetContactWidth() / (Track.GetTrackWidth() * 0.5);
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

	TestTrue(
		TEXT("The lap starts at a corner exit, so it reads as leaving a corner rather than in one"),
		Track->GetCorneringAmountAtDistance(0.0) < 0.2);
	TestTrue(TEXT("The middle of the first straight is not cornering"), Track->GetCorneringAmountAtDistance(Length * 0.097) < 0.05);
	TestTrue(TEXT("The middle of the first corner is cornering"), Track->GetCorneringAmountAtDistance(Length * 0.347) > 0.9);
	TestTrue(TEXT("The middle of the second straight is not cornering"), Track->GetCorneringAmountAtDistance(Length * 0.597) < 0.05);
	TestTrue(TEXT("The middle of the second corner is cornering"), Track->GetCorneringAmountAtDistance(Length * 0.847) > 0.9);

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

	FString Profile;

	for (int32 Slice = 0; Slice < 20; ++Slice)
	{
		Profile += FString::Printf(TEXT("%.2f "), Track->GetCorneringAmountAtDistance(Length * Slice / 20));
	}

	AddInfo(FString::Printf(TEXT("Cornering amount across the lap in twentieths: %s"), *Profile));

	const double MeasuredCorneringFraction = static_cast<double>(CorneringSamples) / SampleCount;
	const double StadiumPerimeter = 2.0 * Track->GetStraightLength() + 2.0 * UE_DOUBLE_PI * Track->GetCornerRadius();
	const double ExpectedCorneringFraction = 2.0 * UE_DOUBLE_PI * Track->GetCornerRadius() / StadiumPerimeter;

	AddInfo(FString::Printf(
		TEXT("Cornering fraction of the lap: %.4f measured against %.4f from the stadium geometry"),
		MeasuredCorneringFraction,
		ExpectedCorneringFraction));

	AddInfo(FString::Printf(
		TEXT("Total turning %.5f radians against %.5f for any closed planar loop"),
		Track->GetTotalTurningRadians(),
		2.0 * UE_DOUBLE_PI));

	TestTrue(
		TEXT("The sampled curvature integrates to one full turn, as any closed planar loop must"),
		FMath::IsNearlyEqual(FMath::Abs(Track->GetTotalTurningRadians()), 2.0 * UE_DOUBLE_PI, 0.15));

	TestTrue(
		TEXT("The share of the lap reported as cornering is close to the stadium's own geometry"),
		FMath::IsNearlyEqual(MeasuredCorneringFraction, ExpectedCorneringFraction, 0.10));

	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRacetrackCircuitSpaLayoutFitsTheUnfoldedRibbon,
	"RubberAccumDemo.Track.SpaLayoutFitsTheUnfoldedRibbon",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRacetrackCircuitSpaLayoutFitsTheUnfoldedRibbon::RunTest(const FString& Parameters)
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

	Track->SetLayout(ETrackLayout::Spa, 150000.0);

	const double Length = Track->GetTrackLength();

	FSurfaceTrackRibbon Ribbon;
	Ribbon.TrackLength = Length;
	Ribbon.TrackWidth = Track->GetTrackWidth();
	Ribbon.CentimetresPerTexel = 9.765625;

	AddInfo(FString::Printf(
		TEXT("Spa layout: %.1f cm of lap, %d x %d texels, %lld bytes at RGBA16f"),
		Length,
		Ribbon.AlongTrackTexels(),
		Ribbon.AcrossTrackTexels(),
		Ribbon.TexelCount() * 8));

	TestTrue(
		TEXT("The rescale lands the lap within one percent of the requested length"),
		FMath::IsNearlyEqual(Length, 150000.0, 1500.0));

	TestTrue(TEXT("The Spa layout fits the ribbon unfolded"), Ribbon.FitsUnfolded());

	const FVector Start = Track->GetWorldPositionAtDistance(0.0, 0.0);
	const FVector AfterOneLap = Track->GetWorldPositionAtDistance(Length, 0.0);

	TestTrue(TEXT("The Spa layout is a closed loop"), Start.Equals(AfterOneLap, 1.0));

	double LowestPoint = TNumericLimits<double>::Max();
	double HighestPoint = TNumericLimits<double>::Lowest();
	double SolidLength = 0.0;
	double FlatLength = 0.0;

	constexpr int32 ElevationSamples = 2000;

	FVector Previous = Track->GetWorldPositionAtDistance(0.0, 0.0);

	for (int32 Sample = 1; Sample <= ElevationSamples; ++Sample)
	{
		const FVector Current = Track->GetWorldPositionAtDistance(Length * Sample / ElevationSamples, 0.0);

		LowestPoint = FMath::Min(LowestPoint, Current.Z);
		HighestPoint = FMath::Max(HighestPoint, Current.Z);

		SolidLength += (Current - Previous).Size();
		FlatLength += FVector2D(Current.X - Previous.X, Current.Y - Previous.Y).Size();

		Previous = Current;
	}

	const double Rise = HighestPoint - LowestPoint;

	AddInfo(FString::Printf(
		TEXT("Spa elevation: %.1f cm of rise, %.2f %% of the lap. Lap measures %.1f cm in three dimensions "
			"against %.1f cm projected flat."),
		Rise,
		100.0 * Rise / Length,
		SolidLength,
		FlatLength));

	TestTrue(TEXT("The Spa layout is not flat"), Rise > 1000.0);

	TestTrue(
		TEXT("Distance along the centreline follows the hill rather than its flat projection"),
		SolidLength > FlatLength + 1.0);

	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRacetrackCircuitSpaLayoutHasGradedCorners,
	"RubberAccumDemo.Track.SpaLayoutHasGradedCorners",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRacetrackCircuitSpaLayoutHasGradedCorners::RunTest(const FString& Parameters)
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

	Track->SetLayout(ETrackLayout::Spa, 150000.0);

	const double Length = Track->GetTrackLength();

	constexpr int32 SampleCount = 2000;
	int32 StraightSamples = 0;
	int32 IntermediateSamples = 0;
	int32 HardCorneringSamples = 0;

	for (int32 Sample = 0; Sample < SampleCount; ++Sample)
	{
		const double Amount = Track->GetCorneringAmountAtDistance(Length * Sample / SampleCount);

		StraightSamples += Amount < 0.15 ? 1 : 0;
		IntermediateSamples += (Amount >= 0.15 && Amount <= 0.7) ? 1 : 0;
		HardCorneringSamples += Amount > 0.7 ? 1 : 0;
	}

	AddInfo(FString::Printf(
		TEXT("Spa cornering distribution over %d samples: %.1f %% straight, %.1f %% intermediate, %.1f %% hard"),
		SampleCount,
		100.0 * StraightSamples / SampleCount,
		100.0 * IntermediateSamples / SampleCount,
		100.0 * HardCorneringSamples / SampleCount));

	AddInfo(FString::Printf(
		TEXT("Total turning %.5f radians against %.5f for any closed planar loop"),
		Track->GetTotalTurningRadians(),
		2.0 * UE_DOUBLE_PI));

	TestTrue(
		TEXT("The Spa layout also integrates to exactly one full turn"),
		FMath::IsNearlyEqual(FMath::Abs(Track->GetTotalTurningRadians()), 2.0 * UE_DOUBLE_PI, 0.15));

	TestTrue(TEXT("Spa has genuine straights where nothing is cornering"), StraightSamples > SampleCount / 10);
	TestTrue(TEXT("Spa has at least one corner at full cornering"), HardCorneringSamples > 0);

	TestTrue(
		TEXT("Spa grades its corners rather than being on or off like the stadium"),
		IntermediateSamples > SampleCount / 20);

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

	Car->ConfigureDriver(Track, Length * StraightLapFraction, 0.0, 0.0, UnusedDriverSeed);
	TickCar(*Car, 1);
	const double StraightSpeed = Car->GetSpeedCentimetresPerSecond();

	Car->ConfigureDriver(Track, Length * CornerLapFraction, 0.0, 0.0, UnusedDriverSeed);
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

	TestTrue(
		TEXT("The sample point just before the corner is still on a straight"),
		Track->GetCorneringAmountAtDistance(BeforeEntry) < 0.05);

	Car->ConfigureDriver(Track, BeforeEntry, 0.0, 0.0, UnusedDriverSeed);
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
	constexpr double WideBaselineAlphaNoSpawnedFieldProduces = 0.6;

	Uncommitted->ConfigureDriver(Track, CornerDistance, WideBaselineAlphaNoSpawnedFieldProduces, 0.0, UnusedDriverSeed);
	Committed->ConfigureDriver(Track, CornerDistance, WideBaselineAlphaNoSpawnedFieldProduces, 0.9, UnusedDriverSeed);

	TickCar(*Uncommitted, 60);
	TickCar(*Committed, 60);

	const double UncommittedAlpha = Uncommitted->GetLateralOffsetAlpha();
	const double CommittedAlpha = Committed->GetLateralOffsetAlpha();

	AddInfo(FString::Printf(
		TEXT("From the same baseline offset %.2f, the uncommitted driver holds %.3f and the committed driver holds %.3f"),
		WideBaselineAlphaNoSpawnedFieldProduces,
		UncommittedAlpha,
		CommittedAlpha));

	TestTrue(
		TEXT("A driver with no line commitment keeps its baseline offset through the corner"),
		FMath::IsNearlyEqual(UncommittedAlpha, WideBaselineAlphaNoSpawnedFieldProduces, 0.01));

	const double ApexSide = FMath::Sign(Track->GetSignedCorneringAtDistance(CornerDistance));

	TestTrue(
		TEXT("A committed driver crosses to the inside of the corner rather than to the centreline"),
		CommittedAlpha * ApexSide > 0.2);

	constexpr int32 SeededDriverCount = 8;
	constexpr double FieldBaselineAlpha = 0.0;

	TArray<ASplineFollowingCar*> SeededDrivers;
	TArray<double> ApexAlphas;

	for (int32 Index = 0; Index < SeededDriverCount; ++Index)
	{
		ASplineFollowingCar* Driver = World->SpawnActor<ASplineFollowingCar>();

		if (Driver == nullptr)
		{
			AddError(TEXT("Failed to spawn a seeded driver."));

			for (ASplineFollowingCar* Spawned : SeededDrivers)
			{
				Spawned->Destroy();
			}

			Committed->Destroy();
			Uncommitted->Destroy();
			Track->Destroy();
			return false;
		}

		SeededDrivers.Add(Driver);

		Driver->ConfigureDriver(Track, CornerDistance, FieldBaselineAlpha, 0.9, Index + 1);
		TickCar(*Driver, 60);

		ApexAlphas.Add(Driver->GetLateralOffsetAlpha());
	}

	double WidestArc = TNumericLimits<double>::Lowest();
	double TightestArc = TNumericLimits<double>::Max();
	FString ArcList;

	for (const double Alpha : ApexAlphas)
	{
		WidestArc = FMath::Max(WidestArc, Alpha);
		TightestArc = FMath::Min(TightestArc, Alpha);
		ArcList += FString::Printf(TEXT("%.4f "), Alpha);
	}

	const double ArcSpread = WidestArc - TightestArc;
	const double OneMarkWidthInAlpha = AlphaPerContactWidth(*Track, *Committed);

	AddInfo(FString::Printf(
		TEXT("%d drivers sharing baseline %.2f and commitment 0.90, differing only in seed, hold apex offsets "
			"%s- a spread of %.4f in alpha against a %.4f mark width"),
		SeededDriverCount,
		FieldBaselineAlpha,
		*ArcList,
		ArcSpread,
		OneMarkWidthInAlpha));

	TestTrue(
		TEXT("Drivers identical but for their seed fan into arcs separated by more than one mark width, which "
			"is what spreads traffic now that baselines no longer do"),
		ArcSpread > OneMarkWidthInAlpha);

	for (ASplineFollowingCar* Driver : SeededDrivers)
	{
		Driver->Destroy();
	}

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

	Car->ConfigureDriver(Track, Length * StraightLapFraction, 0.0, 0.0, UnusedDriverSeed);
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

	Car->ConfigureDriver(Track, Length * CornerLapFraction, 0.0, 0.0, UnusedDriverSeed);
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


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSplineFollowingCarFieldHoldsItsSpacing,
	"RubberAccumDemo.Car.FieldHoldsItsSpacing",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSplineFollowingCarFieldHoldsItsSpacing::RunTest(const FString& Parameters)
{
	UWorld* World = FindRenderableWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No renderable world available."));
		return false;
	}

	ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>();
	ASplineFollowingCar* Leader = World->SpawnActor<ASplineFollowingCar>();
	ASplineFollowingCar* Follower = World->SpawnActor<ASplineFollowingCar>();

	if (Track == nullptr || Leader == nullptr || Follower == nullptr)
	{
		AddError(TEXT("Failed to spawn the circuit or the cars."));
		return false;
	}

	constexpr double Spacing = 1400.0;

	Leader->ConfigureDriver(Track, Spacing, 0.3, 0.8, UnusedDriverSeed);
	Follower->ConfigureDriver(Track, 0.0, -0.3, 0.2, UnusedDriverSeed);

	const double StartingGap = Leader->GetDistanceAlongTrack() - Follower->GetDistanceAlongTrack();

	constexpr int32 LapTicks = 3000;

	TickCar(*Leader, LapTicks);
	TickCar(*Follower, LapTicks);

	const double FinalGap = Leader->GetDistanceAlongTrack() - Follower->GetDistanceAlongTrack();

	AddInfo(FString::Printf(
		TEXT("Two drivers on different lines and commitments started %.0f cm apart and ended %.0f cm "
			"apart after %d ticks, a drift of %.0f cm"),
		StartingGap,
		FinalGap,
		LapTicks,
		FinalGap - StartingGap));

	TestTrue(
		TEXT("Cars that differ only in line keep their grid spacing, so the field stays a train"),
		FMath::Abs(FinalGap - StartingGap) < Spacing * 0.5);

	Follower->Destroy();
	Leader->Destroy();
	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSplineFollowingCarTheSameSeedReproducesTheSameRun,
	"RubberAccumDemo.Car.TheSameSeedReproducesTheSameRun",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSplineFollowingCarTheSameSeedReproducesTheSameRun::RunTest(const FString& Parameters)
{
	UWorld* World = FindRenderableWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No renderable world available."));
		return false;
	}

	ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>();
	ASplineFollowingCar* First = World->SpawnActor<ASplineFollowingCar>();
	ASplineFollowingCar* Repeat = World->SpawnActor<ASplineFollowingCar>();
	ASplineFollowingCar* Different = World->SpawnActor<ASplineFollowingCar>();

	if (Track == nullptr || First == nullptr || Repeat == nullptr || Different == nullptr)
	{
		AddError(TEXT("Failed to spawn the circuit or the cars."));
		return false;
	}

	constexpr int32 SharedSeed = 4177;
	constexpr int32 OtherSeed = 4178;
	constexpr int32 SampleInterval = 50;
	constexpr int32 TicksAcrossACorner = 900;

	const double StartOnTheStraight = Track->GetTrackLength() * StraightLapFraction;

	First->ConfigureDriver(Track, StartOnTheStraight, 0.0, 0.9, SharedSeed);
	Repeat->ConfigureDriver(Track, StartOnTheStraight, 0.0, 0.9, SharedSeed);
	Different->ConfigureDriver(Track, StartOnTheStraight, 0.0, 0.9, OtherSeed);

	int32 MatchingSamples = 0;
	int32 DivergentSamples = 0;
	int32 SamplesWhereTheOtherSeedDiffers = 0;

	for (int32 Tick = 0; Tick < TicksAcrossACorner; ++Tick)
	{
		First->Tick(TickSeconds);
		Repeat->Tick(TickSeconds);
		Different->Tick(TickSeconds);

		if (Tick % SampleInterval != 0)
		{
			continue;
		}

		const bool bIdentical = First->GetLateralOffsetAlpha() == Repeat->GetLateralOffsetAlpha()
			&& First->GetDistanceAlongTrack() == Repeat->GetDistanceAlongTrack()
			&& First->GetCornerDeviationAlpha() == Repeat->GetCornerDeviationAlpha();

		MatchingSamples += bIdentical ? 1 : 0;
		DivergentSamples += bIdentical ? 0 : 1;

		SamplesWhereTheOtherSeedDiffers +=
			First->GetLateralOffsetAlpha() != Different->GetLateralOffsetAlpha() ? 1 : 0;
	}

	AddInfo(FString::Printf(
		TEXT("Over %d ticks sampled every %d, two cars on seed %d agreed exactly at %d of %d samples and "
			"differed at %d. Seed %d held deviation %.6f against %.6f"),
		TicksAcrossACorner,
		SampleInterval,
		SharedSeed,
		MatchingSamples,
		MatchingSamples + DivergentSamples,
		DivergentSamples,
		OtherSeed,
		Different->GetCornerDeviationAlpha(),
		First->GetCornerDeviationAlpha()));

	TestEqual(
		TEXT("Two cars given the same seed reproduce the run exactly, with no tolerance"),
		DivergentSamples,
		0);

	TestTrue(
		TEXT("The run actually crossed a corner, so the seeded draw was exercised rather than skipped"),
		First->GetCornerDeviationAlpha() != 0.0);

	TestTrue(
		TEXT("A different seed produces a different run, so the seed is genuinely read"),
		SamplesWhereTheOtherSeedDiffers > 0);

	Different->Destroy();
	Repeat->Destroy();
	First->Destroy();
	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSplineFollowingCarCornerDeviationsClusterOnTheApexRatherThanSmearing,
	"RubberAccumDemo.Car.CornerDeviationsClusterOnTheApexRatherThanSmearing",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSplineFollowingCarCornerDeviationsClusterOnTheApexRatherThanSmearing::RunTest(const FString& Parameters)
{
	UWorld* World = FindRenderableWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No renderable world available."));
		return false;
	}

	USurfaceAccumulationSubsystem* Subsystem = World->GetSubsystem<USurfaceAccumulationSubsystem>();

	ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>();
	ASplineFollowingCar* Car = World->SpawnActor<ASplineFollowingCar>();

	if (Track == nullptr || Car == nullptr)
	{
		AddError(TEXT("Failed to spawn the circuit or the car."));
		return false;
	}

	constexpr int32 DistributionSeed = 90125;
	constexpr int32 TicksAcrossManyLaps = 60000;
	constexpr int32 ContactDrainInterval = 5000;

	Car->ConfigureDriver(Track, Track->GetTrackLength() * StraightLapFraction, 0.0, 0.9, DistributionSeed);

	TArray<double> Deviations;
	int32 DrawsSeen = Car->GetCornerDrawCount();

	for (int32 Tick = 0; Tick < TicksAcrossManyLaps; ++Tick)
	{
		Car->Tick(TickSeconds);

		if (Car->GetCornerDrawCount() != DrawsSeen)
		{
			DrawsSeen = Car->GetCornerDrawCount();
			Deviations.Add(Car->GetCornerDeviationAlpha());
		}

		if (Subsystem != nullptr && Tick % ContactDrainInterval == ContactDrainInterval - 1)
		{
			Subsystem->ResetSurface();
		}
	}

	if (Deviations.Num() < 40)
	{
		AddError(FString::Printf(
			TEXT("Only %d corner draws in %d ticks, too few to say anything about the shape"),
			Deviations.Num(),
			TicksAcrossManyLaps));
		Car->Destroy();
		Track->Destroy();
		return false;
	}

	const double Limit = Car->GetMaxCornerDeviationAlpha();

	double Total = 0.0;
	double Largest = 0.0;
	int32 NearTheApex = 0;
	int32 OutsideTheLimit = 0;

	for (const double Deviation : Deviations)
	{
		Total += Deviation;
		Largest = FMath::Max(Largest, FMath::Abs(Deviation));
		NearTheApex += FMath::Abs(Deviation) < Limit / 3.0 ? 1 : 0;
		OutsideTheLimit += FMath::Abs(Deviation) > Limit ? 1 : 0;
	}

	const double Mean = Total / Deviations.Num();
	const double CentralShare = static_cast<double>(NearTheApex) / Deviations.Num();

	constexpr double TriangularCentralShare = 5.0 / 9.0;
	constexpr double UniformCentralShare = 1.0 / 3.0;

	AddInfo(FString::Printf(
		TEXT("%d corner draws on seed %d: mean %.5f, largest magnitude %.5f against a limit of %.5f, and "
			"%.1f %% of draws inside the central third against %.1f %% for a triangular draw and %.1f %% for "
			"a uniform one"),
		Deviations.Num(),
		DistributionSeed,
		Mean,
		Largest,
		Limit,
		100.0 * CentralShare,
		100.0 * TriangularCentralShare,
		100.0 * UniformCentralShare));

	TestEqual(
		TEXT("No draw escapes the deviation limit, so the apex target cannot leave the clamp"),
		OutsideTheLimit,
		0);

	TestTrue(
		TEXT("The draws are centred on the apex rather than biased to one side"),
		FMath::Abs(Mean) < Limit * 0.15);

	TestTrue(
		TEXT("The draws pile up on the apex rather than smearing evenly, which is what makes one dominant "
			"line instead of a band"),
		CentralShare > (TriangularCentralShare + UniformCentralShare) * 0.5);

	TestTrue(
		TEXT("The draws still reach the outer third, so the outliers are real arcs and not a clipped tail"),
		Largest > Limit * 0.66);

	if (Subsystem != nullptr)
	{
		Subsystem->ResetSurface();
	}

	Car->Destroy();
	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSplineFollowingCarHoldsOneDeviationFromCornerEntryToExit,
	"RubberAccumDemo.Car.HoldsOneDeviationFromCornerEntryToExit",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSplineFollowingCarHoldsOneDeviationFromCornerEntryToExit::RunTest(const FString& Parameters)
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

	constexpr int32 HeldSeed = 31337;
	constexpr int32 TicksAcrossTwoCorners = 3000;

	Car->ConfigureDriver(Track, Track->GetTrackLength() * StraightLapFraction, 0.0, 0.9, HeldSeed);

	bool bSeenFirstCorner = false;
	double DeviationAtFirstDraw = 0.0;
	double DeviationAtSecondDraw = 0.0;
	int32 TicksInsideTheFirstCorner = 0;
	int32 ChangesWhileCommitted = 0;
	int32 DrawsSeen = 0;

	const double StartingDistance = Car->GetDistanceAlongTrack();
	TArray<double> DrawDistances;

	for (int32 Tick = 0; Tick < TicksAcrossTwoCorners; ++Tick)
	{
		Car->Tick(TickSeconds);

		const double Deviation = Car->GetCornerDeviationAlpha();

		if (Car->GetCornerDrawCount() != DrawsSeen)
		{
			DrawsSeen = Car->GetCornerDrawCount();
			DrawDistances.Add(Car->GetDistanceAlongTrack());

			if (DrawsSeen == 1)
			{
				bSeenFirstCorner = true;
				DeviationAtFirstDraw = Deviation;
			}
			else if (DrawsSeen == 2)
			{
				DeviationAtSecondDraw = Deviation;
			}
		}

		if (Car->IsCommittedToACorner() && DrawsSeen == 1)
		{
			++TicksInsideTheFirstCorner;
			ChangesWhileCommitted += Deviation != DeviationAtFirstDraw ? 1 : 0;
		}
	}

	const double DistanceCovered = Car->GetDistanceAlongTrack() - StartingDistance;
	const double LapsCovered = DistanceCovered / Track->GetTrackLength();
	const int32 CornersPassed = FMath::CeilToInt32(2.0 * LapsCovered);

	FString DrawGaps;

	for (int32 Index = 1; Index < DrawDistances.Num(); ++Index)
	{
		DrawGaps += FString::Printf(TEXT("%.0f "), DrawDistances[Index] - DrawDistances[Index - 1]);
	}

	AddInfo(FString::Printf(
		TEXT("Drew %d times in %d ticks covering %.0f cm, which is %.2f laps of a %.0f cm circuit with two "
			"corners a lap, so %.2f draws a corner. Gaps between draws: %s"),
		DrawsSeen,
		TicksAcrossTwoCorners,
		DistanceCovered,
		LapsCovered,
		Track->GetTrackLength(),
		DrawsSeen / FMath::Max(2.0 * LapsCovered, UE_DOUBLE_SMALL_NUMBER),
		*DrawGaps));

	AddInfo(FString::Printf(
		TEXT("The first held deviation %.6f unchanged across %d committed ticks, and the second drew %.6f"),
		DeviationAtFirstDraw,
		TicksInsideTheFirstCorner,
		DeviationAtSecondDraw));

	constexpr double ProbeSpacing = 25.0;
	const int32 ProbeCount = FMath::FloorToInt32(Track->GetTrackLength() / ProbeSpacing);

	FString LowRuns;
	int32 RunLength = 0;

	for (int32 Probe = 0; Probe <= ProbeCount; ++Probe)
	{
		const double Distance = Probe * ProbeSpacing;
		const double Signal = FMath::Max(
			Track->GetCorneringAmountAtDistance(Distance),
			Track->GetCorneringAmountAtDistance(Distance + 2500.0));

		if (Signal <= 0.05 && Probe < ProbeCount)
		{
			++RunLength;
			continue;
		}

		if (RunLength > 0)
		{
			LowRuns += FString::Printf(
				TEXT("%.0f@%.0f "), RunLength * ProbeSpacing, (Probe - RunLength) * ProbeSpacing);
			RunLength = 0;
		}
	}

	AddInfo(FString::Printf(
		TEXT("Runs of the trigger signal at or below the exit threshold over one lap, as length@start: %s"),
		*LowRuns));

	TestTrue(TEXT("The run entered a corner at all"), bSeenFirstCorner);

	TestTrue(
		TEXT("The car draws once a corner rather than several times, so a dip in the cornering profile "
			"cannot split one corner into several arcs"),
		DrawsSeen <= CornersPassed + 1);

	TestTrue(
		TEXT("The car does draw on every corner rather than skipping them"),
		DrawsSeen >= CornersPassed - 1);

	TestTrue(
		TEXT("The corner lasted long enough for a per-frame redraw to have shown up"),
		TicksInsideTheFirstCorner > 100);

	TestEqual(
		TEXT("The deviation drawn on entry does not change anywhere inside the corner, so the car takes one "
			"arc rather than a jittering one"),
		ChangesWhileCommitted,
		0);

	TestTrue(
		TEXT("The run reached a second corner, so a redraw would have been observable"),
		DrawsSeen >= 2);

	TestTrue(
		TEXT("The next corner draws again rather than holding the first deviation for the whole lap"),
		DeviationAtSecondDraw != DeviationAtFirstDraw);

	Car->Destroy();
	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRacetrackCircuitSpawnedStripesMergeOnTheStraight,
	"RubberAccumDemo.Track.SpawnedStripesMergeOnTheStraight",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRacetrackCircuitSpawnedStripesMergeOnTheStraight::RunTest(const FString& Parameters)
{
	UWorld* World = FindRenderableWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No renderable world available."));
		return false;
	}

	ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>();
	ASplineFollowingCar* Reference = World->SpawnActor<ASplineFollowingCar>();

	if (Track == nullptr || Reference == nullptr)
	{
		AddError(TEXT("Failed to spawn the circuit or the reference car."));
		return false;
	}

	const double Spacing = Track->GetAdjacentDriverSpacingCentimetres();
	const double MarkWidth = Reference->GetContactWidth();
	const double MergeTarget = MarkWidth / 4.0;
	const double BandWidth = 2.0 * Track->GetDriverLateralSpread() * Track->GetTrackWidth() * 0.5 + MarkWidth;
	const double CarsOverATexel = Spacing > UE_DOUBLE_SMALL_NUMBER
		? FMath::Min(MarkWidth / Spacing, static_cast<double>(Track->GetCarCount()))
		: Track->GetCarCount();

	AddInfo(FString::Printf(
		TEXT("%d cars across %.4f of the track width on a %.0f cm track put adjacent baselines %.3f cm apart "
			"against a %.1f cm mark, so the target is %.3f cm, the merged band is %.1f cm wide and a straight "
			"texel is crossed by %.2f cars a lap"),
		Track->GetCarCount(),
		Track->GetDriverLateralSpread() * 2.0,
		Track->GetTrackWidth(),
		Spacing,
		MarkWidth,
		MergeTarget,
		BandWidth,
		CarsOverATexel));

	TestTrue(
		TEXT("Adjacent stripes overlap at least fourfold, so the field paints one merged band on a straight "
			"rather than separate stripes with gaps between them"),
		Spacing <= MergeTarget);

	TestTrue(
		TEXT("The field is not collapsed onto a single authored line, so the dominant line is still where "
			"independent drivers converge rather than a constant in a header"),
		Spacing > 0.0);

	TestTrue(
		TEXT("The merged band is still a band and not the whole track"),
		BandWidth < Track->GetTrackWidth() * 0.5);

	Reference->Destroy();
	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRacetrackCircuitCountedPassesOverATexelAgainstThePrediction,
	"RubberAccumDemo.Track.CountedPassesOverATexelAgainstThePrediction",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRacetrackCircuitCountedPassesOverATexelAgainstThePrediction::RunTest(const FString& Parameters)
{
	UWorld* World = FindRenderableWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No renderable world available."));
		return false;
	}

	USurfaceAccumulationSubsystem* Subsystem = World->GetSubsystem<USurfaceAccumulationSubsystem>();

	ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>();

	if (Track == nullptr)
	{
		AddError(TEXT("Failed to spawn ARacetrackCircuit."));
		return false;
	}

	const int32 FieldSize = Track->GetCarCount();
	const double Length = Track->GetTrackLength();

	if (FieldSize <= 1 || Length <= UE_DOUBLE_SMALL_NUMBER)
	{
		AddError(TEXT("The circuit reports no usable field or no length."));
		Track->Destroy();
		return false;
	}

	TArray<ASplineFollowingCar*> Field;
	TArray<double> PreviousDistances;

	for (int32 Index = 0; Index < FieldSize; ++Index)
	{
		ASplineFollowingCar* Car = World->SpawnActor<ASplineFollowingCar>();

		if (Car == nullptr)
		{
			AddError(TEXT("Failed to spawn a car for the field."));

			for (ASplineFollowingCar* Spawned : Field)
			{
				Spawned->Destroy();
			}

			Track->Destroy();
			return false;
		}

		Car->ConfigureDriver(
			Track,
			Length * StraightLapFraction,
			Track->GetDriverBaselineLateralOffsetAlpha(Index),
			Track->GetDriverLineCommitment(Index),
			Track->GetDriverSeedForGridIndex(Index));

		Field.Add(Car);
		PreviousDistances.Add(Car->GetDistanceAlongTrack());
	}

	const double MarkWidth = Field[0]->GetContactWidth();
	const double HalfMarkInAlpha = AlphaPerContactWidth(*Track, *Field[0]) * 0.5;

	constexpr int32 LayoutSlices = 2000;

	double StraightDistance = -1.0;
	double ApexDistance = -1.0;
	int32 LongestStraightRun = 0;
	int32 LongestApexRun = 0;
	int32 StraightRun = 0;
	int32 ApexRun = 0;

	for (int32 Slice = 0; Slice <= 2 * LayoutSlices; ++Slice)
	{
		const double Cornering = Track->GetCorneringAmountAtDistance(
			Length * (Slice % LayoutSlices) / LayoutSlices);

		StraightRun = Cornering < 0.02 ? StraightRun + 1 : 0;
		ApexRun = Cornering > 0.98 ? ApexRun + 1 : 0;

		if (StraightRun > LongestStraightRun)
		{
			LongestStraightRun = StraightRun;
			StraightDistance = Length * (Slice - StraightRun * 0.5) / LayoutSlices;
		}

		if (ApexRun > LongestApexRun)
		{
			LongestApexRun = ApexRun;
			ApexDistance = Length * (Slice - ApexRun * 0.5) / LayoutSlices;
		}
	}

	if (StraightDistance < 0.0 || ApexDistance < 0.0)
	{
		AddError(TEXT("Could not find both a straight and an apex sample on this layout."));

		for (ASplineFollowingCar* Spawned : Field)
		{
			Spawned->Destroy();
		}

		Track->Destroy();
		return false;
	}

	const FSurfaceTrackRibbon& Ribbon = Subsystem != nullptr ? Subsystem->GetRibbon() : FSurfaceTrackRibbon();
	const double CentimetresPerTexel = Ribbon.SafeCentimetresPerTexel();
	const int32 LateralTexels = FMath::Max(FMath::CeilToInt32(Track->GetTrackWidth() / CentimetresPerTexel), 1);

	TArray<int32> StraightHits;
	TArray<int32> ApexHits;
	StraightHits.SetNumZeroed(LateralTexels);
	ApexHits.SetNumZeroed(LateralTexels);

	constexpr int32 TicksAcrossSeveralLaps = 12000;
	double TotalDistanceCovered = 0.0;

	for (int32 Tick = 0; Tick < TicksAcrossSeveralLaps; ++Tick)
	{
		for (int32 Index = 0; Index < Field.Num(); ++Index)
		{
			ASplineFollowingCar* Car = Field[Index];
			const double Before = PreviousDistances[Index];

			Car->Tick(TickSeconds);

			const double After = Car->GetDistanceAlongTrack();
			PreviousDistances[Index] = After;

			const int32 StraightCrossings = FMath::FloorToInt32((After - StraightDistance) / Length)
				- FMath::FloorToInt32((Before - StraightDistance) / Length);

			const int32 ApexCrossings = FMath::FloorToInt32((After - ApexDistance) / Length)
				- FMath::FloorToInt32((Before - ApexDistance) / Length);

			if (StraightCrossings == 0 && ApexCrossings == 0)
			{
				continue;
			}

			const double Alpha = Car->GetLateralOffsetAlpha();

			for (int32 Texel = 0; Texel < LateralTexels; ++Texel)
			{
				const double TexelAlpha = 2.0 * (Texel + 0.5) / LateralTexels - 1.0;

				if (FMath::Abs(Alpha - TexelAlpha) > HalfMarkInAlpha)
				{
					continue;
				}

				StraightHits[Texel] += StraightCrossings;
				ApexHits[Texel] += ApexCrossings;
			}
		}

		if (Subsystem != nullptr && Tick % 4000 == 3999)
		{
			Subsystem->ResetSurface();
		}
	}

	for (int32 Index = 0; Index < Field.Num(); ++Index)
	{
		TotalDistanceCovered += Field[Index]->GetDistanceAlongTrack() - Length * StraightLapFraction;
	}

	const double LapsEachCar = TotalDistanceCovered / (Field.Num() * Length);

	int32 MostStraightHits = 0;
	int32 MostApexHits = 0;
	int32 StraightTexelsInked = 0;
	int32 ApexTexelsInked = 0;

	for (int32 Texel = 0; Texel < LateralTexels; ++Texel)
	{
		MostStraightHits = FMath::Max(MostStraightHits, StraightHits[Texel]);
		MostApexHits = FMath::Max(MostApexHits, ApexHits[Texel]);
		StraightTexelsInked += StraightHits[Texel] > 0 ? 1 : 0;
		ApexTexelsInked += ApexHits[Texel] > 0 ? 1 : 0;
	}

	const double StraightPassesPerLap = MostStraightHits / FMath::Max(LapsEachCar, UE_DOUBLE_SMALL_NUMBER);
	const double ApexPassesPerLap = MostApexHits / FMath::Max(LapsEachCar, UE_DOUBLE_SMALL_NUMBER);

	const double PredictedStraightPasses = FMath::Clamp(
		MarkWidth / Track->GetAdjacentDriverSpacingCentimetres(), 1.0, static_cast<double>(FieldSize));

	const float StraightPerContact = RacetrackSurfaceMapping::SlipToExposure(
		Field[0]->GetStraightSlipAmount(), USurfaceAccumulationSubsystem::GetDepositionScale());

	const float ApexPerContact = RacetrackSurfaceMapping::SlipToExposure(
		Field[0]->GetCorneringSlipAmount(), USurfaceAccumulationSubsystem::GetDepositionScale());

	const double StraightExposurePerLap = StraightPassesPerLap * StraightPerContact;
	const double ApexExposurePerLap = ApexPassesPerLap * ApexPerContact;

	AddInfo(FString::Printf(
		TEXT("%d cars over %.2f laps, counting passes across %d lateral texels at %.4f cm each, sampled at "
			"the middle of the longest straight and the longest apex. The busiest straight texel took %.2f "
			"passes a lap across %d inked texels, against %.2f predicted from the stripe spacing. The "
			"busiest apex texel took %.2f across %d inked texels, against the %d the one-texel figure assumes"),
		FieldSize,
		LapsEachCar,
		LateralTexels,
		CentimetresPerTexel,
		StraightPassesPerLap,
		StraightTexelsInked,
		PredictedStraightPasses,
		ApexPassesPerLap,
		ApexTexelsInked,
		FieldSize));

	AddInfo(FString::Printf(
		TEXT("Measured exposure a lap: %.6f on the busiest straight texel and %.6f on the busiest apex "
			"texel, so 63 %% coverage in about %.0f and %.0f laps, and a measured contrast of %.1f to 1"),
		StraightExposurePerLap,
		ApexExposurePerLap,
		1.0 / (3.0 * FMath::Max(StraightExposurePerLap, UE_DOUBLE_SMALL_NUMBER)),
		1.0 / (3.0 * FMath::Max(ApexExposurePerLap, UE_DOUBLE_SMALL_NUMBER)),
		ApexExposurePerLap / FMath::Max(StraightExposurePerLap, UE_DOUBLE_SMALL_NUMBER)));

	TestTrue(
		TEXT("The field completed enough laps for a per-lap count to mean anything"),
		LapsEachCar > 2.0);

	TestTrue(
		TEXT("No straight texel is crossed by the whole field, so the saturation table may not treat a "
			"straight texel as taking every car the way an apex texel is assumed to"),
		StraightPassesPerLap < FieldSize * 0.5);

	TestTrue(
		TEXT("No apex texel is crossed by the whole field either, so the one-texel apex figure is an upper "
			"bound on concentration and not a prediction"),
		ApexPassesPerLap < FieldSize);

	TestTrue(
		TEXT("The apex still lays more exposure a lap than the straight, so a racing line appears at all"),
		ApexExposurePerLap > StraightExposurePerLap);

	TestTrue(
		TEXT("The contrast between apex and straight has collapsed from the hundredfold the separated "
			"stripes gave, which is the change C6 is for"),
		ApexExposurePerLap / FMath::Max(StraightExposurePerLap, UE_DOUBLE_SMALL_NUMBER) < 20.0);

	if (Subsystem != nullptr)
	{
		Subsystem->ResetSurface();
	}

	for (ASplineFollowingCar* Car : Field)
	{
		Car->Destroy();
	}

	Track->Destroy();

	return true;
}

#endif
