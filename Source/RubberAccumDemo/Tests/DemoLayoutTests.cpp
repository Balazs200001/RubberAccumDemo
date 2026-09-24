#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SplineComponent.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "SurfaceAccumulationBuffer.h"
#include "SurfaceAccumulationMapping.h"
#include "SurfaceAccumulationSubsystem.h"
#include "SurfaceTrackRibbon.h"
#include "../RacetrackCircuit.h"
#include "../SplineFollowingCar.h"

namespace
{
	constexpr double DemoLapLength = 180000.0;
	constexpr float RealTimeTick = 1.0f / 60.0f;
	constexpr float CompressedTick = RealTimeTick * 20.0f;

	UWorld* FindDemoWorld()
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

	class FScopedDepositionScale
	{
	public:
		explicit FScopedDepositionScale(float Scale)
			: Variable(IConsoleManager::Get().FindConsoleVariable(
				RacetrackSurfaceMapping::DepositionScaleVariableName))
			, Restore(Variable != nullptr ? Variable->GetFloat() : 1.0f)
		{
			if (Variable != nullptr)
			{
				Variable->Set(Scale, ECVF_SetByCode);
			}
		}

		~FScopedDepositionScale()
		{
			if (Variable != nullptr)
			{
				Variable->Set(Restore, ECVF_SetByCode);
			}
		}

	private:
		IConsoleVariable* Variable;
		float Restore;
	};

	class FScopedDecayRate
	{
	public:
		explicit FScopedDecayRate(float Rate)
			: Variable(IConsoleManager::Get().FindConsoleVariable(
				RacetrackSurfaceMapping::DecayRateVariableName))
			, Restore(Variable != nullptr ? Variable->GetFloat() : 0.0f)
		{
			if (Variable != nullptr)
			{
				Variable->Set(Rate, ECVF_SetByCode);
			}
		}

		~FScopedDecayRate()
		{
			if (Variable != nullptr)
			{
				Variable->Set(Restore, ECVF_SetByCode);
			}
		}

	private:
		IConsoleVariable* Variable;
		float Restore;
	};

	struct FStrokeTally
	{
		int32 Count = 0;
		double WidestLateralTexels = 0.0;
		double LongestAlongCentimetres = 0.0;
		double FurthestAlphaFromCentre = 0.0;
		int32 EndpointsBeyondTheTrackEdge = 0;
		float StrongestIntensity = 0.0f;
	};

	double StrokeEndpointAlpha(const FSurfaceBrushStroke& Stroke, const FSurfaceTrackRibbon& Ribbon, double AtlasY)
	{
		const double AcrossTrack = AtlasY * Ribbon.RowCount() - Stroke.RowIndex;

		return AcrossTrack * 2.0 - 1.0;
	}

	void TallyStroke(
		const FSurfaceBrushStroke& Stroke,
		const FSurfaceTrackRibbon& Ribbon,
		FStrokeTally& Tally)
	{
		const double LateralTexels =
			FMath::Abs(Stroke.RegionEnd.Y - Stroke.RegionStart.Y) * Ribbon.AtlasHeight();

		const double AlongCentimetres =
			FMath::Abs(Stroke.RegionEnd.X - Stroke.RegionStart.X)
			* Ribbon.AtlasWidth() * Ribbon.CentimetresPerTexel;

		const double StartAlpha = StrokeEndpointAlpha(Stroke, Ribbon, Stroke.RegionStart.Y);
		const double EndAlpha = StrokeEndpointAlpha(Stroke, Ribbon, Stroke.RegionEnd.Y);
		const double FurthestAlpha = FMath::Max(FMath::Abs(StartAlpha), FMath::Abs(EndAlpha));

		++Tally.Count;
		Tally.WidestLateralTexels = FMath::Max(Tally.WidestLateralTexels, LateralTexels);
		Tally.LongestAlongCentimetres = FMath::Max(Tally.LongestAlongCentimetres, AlongCentimetres);
		Tally.FurthestAlphaFromCentre = FMath::Max(Tally.FurthestAlphaFromCentre, FurthestAlpha);
		Tally.EndpointsBeyondTheTrackEdge += FurthestAlpha > 1.0 ? 1 : 0;
		Tally.StrongestIntensity = FMath::Max(Tally.StrongestIntensity, Stroke.Intensity);
	}

	ARacetrackCircuit* SpawnDemoCircuit(UWorld& World, TArray<ASplineFollowingCar*>& OutField)
	{
		ARacetrackCircuit* Track = World.SpawnActor<ARacetrackCircuit>();

		if (Track == nullptr)
		{
			return nullptr;
		}

		Track->SetLayout(ETrackLayout::Spa, DemoLapLength);

		const int32 FieldSize = Track->GetCarCount();
		const double Length = Track->GetTrackLength();

		for (int32 Index = 0; Index < FieldSize; ++Index)
		{
			ASplineFollowingCar* Car = World.SpawnActor<ASplineFollowingCar>();

			if (Car == nullptr)
			{
				continue;
			}

			Car->ConfigureDriver(
				Track,
				Length * Index / FMath::Max(FieldSize, 1),
				Track->GetDriverBaselineLateralOffsetAlpha(Index),
				Track->GetDriverLineCommitment(Index),
				Track->GetDriverSeedForGridIndex(Index));

			OutField.Add(Car);
		}

		return Track;
	}

	void DestroyDemo(ARacetrackCircuit* Track, TArray<ASplineFollowingCar*>& Field)
	{
		for (ASplineFollowingCar* Car : Field)
		{
			if (Car != nullptr)
			{
				Car->Destroy();
			}
		}

		if (Track != nullptr)
		{
			Track->Destroy();
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDemoLayoutMarblesReachTheBuffer,
	"RubberAccumDemo.Demo.MarblesReachTheBufferOnTheFoldedLayout",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDemoLayoutMarblesReachTheBuffer::RunTest(const FString& Parameters)
{
	UWorld* World = FindDemoWorld();

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

	TArray<ASplineFollowingCar*> Field;
	ARacetrackCircuit* Track = SpawnDemoCircuit(*World, Field);

	if (Track == nullptr || Field.Num() == 0)
	{
		AddError(TEXT("Failed to spawn the demo circuit or its field."));
		DestroyDemo(Track, Field);
		return false;
	}

	const FScopedDepositionScale Deposition(16.0f);

	Subsystem->ResetSurface();

	const FSurfaceTrackRibbon& Ribbon = Subsystem->GetRibbon();

	FStrokeTally Buildup;
	FStrokeTally Skid;
	FStrokeTally Marbles;

	int32 MarbleContactsRegistered = 0;
	int32 TicksWithAnyMarbleContact = 0;

	constexpr int32 TickCount = 3000;

	for (int32 Tick = 0; Tick < TickCount; ++Tick)
	{
		for (ASplineFollowingCar* Car : Field)
		{
			Car->Tick(RealTimeTick);
		}

		int32 MarblesThisTick = 0;

		for (const FSurfaceTrackContact& Contact : Subsystem->GetPendingContacts())
		{
			MarblesThisTick += Contact.Channel == EAccumulationChannel::Marbles ? 1 : 0;
		}

		MarbleContactsRegistered += MarblesThisTick;
		TicksWithAnyMarbleContact += MarblesThisTick > 0 ? 1 : 0;

		Subsystem->Tick(RealTimeTick);

		for (const FSurfaceBrushStroke& Stroke : Subsystem->GetStrokesResolvedThisFrame())
		{
			switch (Stroke.Channel)
			{
			case EAccumulationChannel::RubberBuildup:
				TallyStroke(Stroke, Ribbon, Buildup);
				break;
			case EAccumulationChannel::SkidMark:
				TallyStroke(Stroke, Ribbon, Skid);
				break;
			case EAccumulationChannel::Marbles:
				TallyStroke(Stroke, Ribbon, Marbles);
				break;
			}
		}
	}

	const int32 MarblesDiscarded =
		Subsystem->GetContactsDiscardedOutsideRibbonInChannel(EAccumulationChannel::Marbles);
	const int32 BuildupDiscarded =
		Subsystem->GetContactsDiscardedOutsideRibbonInChannel(EAccumulationChannel::RubberBuildup);

	AddInfo(FString::Printf(
		TEXT("Demo layout: %.0f cm lap, %.0f cm wide, %d x %d atlas in %d rows. %d cars over %d ticks."),
		Ribbon.TrackLength,
		Ribbon.TrackWidth,
		Ribbon.AtlasWidth(),
		Ribbon.AtlasHeight(),
		Ribbon.RowCount(),
		Field.Num(),
		TickCount));

	AddInfo(FString::Printf(
		TEXT("Marble contacts registered: %d over %d of %d ticks. Discarded outside the ribbon: %d marble, "
			"%d buildup."),
		MarbleContactsRegistered,
		TicksWithAnyMarbleContact,
		TickCount,
		MarblesDiscarded,
		BuildupDiscarded));

	AddInfo(FString::Printf(
		TEXT("Strokes resolved | buildup %d, strongest intensity %.6f | skid %d, strongest %.6f | "
			"marbles %d, strongest %.6f"),
		Buildup.Count, Buildup.StrongestIntensity,
		Skid.Count, Skid.StrongestIntensity,
		Marbles.Count, Marbles.StrongestIntensity));

	AddInfo(FString::Printf(
		TEXT("Stroke geometry at a real-time tick | buildup: %.1f cm longest, %.2f texels widest lateral | "
			"marbles: %.1f cm longest, %.2f texels widest lateral"),
		Buildup.LongestAlongCentimetres, Buildup.WidestLateralTexels,
		Marbles.LongestAlongCentimetres, Marbles.WidestLateralTexels));

	TestTrue(
		TEXT("The demo layout folds, so this test exercises the case the Stadium tests cannot"),
		Ribbon.RowCount() > 1);

	TestTrue(
		TEXT("Cars on the demo layout register marble contacts at all"),
		MarbleContactsRegistered > 0);

	TestTrue(
		TEXT("Marble contacts survive registration rather than being discarded outside the ribbon"),
		MarblesDiscarded == 0);

	TestTrue(
		TEXT("Marble contacts become painted strokes rather than being dropped in stroke construction"),
		Marbles.Count > 0);

	TestTrue(
		TEXT("Marble strokes carry a non-zero intensity, so they deposit something"),
		Marbles.StrongestIntensity > 0.0f);

	UTextureRenderTarget2D* Painted = Subsystem->GetBuffer() != nullptr
		? Subsystem->GetBuffer()->GetTarget()
		: nullptr;

	if (Painted == nullptr)
	{
		AddError(TEXT("No accumulation target was allocated, so nothing can be read back."));
		DestroyDemo(Track, Field);
		return false;
	}

	constexpr int32 ProbeCount = 96;

	const int32 AcrossTexels = Ribbon.AcrossTrackTexels();

	float StrongestBuildup = 0.0f;
	float StrongestSkid = 0.0f;
	float StrongestMarbles = 0.0f;
	int32 MarblePeakTexel = 0;
	int32 BuildupPeakTexel = 0;

	for (int32 Probe = 0; Probe < ProbeCount; ++Probe)
	{
		const double AlongTrack = static_cast<double>(Probe) / ProbeCount;
		const int32 RowIndex = Ribbon.RowIndexAt(AlongTrack);

		const int32 Column = FMath::Clamp(
			FMath::RoundToInt32(Ribbon.RibbonToAtlasInRow(RowIndex, AlongTrack, 0.0).X * Ribbon.AtlasWidth()),
			0,
			Ribbon.AtlasWidth() - 1);

		const TArray<FLinearColor> Band = UKismetRenderingLibrary::ReadRenderTargetRawPixelArea(
			World, Painted, Column, RowIndex * AcrossTexels, 1, AcrossTexels, false);

		for (int32 Texel = 0; Texel < Band.Num(); ++Texel)
		{
			StrongestBuildup = FMath::Max(StrongestBuildup, Band[Texel].R);
			StrongestSkid = FMath::Max(StrongestSkid, Band[Texel].G);

			if (Band[Texel].B > StrongestMarbles)
			{
				StrongestMarbles = Band[Texel].B;
				MarblePeakTexel = Texel;
			}

			if (Band[Texel].R >= StrongestBuildup)
			{
				BuildupPeakTexel = Texel;
			}
		}
	}

	AddInfo(FString::Printf(
		TEXT("Read back over %d columns across the ribbon, %d texels each. Peak exposure | buildup %.6f "
			"(coverage %.4f) | skid %.6f (coverage %.4f) | marbles %.6f (coverage %.4f)"),
		ProbeCount,
		AcrossTexels,
		StrongestBuildup, RacetrackSurfaceMapping::ExposureToCoverage(StrongestBuildup),
		StrongestSkid, RacetrackSurfaceMapping::ExposureToCoverage(StrongestSkid),
		StrongestMarbles, RacetrackSurfaceMapping::ExposureToCoverage(StrongestMarbles)));

	AddInfo(FString::Printf(
		TEXT("The marble peak sits at across-track texel %d of %d and the buildup peak at %d, a separation "
			"of %d texels, or %.1f cm"),
		MarblePeakTexel,
		AcrossTexels,
		BuildupPeakTexel,
		FMath::Abs(MarblePeakTexel - BuildupPeakTexel),
		FMath::Abs(MarblePeakTexel - BuildupPeakTexel) * Ribbon.CentimetresPerTexel));

	TestTrue(
		TEXT("The marble channel holds a non-zero exposure in the buffer after a run, which is the claim "
			"the write-side counts alone cannot make"),
		StrongestMarbles > 0.0f);

	Subsystem->ResetSurface();
	DestroyDemo(Track, Field);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDemoLayoutChannelsAtSteadyStateUnderDecay,
	"RubberAccumDemo.Demo.EveryChannelIsVisibleAtSteadyStateUnderDecay",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDemoLayoutChannelsAtSteadyStateUnderDecay::RunTest(const FString& Parameters)
{
	UWorld* World = FindDemoWorld();

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

	TArray<ASplineFollowingCar*> Field;
	ARacetrackCircuit* Track = SpawnDemoCircuit(*World, Field);

	if (Track == nullptr || Field.Num() == 0)
	{
		AddError(TEXT("Failed to spawn the demo circuit or its field."));
		DestroyDemo(Track, Field);
		return false;
	}

	const FScopedDepositionScale Deposition(16.0f);
	const FScopedDecayRate Decay(16.0f);

	Subsystem->ResetSurface();

	const FSurfaceTrackRibbon& Ribbon = Subsystem->GetRibbon();
	const float HalfLife = Subsystem->GetDecayHalfLifeSeconds();

	constexpr int32 TickCount = 2500;

	for (int32 Tick = 0; Tick < TickCount; ++Tick)
	{
		for (ASplineFollowingCar* Car : Field)
		{
			Car->Tick(CompressedTick);
		}

		Subsystem->Tick(CompressedTick);
	}

	UTextureRenderTarget2D* Painted = Subsystem->GetBuffer() != nullptr
		? Subsystem->GetBuffer()->GetTarget()
		: nullptr;

	if (Painted == nullptr)
	{
		AddError(TEXT("No accumulation target was allocated."));
		DestroyDemo(Track, Field);
		return false;
	}

	constexpr int32 ProbeCount = 96;

	const int32 AcrossTexels = Ribbon.AcrossTrackTexels();

	float Peak[AccumulationChannelCount] = {};

	for (int32 Probe = 0; Probe < ProbeCount; ++Probe)
	{
		const double AlongTrack = static_cast<double>(Probe) / ProbeCount;
		const int32 RowIndex = Ribbon.RowIndexAt(AlongTrack);

		const int32 Column = FMath::Clamp(
			FMath::RoundToInt32(Ribbon.RibbonToAtlasInRow(RowIndex, AlongTrack, 0.0).X * Ribbon.AtlasWidth()),
			0,
			Ribbon.AtlasWidth() - 1);

		const TArray<FLinearColor> Band = UKismetRenderingLibrary::ReadRenderTargetRawPixelArea(
			World, Painted, Column, RowIndex * AcrossTexels, 1, AcrossTexels, false);

		for (const FLinearColor& Texel : Band)
		{
			Peak[0] = FMath::Max(Peak[0], Texel.R);
			Peak[1] = FMath::Max(Peak[1], Texel.G);
			Peak[2] = FMath::Max(Peak[2], Texel.B);
		}
	}

	AddInfo(FString::Printf(
		TEXT("Steady state under decay. Half-life %.0f s of track time at rate 16, so a time constant of "
			"%.0f s, ticked %d times at %.4f s which is %.0f s of track time, about %.1f time constants."),
		HalfLife,
		HalfLife / 16.0f,
		TickCount,
		CompressedTick,
		TickCount * CompressedTick,
		TickCount * CompressedTick / (HalfLife / 16.0f)));

	AddInfo(FString::Printf(
		TEXT("Deposition 16 with decay 16 reaches the same equilibrium as the demo's 2 and 2, eight times "
			"sooner. Peak exposure | buildup %.6f (coverage %.4f) | skid %.6f (coverage %.4f) | "
			"marbles %.6f (coverage %.4f)"),
		Peak[0], RacetrackSurfaceMapping::ExposureToCoverage(Peak[0]),
		Peak[1], RacetrackSurfaceMapping::ExposureToCoverage(Peak[1]),
		Peak[2], RacetrackSurfaceMapping::ExposureToCoverage(Peak[2])));

	TestTrue(
		TEXT("The skid channel reaches a coverage a viewer could see at the demo's own settings"),
		RacetrackSurfaceMapping::ExposureToCoverage(Peak[1]) > 0.05f);

	TestTrue(
		TEXT("The marble channel reaches a coverage a viewer could see at the demo's own settings"),
		RacetrackSurfaceMapping::ExposureToCoverage(Peak[2]) > 0.05f);

	Subsystem->ResetSurface();
	DestroyDemo(Track, Field);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDemoLayoutSlipSeparatesStraightsFromCorners,
	"RubberAccumDemo.Demo.SlipSeparatesStraightsFromCornersOnTheDemoLayout",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDemoLayoutSlipSeparatesStraightsFromCorners::RunTest(const FString& Parameters)
{
	UWorld* World = FindDemoWorld();

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
		AddError(TEXT("Failed to spawn the circuit."));
		return false;
	}

	Track->SetLayout(ETrackLayout::Spa, DemoLapLength);

	ASplineFollowingCar* Car = World->SpawnActor<ASplineFollowingCar>();

	if (Car == nullptr)
	{
		AddError(TEXT("Failed to spawn a car."));
		Track->Destroy();
		return false;
	}

	Car->ConfigureDriver(Track, 0.0, 0.0, Track->GetDriverLineCommitment(0), 0);

	constexpr int32 BinCount = 10;

	double SlipTotalPerBin[BinCount] = {};
	int32 SamplesPerBin[BinCount] = {};

	TArray<double> CorneringSamples;
	const double Length = Track->GetTrackLength();

	int32 Ticks = 0;

	while (Car->GetDistanceAlongTrack() < Length && Ticks < 20000)
	{
		const double CorneringHere = Track->GetCorneringAmountAtDistance(Car->GetDistanceAlongTrack());

		Subsystem->ResetSurface();
		Car->Tick(RealTimeTick);

		double WidestSlip = 0.0;

		for (const FSurfaceTrackContact& Contact : Subsystem->GetPendingContacts())
		{
			if (Contact.Channel != EAccumulationChannel::Marbles)
			{
				WidestSlip = FMath::Max(WidestSlip, static_cast<double>(Contact.SlipAmount));
			}
		}

		const int32 Bin = FMath::Clamp(FMath::FloorToInt32(CorneringHere * BinCount), 0, BinCount - 1);

		SlipTotalPerBin[Bin] += WidestSlip;
		++SamplesPerBin[Bin];
		CorneringSamples.Add(CorneringHere);

		++Ticks;
	}

	CorneringSamples.Sort();

	const auto Percentile = [&CorneringSamples](double Fraction)
	{
		const int32 Index = FMath::Clamp(
			FMath::FloorToInt32(Fraction * CorneringSamples.Num()), 0, CorneringSamples.Num() - 1);

		return CorneringSamples.Num() > 0 ? CorneringSamples[Index] : 0.0;
	};

	AddInfo(FString::Printf(
		TEXT("One lap of the demo layout in %d ticks. Cornering amount at the car, by percentile of lap "
			"time: p10 %.3f, p25 %.3f, p50 %.3f, p75 %.3f, p90 %.3f, p99 %.3f"),
		Ticks,
		Percentile(0.10), Percentile(0.25), Percentile(0.50),
		Percentile(0.75), Percentile(0.90), Percentile(0.99)));

	FString BinReport;

	for (int32 Bin = 0; Bin < BinCount; ++Bin)
	{
		const double MeanSlip = SamplesPerBin[Bin] > 0 ? SlipTotalPerBin[Bin] / SamplesPerBin[Bin] : 0.0;

		BinReport += FString::Printf(
			TEXT("\n  cornering %.1f-%.1f: %5d ticks, mean deposited slip %.4f"),
			Bin / static_cast<double>(BinCount),
			(Bin + 1) / static_cast<double>(BinCount),
			SamplesPerBin[Bin],
			MeanSlip);
	}

	AddInfo(FString::Printf(
		TEXT("Slip the car actually registered, bucketed by the cornering at its own position:%s"),
		*BinReport));

	const double StraightSlip = SamplesPerBin[0] > 0 ? SlipTotalPerBin[0] / SamplesPerBin[0] : 0.0;
	const double CornerSlip =
		SamplesPerBin[BinCount - 1] > 0 ? SlipTotalPerBin[BinCount - 1] / SamplesPerBin[BinCount - 1] : 0.0;

	AddInfo(FString::Printf(
		TEXT("Least-cornering bucket deposits %.4f slip against %.4f in the most-cornering bucket, a ratio "
			"of %.2f to 1. The model's own range is %.2f to %.2f, which is %.2f to 1."),
		StraightSlip,
		CornerSlip,
		StraightSlip > UE_DOUBLE_SMALL_NUMBER ? CornerSlip / StraightSlip : 0.0,
		Car->GetStraightSlipAmount(),
		Car->GetCorneringSlipAmount(),
		Car->GetCorneringSlipAmount() / FMath::Max(Car->GetStraightSlipAmount(), UE_SMALL_NUMBER)));

	TestTrue(
		TEXT("Some of the lap is genuinely straight, meaning the cornering profile reports near zero "
			"rather than reading gentle spline wobble as cornering"),
		Percentile(0.10) < 0.1);

	TestTrue(
		TEXT("The slip a car deposits on the straightest part of the lap is close to its own "
			"StraightSlipAmount rather than inflated toward the cornering value"),
		StraightSlip < Car->GetStraightSlipAmount() * 2.0);

	Subsystem->ResetSurface();
	Car->Destroy();
	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDemoLayoutStrokesDoNotSmearUnderCompressedTime,
	"RubberAccumDemo.Demo.StrokesDoNotSmearAcrossTheTrackUnderCompressedTime",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDemoLayoutStrokesDoNotSmearUnderCompressedTime::RunTest(const FString& Parameters)
{
	UWorld* World = FindDemoWorld();

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

	TArray<ASplineFollowingCar*> Field;
	ARacetrackCircuit* Track = SpawnDemoCircuit(*World, Field);

	if (Track == nullptr || Field.Num() == 0)
	{
		AddError(TEXT("Failed to spawn the demo circuit or its field."));
		DestroyDemo(Track, Field);
		return false;
	}

	Subsystem->ResetSurface();

	const FSurfaceTrackRibbon& Ribbon = Subsystem->GetRibbon();
	const double MarkWidthTexels = Ribbon.WorldDistanceToTexels(Field[0]->GetContactWidth());

	FStrokeTally RealTime;
	FStrokeTally Compressed;

	constexpr int32 SettleTicks = 200;
	constexpr int32 SampleTicks = 300;

	for (int32 Tick = 0; Tick < SettleTicks; ++Tick)
	{
		for (ASplineFollowingCar* Car : Field)
		{
			Car->Tick(RealTimeTick);
		}

		Subsystem->Tick(RealTimeTick);
	}

	for (int32 Tick = 0; Tick < SampleTicks; ++Tick)
	{
		for (ASplineFollowingCar* Car : Field)
		{
			Car->Tick(RealTimeTick);
		}

		Subsystem->Tick(RealTimeTick);

		for (const FSurfaceBrushStroke& Stroke : Subsystem->GetStrokesResolvedThisFrame())
		{
			TallyStroke(Stroke, Ribbon, RealTime);
		}
	}

	for (int32 Tick = 0; Tick < SampleTicks; ++Tick)
	{
		for (ASplineFollowingCar* Car : Field)
		{
			Car->Tick(CompressedTick);
		}

		Subsystem->Tick(CompressedTick);

		for (const FSurfaceBrushStroke& Stroke : Subsystem->GetStrokesResolvedThisFrame())
		{
			TallyStroke(Stroke, Ribbon, Compressed);
		}
	}

	AddInfo(FString::Printf(
		TEXT("A %.1f cm contact patch is %.2f texels on a track %d texels wide. The lateral extrapolation "
			"is capped at %.4f s."),
		Field[0]->GetContactWidth(),
		MarkWidthTexels,
		Ribbon.AcrossTrackTexels(),
		RacetrackSurfaceMapping::MaximumLateralExtrapolationSeconds));

	AddInfo(FString::Printf(
		TEXT("Real time, %d ticks at %.4f s: %d strokes, longest %.1f cm along the track, widest lateral "
			"span %.2f texels"),
		SampleTicks, RealTimeTick, RealTime.Count,
		RealTime.LongestAlongCentimetres, RealTime.WidestLateralTexels));

	AddInfo(FString::Printf(
		TEXT("Compressed 20x, %d ticks at %.4f s: %d strokes, longest %.1f cm along the track, widest "
			"lateral span %.2f texels"),
		SampleTicks, CompressedTick, Compressed.Count,
		Compressed.LongestAlongCentimetres, Compressed.WidestLateralTexels));

	AddInfo(FString::Printf(
		TEXT("Lateral smear ratio against the mark's own width: real time %.1fx, compressed %.1fx"),
		RealTime.WidestLateralTexels / FMath::Max(MarkWidthTexels, 1.0),
		Compressed.WidestLateralTexels / FMath::Max(MarkWidthTexels, 1.0)));

	AddInfo(FString::Printf(
		TEXT("Furthest a painted stroke endpoint reaches across the track: real time alpha %.4f with %d "
			"endpoints past the edge, compressed alpha %.4f with %d past the edge. No wheel on this field "
			"goes beyond alpha 0.7749, so anything past that was invented by the write path rather than "
			"driven over"),
		RealTime.FurthestAlphaFromCentre,
		RealTime.EndpointsBeyondTheTrackEdge,
		Compressed.FurthestAlphaFromCentre,
		Compressed.EndpointsBeyondTheTrackEdge));

	TestTrue(
		TEXT("No painted stroke reaches past the track edge on this field, because no car goes near it. A "
			"mark off the track would mean the write path invented lateral movement"),
		RealTime.EndpointsBeyondTheTrackEdge == 0 && Compressed.EndpointsBeyondTheTrackEdge == 0);

	TestTrue(
		TEXT("At real time a stroke does not smear sideways by more than a few mark widths, so what is "
			"painted is a tyre track rather than a diagonal across the track"),
		RealTime.WidestLateralTexels <= MarkWidthTexels * 4.0);

	TestTrue(
		TEXT("Under 20x compression a stroke does not smear sideways across the track, which is what the "
			"capped lateral extrapolation exists to prevent. Without the cap this measured 97 texels, "
			"fifteen times the mark's own width"),
		Compressed.WidestLateralTexels <= MarkWidthTexels * 4.0);

	TestTrue(
		TEXT("A compressed stroke is long along the track and that is not a defect, because the ribbon is "
			"unrolled: a car holding its line through a corner is a straight line in track space. Only the "
			"lateral term needed bounding"),
		Compressed.LongestAlongCentimetres > RealTime.LongestAlongCentimetres * 10.0);

	Subsystem->ResetSurface();
	DestroyDemo(Track, Field);

	return true;
}

#endif
