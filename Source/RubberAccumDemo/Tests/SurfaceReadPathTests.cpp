#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "RenderingThread.h"
#include "UObject/StrongObjectPtr.h"
#include "SurfaceAccumulationBuffer.h"
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
		Ribbon.TrackLength = 62500.0;
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

	UKismetMaterialLibrary::SetScalarParameterValue(
		World,
		Collection,
		RacetrackSurfaceMapping::SaturationConstantParameterName,
		RacetrackSurfaceMapping::SaturationConstant);

	UKismetMaterialLibrary::SetScalarParameterValue(
		World,
		Collection,
		RacetrackSurfaceMapping::AccumulationRowCountParameterName,
		static_cast<float>(Ribbon.RowCount()));

	TStrongObjectPtr<USurfaceAccumulationBuffer> Buffer(NewObject<USurfaceAccumulationBuffer>());

	if (!Buffer->Allocate(World, Ribbon, RTF_RGBA16f))
	{
		AddError(TEXT("Failed to allocate the folded accumulation buffer."));
		return false;
	}

	const int32 PaintedRow = Ribbon.RowIndexAt((PaintedAlongTrackStart + PaintedAlongTrackEnd) * 0.5);

	FSurfaceBrushStroke Stroke;
	Stroke.RegionStart = Ribbon.RibbonToAtlasInRow(PaintedRow, PaintedAlongTrackStart, PaintedAcrossTrack);
	Stroke.RegionEnd = Ribbon.RibbonToAtlasInRow(PaintedRow, PaintedAlongTrackEnd, PaintedAcrossTrack);
	Stroke.WidthInTexels = 16.0f;
	Stroke.Intensity = 0.5f;
	Stroke.RowIndex = PaintedRow;
	Stroke.Channel = EAccumulationChannel::SkidMark;

	TestEqual(TEXT("The painted span belongs to the second row"), PaintedRow, 1);

	Buffer->PaintStrokes(World, TArray<FSurfaceBrushStroke>({ Stroke }));
	FlushRenderingCommands();

	UMaterialInstanceDynamic* Sampler = UMaterialInstanceDynamic::Create(DebugMaterial, World);

	if (Sampler == nullptr)
	{
		AddError(TEXT("Could not create a dynamic instance of the debug material."));
		Buffer->Release();
		return false;
	}

	Sampler->SetTextureParameterValue(
		RacetrackSurfaceMapping::AccumulationTargetParameterName,
		Buffer->GetTarget());

	UTextureRenderTarget2D* Sampled = UKismetRenderingLibrary::CreateRenderTarget2D(
		World, SampledWidth, SampledHeight, RTF_RGBA16f, FLinearColor::Black, false, false);

	if (Sampled == nullptr)
	{
		AddError(TEXT("Could not create the render target the material is sampled into."));
		Buffer->Release();
		return false;
	}

	UKismetRenderingLibrary::DrawMaterialToRenderTarget(World, Sampled, Sampler);
	FlushRenderingCommands();

	const TArray<FLinearColor> Coverage = UKismetRenderingLibrary::ReadRenderTargetRawPixelArea(
		World, Sampled, 0, 0, SampledWidth, SampledHeight, false);

	if (Coverage.Num() != SampledWidth * SampledHeight)
	{
		AddError(FString::Printf(
			TEXT("Readback returned %d texels, expected %d"), Coverage.Num(), SampledWidth * SampledHeight));
		UKismetRenderingLibrary::ReleaseRenderTarget2D(Sampled);
		Buffer->Release();
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

			const bool bSameColumnInTheFirstRow =
				AlongTrack >= PaintedAlongTrackStart - 0.5 && AlongTrack <= PaintedAlongTrackEnd - 0.5;

			if (bSameColumnInTheFirstRow)
			{
				BrightestInTheWrongRow = FMath::Max(BrightestInTheWrongRow, Value);
			}
		}
	}

	if (TotalCoverage <= 0.0)
	{
		AddError(TEXT("The material sampled nothing at all, so the read path could not be checked."));
		UKismetRenderingLibrary::ReleaseRenderTarget2D(Sampled);
		Buffer->Release();
		return false;
	}

	const double CentreAlongTrack = WeightedAlongTrack / TotalCoverage;
	const double CentreAcrossTrack = WeightedAcrossTrack / TotalCoverage;
	const double PaintedCentre = (PaintedAlongTrackStart + PaintedAlongTrackEnd) * 0.5;

	AddInfo(FString::Printf(
		TEXT("Painted at ribbon %.3f..%.3f across %.3f in row %d of %d; sampled back centred on %.3f, %.3f, "
			"peaking at %.4f, with %.4f at the same column of the first row"),
		PaintedAlongTrackStart,
		PaintedAlongTrackEnd,
		PaintedAcrossTrack,
		PaintedRow,
		Ribbon.RowCount(),
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
	Buffer->Release();

	return true;
}

#endif
