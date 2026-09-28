#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SplineComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "SurfaceAccumulationBuffer.h"
#include "SurfaceAccumulationMapping.h"
#include "SurfaceAccumulationSubsystem.h"
#include "SurfaceTrackRibbon.h"
#include "SurfaceTrackSpace.h"
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

	bool WheelLateralOffsets(
		const ARacetrackCircuit& Track,
		const ASplineFollowingCar& Car,
		TArray<double>& OutAlphas)
	{
		const USplineComponent* Centreline = Track.GetCentreline();

		if (Centreline == nullptr)
		{
			return false;
		}

		OutAlphas.Reset(ASplineFollowingCar::WheelCount);

		for (const FVector& WheelPosition : Car.GetWheelPositions())
		{
			FSurfaceTrackCoordinate Coordinate;

			RacetrackSurfaceTrackSpace::WorldToTrack(
				*Centreline, Track.GetTrackWidth(), WheelPosition, Coordinate);

			OutAlphas.Add(Coordinate.LateralOffsetAlpha);
		}

		return true;
	}

	double TightestRadiusOverAChord(const ARacetrackCircuit& Track, double ChordCentimetres)
	{
		const double Length = Track.GetTrackLength();
		const double Chord = FMath::Max(ChordCentimetres, 1.0);
		const int32 SampleCount = FMath::Clamp(FMath::RoundToInt32(Length / Chord), 8, 8192);

		double Tightest = TNumericLimits<double>::Max();

		for (int32 Sample = 0; Sample < SampleCount; ++Sample)
		{
			const double Distance = Length * Sample / SampleCount;

			const FVector Behind = Track.GetWorldPositionAtDistance(Distance - Chord, 0.0);
			const FVector At = Track.GetWorldPositionAtDistance(Distance, 0.0);
			const FVector Ahead = Track.GetWorldPositionAtDistance(Distance + Chord, 0.0);

			const double SideA = FVector2D(At.X - Behind.X, At.Y - Behind.Y).Size();
			const double SideB = FVector2D(Ahead.X - At.X, Ahead.Y - At.Y).Size();
			const double SideC = FVector2D(Ahead.X - Behind.X, Ahead.Y - Behind.Y).Size();

			const double TwiceArea = FMath::Abs(
				(At.X - Behind.X) * (Ahead.Y - Behind.Y) - (At.Y - Behind.Y) * (Ahead.X - Behind.X));

			if (TwiceArea <= UE_DOUBLE_SMALL_NUMBER)
			{
				continue;
			}

			Tightest = FMath::Min(Tightest, SideA * SideB * SideC / (2.0 * TwiceArea));
		}

		return Tightest;
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
	FRacetrackCircuitSpaLayoutFoldsIntoFourRows,
	"RubberAccumDemo.Track.SpaLayoutFoldsIntoFourRows",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRacetrackCircuitSpaLayoutFoldsIntoFourRows::RunTest(const FString& Parameters)
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

	const double RequestedLapLength = Track->GetAuthoredLayoutLapLength();

	Track->SetLayout(ETrackLayout::Spa, RequestedLapLength);

	const double Length = Track->GetTrackLength();
	const double MarkWidth = GetDefault<ASplineFollowingCar>()->GetContactWidth();

	FSurfaceTrackRibbon Ribbon;
	Ribbon.TrackLength = Length;
	Ribbon.TrackWidth = Track->GetTrackWidth();
	Ribbon.CentimetresPerTexel = Track->GetSurfaceCentimetresPerTexel();

	AddInfo(FString::Printf(
		TEXT("Spa layout: %.1f cm of lap at %.1f cm wide, %.6f cm per texel, %d x %d texels in %d rows of %d, "
			"allocating %d x %d for %lld bytes at RGBA16f, and a %.1f cm contact patch stores as %.2f texels"),
		Length,
		Ribbon.TrackWidth,
		Ribbon.CentimetresPerTexel,
		Ribbon.AlongTrackTexels(),
		Ribbon.AcrossTrackTexels(),
		Ribbon.RowCount(),
		Ribbon.TexelsPerRow(),
		Ribbon.AtlasWidth(),
		Ribbon.AtlasHeight(),
		Ribbon.AtlasTexelCount() * 8,
		MarkWidth,
		Ribbon.WorldDistanceToTexels(MarkWidth)));

	TestTrue(
		TEXT("The rescale lands the lap within one percent of the requested length"),
		FMath::IsNearlyEqual(Length, RequestedLapLength, RequestedLapLength * 0.01));

	TestEqual(TEXT("The Spa layout folds into four rows"), Ribbon.RowCount(), 4);
	TestTrue(TEXT("The folded Spa layout fits the texture cap"), Ribbon.FitsTheTextureCap());

	AddInfo(FString::Printf(
		TEXT("A row uses %d of the %d texel maximum, leaving %d of margin against a rescale landing a "
			"fraction long"),
		Ribbon.TexelsPerRow(),
		FSurfaceTrackRibbon::MaximumTexelsPerRow,
		FSurfaceTrackRibbon::MaximumTexelsPerRow - Ribbon.TexelsPerRow()));

	TestTrue(
		TEXT("A row keeps real margin under the maximum rather than sitting on it, so a lap that rescales a "
			"fraction long cannot push the ceil over the cap and refuse the allocation"),
		Ribbon.TexelsPerRow() <= FSurfaceTrackRibbon::MaximumTexelsPerRow - 512);

	const int64 CommittedBytes = 117964800ll;
	const int64 AllocatedBytes = Ribbon.AtlasTexelCount() * 8;

	TestTrue(
		FString::Printf(
			TEXT("The buffer allocates the %lld bytes C9 committed to, give or take what the measured rescale "
				"moves the ceil by, got %lld"),
			CommittedBytes,
			AllocatedBytes),
		FMath::Abs(AllocatedBytes - CommittedBytes) <= CommittedBytes / 100);

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

	Track->SetLayout(ETrackLayout::Spa, Track->GetAuthoredLayoutLapLength());

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
	FSplineFollowingCarRegistersFourContactsATick,
	"RubberAccumDemo.Car.RegistersFourContactsATick",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSplineFollowingCarRegistersFourContactsATick::RunTest(const FString& Parameters)
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

	float StraightSlip = 0.0f;
	float CornerSlip = 0.0f;
	int32 StraightContactCount = 0;
	int32 CornerContactCount = 0;

	Car->ConfigureDriver(Track, Length * StraightLapFraction, 0.0, 0.0, UnusedDriverSeed);
	Subsystem->ResetSurface();
	Car->Tick(TickSeconds);

	StraightContactCount = Subsystem->GetPendingContactCount();

	for (const FSurfaceTrackContact& Contact : Subsystem->GetPendingContacts())
	{
		StraightSlip = FMath::Max(StraightSlip, Contact.SlipAmount);
	}

	Car->ConfigureDriver(Track, Length * CornerLapFraction, 0.0, 0.0, UnusedDriverSeed);
	TickCar(*Car, 30);
	Subsystem->ResetSurface();
	Car->Tick(TickSeconds);

	CornerContactCount = Subsystem->GetPendingContactCount();
	CornerSlip = 1.0f;

	for (const FSurfaceTrackContact& Contact : Subsystem->GetPendingContacts())
	{
		CornerSlip = FMath::Min(CornerSlip, Contact.SlipAmount);
	}

	AddInfo(FString::Printf(
		TEXT("One car tick registered %d contacts on a straight and %d in a corner, against %d wheels. The "
			"least-slipping wheel carries %.3f in the corner against %.3f on the straight"),
		StraightContactCount,
		CornerContactCount,
		ASplineFollowingCar::WheelCount,
		CornerSlip,
		StraightSlip));

	TestEqual(
		TEXT("One car tick on a straight registers one contact per wheel through the world-space API, not one "
			"for the car"),
		StraightContactCount,
		ASplineFollowingCar::WheelCount);

	TestEqual(
		TEXT("One car tick in a corner registers one contact per wheel and nothing else, because marbles are "
			"the plugin's to throw rather than a contact of their own"),
		CornerContactCount,
		ASplineFollowingCar::WheelCount);

	TestTrue(
		TEXT("Even the least-slipping wheel in a corner carries more slip than any wheel on a straight, so "
			"the comparison is about cornering rather than about a locked wheel"),
		CornerContactCount > 0 && CornerSlip > StraightSlip);

	Subsystem->ResetSurface();

	Car->Destroy();
	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSplineFollowingCarLaysFourTracksInACornerAndTwoOnAStraight,
	"RubberAccumDemo.Car.LaysFourTracksInACornerAndTwoOnAStraight",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSplineFollowingCarLaysFourTracksInACornerAndTwoOnAStraight::RunTest(const FString& Parameters)
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

	const double MarkWidthInAlpha = AlphaPerContactWidth(*Track, *Car);
	const double HalfWidth = Track->GetTrackWidth() * 0.5;
	const double Wheelbase = Car->GetWheelbase();

	const int32 FrontLeft = static_cast<int32>(ECarWheel::FrontLeft);
	const int32 FrontRight = static_cast<int32>(ECarWheel::FrontRight);
	const int32 RearLeft = static_cast<int32>(ECarWheel::RearLeft);

	TArray<double> Alphas;

	const double StadiumLength = Track->GetTrackLength();
	const double SteadyCornerDistance = StadiumLength * CornerLapFraction;

	Car->ConfigureDriver(Track, SteadyCornerDistance, 0.0, 0.0, UnusedDriverSeed);
	TickCar(*Car, 120);

	const bool bSteadyRead = WheelLateralOffsets(*Track, *Car, Alphas);

	const double SteadyInsideSign =
		FMath::Sign(Track->GetSignedCorneringAtDistance(Car->GetDistanceAlongTrack()));

	const double SteadyOfftracking = bSteadyRead ? (Alphas[RearLeft] - Alphas[FrontLeft]) * HalfWidth : 0.0;
	const double SteadySideSeparation = bSteadyRead ? FMath::Abs(Alphas[FrontLeft] - Alphas[FrontRight]) : 0.0;
	const double PredictedOfftracking = Wheelbase * Wheelbase / (2.0 * Track->GetCornerRadius());

	Car->ConfigureDriver(Track, 0.0, 0.0, 0.0, UnusedDriverSeed);
	TickCar(*Car, 30);

	const bool bStraightRead = WheelLateralOffsets(*Track, *Car, Alphas);
	const double StraightSeparation = bStraightRead ? FMath::Abs(Alphas[RearLeft] - Alphas[FrontLeft]) : 0.0;

	if (!bSteadyRead || !bStraightRead)
	{
		AddError(TEXT("The circuit reported no centreline to convert wheel positions against."));
		Car->Destroy();
		Track->Destroy();
		return false;
	}

	AddInfo(FString::Printf(
		TEXT("Stadium, constant %0.f cm radius, a driver holding the centreline with no line commitment so "
			"nothing but the wheelbase separates the axles: the rear tracks %.3f cm to the %s of the front "
			"against %.3f cm from Wheelbase squared over twice the radius. Left to right separation is "
			"%.2f cm against a %.0f cm wheel track, and a straight gives %.3f cm"),
		Track->GetCornerRadius(),
		FMath::Abs(SteadyOfftracking),
		SteadyOfftracking * SteadyInsideSign > 0.0 ? TEXT("inside") : TEXT("outside"),
		PredictedOfftracking,
		SteadySideSeparation * HalfWidth,
		Car->GetWheelTrack(),
		StraightSeparation * HalfWidth));

	TestTrue(
		TEXT("In steady cornering the rear axle tracks to the inside of the front, which is the offtracking "
			"a rigid wheelbase produces and is what makes four wheels four tracks rather than two"),
		SteadyOfftracking * SteadyInsideSign > 0.0);

	TestTrue(
		TEXT("Steady-state offtracking matches Wheelbase squared over twice the corner radius, so the pose "
			"is the bicycle model rather than four offsets that happen to differ"),
		FMath::IsNearlyEqual(
			FMath::Abs(SteadyOfftracking), PredictedOfftracking, PredictedOfftracking * 0.35));

	TestTrue(
		TEXT("A car running straight lays two tracks, not four, because the front and rear wheels on one "
			"side then sit at the same lateral offset"),
		StraightSeparation < MarkWidthInAlpha * 0.25);

	TestTrue(
		TEXT("Left and right wheels stay a wheel track apart, so a single locked wheel is separable from "
			"its opposite number"),
		FMath::IsNearlyEqual(
			SteadySideSeparation * HalfWidth, Car->GetWheelTrack(), Car->GetWheelTrack() * 0.1));

	TestTrue(
		TEXT("The wheel track is wider than a contact patch, without which four tracks could never read as "
			"four"),
		Car->GetWheelTrack() > Car->GetContactWidth());

	Track->SetLayout(ETrackLayout::Spa, Track->GetAuthoredLayoutLapLength());

	const double SpaLength = Track->GetTrackLength();

	Car->ConfigureDriver(Track, 0.0, 0.0, 0.8, UnusedDriverSeed);

	constexpr int32 MostTicksWorthWaiting = 60000;

	constexpr double StraightnessBands[] = { 0.05, 0.02, 0.01 };
	constexpr int32 StraightestBand = UE_ARRAY_COUNT(StraightnessBands) - 1;

	double WidestSeparation = 0.0;
	double WidestSeparationInBand[UE_ARRAY_COUNT(StraightnessBands)] = {};
	int32 SamplesInBand[UE_ARRAY_COUNT(StraightnessBands)] = {};
	int32 SamplesOnTheSecondLap = 0;
	int32 SamplesSeparatedByAMark = 0;

	for (int32 Tick = 0;
		Tick < MostTicksWorthWaiting && Car->GetDistanceAlongTrack() < SpaLength * 2.0;
		++Tick)
	{
		Car->Tick(TickSeconds);

		if (Car->GetDistanceAlongTrack() < SpaLength || !WheelLateralOffsets(*Track, *Car, Alphas))
		{
			continue;
		}

		const double Separation = FMath::Abs(Alphas[RearLeft] - Alphas[FrontLeft]);
		const double Cornering = Track->GetCorneringAmountAtDistance(Car->GetDistanceAlongTrack());

		++SamplesOnTheSecondLap;
		SamplesSeparatedByAMark += Separation > MarkWidthInAlpha ? 1 : 0;
		WidestSeparation = FMath::Max(WidestSeparation, Separation);

		for (int32 Band = 0; Band < UE_ARRAY_COUNT(StraightnessBands); ++Band)
		{
			if (Cornering < StraightnessBands[Band])
			{
				WidestSeparationInBand[Band] = FMath::Max(WidestSeparationInBand[Band], Separation);
				++SamplesInBand[Band];
			}
		}
	}

	AddInfo(FString::Printf(
		TEXT("Spa at %.0f cm of lap, one driver over %d sampled ticks of a settled lap: the front and rear "
			"tracks separate by at most %.2f cm, which is %.2f mark widths, and exceed one mark width over "
			"%.1f %% of the lap"),
		SpaLength,
		SamplesOnTheSecondLap,
		WidestSeparation * HalfWidth,
		WidestSeparation / MarkWidthInAlpha,
		SamplesOnTheSecondLap > 0 ? 100.0 * SamplesSeparatedByAMark / SamplesOnTheSecondLap : 0.0));

	for (int32 Band = 0; Band < UE_ARRAY_COUNT(StraightnessBands); ++Band)
	{
		AddInfo(FString::Printf(
			TEXT("Below %.2f of sustained cornering, which is %.1f %% of the lap, the widest separation is "
				"%.2f cm, %.0f %% of the corner maximum"),
			StraightnessBands[Band],
			SamplesOnTheSecondLap > 0 ? 100.0 * SamplesInBand[Band] / SamplesOnTheSecondLap : 0.0,
			WidestSeparationInBand[Band] * HalfWidth,
			WidestSeparation > 0.0 ? 100.0 * WidestSeparationInBand[Band] / WidestSeparation : 0.0));
	}

	TestTrue(
		TEXT("The lap was actually sampled rather than the car never completing one"),
		SamplesOnTheSecondLap > 1000);

	TestTrue(
		TEXT("On the rescaled layout the front and rear tracks separate by more than a contact patch, so "
			"four wheels store as four marks rather than two, which they would not at the old scale"),
		WidestSeparation > MarkWidthInAlpha);

	TestTrue(
		TEXT("A corner adds more than a whole contact patch of separation on top of whatever the straightest "
			"track shows, so the separation is corner geometry and not a constant offset applied everywhere"),
		WidestSeparation - WidestSeparationInBand[StraightestBand] > MarkWidthInAlpha);

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
	const double WheelTrack = Reference->GetWheelTrack();
	const double BandWidth = 2.0 * Track->GetDriverLateralSpread() * Track->GetTrackWidth() * 0.5 + MarkWidth;
	const double GapBetweenBands = WheelTrack - BandWidth;
	const double WholeFeature = WheelTrack + BandWidth;
	const double CarsOverATexel = Spacing > UE_DOUBLE_SMALL_NUMBER
		? FMath::Min(MarkWidth / Spacing, static_cast<double>(Track->GetCarCount()))
		: Track->GetCarCount();

	AddInfo(FString::Printf(
		TEXT("%d cars across %.4f of the track width on a %.0f cm track put adjacent baselines %.3f cm apart "
			"against a %.1f cm mark, so the per-band target is %.3f cm and a straight texel is crossed by "
			"%.2f cars a lap, each laying %d coincident wheels"),
		Track->GetCarCount(),
		Track->GetDriverLateralSpread() * 2.0,
		Track->GetTrackWidth(),
		Spacing,
		MarkWidth,
		MergeTarget,
		CarsOverATexel,
		ASplineFollowingCar::WheelsSharingATrack));

	AddInfo(FString::Printf(
		TEXT("Four wheels put a second band at the %.0f cm wheel track. Each band is %.1f cm wide, so the "
			"two are %s by %.1f cm and the whole feature spans %.1f cm, which is %.1f %% of the track"),
		WheelTrack,
		BandWidth,
		GapBetweenBands > 0.0 ? TEXT("separated") : TEXT("overlapped"),
		FMath::Abs(GapBetweenBands),
		WholeFeature,
		100.0 * WholeFeature / Track->GetTrackWidth()));

	TestTrue(
		TEXT("Adjacent cars' corresponding wheels overlap at least fourfold, so each wheel band merges on a "
			"straight rather than reading as separate stripes with gaps between them"),
		Spacing <= MergeTarget);

	TestTrue(
		TEXT("The field is not collapsed onto a single authored line, so the dominant line is still where "
			"independent drivers converge rather than a constant in a header"),
		Spacing > 0.0);

	TestTrue(
		TEXT("The wheel track is wider than a contact patch, so the two bands are a real second population "
			"of marks rather than a widening of the first"),
		WheelTrack > MarkWidth);

	TestTrue(
		TEXT("Both wheel bands together are still a feature on the track rather than the whole of it"),
		WholeFeature < Track->GetTrackWidth() * 0.5);

	Reference->Destroy();
	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSplineFollowingCarLocksOneFrontWheelUnderBraking,
	"RubberAccumDemo.Car.LocksOneFrontWheelUnderBraking",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSplineFollowingCarLocksOneFrontWheelUnderBraking::RunTest(const FString& Parameters)
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
	ASplineFollowingCar* Repeat = World->SpawnActor<ASplineFollowingCar>();

	if (Track == nullptr || Car == nullptr || Repeat == nullptr)
	{
		AddError(TEXT("Failed to spawn the circuit or the cars."));
		return false;
	}

	constexpr int32 LockupSeed = 77003;
	constexpr int32 TicksAcrossManyBrakingZones = 30000;

	Car->ConfigureDriver(Track, Track->GetTrackLength() * StraightLapFraction, 0.0, 0.8, LockupSeed);
	Repeat->ConfigureDriver(Track, Track->GetTrackLength() * StraightLapFraction, 0.0, 0.8, LockupSeed);

	int32 LockedTicks = 0;
	int32 TicksWithAnyLock = 0;
	int32 DivergentSamples = 0;
	int32 RearLocks = 0;
	int32 LeftLocks = 0;
	int32 RightLocks = 0;
	int32 SkidContacts = 0;
	int32 BuildupContacts = 0;
	int32 TicksWithAFullSetOfContacts = 0;

	for (int32 Tick = 0; Tick < TicksAcrossManyBrakingZones; ++Tick)
	{
		Subsystem->ResetSurface();

		Car->Tick(TickSeconds);
		Repeat->Tick(TickSeconds);

		const TOptional<ECarWheel> Locked = Car->GetLockedWheel();

		if (Locked != Repeat->GetLockedWheel())
		{
			++DivergentSamples;
		}

		if (!Locked.IsSet())
		{
			continue;
		}

		++TicksWithAnyLock;
		LockedTicks += Car->IsBraking() ? 1 : 0;

		RearLocks += (Locked.GetValue() == ECarWheel::RearLeft
			|| Locked.GetValue() == ECarWheel::RearRight) ? 1 : 0;

		LeftLocks += Locked.GetValue() == ECarWheel::FrontLeft ? 1 : 0;
		RightLocks += Locked.GetValue() == ECarWheel::FrontRight ? 1 : 0;

		const TArray<FSurfaceTrackContact>& Contacts = Subsystem->GetPendingContacts();

		if (Contacts.Num() != ASplineFollowingCar::WheelCount * 2)
		{
			continue;
		}

		++TicksWithAFullSetOfContacts;

		for (int32 Index = 0; Index < ASplineFollowingCar::WheelCount; ++Index)
		{
			SkidContacts += Contacts[Index].SlipAmount >= 1.0f ? 1 : 0;
			BuildupContacts += Contacts[Index].SlipAmount < 1.0f ? 1 : 0;
		}
	}

	AddInfo(FString::Printf(
		TEXT("Over %d ticks on seed %d the car drew %d braking events and held a locked wheel for %d ticks, "
			"all %d of them while braking. %d were front left and %d front right, %d were a rear wheel"),
		TicksAcrossManyBrakingZones,
		LockupSeed,
		Car->GetBrakingDrawCount(),
		TicksWithAnyLock,
		LockedTicks,
		LeftLocks,
		RightLocks,
		RearLocks));

	AddInfo(FString::Printf(
		TEXT("Across %d ticks where all eight contacts landed, %d reported full slip, which the plugin reads as "
			"a lockup, and %d reported less, which it reads as rubber, so the locked wheel is %.2f of the four "
			"on average"),
		TicksWithAFullSetOfContacts,
		SkidContacts,
		BuildupContacts,
		TicksWithAFullSetOfContacts > 0
			? static_cast<double>(SkidContacts) / TicksWithAFullSetOfContacts
			: 0.0));

	TestTrue(
		TEXT("The car found braking zones to draw against at all"),
		Car->GetBrakingDrawCount() > 0);

	TestTrue(
		TEXT("At least one braking event locked a wheel, so the probability is not effectively zero"),
		TicksWithAnyLock > 0);

	TestEqual(
		TEXT("A locked wheel is only ever held while the car is braking"),
		LockedTicks,
		TicksWithAnyLock);

	TestEqual(
		TEXT("Only a front wheel ever locks, because this model has no trailing throttle or handbrake to "
			"lock a rear one with"),
		RearLocks,
		0);

	TestEqual(
		TEXT("Two cars on the same seed lock the same wheel at the same moment, so the lockup is reproducible "
			"from FieldRandomSeed and the grid index like everything else in the harness"),
		DivergentSamples,
		0);

	TestEqual(
		TEXT("Exactly one of the four wheels reports full slip while a wheel is locked"),
		SkidContacts,
		TicksWithAFullSetOfContacts);

	TestEqual(
		TEXT("The other three keep reporting rolling slip below one, so they keep laying rubber rather than "
			"being suppressed by their neighbour locking"),
		BuildupContacts,
		TicksWithAFullSetOfContacts * (ASplineFollowingCar::WheelCount - 1));

	Subsystem->ResetSurface();

	Repeat->Destroy();
	Car->Destroy();
	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRacetrackCircuitNoWheelFallsOffTheRibbon,
	"RubberAccumDemo.Track.NoWheelFallsOffTheRibbon",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRacetrackCircuitNoWheelFallsOffTheRibbon::RunTest(const FString& Parameters)
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

	if (Track == nullptr)
	{
		AddError(TEXT("Failed to spawn ARacetrackCircuit."));
		return false;
	}

	const int32 FieldSize = Track->GetCarCount();
	const double Length = Track->GetTrackLength();

	TArray<ASplineFollowingCar*> Field;

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
	}

	constexpr int32 TicksAcrossSeveralLaps = 6000;
	constexpr int32 ContactDrainInterval = 500;

	double WidestWheelAlpha = 0.0;
	TArray<double> WheelAlphas;

	Subsystem->ResetSurface();

	for (int32 Tick = 0; Tick < TicksAcrossSeveralLaps; ++Tick)
	{
		for (ASplineFollowingCar* Car : Field)
		{
			Car->Tick(TickSeconds);

			if (WheelLateralOffsets(*Track, *Car, WheelAlphas))
			{
				for (const double Alpha : WheelAlphas)
				{
					WidestWheelAlpha = FMath::Max(WidestWheelAlpha, FMath::Abs(Alpha));
				}
			}
		}

		if (Tick % ContactDrainInterval == ContactDrainInterval - 1)
		{
			const int32 DiscardedSoFar = Subsystem->GetContactsDiscardedOutsideRibbon();
			Subsystem->ResetSurface();

			if (DiscardedSoFar > 0)
			{
				break;
			}
		}
	}

	const int32 Discarded = Subsystem->GetContactsDiscardedOutsideRibbon();

	AddInfo(FString::Printf(
		TEXT("%d cars laying %d wheels each over %d ticks on a %.0f cm track put the widest wheel at alpha "
			"%.4f, leaving %.4f of margin before a contact is discarded. Wheel contacts discarded outside the "
			"ribbon: %d"),
		FieldSize,
		ASplineFollowingCar::WheelCount,
		TicksAcrossSeveralLaps,
		Track->GetTrackWidth(),
		WidestWheelAlpha,
		1.0 - WidestWheelAlpha,
		Discarded));

	TestEqual(
		TEXT("No wheel contact is discarded for falling outside the ribbon, which would be silent otherwise "
			"because nothing in the running game reads the discard counter. Every contact is a wheel, so the "
			"single total is the wheel count"),
		Discarded,
		0);

	TestTrue(
		TEXT("The widest wheel keeps real margin inside the ribbon rather than grazing the edge, which is "
			"what sets the lower bound on track width once wheels sit off the car's own line"),
		WidestWheelAlpha < 0.95);

	Subsystem->ResetSurface();

	for (ASplineFollowingCar* Car : Field)
	{
		Car->Destroy();
	}

	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRacetrackCircuitRoadNeverPassesThroughItself,
	"RubberAccumDemo.Track.RoadNeverPassesThroughItself",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRacetrackCircuitRoadNeverPassesThroughItself::RunTest(const FString& Parameters)
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

	const double Step = Track->GetRoadSegmentLength();
	const double HalfWidth = Track->GetTrackWidth() * 0.5;

	AddInfo(TEXT("layout | tightest edge | advance per ring | implied radius | radius vs half width"));

	for (const ETrackLayout Layout : { ETrackLayout::Stadium, ETrackLayout::Spa })
	{
		Track->SetLayout(Layout, Track->GetAuthoredLayoutLapLength());

		const double Length = Track->GetTrackLength();
		const int32 Rings = FMath::Max(FMath::RoundToInt32(Length / Step), 3);

		double TightestAdvance = TNumericLimits<double>::Max();
		double TightestAtDistance = 0.0;
		double TightestEdgeAlpha = 0.0;

		for (int32 Ring = 0; Ring < Rings; ++Ring)
		{
			const double Distance = Length * Ring / Rings;
			const double NextDistance = Length * (Ring + 1) / Rings;
			const FVector Forward = Track->GetWorldDirectionAtDistance(Distance);

			for (const double Alpha : { -1.0, 1.0 })
			{
				const FVector Here = Track->GetWorldPositionAtDistance(Distance, Alpha);
				const FVector Ahead = Track->GetWorldPositionAtDistance(NextDistance, Alpha);
				const double Advance = FVector::DotProduct(Ahead - Here, Forward);

				if (Advance < TightestAdvance)
				{
					TightestAdvance = Advance;
					TightestAtDistance = Distance;
					TightestEdgeAlpha = Alpha;
				}
			}
		}

		const double RingLength = Length / Rings;
		const double AdvanceRatio = TightestAdvance / RingLength;
		const double ImpliedRadius = AdvanceRatio < 1.0
			? HalfWidth / FMath::Max(1.0 - AdvanceRatio, UE_DOUBLE_SMALL_NUMBER)
			: TNumericLimits<double>::Max();

		AddInfo(FString::Printf(
			TEXT("%s | %s edge at %.0f cm, %.1f %% of the lap | %.2f of %.2f cm | %.0f cm | %.2f"),
			Layout == ETrackLayout::Stadium ? TEXT("Stadium") : TEXT("Spa"),
			TightestEdgeAlpha < 0.0 ? TEXT("left") : TEXT("right"),
			TightestAtDistance,
			100.0 * TightestAtDistance / Length,
			TightestAdvance,
			RingLength,
			ImpliedRadius,
			ImpliedRadius / HalfWidth));

		TestTrue(
			FString::Printf(
				TEXT("The %s road never doubles back on itself: the tightest edge still advances %.2f cm per "
					"%.2f cm ring, and a negative advance is the offset curve crossing its own centre of "
					"curvature"),
				Layout == ETrackLayout::Stadium ? TEXT("Stadium") : TEXT("Spa"),
				TightestAdvance,
				RingLength),
			TightestAdvance > 0.0);

		TestTrue(
			FString::Printf(
				TEXT("The %s road keeps its inside edge off a pinch point, at %.0f cm of implied radius against "
					"a %.0f cm half width"),
				Layout == ETrackLayout::Stadium ? TEXT("Stadium") : TEXT("Spa"),
				ImpliedRadius,
				HalfWidth),
			ImpliedRadius > HalfWidth);
	}

	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRacetrackCircuitBothLayoutsFitTheCapAndTheMemoryBudget,
	"RubberAccumDemo.Track.BothLayoutsFitTheCapAndTheMemoryBudget",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRacetrackCircuitBothLayoutsFitTheCapAndTheMemoryBudget::RunTest(const FString& Parameters)
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

	constexpr int64 CommittedBudgetBytes = 118ll * 1024ll * 1024ll;

	const double CentimetresPerTexel = Track->GetSurfaceCentimetresPerTexel();
	const double MarkWidth = GetDefault<ASplineFollowingCar>()->GetContactWidth();

	AddInfo(TEXT("layout | lap cm | along | across | rows | atlas | bytes | MiB | mark texels | tightest radius vs half width"));

	for (const ETrackLayout Layout : { ETrackLayout::Stadium, ETrackLayout::Spa })
	{
		Track->SetLayout(Layout, Track->GetAuthoredLayoutLapLength());

		FSurfaceTrackRibbon Ribbon;
		Ribbon.TrackLength = Track->GetTrackLength();
		Ribbon.TrackWidth = Track->GetTrackWidth();
		Ribbon.CentimetresPerTexel = CentimetresPerTexel;

		const int64 Bytes = Ribbon.AtlasTexelCount() * 8;
		const double TightestRadius = TightestRadiusOverAChord(*Track, Track->GetRoadSegmentLength());
		const double HalfWidth = Track->GetTrackWidth() * 0.5;

		AddInfo(FString::Printf(
			TEXT("%s | %.0f | %d | %d | %d | %d x %d | %lld | %.2f | %.2f | %.0f vs %.0f, ratio %.2f"),
			Layout == ETrackLayout::Stadium ? TEXT("Stadium") : TEXT("Spa"),
			Ribbon.TrackLength,
			Ribbon.AlongTrackTexels(),
			Ribbon.AcrossTrackTexels(),
			Ribbon.RowCount(),
			Ribbon.AtlasWidth(),
			Ribbon.AtlasHeight(),
			Bytes,
			Bytes / (1024.0 * 1024.0),
			Ribbon.WorldDistanceToTexels(MarkWidth),
			TightestRadius,
			HalfWidth,
			TightestRadius / HalfWidth));

		TestTrue(
			FString::Printf(
				TEXT("The %s layout folds to an atlas inside the texture cap at %.6f cm per texel"),
				Layout == ETrackLayout::Stadium ? TEXT("Stadium") : TEXT("Spa"),
				CentimetresPerTexel),
			Ribbon.FitsTheTextureCap());

		TestTrue(
			FString::Printf(
				TEXT("The %s layout stays inside the 118 MiB default C9 committed to, at %lld bytes"),
				Layout == ETrackLayout::Stadium ? TEXT("Stadium") : TEXT("Spa"),
				Bytes),
			Bytes <= CommittedBudgetBytes);

		TestTrue(
			FString::Printf(
				TEXT("A contact patch on the %s layout spans several texels rather than one or two, which is "
					"what the rescale was for"),
				Layout == ETrackLayout::Stadium ? TEXT("Stadium") : TEXT("Spa")),
			Ribbon.WorldDistanceToTexels(MarkWidth) >= 4.0);
	}

	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRacetrackCircuitStartsPausedAndReleasesTheFieldOnDemand,
	"RubberAccumDemo.Track.StartsPausedAndReleasesTheFieldOnDemand",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRacetrackCircuitStartsPausedAndReleasesTheFieldOnDemand::RunTest(const FString& Parameters)
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

	Track->DispatchBeginPlay();

	const int32 FieldSize = Track->GetSpawnedCarCount();

	if (FieldSize <= 0)
	{
		AddError(TEXT("The circuit began play without spawning a field."));
		Track->Destroy();
		return false;
	}

	int32 TickingWhilePaused = 0;
	int32 TickingAfterRelease = 0;
	int32 TickingAfterRefreeze = 0;

	const bool bPausedOnBeginPlay = Track->IsFieldPaused();

	for (TActorIterator<ASplineFollowingCar> It(World); It; ++It)
	{
		TickingWhilePaused += It->IsActorTickEnabled() ? 1 : 0;
	}

	Track->SetFieldPaused(false);

	for (TActorIterator<ASplineFollowingCar> It(World); It; ++It)
	{
		TickingAfterRelease += It->IsActorTickEnabled() ? 1 : 0;
	}

	Track->SetFieldPaused(true);

	for (TActorIterator<ASplineFollowingCar> It(World); It; ++It)
	{
		TickingAfterRefreeze += It->IsActorTickEnabled() ? 1 : 0;
	}

	AddInfo(FString::Printf(
		TEXT("A circuit beginning play spawned %d cars and reported %s. Cars with tick enabled: %d while "
			"frozen, %d after release, %d after freezing again"),
		FieldSize,
		bPausedOnBeginPlay ? TEXT("paused") : TEXT("running"),
		TickingWhilePaused,
		TickingAfterRelease,
		TickingAfterRefreeze));

	TestTrue(TEXT("The circuit begins play with the field frozen"), bPausedOnBeginPlay);

	TestEqual(
		TEXT("No car ticks while the field is frozen, so nothing moves and nothing is deposited"),
		TickingWhilePaused,
		0);

	TestEqual(
		TEXT("Every car ticks again once the field is released"),
		TickingAfterRelease,
		FieldSize);

	TestEqual(
		TEXT("Freezing again stops every car, so the control toggles rather than firing once"),
		TickingAfterRefreeze,
		0);

	Track->SetFieldPaused(false);

	TestFalse(TEXT("Releasing the field clears the paused flag it reports"), Track->IsFieldPaused());

	for (TActorIterator<ASplineFollowingCar> It(World); It; ++It)
	{
		It->Destroy();
	}

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
	constexpr int32 ContactDrainInterval = 1000;
	double TotalDistanceCovered = 0.0;

	TArray<double> WheelAlphas;

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

			if (!WheelLateralOffsets(*Track, *Car, WheelAlphas))
			{
				continue;
			}

			for (const double Alpha : WheelAlphas)
			{
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
		}

		if (Subsystem != nullptr && Tick % ContactDrainInterval == ContactDrainInterval - 1)
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

	const int32 WheelsOverATexel = ASplineFollowingCar::WheelsSharingATrack;

	const double PredictedStraightPasses = FMath::Clamp(
		MarkWidth / Track->GetAdjacentDriverSpacingCentimetres(), 1.0, static_cast<double>(FieldSize))
		* WheelsOverATexel;

	const float StraightPerContact = RacetrackSurfaceMapping::SlipToExposure(
		Field[0]->GetStraightSlipAmount(), USurfaceAccumulationSubsystem::GetDepositionScale());

	const float ApexPerContact = RacetrackSurfaceMapping::SlipToExposure(
		Field[0]->GetCorneringSlipAmount(), USurfaceAccumulationSubsystem::GetDepositionScale());

	const double StraightExposurePerLap = StraightPassesPerLap * StraightPerContact;
	const double ApexExposurePerLap = ApexPassesPerLap * ApexPerContact;

	AddInfo(FString::Printf(
		TEXT("%d cars laying %d wheels each over %.2f laps, counting passes across %d lateral texels at "
			"%.4f cm each, sampled at the middle of the longest straight and the longest apex. The busiest "
			"straight texel took %.2f passes a lap across %d inked texels spanning %.0f cm, against %.2f "
			"predicted from the stripe spacing times the %d wheels that share a track. The busiest apex "
			"texel took %.2f across %d inked texels spanning %.0f cm, against the %d the one-texel figure "
			"assumes"),
		FieldSize,
		ASplineFollowingCar::WheelCount,
		LapsEachCar,
		LateralTexels,
		CentimetresPerTexel,
		StraightPassesPerLap,
		StraightTexelsInked,
		StraightTexelsInked * CentimetresPerTexel,
		PredictedStraightPasses,
		WheelsOverATexel,
		ApexPassesPerLap,
		ApexTexelsInked,
		ApexTexelsInked * CentimetresPerTexel,
		FieldSize * WheelsOverATexel));

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
		TEXT("No straight texel is crossed by every wheel in the field, so the saturation table may not "
			"treat a straight texel as taking the whole field the way an apex texel is assumed to"),
		StraightPassesPerLap < FieldSize * WheelsOverATexel);

	TestTrue(
		TEXT("The straight count still exceeds what the static stripe spacing predicts, because the field "
			"is bunched tighter than its baselines mid-straight while it is still lerping back from the "
			"previous corner, so the static formula describes a steady state this layout never reaches"),
		StraightPassesPerLap > PredictedStraightPasses);

	TestTrue(
		TEXT("No apex texel is crossed by the whole field either, so the one-texel apex figure is an upper "
			"bound on concentration and not a prediction"),
		ApexPassesPerLap < FieldSize * WheelsOverATexel);

	TestTrue(
		TEXT("The apex still lays more exposure a lap than the straight, so a racing line appears at all"),
		ApexExposurePerLap > StraightExposurePerLap);

	TestTrue(
		TEXT("The contrast between apex and straight has collapsed from the hundredfold the separated "
			"stripes gave, which is the change C6 is for"),
		ApexExposurePerLap / FMath::Max(StraightExposurePerLap, UE_DOUBLE_SMALL_NUMBER) < 20.0);

	const double HalfLifeInLaps = Track->GetDecayHalfLifeInLaps();
	const double ApexEquilibriumExposure = ApexExposurePerLap * HalfLifeInLaps / UE_LN2;
	const double StraightEquilibriumExposure = StraightExposurePerLap * HalfLifeInLaps / UE_LN2;

	const double ApexEquilibriumCoverage =
		RacetrackSurfaceMapping::ExposureToCoverage(static_cast<float>(ApexEquilibriumExposure));

	const double StraightEquilibriumCoverage =
		RacetrackSurfaceMapping::ExposureToCoverage(static_cast<float>(StraightEquilibriumExposure));

	AddInfo(FString::Printf(
		TEXT("At a half-life of %.1f laps the apex settles at %.4f exposure and %.4f coverage, the "
			"straight at %.4f and %.4f, an equilibrium contrast of %.2f to 1 in coverage. Lap time cancels "
			"out of that entirely, which is why the half-life is held in laps rather than in seconds"),
		HalfLifeInLaps,
		ApexEquilibriumExposure,
		ApexEquilibriumCoverage,
		StraightEquilibriumExposure,
		StraightEquilibriumCoverage,
		ApexEquilibriumCoverage / FMath::Max(StraightEquilibriumCoverage, UE_DOUBLE_SMALL_NUMBER)));

	TestTrue(
		TEXT("The configured half-life settles the apex near the saturation the C9 captures reached, so "
			"turning decay on holds the racing line rather than erasing it or leaving it to keep darkening"),
		ApexEquilibriumCoverage > 0.90 && ApexEquilibriumCoverage < 0.99);

	TestTrue(
		TEXT("The straight settles far below the apex, so the equilibrium is a racing line rather than a "
			"uniformly grey track"),
		StraightEquilibriumCoverage < ApexEquilibriumCoverage * 0.5);

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRacetrackCircuitDebugViewCyclesEveryChannel,
	"RubberAccumDemo.Track.DebugViewCyclesEveryChannel",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRacetrackCircuitDebugViewCyclesEveryChannel::RunTest(const FString& Parameters)
{
	const ESurfaceDebugView Expected[] = {
		ESurfaceDebugView::RubberBuildup,
		ESurfaceDebugView::SkidMarks,
		ESurfaceDebugView::Marbles,
		ESurfaceDebugView::AllChannels
	};

	ESurfaceDebugView View = ESurfaceDebugView::AllChannels;

	for (const ESurfaceDebugView Next : Expected)
	{
		View = ARacetrackCircuit::NextDebugView(View);

		TestTrue(
			*FString::Printf(TEXT("The cycle reaches %s"), ARacetrackCircuit::DebugViewName(Next)),
			View == Next);
	}

	TestTrue(
		TEXT("The cycle returns to all three channels, so it is a loop rather than a dead end"),
		View == ESurfaceDebugView::AllChannels);

	TestEqual(
		TEXT("Showing every channel is an exact multiply by one, so that view is the image every capture "
			"before C10 was taken through"),
		ARacetrackCircuit::DebugViewChannelMask(ESurfaceDebugView::AllChannels),
		FLinearColor::White);

	TestEqual(
		TEXT("The rubber view masks by the same colour the paint path writes rubber with"),
		ARacetrackCircuit::DebugViewChannelMask(ESurfaceDebugView::RubberBuildup),
		USurfaceAccumulationBuffer::ChannelMask(EAccumulationChannel::RubberBuildup));

	TestEqual(
		TEXT("The skid view masks by the same colour the paint path writes skid marks with"),
		ARacetrackCircuit::DebugViewChannelMask(ESurfaceDebugView::SkidMarks),
		USurfaceAccumulationBuffer::ChannelMask(EAccumulationChannel::SkidMark));

	TestEqual(
		TEXT("The marbles view masks by the same colour the paint path would write marbles with, so the "
			"black it shows today is an empty channel rather than a wrong mask"),
		ARacetrackCircuit::DebugViewChannelMask(ESurfaceDebugView::Marbles),
		USurfaceAccumulationBuffer::ChannelMask(EAccumulationChannel::Marbles));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRacetrackCircuitDecayRateReachesZeroAndTheCeiling,
	"RubberAccumDemo.Track.DecayRateReachesZeroAndTheCeiling",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRacetrackCircuitDecayRateReachesZeroAndTheCeiling::RunTest(const FString& Parameters)
{
	const ARacetrackCircuit* Reference = GetDefault<ARacetrackCircuit>();

	if (Reference == nullptr)
	{
		AddError(TEXT("No circuit defaults available."));
		return false;
	}

	TArray<float> Ascending;
	float Rate = 0.0f;

	for (int32 Step = 0; Step < 32; ++Step)
	{
		const float Next = Reference->NextDecayRate(Rate, true);

		if (Next <= Rate)
		{
			break;
		}

		Rate = Next;
		Ascending.Add(Rate);
	}

	TArray<float> Descending;

	for (int32 Step = 0; Step < 32; ++Step)
	{
		const float Next = Reference->NextDecayRate(Rate, false);

		if (Next >= Rate)
		{
			break;
		}

		Rate = Next;
		Descending.Add(Rate);
	}

	AddInfo(FString::Printf(
		TEXT("Pressing faster from off reaches %.2fx in %d steps and pressing slower returns to %.2fx in "
			"%d, so the control spans the whole range without stranding the operator at either end"),
		Ascending.Num() > 0 ? Ascending.Last() : 0.0f,
		Ascending.Num(),
		Descending.Num() > 0 ? Descending.Last() : 0.0f,
		Descending.Num()));

	TestTrue(TEXT("Pressing faster from off starts the fade"), Ascending.Num() > 0);

	if (Ascending.Num() > 0)
	{
		TestEqual(
			TEXT("The first press from off lands on the slowest rate rather than multiplying zero by the "
				"step and staying at zero"),
			Ascending[0],
			0.25f);

		TestEqual(TEXT("Pressing faster stops at the twenty times ceiling"), Ascending.Last(), 20.0f);
	}

	TestTrue(TEXT("Pressing slower walks back down"), Descending.Num() > 0);

	if (Descending.Num() > 0)
	{
		TestEqual(
			TEXT("Pressing slower reaches a true zero, which is what reproduces every measurement taken "
				"before decay existed"),
			Descending.Last(),
			0.0f);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRacetrackCircuitLapSecondsMatchATickedLap,
	"RubberAccumDemo.Track.LapSecondsMatchATickedLap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRacetrackCircuitLapSecondsMatchATickedLap::RunTest(const FString& Parameters)
{
	UWorld* World = FindRenderableWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No renderable world available."));
		return false;
	}

	const ETrackLayout Layouts[] = { ETrackLayout::Stadium, ETrackLayout::Spa };

	for (const ETrackLayout Layout : Layouts)
	{
		ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>();

		if (Track == nullptr)
		{
			AddError(TEXT("Failed to spawn ARacetrackCircuit."));
			return false;
		}

		Track->SetLayout(Layout, Track->GetAuthoredLayoutLapLength());

		const double Length = Track->GetTrackLength();
		const double PredictedLapSeconds = Track->GetLapSeconds();

		ASplineFollowingCar* Car = World->SpawnActor<ASplineFollowingCar>();

		if (Car == nullptr)
		{
			AddError(TEXT("Failed to spawn a car."));
			Track->Destroy();
			return false;
		}

		Car->ConfigureDriver(Track, 0.0, 0.0, Track->GetDriverLineCommitment(0), UnusedDriverSeed);

		int32 Ticks = 0;
		const int32 TickLimit = FMath::CeilToInt32(4.0 * PredictedLapSeconds / TickSeconds) + 1;

		while (Car->GetDistanceAlongTrack() < Length && Ticks < TickLimit)
		{
			Car->Tick(TickSeconds);
			++Ticks;
		}

		const double TickedLapSeconds = Ticks * TickSeconds;

		AddInfo(FString::Printf(
			TEXT("%s at %.0f cm: the speed profile predicts a %.2f s lap and a car took %.2f s over %d "
				"ticks, a difference of %.1f %%. The half-life of %.1f laps is therefore %.0f s here"),
			Layout == ETrackLayout::Stadium ? TEXT("Stadium") : TEXT("Spa"),
			Length,
			PredictedLapSeconds,
			TickedLapSeconds,
			Ticks,
			100.0 * FMath::Abs(TickedLapSeconds - PredictedLapSeconds)
				/ FMath::Max(TickedLapSeconds, UE_DOUBLE_SMALL_NUMBER),
			Track->GetDecayHalfLifeInLaps(),
			Track->GetDecayHalfLifeInLaps() * PredictedLapSeconds));

		TestTrue(
			*FString::Printf(TEXT("The car completed a lap on the %s layout"),
				Layout == ETrackLayout::Stadium ? TEXT("Stadium") : TEXT("Spa")),
			Ticks < TickLimit);

		TestTrue(
			TEXT("Integrating the speed profile predicts the lap time a ticked car actually takes, so the "
				"half-life the circuit publishes is derived from the lap rather than guessed at. The "
				"integration ignores the bounded braking and acceleration rates, so it runs slightly fast"),
			FMath::Abs(TickedLapSeconds - PredictedLapSeconds) < 0.15 * TickedLapSeconds);

		USurfaceAccumulationSubsystem* Subsystem = World->GetSubsystem<USurfaceAccumulationSubsystem>();

		if (Subsystem != nullptr)
		{
			TestTrue(
				TEXT("The circuit published a half-life of its own laps to the subsystem rather than "
					"leaving the plugin's layout-agnostic fallback in place"),
				FMath::IsNearlyEqual(
					Subsystem->GetDecayHalfLifeSeconds(),
					static_cast<float>(Track->GetDecayHalfLifeInLaps() * PredictedLapSeconds),
					1.0f));
		}

		Car->Destroy();
		Track->Destroy();
	}

	return true;
}

namespace
{
	void TickTheWholeField(const ARacetrackCircuit& Track, int32 TickCount)
	{
		for (const TObjectPtr<ASplineFollowingCar>& Entry : Track.GetSpawnedField())
		{
			if (ASplineFollowingCar* Car = Entry.Get())
			{
				TickCar(*Car, TickCount);
			}
		}
	}

	ASplineFollowingCar* FurthestCarByHand(const ARacetrackCircuit& Track)
	{
		ASplineFollowingCar* Furthest = nullptr;
		double Greatest = 0.0;

		for (const TObjectPtr<ASplineFollowingCar>& Entry : Track.GetSpawnedField())
		{
			ASplineFollowingCar* Car = Entry.Get();

			if (!IsValid(Car) || !Car->HasPlacedItsWheels())
			{
				continue;
			}

			if (Furthest == nullptr || Car->GetDistanceAlongTrack() > Greatest)
			{
				Greatest = Car->GetDistanceAlongTrack();
				Furthest = Car;
			}
		}

		return Furthest;
	}

	void DestroyTheField(ARacetrackCircuit& Track)
	{
		TArray<ASplineFollowingCar*> Field;

		for (const TObjectPtr<ASplineFollowingCar>& Entry : Track.GetSpawnedField())
		{
			if (ASplineFollowingCar* Car = Entry.Get())
			{
				Field.Add(Car);
			}
		}

		for (ASplineFollowingCar* Car : Field)
		{
			if (IsValid(Car))
			{
				Car->Destroy();
			}
		}

		Track.Destroy();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRacetrackCircuitTheLeaderIsTheCarFurthestAroundTheTrack,
	"RubberAccumDemo.Demo.TheLeaderIsTheCarFurthestAroundTheTrack",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRacetrackCircuitTheLeaderIsTheCarFurthestAroundTheTrack::RunTest(const FString& Parameters)
{
	UWorld* World = FindRenderableWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No editor, game or PIE world available."));
		return false;
	}

	ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>();

	if (Track == nullptr)
	{
		AddError(TEXT("Could not spawn a circuit."));
		return false;
	}

	Track->PostInitializeComponents();
	Track->DispatchBeginPlay();

	TestTrue(
		TEXT("A field was spawned, so there is something to lead"),
		Track->GetSpawnedCarCount() > 1);

	TestTrue(
		TEXT("Before any car has placed its wheels there is no leader at all, rather than a car whose "
			"four wheels are still at the world origin"),
		Track->FindLeadingCar() == nullptr);

	TickTheWholeField(*Track, 4);

	ASplineFollowingCar* ByHand = FurthestCarByHand(*Track);
	ASplineFollowingCar* Leading = Track->FindLeadingCar();

	TestTrue(TEXT("A leader is found once the field has placed its wheels"), Leading != nullptr);

	TestTrue(
		TEXT("The leader is the car furthest around the track, found the same way an independent pass "
			"over the field finds it"),
		Leading == ByHand);

	if (Leading == nullptr)
	{
		DestroyTheField(*Track);
		return false;
	}

	AddInfo(FString::Printf(
		TEXT("%d cars, the leader %.1f cm around a %.1f cm lap, which is %.2f laps"),
		Track->GetSpawnedCarCount(),
		Leading->GetDistanceAlongTrack(),
		Track->GetTrackLength(),
		Leading->GetDistanceAlongTrack() / FMath::Max(Track->GetTrackLength(), 1.0)));

	TestTrue(
		TEXT("Distance along the track is cumulative rather than wrapped into one lap, which is why "
			"greatest distance already means greatest progress and no lap counter is needed"),
		Leading->GetDistanceAlongTrack() >= Track->GetTrackLength());

	ASplineFollowingCar* Destroyed = Leading;
	const double DistanceWhenDestroyed = Destroyed->GetDistanceAlongTrack();

	Destroyed->Destroy();

	ASplineFollowingCar* Successor = Track->FindLeadingCar();

	TestTrue(TEXT("Destroying the leader leaves a successor"), Successor != nullptr);

	TestTrue(
		TEXT("A destroyed car is skipped rather than read through, so the field survives a car leaving "
			"it mid run"),
		Successor != Destroyed);

	if (Successor != nullptr)
	{
		TestTrue(
			FString::Printf(
				TEXT("The successor is the next car back, %.1f cm against the %.1f cm the destroyed "
					"leader had reached"),
				Successor->GetDistanceAlongTrack(),
				DistanceWhenDestroyed),
			Successor->GetDistanceAlongTrack() < DistanceWhenDestroyed);
	}

	DestroyTheField(*Track);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRacetrackCircuitFourHandlesSurviveAChangeOfLeader,
	"RubberAccumDemo.Demo.FourHandlesSurviveAChangeOfLeader",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRacetrackCircuitFourHandlesSurviveAChangeOfLeader::RunTest(const FString& Parameters)
{
	UWorld* World = FindRenderableWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No editor, game or PIE world available."));
		return false;
	}

	USurfaceAccumulationSubsystem* Subsystem = World->GetSubsystem<USurfaceAccumulationSubsystem>();

	if (Subsystem == nullptr || !Subsystem->IsInitialized())
	{
		AddWarning(TEXT("Skipped: the world's accumulation subsystem is not initialised."));
		return true;
	}

	const int32 QueriesBefore = Subsystem->GetActiveSurfaceQueryCount();

	ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>();

	if (Track == nullptr)
	{
		AddError(TEXT("Could not spawn a circuit."));
		return false;
	}

	Track->PostInitializeComponents();
	Track->DispatchBeginPlay();

	TestEqual(
		TEXT("The circuit acquires one query per wheel of the car it reads, once, rather than one per "
			"car in the field"),
		Track->GetLeaderWheelQueries().Num(),
		ASplineFollowingCar::WheelCount);

	TestEqual(
		TEXT("Those four are the only queries the demo adds"),
		Subsystem->GetActiveSurfaceQueryCount(),
		QueriesBefore + ASplineFollowingCar::WheelCount);

	TickTheWholeField(*Track, 4);
	Track->Tick(1.0f / 60.0f);

	const TArray<FSurfaceQueryHandle> Before = Track->GetLeaderWheelQueries();
	ASplineFollowingCar* FirstLeader = Track->FindLeadingCar();

	TestTrue(TEXT("The circuit read a leader"), FirstLeader != nullptr);

	TestTrue(
		TEXT("The car the circuit read is the one it leads with"),
		Track->GetLeadingCarLastRead() == FirstLeader);

	TestEqual(
		TEXT("Reading four wheels produces four samples"),
		Track->GetLeaderWheelSamples().Num(),
		ASplineFollowingCar::WheelCount);

	if (FirstLeader == nullptr)
	{
		DestroyTheField(*Track);
		return false;
	}

	FirstLeader->Destroy();

	TickTheWholeField(*Track, 1);
	Track->Tick(1.0f / 60.0f);

	const TArray<FSurfaceQueryHandle>& After = Track->GetLeaderWheelQueries();
	const ASplineFollowingCar* SecondLeader = Track->GetLeadingCarLastRead();

	TestTrue(TEXT("A different car is read after the first is destroyed"), SecondLeader != FirstLeader);

	TestTrue(TEXT("And it is a live one"), IsValid(SecondLeader));

	TestEqual(TEXT("Still four queries, not four more"), After.Num(), Before.Num());

	TestEqual(
		TEXT("Destroying the car being read acquires and releases nothing, so the subsystem sees no "
			"handle churn at all"),
		Subsystem->GetActiveSurfaceQueryCount(),
		QueriesBefore + ASplineFollowingCar::WheelCount);

	bool bHandlesUnchanged = After.Num() == Before.Num();

	for (int32 Wheel = 0; Wheel < Before.Num() && bHandlesUnchanged; ++Wheel)
	{
		bHandlesUnchanged =
			After[Wheel].SlotIndex == Before[Wheel].SlotIndex
			&& After[Wheel].Generation == Before[Wheel].Generation;
	}

	TestTrue(
		TEXT("Every handle keeps its slot and its generation across the change of leader, which is what "
			"holding the handles on the circuit rather than on the car buys: a change of leader is a "
			"change of submitted position and nothing else"),
		bHandlesUnchanged);

	DestroyTheField(*Track);

	TestEqual(
		TEXT("Destroying the circuit releases its four queries rather than leaking them into the world "
			"subsystem, which outlives it"),
		Subsystem->GetActiveSurfaceQueryCount(),
		QueriesBefore);

	return true;
}

namespace
{
	constexpr double GridRunLapLength = 180000.0;

	constexpr double GridRunSeconds = 300.0;

	constexpr double OneCarLengthCentimetres = 220.0;

	constexpr double CompressedAgreementCentimetres = 25.0;

	constexpr int32 GridRunFramesBetweenSurfaceResets = 600;

	constexpr double GridRunReportEverySeconds = 60.0;

	struct FGridRunResult
	{
		double SmallestAdjacentGap = TNumericLimits<double>::Max();

		int32 SmallestGapTrailingCar = INDEX_NONE;

		double SmallestGapAtSeconds = 0.0;

		double SmallestWorldSeparation = TNumericLimits<double>::Max();
	};

	FGridRunResult DriveTheGrid(
		FAutomationTestBase& Test,
		UWorld& World,
		USurfaceAccumulationSubsystem& Subsystem,
		float FrameSeconds,
		const TCHAR* Label)
	{
		FGridRunResult Result;

		ARacetrackCircuit* Track = World.SpawnActor<ARacetrackCircuit>();

		if (Track == nullptr)
		{
			Test.AddError(TEXT("Could not spawn a circuit."));
			return Result;
		}

		Track->SetLayout(ETrackLayout::Spa, GridRunLapLength);
		Track->PostInitializeComponents();
		Track->DispatchBeginPlay();

		const TArray<TObjectPtr<ASplineFollowingCar>>& Field = Track->GetSpawnedField();
		const int32 FrameCount = FMath::CeilToInt32(GridRunSeconds / FrameSeconds);
		const int32 FramesPerReport = FMath::Max(FMath::RoundToInt32(GridRunReportEverySeconds / FrameSeconds), 1);

		Test.AddInfo(FString::Printf(
			TEXT("%s: %d cars on a %.0f cm lap, %d frames of %.4f s. Seconds | gap car 0 to 1 | speed 0 | "
				"speed 1 | smallest adjacent gap so far"),
			Label,
			Field.Num(),
			Track->GetTrackLength(),
			FrameCount,
			FrameSeconds));

		for (int32 Frame = 1; Frame <= FrameCount; ++Frame)
		{
			for (const TObjectPtr<ASplineFollowingCar>& Entry : Field)
			{
				if (ASplineFollowingCar* Car = Entry.Get())
				{
					Car->Tick(FrameSeconds);
				}
			}

			for (int32 Trailing = 1; Trailing < Field.Num(); ++Trailing)
			{
				const ASplineFollowingCar* Ahead = Field[Trailing - 1].Get();
				const ASplineFollowingCar* Behind = Field[Trailing].Get();

				if (Ahead == nullptr || Behind == nullptr)
				{
					continue;
				}

				const double Gap = Ahead->GetDistanceAlongTrack() - Behind->GetDistanceAlongTrack();

				if (Gap < Result.SmallestAdjacentGap)
				{
					Result.SmallestAdjacentGap = Gap;
					Result.SmallestGapTrailingCar = Trailing;
					Result.SmallestGapAtSeconds = Frame * FrameSeconds;
				}

				Result.SmallestWorldSeparation = FMath::Min(
					Result.SmallestWorldSeparation,
					FVector::Dist(Ahead->GetActorLocation(), Behind->GetActorLocation()));
			}

			if (Frame % GridRunFramesBetweenSurfaceResets == 0)
			{
				Subsystem.ResetSurface();
			}

			if (Frame % FramesPerReport == 0 && Field.Num() > 1 && Field[0] != nullptr && Field[1] != nullptr)
			{
				Test.AddInfo(FString::Printf(
					TEXT("%s: %.0f | %.1f cm | %.1f | %.1f | %.1f cm"),
					Label,
					Frame * FrameSeconds,
					Field[0]->GetDistanceAlongTrack() - Field[1]->GetDistanceAlongTrack(),
					Field[0]->GetSpeedCentimetresPerSecond(),
					Field[1]->GetSpeedCentimetresPerSecond(),
					Result.SmallestAdjacentGap));
			}
		}

		Subsystem.ResetSurface();
		DestroyTheField(*Track);

		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRacetrackCircuitTheGridNeverDrivesIntoItself,
	"RubberAccumDemo.Demo.TheGridNeverDrivesIntoItself",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRacetrackCircuitTheGridNeverDrivesIntoItself::RunTest(const FString& Parameters)
{
	UWorld* World = FindRenderableWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No editor, game or PIE world available."));
		return false;
	}

	USurfaceAccumulationSubsystem* Subsystem = World->GetSubsystem<USurfaceAccumulationSubsystem>();

	if (Subsystem == nullptr || !Subsystem->IsInitialized())
	{
		AddWarning(TEXT("Skipped: the world's accumulation subsystem is not initialised."));
		return true;
	}

	const FGridRunResult RealTime = DriveTheGrid(*this, *World, *Subsystem, TickSeconds, TEXT("1x"));

	AddInfo(FString::Printf(
		TEXT("1x reference: smallest along track gap %.1f cm, car %d behind car %d, at %.0f s; smallest "
			"world separation %.1f cm"),
		RealTime.SmallestAdjacentGap,
		RealTime.SmallestGapTrailingCar,
		RealTime.SmallestGapTrailingCar - 1,
		RealTime.SmallestGapAtSeconds,
		RealTime.SmallestWorldSeparation));

	TestTrue(
		FString::Printf(
			TEXT("In real time no car comes within a car length of the one ahead: %.1f cm"),
			RealTime.SmallestAdjacentGap),
		RealTime.SmallestAdjacentGap > OneCarLengthCentimetres);

	struct FCompressedRun
	{
		float FrameSeconds;

		const TCHAR* Label;
	};

	const FCompressedRun CompressedRuns[] =
	{
		{ 20.0f / 60.0f, TEXT("20x at 60 fps") },
		{ 20.0f / 30.0f, TEXT("20x at 30 fps") },
		{ 20.0f / 20.0f, TEXT("20x at 20 fps") }
	};

	for (const FCompressedRun& Run : CompressedRuns)
	{
		const FGridRunResult Result = DriveTheGrid(*this, *World, *Subsystem, Run.FrameSeconds, Run.Label);

		AddInfo(FString::Printf(
			TEXT("%s: smallest along track gap %.1f cm, car %d behind car %d, at %.0f s; smallest world "
				"separation %.1f cm; %.1f cm from the real time reference"),
			Run.Label,
			Result.SmallestAdjacentGap,
			Result.SmallestGapTrailingCar,
			Result.SmallestGapTrailingCar - 1,
			Result.SmallestGapAtSeconds,
			Result.SmallestWorldSeparation,
			FMath::Abs(Result.SmallestAdjacentGap - RealTime.SmallestAdjacentGap)));

		TestTrue(
			FString::Printf(
				TEXT("%s, no car drives into the one ahead: the smallest gap is %.1f cm against a %.0f cm "
					"car. A frame this long used to move a car further than its own braking lookahead in "
					"one step, which let a trailing car land on the state of the car in front and stay "
					"there"),
				Run.Label,
				Result.SmallestAdjacentGap,
				OneCarLengthCentimetres),
			Result.SmallestAdjacentGap > OneCarLengthCentimetres);

		TestTrue(
			FString::Printf(
				TEXT("%s drives the same field as real time does, because the car integrates its motion "
					"in steps no longer than a real time frame: %.1f cm against %.1f cm"),
				Run.Label,
				Result.SmallestAdjacentGap,
				RealTime.SmallestAdjacentGap),
			FMath::Abs(Result.SmallestAdjacentGap - RealTime.SmallestAdjacentGap) < CompressedAgreementCentimetres);
	}

	return true;
}

#endif
