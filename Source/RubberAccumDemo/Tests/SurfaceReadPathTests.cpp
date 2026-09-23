#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "SurfaceAccumulationMapping.h"
#include "SurfaceAccumulationSubsystem.h"
#include "SurfaceTrackRibbon.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr const TCHAR* DebugMaterialPath = TEXT("/Game/Materials/M_SurfaceAccumulationDebug.M_SurfaceAccumulationDebug");

	constexpr int32 SampledWidth = 128;
	constexpr int32 SampledHeight = 32;

	constexpr double PaintedAlongTrackStart = 0.70;
	constexpr double PaintedAlongTrackEnd = 0.80;
	constexpr double PaintedAcrossTrack = 0.5;

	constexpr double ReadPathLapCentimetres = 62500.0;
	constexpr float ReadPathSeconds = 1.0f;
	constexpr float ReadPathDepositionScale = 200.0f;

	UWorld* FindReadPathWorld()
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

	FSurfaceTrackRibbon MakeTwoRowReadRibbon()
	{
		FSurfaceTrackRibbon Ribbon;
		Ribbon.TrackLength = ReadPathLapCentimetres;
		Ribbon.TrackWidth = 200.0;
		Ribbon.CentimetresPerTexel = 3.125;
		Ribbon.bClosedLoop = true;
		return Ribbon;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSurfaceReadPathReturnsTheMarkWhereItWasPainted,
	"RubberAccumDemo.Surface.ReadPathReturnsTheMarkWhereItWasPainted",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSurfaceReadPathReturnsTheMarkWhereItWasPainted::RunTest(const FString& Parameters)
{
	if (!FApp::CanEverRender())
	{
		AddWarning(TEXT("Skipped: build cannot render."));
		return true;
	}

	UWorld* World = FindReadPathWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No editor, game or PIE world available."));
		return false;
	}

	USurfaceAccumulationSubsystem* Subsystem = World->GetSubsystem<USurfaceAccumulationSubsystem>();

	if (Subsystem == nullptr)
	{
		AddError(TEXT("Could not resolve USurfaceAccumulationSubsystem from the world."));
		return false;
	}

	if (!Subsystem->IsInitialized())
	{
		AddWarning(TEXT("Skipped: the world's accumulation subsystem is not initialised, so it cannot be ticked."));
		return true;
	}

	UMaterial* DebugMaterial = LoadObject<UMaterial>(nullptr, DebugMaterialPath);

	if (DebugMaterial == nullptr)
	{
		AddError(FString::Printf(TEXT("Could not load %s, which is what closes the read path."), DebugMaterialPath));
		return false;
	}

	UMaterialParameterCollection* Collection = LoadObject<UMaterialParameterCollection>(
		nullptr,
		RacetrackSurfaceMapping::ParameterCollectionAssetPath);

	if (Collection == nullptr)
	{
		AddError(FString::Printf(
			TEXT("Parameter collection %s could not be loaded."),
			RacetrackSurfaceMapping::ParameterCollectionAssetPath));
		return false;
	}

	const FSurfaceTrackRibbon Ribbon = MakeTwoRowReadRibbon();

	TestEqual(TEXT("The read path is exercised against a folded ribbon"), Ribbon.RowCount(), 2);

	Subsystem->ConfigureTrackRibbon(nullptr, Ribbon);
	Subsystem->ResetSurface();

	UTextureRenderTarget2D* Accumulation = Subsystem->GetOrCreateAccumulationTarget();

	if (Accumulation == nullptr)
	{
		AddError(TEXT("The subsystem produced no accumulation target for a folded ribbon."));
		return false;
	}

	TestEqual(TEXT("The target is as wide as one row"), Accumulation->SizeX, Ribbon.AtlasWidth());
	TestEqual(TEXT("The target stacks every row"), Accumulation->SizeY, Ribbon.AtlasHeight());

	IConsoleVariable* DepositionScale = IConsoleManager::Get().FindConsoleVariable(
		RacetrackSurfaceMapping::DepositionScaleVariableName);

	const float RestoreDepositionScale = DepositionScale != nullptr ? DepositionScale->GetFloat() : 1.0f;

	if (DepositionScale != nullptr)
	{
		DepositionScale->Set(ReadPathDepositionScale);
	}

	const double PaintedCentre = (PaintedAlongTrackStart + PaintedAlongTrackEnd) * 0.5;
	const double TravelCentimetres = (PaintedAlongTrackEnd - PaintedAlongTrackStart) * Ribbon.TrackLength;

	Subsystem->RegisterSurfaceContactInTrackSpace(
		PaintedAlongTrackEnd * Ribbon.TrackLength,
		0.0,
		TravelCentimetres / ReadPathSeconds,
		1.0f,
		60.0f,
		EAccumulationChannel::SkidMark);

	Subsystem->Tick(ReadPathSeconds);

	if (DepositionScale != nullptr)
	{
		DepositionScale->Set(RestoreDepositionScale);
	}

	const TArray<FSurfaceBrushStroke>& Painted = Subsystem->GetStrokesResolvedThisFrame();

	TestEqual(TEXT("The painted span stays inside one row and is not split"), Painted.Num(), 1);

	if (Painted.Num() == 1)
	{
		TestEqual(TEXT("The painted span belongs to the second row"), Painted[0].RowIndex, 1);
	}

	UMaterialInstanceDynamic* Sampler = UMaterialInstanceDynamic::Create(DebugMaterial, World);

	if (Sampler == nullptr)
	{
		AddError(TEXT("Could not create a dynamic instance of the debug material."));
		return false;
	}

	Sampler->SetTextureParameterValue(
		RacetrackSurfaceMapping::AccumulationTargetParameterName,
		Accumulation);

	UTextureRenderTarget2D* Sampled = UKismetRenderingLibrary::CreateRenderTarget2D(
		World, SampledWidth, SampledHeight, RTF_RGBA16f, FLinearColor::Black, false, false);

	if (Sampled == nullptr)
	{
		AddError(TEXT("Could not create the render target the material is sampled into."));
		return false;
	}

	UKismetRenderingLibrary::DrawMaterialToRenderTarget(World, Sampled, Sampler);

	const TArray<FLinearColor> Coverage = UKismetRenderingLibrary::ReadRenderTargetRawPixelArea(
		World, Sampled, 0, 0, SampledWidth, SampledHeight, false);

	if (Coverage.Num() != SampledWidth * SampledHeight)
	{
		AddError(FString::Printf(
			TEXT("Readback returned %d texels, expected %d"), Coverage.Num(), SampledWidth * SampledHeight));
		UKismetRenderingLibrary::ReleaseRenderTarget2D(Sampled);
		return false;
	}

	double WeightedAlongTrack = 0.0;
	double WeightedAcrossTrack = 0.0;
	double TotalCoverage = 0.0;
	float BrightestInTheWrongRow = 0.0f;
	float Brightest = 0.0f;

	for (int32 Y = 0; Y < SampledHeight; ++Y)
	{
		for (int32 X = 0; X < SampledWidth; ++X)
		{
			const float Value = Coverage[Y * SampledWidth + X].G;

			if (Value <= 0.0f)
			{
				continue;
			}

			const double AlongTrack = (X + 0.5) / SampledWidth;
			const double AcrossTrack = (Y + 0.5) / SampledHeight;

			WeightedAlongTrack += AlongTrack * Value;
			WeightedAcrossTrack += AcrossTrack * Value;
			TotalCoverage += Value;

			Brightest = FMath::Max(Brightest, Value);

			const double SameColumnOfTheFirstRow = AlongTrack + 1.0 / Ribbon.RowCount();

			if (SameColumnOfTheFirstRow >= PaintedAlongTrackStart && SameColumnOfTheFirstRow <= PaintedAlongTrackEnd)
			{
				BrightestInTheWrongRow = FMath::Max(BrightestInTheWrongRow, Value);
			}
		}
	}

	if (TotalCoverage <= 0.0)
	{
		AddError(TEXT("The material sampled nothing at all, so the read path could not be checked."));
		UKismetRenderingLibrary::ReleaseRenderTarget2D(Sampled);
		return false;
	}

	const double CentreAlongTrack = WeightedAlongTrack / TotalCoverage;
	const double CentreAcrossTrack = WeightedAcrossTrack / TotalCoverage;

	AddInfo(FString::Printf(
		TEXT("Painted ribbon %.3f..%.3f across %.3f into row 1 of %d on a %d x %d atlas; sampled back centred "
			"on %.3f, %.3f, peaking at %.4f, with %.4f at the same column of the first row"),
		PaintedAlongTrackStart,
		PaintedAlongTrackEnd,
		PaintedAcrossTrack,
		Ribbon.RowCount(),
		Ribbon.AtlasWidth(),
		Ribbon.AtlasHeight(),
		CentreAlongTrack,
		CentreAcrossTrack,
		Brightest,
		BrightestInTheWrongRow));

	TestTrue(
		FString::Printf(
			TEXT("The mark reads back at the lap fraction it was painted at, %.3f against %.3f"),
			CentreAlongTrack,
			PaintedCentre),
		FMath::Abs(CentreAlongTrack - PaintedCentre) < 0.02);

	TestTrue(
		FString::Printf(
			TEXT("The mark reads back at the track fraction it was painted at, %.3f against %.3f"),
			CentreAcrossTrack,
			PaintedAcrossTrack),
		FMath::Abs(CentreAcrossTrack - PaintedAcrossTrack) < 0.04);

	TestTrue(
		FString::Printf(
			TEXT("Nothing appears half a lap away, which is where a read that ignored the fold would put it: "
				"%.4f against a peak of %.4f"),
			BrightestInTheWrongRow,
			Brightest),
		BrightestInTheWrongRow <= Brightest * 0.05f);

	UKismetRenderingLibrary::ReleaseRenderTarget2D(Sampled);

	return true;
}

#endif
