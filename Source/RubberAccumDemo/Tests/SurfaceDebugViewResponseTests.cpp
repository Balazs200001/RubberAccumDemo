// Copyright 2026 Balázs Fábián. All Rights Reserved.

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
	constexpr const TCHAR* ProbeMaterialPath = TEXT("/Game/Materials/M_SurfaceCoverageProbe.M_SurfaceCoverageProbe");
	constexpr const TCHAR* PlaneMeshPath = TEXT("/Engine/BasicShapes/Plane.Plane");
	constexpr const TCHAR* ChannelMaskParameterName = TEXT("DebugChannelMask");

	constexpr int32 UniformCaptureSize = 64;
	constexpr int32 StripeCaptureSize = 512;
	constexpr int32 CentreBlock = 8;
	constexpr int32 ExposureTextureSize = 16;

	constexpr double CaptureAltitude = 500000.0;
	constexpr double CaptureDistance = 100.0;
	constexpr float CaptureFieldOfView = 20.0f;

	constexpr double StripeWidthOfTheFrame = 0.04;
	constexpr double StripeLengthOfTheFrame = 0.5;
	constexpr double EnginePlaneCentimetres = 100.0;

	constexpr int32 CapturesPerReading = 8;
	constexpr float SettledWithinLevels = 0.5f;

	constexpr float SweepCoverage = 0.5f;

	const float SweepEmissives[] = {
		0.0f, 0.005f, 0.01f, 0.0125f, 0.015f, 0.0175f, 0.02f, 0.0225f, 0.025f, 0.0275f, 0.03f, 0.035f,
		0.04f, 0.045f, 0.05f, 0.06f, 0.07f, 0.08f, 0.09f, 0.10f, 0.12f, 0.14f, 0.16f, 0.18f, 0.20f, 0.23f,
		0.26f, 0.30f, 0.35f, 0.40f, 0.45f, 0.50f, 0.52f, 0.54f, 0.56f, 0.58f, 0.60f, 0.62f, 0.64f, 0.66f,
		0.68f, 0.70f, 0.80f, 0.90f, 1.0f, 1.5f, 2.0f, 3.0f,
	};

	constexpr float RawSweepRelativeTolerance = 2.0e-3f;

	constexpr float ContextsAgreeWithinLevels = 2.0f;

	const float LinearCoverages[] = { 0.01f, 0.02f, 0.05f, 0.10f, 0.18f, 0.25f, 0.33f, 0.50f, 0.66f, 0.76f, 0.90f, 0.99f };

	constexpr float LinearWithinLevels = 2.5f;

	struct FChannelView
	{
		const TCHAR* Name;
		FLinearColor Mask;
		int32 Component;
		float DisplayCeiling;
	};

	const FChannelView ChannelViews[] = {
		{ TEXT("rubber, red"), FLinearColor(1.0f, 0.0f, 0.0f, 0.0f), 0, 254.0f },
		{ TEXT("skid, green"), FLinearColor(0.0f, 1.0f, 0.0f, 0.0f), 1, 235.08f },
		{ TEXT("marbles, blue"), FLinearColor(0.0f, 0.0f, 1.0f, 0.0f), 2, 210.26f },
	};

	enum class EViewContext : uint8
	{
		UniformField,
		StripeOnBlack,
	};

	const EViewContext ViewContexts[] = { EViewContext::UniformField, EViewContext::StripeOnBlack };

	const TCHAR* ContextName(EViewContext Context)
	{
		return Context == EViewContext::UniformField ? TEXT("uniform field") : TEXT("stripe on black");
	}

	enum class EChainSource : uint8
	{
		NeutralisedOnTheCapture,
		AsTheWorldSetsIt,
	};

	const EChainSource ChainSources[] = { EChainSource::NeutralisedOnTheCapture, EChainSource::AsTheWorldSetsIt };

	const TCHAR* ChainSourceName(EChainSource Source)
	{
		return Source == EChainSource::NeutralisedOnTheCapture
			? TEXT("local exposure and bloom neutralised on the capture")
			: TEXT("the chain as the world sets it");
	}

	FPostProcessSettings NeutralisedChainSettings()
	{
		FPostProcessSettings Settings;
		Settings.bOverride_LocalExposureHighlightContrastScale = true;
		Settings.LocalExposureHighlightContrastScale = 1.0f;
		Settings.bOverride_LocalExposureShadowContrastScale = true;
		Settings.LocalExposureShadowContrastScale = 1.0f;
		Settings.bOverride_BloomIntensity = true;
		Settings.BloomIntensity = 0.0f;
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

	float Crosstalk(const FLinearColor& Colour, int32 Index)
	{
		return FMath::Max(Component(Colour, (Index + 1) % 3), Component(Colour, (Index + 2) % 3));
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

	bool CaptureUntilSettled(
		USceneCaptureComponent2D& Capture,
		UTextureRenderTarget2D& Target,
		FLinearColor& OutLast,
		bool& bOutSettled)
	{
		FLinearColor BeforeLast = FLinearColor::Transparent;
		OutLast = FLinearColor::Transparent;

		for (int32 Index = 0; Index < CapturesPerReading; ++Index)
		{
			Capture.CaptureScene();

			if (Index + 2 >= CapturesPerReading)
			{
				BeforeLast = OutLast;

				if (!ReadBlock(Target, Target.SizeX / 2, Target.SizeY / 2, OutLast))
				{
					return false;
				}
			}
		}

		bOutSettled = (OutLast - BeforeLast).GetMax() <= SettledWithinLevels
			&& (BeforeLast - OutLast).GetMax() <= SettledWithinLevels;
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
	UMaterial* ProbeMaterial = LoadObject<UMaterial>(nullptr, ProbeMaterialPath);
	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, PlaneMeshPath);

	if (DebugMaterial == nullptr || ProbeMaterial == nullptr || PlaneMesh == nullptr)
	{
		AddError(FString::Printf(TEXT("Could not load %s, %s or %s."), DebugMaterialPath, ProbeMaterialPath, PlaneMeshPath));
		return false;
	}

	Subsystem->ConfigureTrackRibbon(nullptr, MakeResponseRibbon());

	UTextureRenderTarget2D* ExposureTexture = UKismetRenderingLibrary::CreateRenderTarget2D(
		World, ExposureTextureSize, ExposureTextureSize, RTF_RGBA32f, FLinearColor::Black, false, false);
	UTextureRenderTarget2D* RawTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
		World, ExposureTextureSize, ExposureTextureSize, RTF_RGBA16f, FLinearColor::Black, false, false);
	UTextureRenderTarget2D* UniformTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
		World, UniformCaptureSize, UniformCaptureSize, RTF_RGBA8, FLinearColor::Black, false, false);
	UTextureRenderTarget2D* StripeTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
		World, StripeCaptureSize, StripeCaptureSize, RTF_RGBA8, FLinearColor::Black, false, false);
	UMaterialInstanceDynamic* Probe = UMaterialInstanceDynamic::Create(ProbeMaterial, World);
	UMaterialInstanceDynamic* Debug = UMaterialInstanceDynamic::Create(DebugMaterial, World);

	if (ExposureTexture == nullptr || RawTarget == nullptr || UniformTarget == nullptr || StripeTarget == nullptr
		|| Probe == nullptr || Debug == nullptr)
	{
		AddError(TEXT("Could not create the render targets or the material instances."));
		return false;
	}

	Probe->SetTextureParameterValue(RacetrackSurfaceMapping::AccumulationTargetParameterName, ExposureTexture);
	Debug->SetTextureParameterValue(RacetrackSurfaceMapping::AccumulationTargetParameterName, ExposureTexture);

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

	USceneCaptureComponent2D* Capture = Camera->GetCaptureComponent2D();
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->bAlwaysPersistRenderingState = true;
	Capture->CaptureSource = SCS_FinalColorLDR;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->ShowOnlyActors.Add(Plane);
	Capture->FOVAngle = CaptureFieldOfView;
	Capture->PostProcessBlendWeight = 1.0f;
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetCloud(false);

	const double FrameWidth = 2.0 * CaptureDistance * FMath::Tan(FMath::DegreesToRadians(CaptureFieldOfView * 0.5));

	auto UseContext = [&](EViewContext Context)
	{
		const bool bStripe = Context == EViewContext::StripeOnBlack;

		Plane->SetActorScale3D(bStripe
			? FVector(
				StripeWidthOfTheFrame * FrameWidth / EnginePlaneCentimetres,
				StripeLengthOfTheFrame * FrameWidth / EnginePlaneCentimetres,
				1.0)
			: FVector::OneVector);

		Capture->TextureTarget = bStripe ? StripeTarget : UniformTarget;
	};

	auto SetCoverage = [&](float Coverage)
	{
		const float Exposure = ExposureForCoverage(Coverage);
		UKismetRenderingLibrary::ClearRenderTarget2D(World, ExposureTexture, FLinearColor(Exposure, Exposure, Exposure, 0.0f));
	};

	if (GShaderCompilingManager != nullptr)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}

	int32 FailedReads = 0;
	int32 UnsettledReadings = 0;
	int32 RawSweepMismatches = 0;

	TArray<float> SweepLevels[UE_ARRAY_COUNT(ViewContexts)][UE_ARRAY_COUNT(ChannelViews)];

	PlaneComponent->SetMaterial(0, Probe);
	Capture->PostProcessSettings = NeutralisedChainSettings();
	SetCoverage(SweepCoverage);

	for (int32 ContextIndex = 0; ContextIndex < UE_ARRAY_COUNT(ViewContexts); ++ContextIndex)
	{
		UseContext(ViewContexts[ContextIndex]);

		for (int32 ChannelIndex = 0; ChannelIndex < UE_ARRAY_COUNT(ChannelViews); ++ChannelIndex)
		{
			const FChannelView& View = ChannelViews[ChannelIndex];

			FString Row = FString::Printf(
				TEXT("Raw response through the probe, %s, %s, %s, emissive:(own, others)"),
				ContextName(ViewContexts[ContextIndex]),
				ChainSourceName(EChainSource::NeutralisedOnTheCapture),
				View.Name);

			for (const float Emissive : SweepEmissives)
			{
				Probe->SetVectorParameterValue(ChannelMaskParameterName, View.Mask * (Emissive / SweepCoverage));

				UKismetRenderingLibrary::DrawMaterialToRenderTarget(World, RawTarget, Probe);
				const TArray<FLinearColor> Raw = UKismetRenderingLibrary::ReadRenderTargetRawPixelArea(
					World, RawTarget, 0, 0, ExposureTextureSize, ExposureTextureSize, false);
				const float RawEmissive = Raw.Num() == ExposureTextureSize * ExposureTextureSize
					? Component(Raw[(ExposureTextureSize / 2) * ExposureTextureSize + ExposureTextureSize / 2], View.Component)
					: -1.0f;

				RawSweepMismatches += FMath::Abs(RawEmissive - Emissive) > FMath::Max(Emissive, 1.0e-3f) * RawSweepRelativeTolerance ? 1 : 0;

				FLinearColor Shown;
				bool bSettled = false;

				if (!CaptureUntilSettled(*Capture, *Capture->TextureTarget, Shown, bSettled))
				{
					++FailedReads;
					SweepLevels[ContextIndex][ChannelIndex].Add(-1.0f);
					continue;
				}

				UnsettledReadings += bSettled ? 0 : 1;
				SweepLevels[ContextIndex][ChannelIndex].Add(Component(Shown, View.Component));

				Row += FString::Printf(
					TEXT(" %.4f:(%.1f, %.1f, %.1f)"),
					Emissive,
					Component(Shown, View.Component),
					Component(Shown, (View.Component + 1) % 3),
					Component(Shown, (View.Component + 2) % 3));
			}

			AddInfo(Row);
		}
	}

	for (int32 ChannelIndex = 0; ChannelIndex < UE_ARRAY_COUNT(ChannelViews); ++ChannelIndex)
	{
		float WorstDisagreement = 0.0f;
		float WorstAtEmissive = 0.0f;
		const TArray<float>& Uniform = SweepLevels[0][ChannelIndex];
		const TArray<float>& Stripe = SweepLevels[1][ChannelIndex];

		for (int32 Index = 0; Index < Uniform.Num() && Index < Stripe.Num(); ++Index)
		{
			const float Disagreement = FMath::Abs(Uniform[Index] - Stripe[Index]);

			if (Disagreement > WorstDisagreement)
			{
				WorstDisagreement = Disagreement;
				WorstAtEmissive = SweepEmissives[Index];
			}
		}

		AddInfo(FString::Printf(
			TEXT("%s: with local exposure and bloom neutralised, the uniform field and the stripe disagree by at most "
				"%.1f levels, at emissive %.4f"),
			ChannelViews[ChannelIndex].Name,
			WorstDisagreement,
			WorstAtEmissive));

		TestTrue(
			*FString::Printf(
				TEXT("%s: with local exposure and bloom neutralised, a stripe on black and a uniform field show the same "
					"emissive alike, so a pixel's level depends only on its own texel"),
				ChannelViews[ChannelIndex].Name),
			WorstDisagreement <= ContextsAgreeWithinLevels);
	}

	PlaneComponent->SetMaterial(0, Debug);

	for (const EChainSource Source : ChainSources)
	{
		Capture->PostProcessSettings = Source == EChainSource::NeutralisedOnTheCapture
			? NeutralisedChainSettings()
			: FPostProcessSettings();

		for (const EViewContext Context : ViewContexts)
		{
			UseContext(Context);

			for (const FChannelView& View : ChannelViews)
			{
				Debug->SetVectorParameterValue(ChannelMaskParameterName, View.Mask);

				FString Row = FString::Printf(
					TEXT("Debug material, %s, %s, %s, ceiling %.1f, coverage:(shown, target, crosstalk)"),
					ChainSourceName(Source),
					ContextName(Context),
					View.Name,
					View.DisplayCeiling);

				float WorstError = 0.0f;
				float WorstAtCoverage = 0.0f;
				float WorstCrosstalk = 0.0f;

				for (const float Coverage : LinearCoverages)
				{
					SetCoverage(Coverage);

					FLinearColor Shown;
					bool bSettled = false;

					if (!CaptureUntilSettled(*Capture, *Capture->TextureTarget, Shown, bSettled))
					{
						++FailedReads;
						continue;
					}

					UnsettledReadings += bSettled ? 0 : 1;

					const float Level = Component(Shown, View.Component);
					const float Target = View.DisplayCeiling * Coverage;
					const float Error = Level - Target;

					if (FMath::Abs(Error) > FMath::Abs(WorstError))
					{
						WorstError = Error;
						WorstAtCoverage = Coverage;
					}

					WorstCrosstalk = FMath::Max(WorstCrosstalk, Crosstalk(Shown, View.Component));

					Row += FString::Printf(TEXT(" %.2f:(%.1f, %.1f, %.1f)"), Coverage, Level, Target, Crosstalk(Shown, View.Component));
				}

				AddInfo(Row);

				TestTrue(
					*FString::Printf(
						TEXT("Debug material, %s, %s, %s: every coverage shows within %.1f levels of its share of the "
							"ceiling; worst %+.1f at coverage %.2f, worst crosstalk %.1f"),
						ChainSourceName(Source),
						ContextName(Context),
						View.Name,
						LinearWithinLevels,
						WorstError,
						WorstAtCoverage,
						WorstCrosstalk),
					FMath::Abs(WorstError) <= LinearWithinLevels);
			}
		}
	}

	Plane->Destroy();
	Camera->Destroy();
	UKismetRenderingLibrary::ReleaseRenderTarget2D(ExposureTexture);
	UKismetRenderingLibrary::ReleaseRenderTarget2D(RawTarget);
	UKismetRenderingLibrary::ReleaseRenderTarget2D(UniformTarget);
	UKismetRenderingLibrary::ReleaseRenderTarget2D(StripeTarget);

	AddInfo(FString::Printf(
		TEXT("%d readings moved by more than %.1f levels between their last two captures"),
		UnsettledReadings,
		SettledWithinLevels));

	TestEqual(TEXT("The probe feeds the emissive the sweep says, through the channel mask"), RawSweepMismatches, 0);
	TestEqual(TEXT("Every capture was read back"), FailedReads, 0);

	return true;
}

#endif
