#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SplineComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "SurfaceAccumulationBuffer.h"
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
	constexpr int32 UnusedDriverSeed = 0;

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

	int64 ProductionBytesPerTexel()
	{
		return GPixelFormats[GetPixelFormatFromRenderTargetFormat(USurfaceAccumulationBuffer::ProductionFormat)].BlockBytes;
	}

	int64 CommittedAccumulationBytes()
	{
		return FSurfaceTrackRibbon().AtlasTexelCount() * ProductionBytesPerTexel();
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
			"allocating %d x %d for %lld bytes in the production format, and a %.1f cm contact patch stores as "
			"%.2f texels"),
		Length,
		Ribbon.TrackWidth,
		Ribbon.CentimetresPerTexel,
		Ribbon.AlongTrackTexels(),
		Ribbon.AcrossTrackTexels(),
		Ribbon.RowCount(),
		Ribbon.TexelsPerRow(),
		Ribbon.AtlasWidth(),
		Ribbon.AtlasHeight(),
		Ribbon.AtlasTexelCount() * ProductionBytesPerTexel(),
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

	const int64 CommittedBytes = CommittedAccumulationBytes();
	const int64 AllocatedBytes = Ribbon.AtlasTexelCount() * ProductionBytesPerTexel();

	TestTrue(
		FString::Printf(
			TEXT("The buffer allocates the %lld bytes D2 committed to, the default ribbon in the RGBA32f "
				"production format, give or take what the measured rescale moves the ceil by, got %lld"),
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

	Track->SetLayout(ETrackLayout::Spa, Track->GetAuthoredLayoutLapLength());

	const double MarkWidthInAlpha = AlphaPerContactWidth(*Track, *Car);
	const double HalfWidth = Track->GetTrackWidth() * 0.5;

	const int32 FrontLeft = static_cast<int32>(ECarWheel::FrontLeft);
	const int32 RearLeft = static_cast<int32>(ECarWheel::RearLeft);

	TArray<double> Alphas;

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

	Track->SetLayout(ETrackLayout::Spa, Track->GetAuthoredLayoutLapLength());

	const double HalfWidth = Track->GetTrackWidth() * 0.5;
	const double Length = Track->GetTrackLength();
	const int32 Rings = FMath::Max(FMath::RoundToInt32(Length / Track->GetRoadSegmentLength()), 3);

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
		TEXT("Tightest %s edge at %.0f cm, %.1f %% of the lap: it advances %.2f of a %.2f cm ring, an implied "
			"radius of %.0f cm, %.2f of the half width"),
		TightestEdgeAlpha < 0.0 ? TEXT("left") : TEXT("right"),
		TightestAtDistance,
		100.0 * TightestAtDistance / Length,
		TightestAdvance,
		RingLength,
		ImpliedRadius,
		ImpliedRadius / HalfWidth));

	TestTrue(
		FString::Printf(
			TEXT("The road never doubles back on itself: the tightest edge still advances %.2f cm per %.2f cm "
				"ring, and a negative advance is the offset curve crossing its own centre of curvature"),
			TightestAdvance,
			RingLength),
		TightestAdvance > 0.0);

	TestTrue(
		FString::Printf(
			TEXT("The road keeps its inside edge off a pinch point, at %.0f cm of implied radius against a %.0f cm "
				"half width"),
			ImpliedRadius,
			HalfWidth),
		ImpliedRadius > HalfWidth);

	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRacetrackCircuitSpaFitsTheCapAndTheMemoryBudget,
	"RubberAccumDemo.Track.SpaFitsTheCapAndTheMemoryBudget",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRacetrackCircuitSpaFitsTheCapAndTheMemoryBudget::RunTest(const FString& Parameters)
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

	const int64 CommittedBudgetBytes = CommittedAccumulationBytes() + CommittedAccumulationBytes() / 100;

	const double CentimetresPerTexel = Track->GetSurfaceCentimetresPerTexel();
	const double MarkWidth = GetDefault<ASplineFollowingCar>()->GetContactWidth();

	Track->SetLayout(ETrackLayout::Spa, Track->GetAuthoredLayoutLapLength());

	FSurfaceTrackRibbon Ribbon;
	Ribbon.TrackLength = Track->GetTrackLength();
	Ribbon.TrackWidth = Track->GetTrackWidth();
	Ribbon.CentimetresPerTexel = CentimetresPerTexel;

	const int64 Bytes = Ribbon.AtlasTexelCount() * ProductionBytesPerTexel();
	const double TightestRadius = TightestRadiusOverAChord(*Track, Track->GetRoadSegmentLength());
	const double HalfWidth = Track->GetTrackWidth() * 0.5;

	AddInfo(FString::Printf(
		TEXT("lap %.0f cm | along %d | across %d | rows %d | atlas %d x %d | %lld bytes, %.2f MiB | mark %.2f "
			"texels | tightest radius %.0f vs %.0f half width, ratio %.2f"),
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
		FString::Printf(TEXT("The layout folds to an atlas inside the texture cap at %.6f cm per texel"), CentimetresPerTexel),
		Ribbon.FitsTheTextureCap());

	TestTrue(
		FString::Printf(
			TEXT("The layout stays inside the %lld bytes D2 committed to, the default ribbon at RGBA32f plus the "
				"1 %% a rescale may move it, at %lld bytes"),
			CommittedBudgetBytes,
			Bytes),
		Bytes <= CommittedBudgetBytes);

	TestTrue(
		TEXT("A contact patch spans several texels rather than one or two, which is what the rescale was for"),
		Ribbon.WorldDistanceToTexels(MarkWidth) >= 4.0);

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

	ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>();

	if (Track == nullptr)
	{
		AddError(TEXT("Failed to spawn ARacetrackCircuit."));
		return false;
	}

	Track->SetLayout(ETrackLayout::Spa, Track->GetAuthoredLayoutLapLength());

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
		TEXT("At %.0f cm the speed profile predicts a %.2f s lap and a car took %.2f s over %d ticks, a "
			"difference of %.1f %%. The half-life of %.1f laps is therefore %.0f s"),
		Length,
		PredictedLapSeconds,
		TickedLapSeconds,
		Ticks,
		100.0 * FMath::Abs(TickedLapSeconds - PredictedLapSeconds)
			/ FMath::Max(TickedLapSeconds, UE_DOUBLE_SMALL_NUMBER),
		Track->GetDecayHalfLifeInLaps(),
		Track->GetDecayHalfLifeInLaps() * PredictedLapSeconds));

	TestTrue(TEXT("The car completed a lap"), Ticks < TickLimit);

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

	return true;
}

namespace
{
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
