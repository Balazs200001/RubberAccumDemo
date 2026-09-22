#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "../RacetrackCircuit.h"

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

#endif
