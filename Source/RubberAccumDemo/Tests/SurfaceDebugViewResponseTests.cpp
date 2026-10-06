#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ShaderCompiler.h"
#include "SurfaceAccumulationMapping.h"
#include "SurfaceAccumulationSubsystem.h"
#include "SurfaceTrackRibbon.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace RubberAccumDebugViewResponse
{
	constexpr const TCHAR* DebugMaterialPath = TEXT("/Game/Materials/M_SurfaceAccumulationDebug.M_SurfaceAccumulationDebug");
	constexpr const TCHAR* PlaneMeshPath = TEXT("/Engine/BasicShapes/Plane.Plane");
	constexpr const TCHAR* ChannelMaskParameterName = TEXT("DebugChannelMask");

	constexpr int32 CaptureSize = 64;
	constexpr int32 CentreBlock = 8;
	constexpr int32 ExposureTextureSize = 16;

	constexpr double CaptureAltitude = 500000.0;
	constexpr double CaptureDistance = 100.0;
	constexpr float CaptureFieldOfView = 20.0f;

	constexpr float ExposureAtZeroBias = 1.2f;

	constexpr float RawEmissiveTolerance = 1.0e-3f;

	constexpr int32 CapturesPerPersistentReading = 8;

	constexpr float SettledWithinLevels = 0.5f;

	constexpr int32 StripeCaptureSize = 512;
	constexpr double StripeWidthOfTheFrame = 0.04;
	constexpr double StripeLengthOfTheFrame = 0.5;
	constexpr double EnginePlaneCentimetres = 100.0;

	const float StripeCoverages[] = { 0.10f, 0.33f, 0.50f, 0.76f, 0.95f };

	struct FStripeVariant
	{
		const TCHAR* Name;
		bool bFlattenLocalExposure;
		bool bRemoveBloom;
	};

	const FStripeVariant StripeVariants[] = {
		{ TEXT("full chain"), false, false },
		{ TEXT("local exposure flattened"), true, false },
		{ TEXT("local exposure flattened, no bloom"), true, true },
	};

	FPostProcessSettings SettingsForStripe(const FStripeVariant& Variant)
	{
		FPostProcessSettings Settings;

		if (Variant.bFlattenLocalExposure)
		{
			Settings.bOverride_LocalExposureHighlightContrastScale = true;
			Settings.LocalExposureHighlightContrastScale = 1.0f;
			Settings.bOverride_LocalExposureShadowContrastScale = true;
			Settings.LocalExposureShadowContrastScale = 1.0f;
		}

		if (Variant.bRemoveBloom)
		{
			Settings.bOverride_BloomIntensity = true;
			Settings.BloomIntensity = 0.0f;
		}

		return Settings;
	}

	const float Coverages[] = { 0.05f, 0.10f, 0.18f, 0.25f, 0.33f, 0.50f, 0.75f, 0.95f };

	const float DenseCoverages[] = {
		0.00f, 0.01f, 0.02f, 0.03f, 0.04f, 0.05f, 0.06f, 0.08f, 0.10f, 0.12f, 0.14f, 0.16f, 0.18f,
		0.20f, 0.23f, 0.26f, 0.30f, 0.35f, 0.40f, 0.45f, 0.50f, 0.60f, 0.70f, 0.80f, 0.90f, 0.99f,
	};

	struct FChannelView
	{
		const TCHAR* Name;
		FLinearColor Mask;
		int32 Component;
	};

	const FChannelView ChannelViews[] = {
		{ TEXT("rubber, red"), FLinearColor(1.0f, 0.0f, 0.0f, 0.0f), 0 },
		{ TEXT("skid, green"), FLinearColor(0.0f, 1.0f, 0.0f, 0.0f), 1 },
		{ TEXT("marbles, blue"), FLinearColor(0.0f, 0.0f, 1.0f, 0.0f), 2 },
	};

	enum class EChainStage : uint8
	{
		TonemapperOnly,
		WithLocalExposure,
		WithProjectExposure,
		FullChain,
	};

	const EChainStage ChainStages[] = {
		EChainStage::TonemapperOnly,
		EChainStage::WithLocalExposure,
		EChainStage::WithProjectExposure,
		EChainStage::FullChain,
	};

	struct FCaptureMode
	{
		const TCHAR* Name;
		bool bPersistRenderingState;
		int32 CapturesPerReading;
	};

	const FCaptureMode CaptureModes[] = {
		{ TEXT("one-off capture, no view state"), false, 1 },
		{ TEXT("capture with a persistent view state"), true, CapturesPerPersistentReading },
	};

	FPostProcessSettings SettingsFor(EChainStage Stage)
	{
		FPostProcessSettings Settings;

		if (Stage != EChainStage::FullChain)
		{
			Settings.bOverride_BloomIntensity = true;
			Settings.BloomIntensity = 0.0f;
		}

		if (Stage == EChainStage::TonemapperOnly)
		{
			Settings.bOverride_LocalExposureHighlightContrastScale = true;
			Settings.LocalExposureHighlightContrastScale = 1.0f;
			Settings.bOverride_LocalExposureShadowContrastScale = true;
			Settings.LocalExposureShadowContrastScale = 1.0f;
		}

		if (Stage == EChainStage::TonemapperOnly || Stage == EChainStage::WithLocalExposure)
		{
			Settings.bOverride_AutoExposureBias = true;
			Settings.AutoExposureBias = FMath::Log2(1.0f / ExposureAtZeroBias);
		}

		return Settings;
	}

	float ExposureForCoverage(float Coverage)
	{
		return -FMath::Loge(1.0f - Coverage) / RacetrackSurfaceMapping::SaturationConstant;
	}

	float Component(const FLinearColor& Colour, int32 Index)
	{
		return Index == 0 ? Colour.R : (Index == 1 ? Colour.G : Colour.B);
	}

	bool ReadBlock(UTextureRenderTarget2D& Target, int32 CentreX, int32 CentreY, FLinearColor& OutMeanBytes)
	{
		FTextureRenderTargetResource* Resource = Target.GameThread_GetRenderTargetResource();
		TArray<FColor> Pixels;
		const int32 Width = Target.SizeX;
		const int32 Height = Target.SizeY;

		if (Resource == nullptr || !Resource->ReadPixels(Pixels) || Pixels.Num() != Width * Height)
		{
			return false;
		}

		const int32 FirstX = FMath::Clamp(CentreX - CentreBlock / 2, 0, Width - CentreBlock);
		const int32 FirstY = FMath::Clamp(CentreY - CentreBlock / 2, 0, Height - CentreBlock);
		FLinearColor Sum = FLinearColor::Transparent;

		for (int32 Y = FirstY; Y < FirstY + CentreBlock; ++Y)
		{
			for (int32 X = FirstX; X < FirstX + CentreBlock; ++X)
			{
				const FColor& Pixel = Pixels[Y * Width + X];
				Sum += FLinearColor(Pixel.R, Pixel.G, Pixel.B, Pixel.A);
			}
		}

		OutMeanBytes = Sum / static_cast<float>(CentreBlock * CentreBlock);
		return true;
	}

	bool ReadCentre(UTextureRenderTarget2D& Target, FLinearColor& OutMeanBytes)
	{
		return ReadBlock(Target, Target.SizeX / 2, Target.SizeY / 2, OutMeanBytes);
	}

	bool CaptureUntilSettled(
		USceneCaptureComponent2D& Capture,
		UTextureRenderTarget2D& Target,
		int32 Captures,
		FLinearColor& OutLast,
		FLinearColor& OutBeforeLast)
	{
		for (int32 Index = 0; Index < Captures; ++Index)
		{
			Capture.CaptureScene();

			if (Index + 2 >= Captures)
			{
				OutBeforeLast = OutLast;

				if (!ReadCentre(Target, OutLast))
				{
					return false;
				}
			}
		}

		if (Captures < 2)
		{
			OutBeforeLast = OutLast;
		}

		return true;
	}

	UWorld* FindResponseWorld()
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

	FSurfaceTrackRibbon MakeResponseRibbon()
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
	FSurfaceDebugViewResponse,
	"RubberAccumDemo.Surface.DebugViewResponse",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSurfaceDebugViewResponse::RunTest(const FString& Parameters)
{
	using namespace RubberAccumDebugViewResponse;

	if (!FApp::CanEverRender())
	{
		AddWarning(TEXT("Skipped: build cannot render."));
		return true;
	}

	UWorld* World = FindResponseWorld();
	USurfaceAccumulationSubsystem* Subsystem = World != nullptr ? World->GetSubsystem<USurfaceAccumulationSubsystem>() : nullptr;

	if (Subsystem == nullptr || !Subsystem->IsInitialized())
	{
		AddError(TEXT("No renderable world with an initialised accumulation subsystem."));
		return false;
	}

	UMaterial* DebugMaterial = LoadObject<UMaterial>(nullptr, DebugMaterialPath);
	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, PlaneMeshPath);

	if (DebugMaterial == nullptr || PlaneMesh == nullptr)
	{
		AddError(FString::Printf(TEXT("Could not load %s or %s."), DebugMaterialPath, PlaneMeshPath));
		return false;
	}

	Subsystem->ConfigureTrackRibbon(nullptr, MakeResponseRibbon());

	UTextureRenderTarget2D* ExposureTexture = UKismetRenderingLibrary::CreateRenderTarget2D(
		World, ExposureTextureSize, ExposureTextureSize, RTF_RGBA32f, FLinearColor::Black, false, false);
	UTextureRenderTarget2D* RawTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
		World, ExposureTextureSize, ExposureTextureSize, RTF_RGBA16f, FLinearColor::Black, false, false);
	UTextureRenderTarget2D* LdrTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
		World, CaptureSize, CaptureSize, RTF_RGBA8, FLinearColor::Black, false, false);
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(DebugMaterial, World);

	if (ExposureTexture == nullptr || RawTarget == nullptr || LdrTarget == nullptr || Material == nullptr)
	{
		AddError(TEXT("Could not create the render targets or the material instance."));
		return false;
	}

	Material->SetTextureParameterValue(RacetrackSurfaceMapping::AccumulationTargetParameterName, ExposureTexture);

	const FVector PlaneLocation(0.0, 0.0, CaptureAltitude);

	FActorSpawnParameters TransientSpawn;
	TransientSpawn.ObjectFlags |= RF_Transient;

	AStaticMeshActor* Plane = World->SpawnActor<AStaticMeshActor>(PlaneLocation, FRotator::ZeroRotator, TransientSpawn);
	ASceneCapture2D* Camera = World->SpawnActor<ASceneCapture2D>(
		PlaneLocation + FVector(0.0, 0.0, CaptureDistance), FRotator(-90.0, 0.0, 0.0), TransientSpawn);

	if (Plane == nullptr || Camera == nullptr)
	{
		AddError(TEXT("Could not spawn the plane or the scene capture."));
		return false;
	}

	UStaticMeshComponent* PlaneComponent = Plane->GetStaticMeshComponent();
	PlaneComponent->SetMobility(EComponentMobility::Movable);
	PlaneComponent->SetStaticMesh(PlaneMesh);
	PlaneComponent->SetMaterial(0, Material);

	USceneCaptureComponent2D* Capture = Camera->GetCaptureComponent2D();
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->CaptureSource = SCS_FinalColorLDR;
	Capture->TextureTarget = LdrTarget;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->ShowOnlyActors.Add(Plane);
	Capture->FOVAngle = CaptureFieldOfView;
	Capture->PostProcessBlendWeight = 1.0f;

	if (GShaderCompilingManager != nullptr)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}

	Capture->CaptureScene();

	int32 RawMismatches = 0;
	int32 FailedReads = 0;
	int32 NonMonotonicSteps = 0;
	int32 UnsettledReadings = 0;

	auto SetCoverage = [&](float Coverage)
	{
		const float Exposure = ExposureForCoverage(Coverage);
		UKismetRenderingLibrary::ClearRenderTarget2D(World, ExposureTexture, FLinearColor(Exposure, Exposure, Exposure, 0.0f));
	};

	for (const FCaptureMode& Mode : CaptureModes)
	{
		Capture->bAlwaysPersistRenderingState = Mode.bPersistRenderingState;

		for (const FChannelView& View : ChannelViews)
		{
			Material->SetVectorParameterValue(ChannelMaskParameterName, View.Mask);

			AddInfo(FString::Printf(
				TEXT("%s, %s. Columns: coverage | raw emissive | target 255c | 255 sRGB(raw) | tonemapper only | + "
					"local exposure | + project exposure | + bloom, the view as shown"),
				Mode.Name,
				View.Name));

			float PreviousByStage[UE_ARRAY_COUNT(ChainStages)] = {};

			for (const float Coverage : Coverages)
			{
				SetCoverage(Coverage);

				UKismetRenderingLibrary::DrawMaterialToRenderTarget(World, RawTarget, Material);
				const TArray<FLinearColor> Raw = UKismetRenderingLibrary::ReadRenderTargetRawPixelArea(
					World, RawTarget, 0, 0, ExposureTextureSize, ExposureTextureSize, false);

				const float RawEmissive = Raw.Num() == ExposureTextureSize * ExposureTextureSize
					? Component(Raw[(ExposureTextureSize / 2) * ExposureTextureSize + ExposureTextureSize / 2], View.Component)
					: -1.0f;

				RawMismatches += FMath::Abs(RawEmissive - Coverage) > RawEmissiveTolerance ? 1 : 0;

				const FColor Encoded = FLinearColor(RawEmissive, RawEmissive, RawEmissive).ToFColorSRGB();

				FString Row = FString::Printf(
					TEXT("%.2f | %.4f | %.1f | %d"),
					Coverage,
					RawEmissive,
					255.0f * Coverage,
					Encoded.R);

				for (int32 StageIndex = 0; StageIndex < UE_ARRAY_COUNT(ChainStages); ++StageIndex)
				{
					Capture->PostProcessSettings = SettingsFor(ChainStages[StageIndex]);

					FLinearColor Last;
					FLinearColor BeforeLast;

					if (!CaptureUntilSettled(*Capture, *LdrTarget, Mode.CapturesPerReading, Last, BeforeLast))
					{
						++FailedReads;
						Row += TEXT(" | read failed");
						continue;
					}

					const float Value = Component(Last, View.Component);
					NonMonotonicSteps += Value < PreviousByStage[StageIndex] ? 1 : 0;
					UnsettledReadings += FMath::Abs(Value - Component(BeforeLast, View.Component)) > SettledWithinLevels ? 1 : 0;
					PreviousByStage[StageIndex] = Value;

					Row += FString::Printf(
						TEXT(" | %.1f (others %.1f, %.1f)"),
						Value,
						Component(Last, (View.Component + 1) % 3),
						Component(Last, (View.Component + 2) % 3));
				}

				AddInfo(Row);
			}

			if (!Mode.bPersistRenderingState)
			{
				continue;
			}

			Capture->PostProcessSettings = SettingsFor(EChainStage::FullChain);

			FString Sweep = FString::Printf(TEXT("%s, %s, full chain, coverage:level"), Mode.Name, View.Name);
			float Previous = 0.0f;

			for (const float Coverage : DenseCoverages)
			{
				SetCoverage(Coverage);

				FLinearColor Last;
				FLinearColor BeforeLast;

				if (!CaptureUntilSettled(*Capture, *LdrTarget, Mode.CapturesPerReading, Last, BeforeLast))
				{
					++FailedReads;
					continue;
				}

				const float Value = Component(Last, View.Component);
				NonMonotonicSteps += Value < Previous ? 1 : 0;
				UnsettledReadings += FMath::Abs(Value - Component(BeforeLast, View.Component)) > SettledWithinLevels ? 1 : 0;
				Previous = Value;

				Sweep += FString::Printf(TEXT(" %.2f:%.1f"), Coverage, Value);
			}

			AddInfo(Sweep);
		}
	}

	UTextureRenderTarget2D* StripeTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
		World, StripeCaptureSize, StripeCaptureSize, RTF_RGBA8, FLinearColor::Black, false, false);

	if (StripeTarget != nullptr)
	{
		const double FrameWidth = 2.0 * CaptureDistance * FMath::Tan(FMath::DegreesToRadians(CaptureFieldOfView * 0.5));

		Plane->SetActorScale3D(FVector(
			StripeWidthOfTheFrame * FrameWidth / EnginePlaneCentimetres,
			StripeLengthOfTheFrame * FrameWidth / EnginePlaneCentimetres,
			1.0));

		Capture->TextureTarget = StripeTarget;
		Capture->bAlwaysPersistRenderingState = true;
		Capture->ShowFlags.SetAtmosphere(false);
		Capture->ShowFlags.SetFog(false);
		Capture->ShowFlags.SetCloud(false);

		for (const FChannelView& View : ChannelViews)
		{
			Material->SetVectorParameterValue(ChannelMaskParameterName, View.Mask);

			for (const FStripeVariant& Variant : StripeVariants)
			{
				Capture->PostProcessSettings = SettingsForStripe(Variant);

				FString Row = FString::Printf(
					TEXT("Stripe %.0f %% of the frame wide on black, persistent view state, %s, %s, coverage:(own, others)"),
					StripeWidthOfTheFrame * 100.0,
					View.Name,
					Variant.Name);

				for (const float Coverage : StripeCoverages)
				{
					SetCoverage(Coverage);

					FLinearColor Last;
					FLinearColor BeforeLast;

					if (!CaptureUntilSettled(*Capture, *StripeTarget, CapturesPerPersistentReading, Last, BeforeLast))
					{
						++FailedReads;
						continue;
					}

					UnsettledReadings += FMath::Abs(Component(Last, View.Component) - Component(BeforeLast, View.Component)) > SettledWithinLevels ? 1 : 0;

					Row += FString::Printf(
						TEXT(" %.2f:(%.1f, %.1f, %.1f)"),
						Coverage,
						Component(Last, View.Component),
						Component(Last, (View.Component + 1) % 3),
						Component(Last, (View.Component + 2) % 3));
				}

				FLinearColor Corner;

				if (ReadBlock(*StripeTarget, CentreBlock, CentreBlock, Corner))
				{
					Row += FString::Printf(TEXT(", background (%.1f, %.1f, %.1f)"), Corner.R, Corner.G, Corner.B);
				}

				AddInfo(Row);
			}
		}

		UKismetRenderingLibrary::ReleaseRenderTarget2D(StripeTarget);
	}
	else
	{
		++FailedReads;
	}

	Plane->Destroy();
	Camera->Destroy();
	UKismetRenderingLibrary::ReleaseRenderTarget2D(ExposureTexture);
	UKismetRenderingLibrary::ReleaseRenderTarget2D(RawTarget);
	UKismetRenderingLibrary::ReleaseRenderTarget2D(LdrTarget);

	AddInfo(FString::Printf(
		TEXT("%d persistent readings moved by more than %.1f levels between their last two captures"),
		UnsettledReadings,
		SettledWithinLevels));

	TestEqual(TEXT("The material's raw emissive is the coverage, so the fixture feeds what it says"), RawMismatches, 0);
	TestEqual(TEXT("Every capture was read back"), FailedReads, 0);
	TestEqual(TEXT("Every stage's output rises with coverage"), NonMonotonicSteps, 0);

	return true;
}

#endif
