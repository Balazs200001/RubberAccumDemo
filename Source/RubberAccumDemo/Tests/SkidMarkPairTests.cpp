#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "SurfaceAccumulationMapping.h"
#include "SurfaceAccumulationSubsystem.h"
#include "SurfaceTrackRibbon.h"
#include "../RacetrackCircuit.h"

namespace RubberAccumDemoSkidMarkPairTest
{
	constexpr float TickSeconds = 1.0f / 60.0f;

	constexpr double LineAlpha = 0.35;

	constexpr double FainterBandAlpha = -0.2;

	constexpr double RubberStrokeCentimetres = 400.0;

	constexpr float RubberContactWidth = 40.0f;

	constexpr float RubberSlip = 0.5f;

	constexpr int32 LineContacts = 400;

	constexpr int32 FainterBandContacts = 80;

	UWorld* FindSkidMarkPairWorld()
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

	void LayRubberBand(USurfaceAccumulationSubsystem& Subsystem, double Distance, double Alpha, int32 Contacts)
	{
		for (int32 Contact = 0; Contact < Contacts; ++Contact)
		{
			Subsystem.RegisterSurfaceContactInTrackSpace(
				Distance + RubberStrokeCentimetres * 0.5,
				Alpha,
				RubberStrokeCentimetres / TickSeconds,
				RubberSlip,
				RubberContactWidth);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRacetrackCircuitSkidMarkPairLandsOnAndOffTheRubber,
	"RubberAccumDemo.Track.SkidMarkPairLandsOnAndOffTheRubber",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRacetrackCircuitSkidMarkPairLandsOnAndOffTheRubber::RunTest(const FString& Parameters)
{
	using namespace RubberAccumDemoSkidMarkPairTest;

	UWorld* World = FindSkidMarkPairWorld();

	USurfaceAccumulationSubsystem* Subsystem =
		World != nullptr ? World->GetSubsystem<USurfaceAccumulationSubsystem>() : nullptr;

	if (Subsystem == nullptr)
	{
		AddError(TEXT("No world or subsystem available."));
		return false;
	}

	ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>();

	if (Track == nullptr)
	{
		AddError(TEXT("Failed to spawn ARacetrackCircuit."));
		return false;
	}

	Subsystem->ResetSurface();

	if (Subsystem->GetOrCreateAccumulationTarget() == nullptr)
	{
		AddError(TEXT("No accumulation target."));
		Track->Destroy();
		return false;
	}

	const FSurfaceTrackRibbon& Ribbon = Subsystem->GetRibbon();
	const double Apex = Track->FindLongestApexDistance();
	const double TexelInAlpha = 2.0 / Ribbon.AcrossTrackTexels();

	LayRubberBand(*Subsystem, Apex, LineAlpha, LineContacts);
	LayRubberBand(*Subsystem, Apex, FainterBandAlpha, FainterBandContacts);
	Subsystem->Tick(TickSeconds);

	Track->ArmSkidMarkPair(0.0, 0.0);
	Track->Tick(TickSeconds);
	Subsystem->Tick(TickSeconds);

	const TOptional<FSkidMarkPairPlacement>& Pair = Track->GetSkidMarkPair();

	if (!Pair.IsSet())
	{
		AddError(TEXT("Arming the pair with no delay and ticking once did not lay it."));
		Track->Destroy();
		Subsystem->ResetSurface();
		return false;
	}

	FLinearColor OnTheLine;
	FLinearColor OffTheLine;

	if (!Track->ReadSkidMarkPair(OnTheLine, OffTheLine))
	{
		AddError(TEXT("The pair could not be read back."));
		Track->Destroy();
		Subsystem->ResetSurface();
		return false;
	}

	const float SweptShare = static_cast<float>(
		Pair->MarkLengthCentimetres / (Pair->MarkLengthCentimetres + Pair->ContactWidth));
	const float LaidExposure = RacetrackSurfaceMapping::LockupExposure() * SweptShare;

	AddInfo(FString::Printf(
		TEXT("Longest apex %.0f cm round a %.0f cm lap. Rubber painted at alpha %.4f and a fainter band at %.4f. "
			"The pair went on the line at alpha %.4f over rubber %.6f and off it at alpha %.4f over rubber "
			"%.6f, each one %.0f cm lockup contact, and reads back skid %.6f and %.6f against a predicted %.6f"),
		Apex,
		Ribbon.TrackLength,
		LineAlpha,
		FainterBandAlpha,
		Pair->OnTheLineAlpha,
		Pair->OnTheLineRubber,
		Pair->OffTheLineAlpha,
		Pair->OffTheLineRubber,
		Pair->MarkLengthCentimetres,
		OnTheLine.G,
		OffTheLine.G,
		LaidExposure));

	TestTrue(
		TEXT("The mark meant for the line lands on the most rubbered texel, within two texels of where the "
			"rubber was laid"),
		FMath::Abs(Pair->OnTheLineAlpha - LineAlpha) <= 2.0 * TexelInAlpha);

	TestTrue(
		TEXT("The mark meant for the line sits over real rubber"),
		Pair->OnTheLineRubber > 0.1f);

	TestEqual(
		TEXT("The mark meant for bare track sits over no rubber at all, so the fainter band was avoided"),
		Pair->OffTheLineRubber,
		0.0f);

	TestTrue(
		TEXT("The two marks are at least a quarter of the track width apart"),
		FMath::Abs(Pair->OnTheLineAlpha - Pair->OffTheLineAlpha) >= 0.5 - TexelInAlpha);

	TestEqual(
		TEXT("The two marks read back identical, so any later difference is the surface's doing"),
		OnTheLine.G,
		OffTheLine.G);

	TestTrue(
		TEXT("Each mark is one lockup, so it reads the lockup exposure times the share of its quad the contact "
			"swept, to half-float precision"),
		FMath::IsNearlyEqual(OnTheLine.G, LaidExposure, LaidExposure * 1.0e-3f));

	TestTrue(
		TEXT("Laying the marks does not disturb the rubber under them"),
		FMath::IsNearlyEqual(OnTheLine.R, Pair->OnTheLineRubber, 1.0e-6f));

	Track->Destroy();
	Subsystem->ResetSurface();

	return true;
}

#endif
