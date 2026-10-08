#include "RacetrackCircuit.h"

#include "Camera/PlayerCameraManager.h"
#include "ChaosVehicleWheel.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/InputComponent.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "SurfaceTrackSpace.h"
#include "WheeledVehiclePawn.h"
#include "WheelSurfaceSlip.h"
#include "Components/PostProcessComponent.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "SplineFollowingCar.h"
#include "SurfaceAccumulationBuffer.h"
#include "SurfaceAccumulationMapping.h"
#include "SurfaceAccumulationSubsystem.h"
#include "SurfaceQueryDebugDraw.h"
#include "SurfaceTrackRibbon.h"
#include "UObject/ConstructorHelpers.h"

namespace RacetrackFieldSwitches
{
	static constexpr const TCHAR* ReleaseField = TEXT("ReleaseField");

	static constexpr const TCHAR* FreezeField = TEXT("FreezeField");
}

namespace RacetrackCircuitDisplay
{
	static constexpr const TCHAR* DebugChannelMaskParameterName = TEXT("DebugChannelMask");

	TAutoConsoleVariable<float> CVarMessageSeconds(
		TEXT("RacetrackDemo.MessageSeconds"),
		6.0f,
		TEXT("How long an on-screen control message stays up, in seconds the operator actually waits. ")
		TEXT("The engine expires these messages on the dilated clock, so the figure is multiplied by ")
		TEXT("the current time dilation before it is handed over. Raise it for a still capture, where ")
		TEXT("the surface needs laps to build and the caption has to outlast them."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarFollowTheLeader(
		TEXT("RacetrackDemo.FollowTheLeader"),
		0,
		TEXT("Parks the observer behind and above the leading car instead of framing the whole circuit. ")
		TEXT("The wide framing is what the demo wants to show a racing line emerging over a lap; it puts ")
		TEXT("a metre-long debug bar at about a pixel, so the query overlay cannot be read in it. Every ")
		TEXT("capture this project takes is a standalone run driven from -ExecCmds at frame one, with no ")
		TEXT("way to move the camera afterwards, so the camera has to be able to find the car itself."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarFollowDistance(
		TEXT("RacetrackDemo.FollowDistance"),
		450.0f,
		TEXT("How far behind the leading car the observer sits, in centimetres, when it is following. ")
		TEXT("Close enough that the query bars drawn beside the car are legible, far enough that all ")
		TEXT("four wheels' bars are in frame together."),
		ECVF_Default);

	constexpr double FollowPitchDegrees = 40.0;

	constexpr double FramingLateralFraction = -0.22;

	constexpr double FramingVerticalFraction = 0.08;

	constexpr double OverlayAheadOfTheCar = 60.0;

	constexpr double OverlayBesideTheCar = 230.0;

	void AnnounceOnScreen(const UObject* WorldContext, FName MessageName, const FString& Text)
	{
		if (GEngine == nullptr)
		{
			return;
		}

		const float Dilation = FMath::Max(UGameplayStatics::GetGlobalTimeDilation(WorldContext), UE_SMALL_NUMBER);

		GEngine->AddOnScreenDebugMessage(
			static_cast<uint64>(GetTypeHash(MessageName)),
			CVarMessageSeconds.GetValueOnGameThread() * Dilation,
			FColor::White,
			Text);
	}
}

namespace RacetrackDemoCommands
{
	ARacetrackCircuit* FindCircuit(UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}

		for (TActorIterator<ARacetrackCircuit> It(World); It; ++It)
		{
			return *It;
		}

		UE_LOG(LogRacetrackSurface, Warning,
			TEXT("No ARacetrackCircuit in this world, so the demo console commands have nothing to drive"));

		return nullptr;
	}

	void SetField(const TArray<FString>& Arguments, UWorld* World)
	{
		ARacetrackCircuit* Circuit = FindCircuit(World);

		if (Circuit != nullptr)
		{
			Circuit->SetFieldPaused(Arguments.Num() > 0 && FCString::Atoi(*Arguments[0]) == 0);
		}
	}

	void SetTimeDilation(const TArray<FString>& Arguments, UWorld* World)
	{
		ARacetrackCircuit* Circuit = FindCircuit(World);

		if (Circuit != nullptr && Arguments.Num() > 0)
		{
			Circuit->ApplyTimeDilation(FCString::Atof(*Arguments[0]));
		}
	}

	void SetChannelView(const TArray<FString>& Arguments, UWorld* World)
	{
		ARacetrackCircuit* Circuit = FindCircuit(World);

		if (Circuit == nullptr || Arguments.Num() == 0)
		{
			return;
		}

		const int32 Requested = FMath::Clamp(
			FCString::Atoi(*Arguments[0]),
			0,
			static_cast<int32>(ESurfaceDebugView::Marbles));

		Circuit->SetDebugView(static_cast<ESurfaceDebugView>(Requested));
	}

	void ReportSurface(const TArray<FString>& Arguments, UWorld* World)
	{
		ARacetrackCircuit* Circuit = FindCircuit(World);

		if (Circuit != nullptr)
		{
			Circuit->ReportSurfaceCoverage();
		}
	}

	FAutoConsoleCommandWithWorldAndArgs SurfaceReportCommand(
		TEXT("RacetrackDemo.SurfaceReport"),
		TEXT("Reads the accumulation buffer back and reports the peak coverage in each channel, on screen "
			"and in the log. Stalls the game thread on the readback, so it is a diagnostic rather than "
			"something to leave running."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ReportSurface));

	FAutoConsoleCommandWithWorldAndArgs FieldCommand(
		TEXT("RacetrackDemo.Field"),
		TEXT("1 releases the field, 0 freezes it. The same handler the P key calls, so it announces "
			"itself on screen either way."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetField));

	FAutoConsoleCommandWithWorldAndArgs TimeDilationCommand(
		TEXT("RacetrackDemo.TimeDilation"),
		TEXT("Sets global time dilation, clamped to the circuit's own range. The same handler the . and "
			", keys call."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetTimeDilation));

	FAutoConsoleCommandWithWorldAndArgs ChannelViewCommand(
		TEXT("RacetrackDemo.Channel"),
		TEXT("0 shows all three channels, 1 rubber buildup, 2 skid marks, 3 marbles. The same handler "
			"the C key calls."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetChannelView));

	void SetSurfaceLook(const TArray<FString>& Arguments, UWorld* World)
	{
		ARacetrackCircuit* Circuit = FindCircuit(World);

		if (Circuit == nullptr || Arguments.Num() == 0)
		{
			return;
		}

		const int32 Requested = FMath::Clamp(
			FCString::Atoi(*Arguments[0]),
			0,
			static_cast<int32>(ESurfaceLook::Asphalt));

		Circuit->SetSurfaceLook(static_cast<ESurfaceLook>(Requested));
	}

	FAutoConsoleCommandWithWorldAndArgs SurfaceLookCommand(
		TEXT("RacetrackDemo.SurfaceLook"),
		TEXT("0 shows the debug view, 1 the lit asphalt. The same handler the V key calls."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetSurfaceLook));

	void ArmSkidMarkPair(const TArray<FString>& Arguments, UWorld* World)
	{
		ARacetrackCircuit* Circuit = FindCircuit(World);

		if (Circuit != nullptr)
		{
			Circuit->ArmSkidMarkPair(
				Arguments.Num() > 0 ? FCString::Atod(*Arguments[0]) : 0.0,
				Arguments.Num() > 1 ? FCString::Atod(*Arguments[1]) : 0.0);
		}
	}

	void ToggleDriving(const TArray<FString>& Arguments, UWorld* World)
	{
		ARacetrackCircuit* Circuit = FindCircuit(World);

		if (Circuit != nullptr)
		{
			Circuit->ToggleDriving();
		}
	}

	FAutoConsoleCommandWithWorldAndArgs DriveCommand(
		TEXT("RacetrackDemo.Drive"),
		TEXT("Hands the player the circuit's car, spawned on the centreline nearest the camera the first time, "
			"or gives the fly camera back and parks the car where it is. Time is held at 1x while driving."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ToggleDriving));

	FAutoConsoleCommandWithWorldAndArgs SkidMarkPairCommand(
		TEXT("RacetrackDemo.SkidMarkPair"),
		TEXT("After the first argument's seconds of track time, lays two identical lockup marks across the "
			"middle of the longest apex: one on the texel the field has rubbered most, one on the least "
			"rubbered texel at least a quarter of the track away, then parks the camera over them. Every "
			"second argument's seconds of track time after that it reads both back and reports what each "
			"still holds. The readbacks stall the game thread, so this is a capture tool rather than "
			"something to leave running."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ArmSkidMarkPair));
}

namespace RacetrackSkidMarkPair
{
	constexpr double MarkLengthCentimetres = 300.0;

	constexpr double EdgeMarginCentimetres = 50.0;

	constexpr double ObserverPitchDegrees = 60.0;

	constexpr double ObserverFramingScale = 1.4;

	constexpr int32 ApexSearchSlices = 2000;

	constexpr double ApexCorneringAmount = 0.98;

	int32 AtlasColumnAt(const FSurfaceTrackRibbon& Ribbon, double DistanceAlongCentreline, int32& OutRowIndex)
	{
		const double AlongTrack = Ribbon.NormalisedAlongTrack(Ribbon.WrapDistance(DistanceAlongCentreline));

		OutRowIndex = Ribbon.RowIndexAt(AlongTrack);

		return FMath::Clamp(
			FMath::FloorToInt32(Ribbon.RibbonToAtlasInRow(OutRowIndex, AlongTrack, 0.0).X * Ribbon.AtlasWidth()),
			0,
			Ribbon.AtlasWidth() - 1);
	}

	int32 LateralTexelAt(const FSurfaceTrackRibbon& Ribbon, double LateralOffsetAlpha)
	{
		return FMath::Clamp(
			FMath::FloorToInt32(FSurfaceTrackRibbon::NormalisedAcrossTrack(LateralOffsetAlpha) * Ribbon.AcrossTrackTexels()),
			0,
			Ribbon.AcrossTrackTexels() - 1);
	}

	double LateralTexelCentreAlpha(const FSurfaceTrackRibbon& Ribbon, int32 Texel)
	{
		return 2.0 * (Texel + 0.5) / Ribbon.AcrossTrackTexels() - 1.0;
	}

	TArray<float> RubberUnderAMarkCentredAt(const TArray<FLinearColor>& Band, int32 MarkTexels)
	{
		TArray<float> Footprint;
		Footprint.SetNumZeroed(Band.Num());

		for (int32 Centre = 0; Centre < Band.Num(); ++Centre)
		{
			const int32 First = FMath::Max(Centre - MarkTexels / 2, 0);
			const int32 Last = FMath::Min(Centre - MarkTexels / 2 + MarkTexels, Band.Num());

			for (int32 Texel = First; Texel < Last; ++Texel)
			{
				Footprint[Centre] += Band[Texel].R;
			}
		}

		return Footprint;
	}

	int32 MiddleOfTheRunAt(const TArray<float>& Footprint, int32 Start, int32 End)
	{
		int32 RunEnd = Start;

		while (RunEnd + 1 < End && Footprint[RunEnd + 1] == Footprint[Start])
		{
			++RunEnd;
		}

		return (Start + RunEnd) / 2;
	}
}

namespace RacetrackLayouts
{
	constexpr double CorneringSampleSpacing = 150.0;
	constexpr int32 MaximumCorneringSamples = 4096;
	constexpr double CorneringNormalisationPercentile = 0.75;

	const FVector SpaCentreline[] = {
		{258, 622, 17}, {215, 660, 18}, {160, 715, 19}, {110, 765, 19},
		{75, 795, 19}, {48, 800, 19}, {35, 775, 19}, {42, 745, 18},
		{75, 700, 16}, {110, 655, 13}, {150, 600, 9}, {185, 550, 5},
		{205, 490, 2}, {215, 440, 0}, {235, 400, 3}, {270, 370, 9},
		{310, 330, 17}, {350, 290, 23}, {420, 240, 28}, {520, 180, 34},
		{620, 130, 39}, {720, 90, 43}, {830, 55, 46}, {930, 30, 48},
		{1020, 22, 49}, {1075, 40, 49}, {1105, 75, 48}, {1145, 85, 48},
		{1190, 60, 47}, {1240, 40, 46}, {1300, 45, 45}, {1350, 75, 44},
		{1372, 120, 43}, {1360, 165, 42}, {1320, 190, 41}, {1270, 195, 40},
		{1180, 215, 37}, {1080, 255, 34}, {990, 290, 31}, {920, 320, 29},
		{890, 370, 27}, {905, 420, 26}, {960, 450, 25}, {1050, 470, 24},
		{1150, 490, 23}, {1225, 515, 22}, {1240, 560, 21}, {1212, 582, 20},
		{1196, 600, 20}, {1196, 628, 19}, {1224, 650, 19}, {1270, 665, 18},
		{1330, 685, 17}, {1340, 730, 15}, {1310, 780, 14}, {1250, 815, 13},
		{1180, 838, 12}, {1100, 845, 11}, {1030, 835, 11}, {975, 800, 10},
		{950, 740, 10}, {945, 670, 10}, {940, 600, 10}, {900, 545, 11},
		{830, 505, 11}, {760, 478, 12}, {700, 468, 12}, {620, 480, 13},
		{540, 502, 13}, {470, 520, 14}, {430, 534, 15}, {405, 540, 15},
		{388, 550, 15}, {378, 568, 16}, {376, 590, 16}, {366, 608, 16},
		{345, 618, 17}, {310, 622, 17}
	};
}

using RacetrackLayouts::CorneringNormalisationPercentile;
using RacetrackLayouts::CorneringSampleSpacing;
using RacetrackLayouts::MaximumCorneringSamples;

ARacetrackCircuit::ARacetrackCircuit()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	SetRootComponent(Spline);

	Road = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Road"));
	Road->SetupAttachment(Spline);
	Road->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Road->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Road->bUseAsyncCooking = false;

	EdgeWalls = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("EdgeWalls"));
	EdgeWalls->SetupAttachment(Spline);
	EdgeWalls->SetVisibility(false);
	EdgeWalls->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	EdgeWalls->SetCollisionResponseToAllChannels(ECR_Ignore);
	EdgeWalls->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Block);
	EdgeWalls->bUseAsyncCooking = false;

	PerTexelDebugView = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PerTexelDebugView"));
	PerTexelDebugView->SetupAttachment(Spline);
	PerTexelDebugView->bUnbound = true;
	PerTexelDebugView->Settings.bOverride_LocalExposureHighlightContrastScale = true;
	PerTexelDebugView->Settings.LocalExposureHighlightContrastScale = 1.0f;
	PerTexelDebugView->Settings.bOverride_LocalExposureShadowContrastScale = true;
	PerTexelDebugView->Settings.LocalExposureShadowContrastScale = 1.0f;
	PerTexelDebugView->Settings.bOverride_BloomIntensity = true;
	PerTexelDebugView->Settings.BloomIntensity = 0.0f;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DebugSurfaceMaterial(
		TEXT("/Game/Materials/M_SurfaceAccumulationDebug.M_SurfaceAccumulationDebug"));

	if (DebugSurfaceMaterial.Succeeded())
	{
		RoadMaterial = DebugSurfaceMaterial.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> AsphaltRoadMaterial(
		TEXT("/Game/Materials/M_SurfaceAsphalt.M_SurfaceAsphalt"));

	if (AsphaltRoadMaterial.Succeeded())
	{
		AsphaltMaterial = AsphaltRoadMaterial.Object;
	}

	CarClass = ASplineFollowingCar::StaticClass();
}

void ARacetrackCircuit::BeginPlay()
{
	Super::BeginPlay();

	BindAccumulationTargetToRoad();
	SpawnField();
	AcquireLeaderWheelQueries();
	SetFieldPaused(StartsPausedAfterCommandLine());
	BindTimeControls();
	PlaceObserverAboveTrack();
}

void ARacetrackCircuit::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseLeaderWheelQueries();

	Super::EndPlay(EndPlayReason);
}

void ARacetrackCircuit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDriving)
	{
		ReadWriteAndDrawTheDrivenWheels(DeltaSeconds);
	}
	else
	{
		ReadAndDrawTheLeadersWheels();
	}

	AdvanceSkidMarkPair(DeltaSeconds);
}

double ARacetrackCircuit::FindLongestApexDistance() const
{
	const double Length = GetTrackLength();

	if (Length <= UE_DOUBLE_SMALL_NUMBER)
	{
		return 0.0;
	}

	const int32 Slices = RacetrackSkidMarkPair::ApexSearchSlices;

	double ApexDistance = 0.0;
	double MostCornering = -1.0;
	int32 LongestRun = 0;
	int32 Run = 0;

	for (int32 Slice = 0; Slice <= 2 * Slices; ++Slice)
	{
		const double Distance = Length * (Slice % Slices) / Slices;
		const double Cornering = GetCorneringAmountAtDistance(Distance);

		Run = Cornering > RacetrackSkidMarkPair::ApexCorneringAmount ? Run + 1 : 0;

		if (Run > LongestRun)
		{
			LongestRun = Run;
			ApexDistance = Length * (Slice - Run * 0.5) / Slices;
		}

		if (LongestRun == 0 && Cornering > MostCornering)
		{
			MostCornering = Cornering;
			ApexDistance = Distance;
		}
	}

	return WrapDistance(ApexDistance);
}

void ARacetrackCircuit::ArmSkidMarkPair(double DelaySeconds, double ReportIntervalSeconds)
{
	if (bDriving)
	{
		RacetrackCircuitDisplay::AnnounceOnScreen(
			this,
			TEXT("RubberAccumDemo.SkidMarkPair"),
			TEXT("RacetrackDemo.SkidMarkPair is refused while driving: it would move the car you are driving"));
		return;
	}

	SecondsUntilSkidMarkPair = FMath::Max(DelaySeconds, 0.0);
	SkidMarkPairReportInterval = FMath::Max(ReportIntervalSeconds, 0.0);
	SecondsUntilSkidMarkPairReport = 0.0;
	SecondsSinceSkidMarkPairLaid = 0.0;
	SkidMarkPair.Reset();
	SkidMarkPairLaidSkid.Reset();

	const double LapSeconds = GetLapSeconds();
	const double LapsUntilLaid = LapSeconds > UE_DOUBLE_SMALL_NUMBER ? SecondsUntilSkidMarkPair.GetValue() / LapSeconds : 0.0;

	UE_LOG(LogRacetrackSurface, Log,
		TEXT("%s will lay a skid mark pair at the longest apex after %.0f s of track time, about %.1f laps, and "
			"report on it every %.0f s"),
		*GetName(),
		SecondsUntilSkidMarkPair.GetValue(),
		LapsUntilLaid,
		SkidMarkPairReportInterval);

	RacetrackCircuitDisplay::AnnounceOnScreen(
		this,
		TEXT("RubberAccumDemo.SkidMarkPair"),
		FString::Printf(TEXT("Skid mark pair due at the longest apex in about %.1f laps"), LapsUntilLaid));
}

void ARacetrackCircuit::AdvanceSkidMarkPair(float DeltaSeconds)
{
	if (SecondsUntilSkidMarkPair.IsSet())
	{
		SecondsUntilSkidMarkPair = SecondsUntilSkidMarkPair.GetValue() - DeltaSeconds;

		if (SecondsUntilSkidMarkPair.GetValue() <= 0.0)
		{
			SecondsUntilSkidMarkPair.Reset();
			LaySkidMarkPair(DeltaSeconds);
		}

		return;
	}

	if (!SkidMarkPair.IsSet())
	{
		return;
	}

	SecondsSinceSkidMarkPairLaid += DeltaSeconds;

	if (!SkidMarkPairLaidSkid.IsSet())
	{
		FLinearColor OnTheLine;
		FLinearColor OffTheLine;

		if (ReadSkidMarkPair(OnTheLine, OffTheLine))
		{
			SkidMarkPairLaidSkid = FVector2D(OnTheLine.G, OffTheLine.G);
			SecondsSinceSkidMarkPairLaid = 0.0;

			UE_LOG(LogRacetrackSurface, Log,
				TEXT("Skid mark pair laid with skid %.6f on the line over rubber %.6f, and %.6f off it over "
					"rubber %.6f"),
				OnTheLine.G,
				OnTheLine.R,
				OffTheLine.G,
				OffTheLine.R);
		}

		return;
	}

	if (SkidMarkPairReportInterval <= 0.0)
	{
		return;
	}

	SecondsUntilSkidMarkPairReport -= DeltaSeconds;

	if (SecondsUntilSkidMarkPairReport > 0.0)
	{
		return;
	}

	SecondsUntilSkidMarkPairReport = SkidMarkPairReportInterval;

	ReportSkidMarkPair();
}

void ARacetrackCircuit::LaySkidMarkPair(float DeltaSeconds)
{
	UWorld* World = GetWorld();

	USurfaceAccumulationSubsystem* Subsystem = World != nullptr
		? World->GetSubsystem<USurfaceAccumulationSubsystem>()
		: nullptr;

	USurfaceAccumulationBuffer* Buffer = Subsystem != nullptr ? Subsystem->GetBuffer() : nullptr;
	UTextureRenderTarget2D* Target = Buffer != nullptr ? Buffer->GetTarget() : nullptr;

	if (Target == nullptr)
	{
		RacetrackCircuitDisplay::AnnounceOnScreen(
			this,
			TEXT("RubberAccumDemo.SkidMarkPair"),
			TEXT("No accumulation buffer is allocated yet, so there is no line to lay a skid mark pair on"));
		return;
	}

	const FSurfaceTrackRibbon& Ribbon = Subsystem->GetRibbon();
	const int32 AcrossTexels = Ribbon.AcrossTrackTexels();
	const double Distance = FindLongestApexDistance();

	int32 RowIndex = 0;
	const int32 Column = RacetrackSkidMarkPair::AtlasColumnAt(Ribbon, Distance, RowIndex);

	const TArray<FLinearColor> Band = UKismetRenderingLibrary::ReadRenderTargetRawPixelArea(
		this, Target, Column, RowIndex * AcrossTexels, 1, AcrossTexels, false);

	if (Band.Num() != AcrossTexels)
	{
		return;
	}

	const ASplineFollowingCar* Reference = CarClass.GetDefaultObject();
	const float ContactWidth = Reference != nullptr ? Reference->GetContactWidth() : 20.0f;

	const int32 Margin = FMath::CeilToInt32(Ribbon.WorldDistanceToTexels(RacetrackSkidMarkPair::EdgeMarginCentimetres));
	const int32 Separation = FMath::Max(AcrossTexels / 4, 1);
	const int32 MarkTexels = FMath::Max(FMath::RoundToInt32(Ribbon.WorldDistanceToTexels(ContactWidth)), 1);
	const TArray<float> Footprint = RacetrackSkidMarkPair::RubberUnderAMarkCentredAt(Band, MarkTexels);

	int32 OnTheLine = INDEX_NONE;

	for (int32 Texel = Margin; Texel < AcrossTexels - Margin; ++Texel)
	{
		if (OnTheLine == INDEX_NONE || Footprint[Texel] > Footprint[OnTheLine])
		{
			OnTheLine = Texel;
		}
	}

	if (OnTheLine != INDEX_NONE)
	{
		OnTheLine = RacetrackSkidMarkPair::MiddleOfTheRunAt(Footprint, OnTheLine, AcrossTexels - Margin);
	}

	int32 OffTheLine = INDEX_NONE;

	for (int32 Texel = Margin; OnTheLine != INDEX_NONE && Texel < AcrossTexels - Margin; ++Texel)
	{
		const int32 Apart = FMath::Abs(Texel - OnTheLine);

		if (Apart < Separation)
		{
			continue;
		}

		const bool bLessRubber = OffTheLine == INDEX_NONE || Footprint[Texel] < Footprint[OffTheLine];
		const bool bAsBareButCloser = OffTheLine != INDEX_NONE
			&& Footprint[Texel] == Footprint[OffTheLine]
			&& Apart < FMath::Abs(OffTheLine - OnTheLine);

		if (bLessRubber || bAsBareButCloser)
		{
			OffTheLine = Texel;
		}
	}

	if (OffTheLine == INDEX_NONE)
	{
		RacetrackCircuitDisplay::AnnounceOnScreen(
			this,
			TEXT("RubberAccumDemo.SkidMarkPair"),
			TEXT("The track is too narrow to lay two marks a quarter of its width apart"));
		return;
	}

	const double MarkLength = RacetrackSkidMarkPair::MarkLengthCentimetres;

	FSkidMarkPairPlacement Placement;
	Placement.DistanceAlongCentreline = Distance;
	Placement.OnTheLineAlpha = RacetrackSkidMarkPair::LateralTexelCentreAlpha(Ribbon, OnTheLine);
	Placement.OffTheLineAlpha = RacetrackSkidMarkPair::LateralTexelCentreAlpha(Ribbon, OffTheLine);
	Placement.OnTheLineRubber = Band[OnTheLine].R;
	Placement.OffTheLineRubber = Band[OffTheLine].R;
	Placement.MarkLengthCentimetres = MarkLength;
	Placement.ContactWidth = ContactWidth;

	const double Speed = MarkLength / FMath::Max(static_cast<double>(DeltaSeconds), UE_DOUBLE_SMALL_NUMBER);
	const double EndDistance = Distance + MarkLength * 0.5;
	const double MarkAlphas[] = { Placement.OnTheLineAlpha, Placement.OffTheLineAlpha };

	for (const double Alpha : MarkAlphas)
	{
		Subsystem->RegisterSurfaceContactInTrackSpace(EndDistance, Alpha, Speed, 1.0f, ContactWidth);
	}

	SkidMarkPair = Placement;
	SkidMarkPairLaidSkid.Reset();
	SecondsSinceSkidMarkPairLaid = 0.0;
	SecondsUntilSkidMarkPairReport = SkidMarkPairReportInterval;

	PlaceObserverAboveSkidMarkPair();

	UE_LOG(LogRacetrackSurface, Log,
		TEXT("%s laid a skid mark pair %.0f cm round the lap, at the middle of the longest apex: on the line at "
			"alpha %.4f over rubber %.6f, off it at alpha %.4f over rubber %.6f, each %.0f cm long and laid by "
			"one lockup contact"),
		*GetName(),
		Distance,
		Placement.OnTheLineAlpha,
		Placement.OnTheLineRubber,
		Placement.OffTheLineAlpha,
		Placement.OffTheLineRubber,
		MarkLength);

	RacetrackCircuitDisplay::AnnounceOnScreen(
		this,
		TEXT("RubberAccumDemo.SkidMarkPair"),
		FString::Printf(
			TEXT("Two identical skid marks laid: one on the rubbered line, one %.1f m off it on bare track"),
			FMath::Abs(Placement.OnTheLineAlpha - Placement.OffTheLineAlpha) * TrackWidth * 0.5 / 100.0));
}

bool ARacetrackCircuit::ReadSkidMarkPair(FLinearColor& OutOnTheLine, FLinearColor& OutOffTheLine)
{
	if (!SkidMarkPair.IsSet())
	{
		return false;
	}

	UWorld* World = GetWorld();

	USurfaceAccumulationSubsystem* Subsystem = World != nullptr
		? World->GetSubsystem<USurfaceAccumulationSubsystem>()
		: nullptr;

	USurfaceAccumulationBuffer* Buffer = Subsystem != nullptr ? Subsystem->GetBuffer() : nullptr;
	UTextureRenderTarget2D* Target = Buffer != nullptr ? Buffer->GetTarget() : nullptr;

	if (Target == nullptr)
	{
		return false;
	}

	const FSurfaceTrackRibbon& Ribbon = Subsystem->GetRibbon();
	const int32 AcrossTexels = Ribbon.AcrossTrackTexels();

	int32 RowIndex = 0;
	const int32 Column = RacetrackSkidMarkPair::AtlasColumnAt(Ribbon, SkidMarkPair->DistanceAlongCentreline, RowIndex);

	const TArray<FLinearColor> OnTheLine = UKismetRenderingLibrary::ReadRenderTargetRawPixelArea(
		this,
		Target,
		Column,
		RowIndex * AcrossTexels + RacetrackSkidMarkPair::LateralTexelAt(Ribbon, SkidMarkPair->OnTheLineAlpha),
		1,
		1,
		false);

	const TArray<FLinearColor> OffTheLine = UKismetRenderingLibrary::ReadRenderTargetRawPixelArea(
		this,
		Target,
		Column,
		RowIndex * AcrossTexels + RacetrackSkidMarkPair::LateralTexelAt(Ribbon, SkidMarkPair->OffTheLineAlpha),
		1,
		1,
		false);

	if (OnTheLine.Num() != 1 || OffTheLine.Num() != 1)
	{
		return false;
	}

	OutOnTheLine = OnTheLine[0];
	OutOffTheLine = OffTheLine[0];

	return true;
}

void ARacetrackCircuit::ReportSkidMarkPair()
{
	FLinearColor OnTheLine;
	FLinearColor OffTheLine;

	if (!SkidMarkPairLaidSkid.IsSet() || !ReadSkidMarkPair(OnTheLine, OffTheLine))
	{
		return;
	}

	const FVector2D Laid = SkidMarkPairLaidSkid.GetValue();
	const double OnTheLineKept = OnTheLine.G / FMath::Max(Laid.X, UE_DOUBLE_SMALL_NUMBER);
	const double OffTheLineKept = OffTheLine.G / FMath::Max(Laid.Y, UE_DOUBLE_SMALL_NUMBER);

	const double LapSeconds = GetLapSeconds();
	const double Laps = LapSeconds > UE_DOUBLE_SMALL_NUMBER ? SecondsSinceSkidMarkPairLaid / LapSeconds : 0.0;

	UE_LOG(LogRacetrackSurface, Log,
		TEXT("Skid mark pair after %.0f s of track time, %.2f laps: on the line keeps %.4f of its laid skid over "
			"rubber %.6f, off the line keeps %.4f over rubber %.6f, a ratio of %.3f"),
		SecondsSinceSkidMarkPairLaid,
		Laps,
		OnTheLineKept,
		OnTheLine.R,
		OffTheLineKept,
		OffTheLine.R,
		OffTheLineKept / FMath::Max(OnTheLineKept, UE_DOUBLE_SMALL_NUMBER));

	RacetrackCircuitDisplay::AnnounceOnScreen(
		this,
		TEXT("RubberAccumDemo.SkidMarkPairReport"),
		FString::Printf(
			TEXT("%.1f laps after laying: the mark on the line keeps %.0f %%, the mark off it %.0f %%"),
			Laps,
			OnTheLineKept * 100.0,
			OffTheLineKept * 100.0));
}

void ARacetrackCircuit::PlaceObserverAboveSkidMarkPair()
{
	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);

	if (bDriving || !SkidMarkPair.IsSet() || Controller == nullptr || Controller->GetPawn() == nullptr)
	{
		return;
	}

	const double Distance = SkidMarkPair->DistanceAlongCentreline;
	const FVector OnTheLine = GetWorldPositionAtDistance(Distance, SkidMarkPair->OnTheLineAlpha);
	const FVector OffTheLine = GetWorldPositionAtDistance(Distance, SkidMarkPair->OffTheLineAlpha);
	const FVector Focus = (OnTheLine + OffTheLine) * 0.5;
	const FVector Forward = GetWorldDirectionAtDistance(Distance);
	const FVector Up = GetWorldUpVectorAtDistance(Distance);

	const double Spread = FMath::Max(
		FVector::Distance(OnTheLine, OffTheLine),
		RacetrackSkidMarkPair::MarkLengthCentimetres);
	const double Range = Spread * RacetrackSkidMarkPair::ObserverFramingScale;
	const double Pitch = FMath::DegreesToRadians(RacetrackSkidMarkPair::ObserverPitchDegrees);

	const FVector Eye = Focus - Forward * (Range * FMath::Cos(Pitch)) + Up * (Range * FMath::Sin(Pitch));
	const FRotator Look = (Focus - Eye).Rotation();

	Controller->GetPawn()->SetActorLocationAndRotation(Eye, Look);
	Controller->SetControlRotation(Look);
}

ASplineFollowingCar* ARacetrackCircuit::FindLeadingCar() const
{
	ASplineFollowingCar* Leading = nullptr;
	double FurthestDistanceAlongTrack = 0.0;

	for (const TObjectPtr<ASplineFollowingCar>& Entry : SpawnedField)
	{
		ASplineFollowingCar* Car = Entry.Get();

		if (!IsValid(Car) || !Car->HasPlacedItsWheels())
		{
			continue;
		}

		const double DistanceAlongTrack = Car->GetDistanceAlongTrack();

		if (Leading == nullptr || DistanceAlongTrack > FurthestDistanceAlongTrack)
		{
			FurthestDistanceAlongTrack = DistanceAlongTrack;
			Leading = Car;
		}
	}

	return Leading;
}

void ARacetrackCircuit::AcquireLeaderWheelQueries()
{
	ReleaseLeaderWheelQueries();

	UWorld* World = GetWorld();

	USurfaceAccumulationSubsystem* Subsystem = World != nullptr
		? World->GetSubsystem<USurfaceAccumulationSubsystem>()
		: nullptr;

	if (Subsystem == nullptr)
	{
		return;
	}

	LeaderWheelQueries.Reserve(ASplineFollowingCar::WheelCount);

	for (int32 Wheel = 0; Wheel < ASplineFollowingCar::WheelCount; ++Wheel)
	{
		LeaderWheelQueries.Add(Subsystem->AcquireSurfaceQuery());
	}

	LeaderWheelSamples.SetNum(ASplineFollowingCar::WheelCount);
}

void ARacetrackCircuit::ReleaseLeaderWheelQueries()
{
	UWorld* World = GetWorld();

	USurfaceAccumulationSubsystem* Subsystem = World != nullptr
		? World->GetSubsystem<USurfaceAccumulationSubsystem>()
		: nullptr;

	if (Subsystem != nullptr)
	{
		for (const FSurfaceQueryHandle& Handle : LeaderWheelQueries)
		{
			Subsystem->ReleaseSurfaceQuery(Handle);
		}
	}

	LeaderWheelQueries.Reset();
	LeaderWheelSamples.Reset();
	Leader.Reset();
}

void ARacetrackCircuit::ReadAndDrawTheLeadersWheels()
{
	if (LeaderWheelQueries.Num() != ASplineFollowingCar::WheelCount)
	{
		return;
	}

	UWorld* World = GetWorld();

	USurfaceAccumulationSubsystem* Subsystem = World != nullptr
		? World->GetSubsystem<USurfaceAccumulationSubsystem>()
		: nullptr;

	ASplineFollowingCar* Leading = FindLeadingCar();

	Leader = Leading;

	if (Subsystem == nullptr || Leading == nullptr)
	{
		return;
	}

	const TArray<FVector>& WheelPositions = Leading->GetWheelPositions();

	if (WheelPositions.Num() != ASplineFollowingCar::WheelCount)
	{
		return;
	}

	const double DistanceAlongTrack = Leading->GetDistanceAlongTrack();

	RacetrackSurfaceQueryOverlay::FOverlayBasis Basis;
	Basis.Along = GetWorldDirectionAtDistance(DistanceAlongTrack);
	Basis.Across = GetWorldRightVectorAtDistance(DistanceAlongTrack);
	Basis.Up = GetWorldUpVectorAtDistance(DistanceAlongTrack);

	const FVector BesideTheCar =
		Basis.Along * RacetrackCircuitDisplay::OverlayAheadOfTheCar
		+ Basis.Across * RacetrackCircuitDisplay::OverlayBesideTheCar;

	LeaderWheelSamples.SetNum(ASplineFollowingCar::WheelCount);

	for (int32 Wheel = 0; Wheel < ASplineFollowingCar::WheelCount; ++Wheel)
	{
		LeaderWheelSamples[Wheel] = Subsystem->QuerySurfaceState(LeaderWheelQueries[Wheel], WheelPositions[Wheel]);

		RacetrackSurfaceQueryOverlay::DrawSurfaceStateSample(
			World,
			WheelPositions[Wheel] + BesideTheCar,
			Basis,
			LeaderWheelSamples[Wheel]);
	}

	if (RacetrackCircuitDisplay::CVarFollowTheLeader.GetValueOnGameThread() != 0)
	{
		PlaceObserverBehindTheLeader(*Leading);
	}
}

void ARacetrackCircuit::PlaceObserverBehindTheLeader(const ASplineFollowingCar& Leading)
{
	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);

	if (Controller == nullptr || Controller->GetPawn() == nullptr)
	{
		return;
	}

	const double DistanceAlongTrack = Leading.GetDistanceAlongTrack();
	const FVector Forward = GetWorldDirectionAtDistance(DistanceAlongTrack);
	const FVector Right = GetWorldRightVectorAtDistance(DistanceAlongTrack);
	const FVector Up = GetWorldUpVectorAtDistance(DistanceAlongTrack);
	const FVector Focus = Leading.GetActorLocation();

	const double Pitch = FMath::DegreesToRadians(RacetrackCircuitDisplay::FollowPitchDegrees);
	const double Distance = FMath::Max(RacetrackCircuitDisplay::CVarFollowDistance.GetValueOnGameThread(), 1.0f);

	const FVector Eye = Focus - Forward * (Distance * FMath::Cos(Pitch)) + Up * (Distance * FMath::Sin(Pitch));

	const FVector AimedAt = Focus
		- Right * (Distance * RacetrackCircuitDisplay::FramingLateralFraction)
		- Up * (Distance * RacetrackCircuitDisplay::FramingVerticalFraction);

	const FRotator Look = (AimedAt - Eye).Rotation();

	Controller->GetPawn()->SetActorLocationAndRotation(Eye, Look);
	Controller->SetControlRotation(Look);
}

void ARacetrackCircuit::ToggleDriving()
{
	if (bDriving)
	{
		StopDriving();
	}
	else
	{
		StartDriving();
	}
}

void ARacetrackCircuit::StartDriving()
{
	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);

	if (Controller == nullptr)
	{
		return;
	}

	if (DriveVehicleClass == nullptr)
	{
		RacetrackCircuitDisplay::AnnounceOnScreen(
			this,
			TEXT("RubberAccumDemo.Drive"),
			TEXT("No vehicle class is set on the circuit, so there is no car to drive"));
		return;
	}

	APawn* Observer = Controller->GetPawn();
	AWheeledVehiclePawn* Vehicle = DrivenVehicle.Get();

	if (Vehicle == nullptr)
	{
		Vehicle = SpawnVehicleNearTheCamera(*Controller);
		DrivenVehicle = Vehicle;
	}

	if (Vehicle == nullptr)
	{
		RacetrackCircuitDisplay::AnnounceOnScreen(
			this,
			TEXT("RubberAccumDemo.Drive"),
			TEXT("The car could not be spawned on the track"));
		return;
	}

	ApplyTimeDilation(1.0f);

	FlyPawn = Observer;
	Controller->Possess(Vehicle);
	SetDriveMappingContext(*Controller, true);

	if (UChaosVehicleMovementComponent* Movement = Vehicle->GetVehicleMovementComponent())
	{
		Movement->SetHandbrakeInput(false);
	}

	DrivenWheelPreviousContacts.Reset();
	DrivenWheelHadContact.Reset();
	bDriving = true;

	UE_LOG(LogRacetrackSurface, Log, TEXT("%s handed %s to the player"), *GetName(), *Vehicle->GetName());

	RacetrackCircuitDisplay::AnnounceOnScreen(
		this,
		TEXT("RubberAccumDemo.Drive"),
		TEXT("Driving. RacetrackDemo.Drive gives the fly camera back. Time is held at 1x"));

	if (RacetrackCircuitDisplay::CVarFollowTheLeader.GetValueOnGameThread() != 0)
	{
		RacetrackCircuitDisplay::AnnounceOnScreen(
			this,
			TEXT("RubberAccumDemo.FollowTheLeader"),
			TEXT("RacetrackDemo.FollowTheLeader is ignored while driving: it would move the car you are driving"));
	}
}

void ARacetrackCircuit::StopDriving()
{
	bDriving = false;

	ParkTheDrivenVehicle();

	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);

	if (Controller != nullptr)
	{
		SetDriveMappingContext(*Controller, false);

		if (APawn* Observer = FlyPawn.Get())
		{
			Controller->Possess(Observer);
		}
	}

	UE_LOG(LogRacetrackSurface, Log, TEXT("%s gave the fly camera back and parked the car"), *GetName());

	RacetrackCircuitDisplay::AnnounceOnScreen(
		this,
		TEXT("RubberAccumDemo.Drive"),
		TEXT("Fly camera back. The car is parked where it stopped, and RacetrackDemo.Drive takes it again"));
}

AWheeledVehiclePawn* ARacetrackCircuit::SpawnVehicleNearTheCamera(const APlayerController& Controller)
{
	UWorld* World = GetWorld();

	if (World == nullptr || Spline == nullptr)
	{
		return nullptr;
	}

	const FVector CameraLocation = Controller.PlayerCameraManager != nullptr
		? Controller.PlayerCameraManager->GetCameraLocation()
		: GetActorLocation();

	FSurfaceTrackCoordinate Nearest;

	if (RacetrackSurfaceTrackSpace::WorldToTrack(*Spline, TrackWidth, CameraLocation, Nearest)
		== ETrackSpaceStatus::NoCentreline)
	{
		return nullptr;
	}

	Nearest.LateralOffsetAlpha = 0.0;

	const double Distance = Nearest.DistanceAlongCentreline;
	const FVector Up = GetWorldUpVectorAtDistance(Distance);
	const FVector OnTheRoad = RacetrackSurfaceTrackSpace::TrackToWorld(*Spline, TrackWidth, Nearest)
		+ Up * (RoadHeightAboveSpline + DriveSpawnHeight);
	const FRotator AlongTheTrack = FRotationMatrix::MakeFromXZ(GetWorldDirectionAtDistance(Distance), Up).Rotator();

	FActorSpawnParameters AlwaysSpawn;
	AlwaysSpawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AWheeledVehiclePawn* Vehicle =
		World->SpawnActor<AWheeledVehiclePawn>(DriveVehicleClass, OnTheRoad, AlongTheTrack, AlwaysSpawn);

	if (Vehicle != nullptr)
	{
		UE_LOG(LogRacetrackSurface, Log,
			TEXT("%s spawned %s on the centreline %.0f cm round the lap, the nearest point to the camera"),
			*GetName(),
			*Vehicle->GetName(),
			Distance);
	}

	return Vehicle;
}

void ARacetrackCircuit::SetDriveMappingContext(const APlayerController& Controller, bool bAdd) const
{
	UEnhancedInputLocalPlayerSubsystem* Input =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller.GetLocalPlayer());

	if (Input == nullptr || DriveMappingContext == nullptr)
	{
		return;
	}

	if (bAdd)
	{
		Input->AddMappingContext(DriveMappingContext, 0);
	}
	else
	{
		Input->RemoveMappingContext(DriveMappingContext);
	}
}

void ARacetrackCircuit::ParkTheDrivenVehicle() const
{
	const AWheeledVehiclePawn* Vehicle = DrivenVehicle.Get();
	UChaosVehicleMovementComponent* Movement = Vehicle != nullptr ? Vehicle->GetVehicleMovementComponent() : nullptr;

	if (Movement == nullptr)
	{
		return;
	}

	Movement->SetThrottleInput(0.0f);
	Movement->SetBrakeInput(0.0f);
	Movement->SetSteeringInput(0.0f);
	Movement->SetHandbrakeInput(true);
}

void ARacetrackCircuit::ReadWriteAndDrawTheDrivenWheels(float DeltaSeconds)
{
	UWorld* World = GetWorld();

	USurfaceAccumulationSubsystem* Subsystem = World != nullptr
		? World->GetSubsystem<USurfaceAccumulationSubsystem>()
		: nullptr;

	const AWheeledVehiclePawn* Vehicle = DrivenVehicle.Get();

	const UChaosWheeledVehicleMovementComponent* Movement = Vehicle != nullptr
		? Cast<UChaosWheeledVehicleMovementComponent>(Vehicle->GetVehicleMovementComponent())
		: nullptr;

	if (Subsystem == nullptr || Movement == nullptr || Spline == nullptr)
	{
		return;
	}

	const int32 WheelCount = FMath::Min(Movement->GetNumWheels(), LeaderWheelQueries.Num());

	DrivenWheelPreviousContacts.SetNum(WheelCount);
	DrivenWheelHadContact.SetNum(WheelCount);
	LeaderWheelSamples.SetNum(LeaderWheelQueries.Num());

	FSurfaceTrackCoordinate AtTheCar;
	RacetrackSurfaceTrackSpace::WorldToTrack(*Spline, TrackWidth, Vehicle->GetActorLocation(), AtTheCar);

	RacetrackSurfaceQueryOverlay::FOverlayBasis Basis;
	Basis.Along = GetWorldDirectionAtDistance(AtTheCar.DistanceAlongCentreline);
	Basis.Across = GetWorldRightVectorAtDistance(AtTheCar.DistanceAlongCentreline);
	Basis.Up = GetWorldUpVectorAtDistance(AtTheCar.DistanceAlongCentreline);

	const FVector BesideTheCar =
		Basis.Along * RacetrackCircuitDisplay::OverlayAheadOfTheCar
		+ Basis.Across * RacetrackCircuitDisplay::OverlayBesideTheCar;

	FWheelSlipCalibration Calibration;
	Calibration.FullRubberSlipRatio = DriveFullRubberSlipRatio;
	Calibration.FullRubberSlipAngle = DriveFullRubberSlipAngle;
	Calibration.LockedWheelSpeedFraction = DriveLockedWheelSpeedFraction;
	Calibration.SlowestSkiddingGroundSpeed = DriveSlowestSkiddingGroundSpeed;

	for (int32 Wheel = 0; Wheel < WheelCount; ++Wheel)
	{
		const FWheelStatus& State = Movement->GetWheelState(Wheel);

		if (!State.bInContact)
		{
			DrivenWheelHadContact[Wheel] = false;
			continue;
		}

		const FVector Velocity = DrivenWheelHadContact[Wheel] && DeltaSeconds > UE_SMALL_NUMBER
			? (State.ContactPoint - DrivenWheelPreviousContacts[Wheel]) / DeltaSeconds
			: Vehicle->GetVelocity();

		DrivenWheelPreviousContacts[Wheel] = State.ContactPoint;
		DrivenWheelHadContact[Wheel] = true;

		const UChaosVehicleWheel* WheelSetup =
			Movement->Wheels.IsValidIndex(Wheel) ? Movement->Wheels[Wheel].Get() : nullptr;

		if (WheelSetup != nullptr)
		{
			FWheelSlipReading Reading;
			Reading.BrakeTorque = State.BrakeTorque;
			Reading.SlipMagnitude = State.SlipMagnitude;
			Reading.SlipAngle = State.SlipAngle;
			Reading.GroundSpeed = static_cast<float>(Velocity.Size());

			Subsystem->RegisterSurfaceContact(
				State.ContactPoint,
				Velocity,
				WheelSurfaceSlip::SurfaceSlipFromWheel(Reading, Calibration),
				WheelSetup->WheelWidth);
		}

		LeaderWheelSamples[Wheel] = Subsystem->QuerySurfaceState(LeaderWheelQueries[Wheel], State.ContactPoint);

		RacetrackSurfaceQueryOverlay::DrawSurfaceStateSample(
			World,
			State.ContactPoint + BesideTheCar,
			Basis,
			LeaderWheelSamples[Wheel]);
	}
}

bool ARacetrackCircuit::StartsPausedAfterCommandLine() const
{
	const TCHAR* CommandLine = FCommandLine::Get();

	if (FParse::Param(CommandLine, RacetrackFieldSwitches::ReleaseField))
	{
		return false;
	}

	if (FParse::Param(CommandLine, RacetrackFieldSwitches::FreezeField))
	{
		return true;
	}

	return bStartPaused;
}

void ARacetrackCircuit::SetFieldPaused(bool bPaused)
{
	bFieldPaused = bPaused;

	for (ASplineFollowingCar* Car : SpawnedField)
	{
		if (Car != nullptr)
		{
			Car->SetActorTickEnabled(!bPaused);
		}
	}

	UE_LOG(LogRacetrackSurface, Log,
		TEXT("%s %s %d cars. The camera is unaffected either way. Press P to %s"),
		*GetName(),
		bPaused ? TEXT("froze") : TEXT("released"),
		SpawnedField.Num(),
		bPaused ? TEXT("release them") : TEXT("freeze them"));

	RacetrackCircuitDisplay::AnnounceOnScreen(
		this,
		TEXT("RubberAccumDemo.FieldPaused"),
		FString::Printf(
			TEXT("Field %s, %d cars"),
			bPaused ? TEXT("frozen") : TEXT("running"),
			SpawnedField.Num()));
}

void ARacetrackCircuit::ToggleFieldPaused()
{
	SetFieldPaused(!bFieldPaused);
}

void ARacetrackCircuit::PlaceObserverAboveTrack()
{
	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);

	if (Controller == nullptr || Controller->GetPawn() == nullptr)
	{
		return;
	}

	const FBox Bounds = GetComponentsBoundingBox(true);

	if (!Bounds.IsValid)
	{
		return;
	}

	const FVector Centre = Bounds.GetCenter();
	const FVector Extent = Bounds.GetExtent();

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	Controller->GetViewportSize(ViewportWidth, ViewportHeight);

	const double Aspect = ViewportHeight > 0
		? static_cast<double>(ViewportWidth) / ViewportHeight
		: 16.0 / 9.0;

	const double FieldOfView = Controller->PlayerCameraManager != nullptr
		? Controller->PlayerCameraManager->GetFOVAngle()
		: 90.0f;

	const double HalfHorizontal = FMath::DegreesToRadians(FieldOfView * 0.5);
	const double HalfVertical = FMath::Atan(FMath::Tan(HalfHorizontal) / Aspect);

	const double Pitch = FMath::DegreesToRadians(ObserverPitchDegrees);

	const double AcrossScreen = Extent.Y;
	const double UpScreen = Extent.X * FMath::Sin(Pitch) + Extent.Z * FMath::Cos(Pitch);

	const double Framing = ObserverFramingScale * FMath::Max(
		AcrossScreen / FMath::Tan(HalfHorizontal),
		UpScreen / FMath::Tan(HalfVertical));

	const FVector Eye(
		Centre.X - Framing * FMath::Cos(Pitch),
		Centre.Y,
		Centre.Z + Framing * FMath::Sin(Pitch));

	const FRotator Look(-ObserverPitchDegrees, 0.0, 0.0);

	Controller->GetPawn()->SetActorLocationAndRotation(Eye, Look);
	Controller->SetControlRotation(Look);

	UE_LOG(LogRacetrackSurface, Log,
		TEXT("%s placed the observer %.0f cm out at %.0f degrees over a %.0f by %.0f cm circuit, "
			"framed for a %.0f degree view at %.2f aspect"),
		*GetName(),
		Framing,
		ObserverPitchDegrees,
		Extent.X * 2.0,
		Extent.Y * 2.0,
		FieldOfView,
		Aspect);
}

void ARacetrackCircuit::BindTimeControls()
{
	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);

	if (Controller == nullptr)
	{
		return;
	}

	EnableInput(Controller);

	if (InputComponent == nullptr)
	{
		return;
	}

	InputComponent->BindKey(EKeys::Period, IE_Pressed, this, &ARacetrackCircuit::SpeedUpTime);
	InputComponent->BindKey(EKeys::Comma, IE_Pressed, this, &ARacetrackCircuit::SlowDownTime);
	InputComponent->BindKey(EKeys::RightBracket, IE_Pressed, this, &ARacetrackCircuit::DepositFaster);
	InputComponent->BindKey(EKeys::LeftBracket, IE_Pressed, this, &ARacetrackCircuit::DepositSlower);
	InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ARacetrackCircuit::ToggleFieldPaused);
	InputComponent->BindKey(EKeys::M, IE_Pressed, this, &ARacetrackCircuit::DecayFaster);
	InputComponent->BindKey(EKeys::N, IE_Pressed, this, &ARacetrackCircuit::DecaySlower);
	InputComponent->BindKey(EKeys::C, IE_Pressed, this, &ARacetrackCircuit::CycleDebugView);
	InputComponent->BindKey(EKeys::V, IE_Pressed, this, &ARacetrackCircuit::ToggleSurfaceLook);

	UE_LOG(LogRacetrackSurface, Log,
		TEXT("Press P to freeze and release the field, starting %s. Press . and , for time, between "
			"%.2fx and %.0fx. Press ] and [ for how much rubber a contact leaves, between %.2fx and "
			"%.0fx of the derived rate. Press M and N for how fast it fades, between off and %.0fx of a "
			"%.0f lap half-life, which is %.0f s on this layout. Press C to look at one accumulation "
			"channel at a time. Press V to switch between the debug view and the lit asphalt. "
			"W A S D, Q E and the mouse fly the camera at all times"),
		bFieldPaused ? TEXT("frozen") : TEXT("running"),
		SlowestTimeDilation,
		FastestTimeDilation,
		SlowestDepositionScale,
		FastestDepositionScale,
		FastestDecayRate,
		DecayHalfLifeInLaps,
		DecayHalfLifeInLaps * GetLapSeconds());

	RacetrackCircuitDisplay::AnnounceOnScreen(
		this,
		TEXT("RubberAccumDemo.Controls"),
		TEXT("P field   . , time   ] [ deposition   M N decay   C channel   V asphalt"));
}

void ARacetrackCircuit::DepositFaster()
{
	ApplyDepositionScale(USurfaceAccumulationSubsystem::GetDepositionScale() * DepositionScaleStep);
}

void ARacetrackCircuit::DepositSlower()
{
	ApplyDepositionScale(USurfaceAccumulationSubsystem::GetDepositionScale() / DepositionScaleStep);
}

void ARacetrackCircuit::ApplyDepositionScale(float Scale)
{
	IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(
		RacetrackSurfaceMapping::DepositionScaleVariableName);

	if (Variable == nullptr)
	{
		return;
	}

	const float Clamped = FMath::Clamp(Scale, SlowestDepositionScale, FastestDepositionScale);

	Variable->Set(Clamped, ECVF_SetByCode);

	UE_LOG(LogRacetrackSurface, Log, TEXT("Deposition now %.2fx the derived rate"), Clamped);

	ReportLapsToSaturation();
}

void ARacetrackCircuit::SpeedUpTime()
{
	ApplyTimeDilation(UGameplayStatics::GetGlobalTimeDilation(this) * TimeDilationStep);
}

void ARacetrackCircuit::SlowDownTime()
{
	ApplyTimeDilation(UGameplayStatics::GetGlobalTimeDilation(this) / TimeDilationStep);
}

void ARacetrackCircuit::ApplyTimeDilation(float Dilation)
{
	if (bDriving)
	{
		RacetrackCircuitDisplay::AnnounceOnScreen(
			this,
			TEXT("RubberAccumDemo.TimeDilation"),
			TEXT("Time is held at 1x while driving"));
		return;
	}

	const float Clamped = FMath::Clamp(Dilation, SlowestTimeDilation, FastestTimeDilation);

	UGameplayStatics::SetGlobalTimeDilation(this, Clamped);

	UE_LOG(LogRacetrackSurface, Log, TEXT("Time dilation now %.2fx"), Clamped);

	RacetrackCircuitDisplay::AnnounceOnScreen(
		this,
		TEXT("RubberAccumDemo.TimeDilation"),
		FString::Printf(TEXT("Time dilation now %.2fx"), Clamped));
}

void ARacetrackCircuit::DecayFaster()
{
	ApplyDecayRate(NextDecayRate(USurfaceAccumulationSubsystem::GetDecayRate(), true));
}

void ARacetrackCircuit::DecaySlower()
{
	ApplyDecayRate(NextDecayRate(USurfaceAccumulationSubsystem::GetDecayRate(), false));
}

float ARacetrackCircuit::NextDecayRate(float CurrentRate, bool bFaster) const
{
	if (!bFaster)
	{
		return CurrentRate <= SlowestDecayRate ? 0.0f : FMath::Max(CurrentRate / DecayRateStep, SlowestDecayRate);
	}

	return CurrentRate <= 0.0f
		? SlowestDecayRate
		: FMath::Min(CurrentRate * DecayRateStep, FastestDecayRate);
}

void ARacetrackCircuit::ApplyDecayRate(float Rate)
{
	IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(
		RacetrackSurfaceMapping::DecayRateVariableName);

	if (Variable == nullptr)
	{
		return;
	}

	const float Clamped = Rate <= 0.0f ? 0.0f : FMath::Clamp(Rate, SlowestDecayRate, FastestDecayRate);

	Variable->Set(Clamped, ECVF_SetByCode);

	if (Clamped <= 0.0f)
	{
		UE_LOG(LogRacetrackSurface, Log,
			TEXT("Decay off. Nothing fades, which is the state every measurement before C10 was taken in"));
		return;
	}

	UWorld* World = GetWorld();

	const USurfaceAccumulationSubsystem* Subsystem =
		World != nullptr ? World->GetSubsystem<USurfaceAccumulationSubsystem>() : nullptr;

	const float HalfLifeSeconds = Subsystem != nullptr
		? Subsystem->GetDecayHalfLifeSeconds() / Clamped
		: 0.0f;

	UE_LOG(LogRacetrackSurface, Log,
		TEXT("Decay now %.2fx, a half-life of %.1f laps and %.0f s of track time, applied in %.0f steps "
			"of %.6f retention"),
		Clamped,
		DecayHalfLifeInLaps / Clamped,
		HalfLifeSeconds,
		USurfaceAccumulationSubsystem::GetDecayStepsPerHalfLife(),
		RacetrackSurfaceMapping::DecayRetentionForProgress(
			1.0f / USurfaceAccumulationSubsystem::GetDecayStepsPerHalfLife()));
}

void ARacetrackCircuit::CycleDebugView()
{
	if (SurfaceLook == ESurfaceLook::Asphalt)
	{
		SetSurfaceLook(ESurfaceLook::Debug);
		return;
	}

	SetDebugView(NextDebugView(DebugView));
}

ESurfaceDebugView ARacetrackCircuit::NextDebugView(ESurfaceDebugView View)
{
	switch (View)
	{
	case ESurfaceDebugView::AllChannels:
		return ESurfaceDebugView::RubberBuildup;
	case ESurfaceDebugView::RubberBuildup:
		return ESurfaceDebugView::SkidMarks;
	case ESurfaceDebugView::SkidMarks:
		return ESurfaceDebugView::Marbles;
	default:
		return ESurfaceDebugView::AllChannels;
	}
}

FLinearColor ARacetrackCircuit::DebugViewChannelMask(ESurfaceDebugView View)
{
	switch (View)
	{
	case ESurfaceDebugView::RubberBuildup:
		return USurfaceAccumulationBuffer::ChannelMask(EAccumulationChannel::RubberBuildup);
	case ESurfaceDebugView::SkidMarks:
		return USurfaceAccumulationBuffer::ChannelMask(EAccumulationChannel::SkidMark);
	case ESurfaceDebugView::Marbles:
		return USurfaceAccumulationBuffer::ChannelMask(EAccumulationChannel::Marbles);
	default:
		return FLinearColor::White;
	}
}

const TCHAR* ARacetrackCircuit::DebugViewName(ESurfaceDebugView View)
{
	switch (View)
	{
	case ESurfaceDebugView::RubberBuildup:
		return TEXT("rubber buildup only");
	case ESurfaceDebugView::SkidMarks:
		return TEXT("skid marks only");
	case ESurfaceDebugView::Marbles:
		return TEXT("marbles only, which nothing writes before phase G");
	default:
		return TEXT("all three channels");
	}
}

void ARacetrackCircuit::SetDebugView(ESurfaceDebugView View)
{
	DebugView = View;

	ApplyDebugViewToRoad();

	UE_LOG(LogRacetrackSurface, Log,
		TEXT("%s now shows %s, masking the sampled coverage by %s"),
		*GetName(),
		DebugViewName(DebugView),
		*DebugViewChannelMask(DebugView).ToString());

	RacetrackCircuitDisplay::AnnounceOnScreen(
		this,
		TEXT("RubberAccumDemo.DebugView"),
		FString::Printf(TEXT("Surface view: %s"), DebugViewName(DebugView)));
}

void ARacetrackCircuit::ReportSurfaceCoverage()
{
	USurfaceAccumulationSubsystem* Subsystem = GetWorld() != nullptr
		? GetWorld()->GetSubsystem<USurfaceAccumulationSubsystem>()
		: nullptr;

	USurfaceAccumulationBuffer* Buffer = Subsystem != nullptr ? Subsystem->GetBuffer() : nullptr;
	UTextureRenderTarget2D* Target = Buffer != nullptr ? Buffer->GetTarget() : nullptr;

	if (Target == nullptr)
	{
		RacetrackCircuitDisplay::AnnounceOnScreen(
			this,
			TEXT("RubberAccumDemo.SurfaceReport"),
			TEXT("No accumulation buffer is allocated yet"));
		return;
	}

	const FSurfaceTrackRibbon& Ribbon = Subsystem->GetRibbon();
	const int32 AcrossTexels = Ribbon.AcrossTrackTexels();

	constexpr int32 ProbeCount = 48;

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
			this, Target, Column, RowIndex * AcrossTexels, 1, AcrossTexels, false);

		for (const FLinearColor& Texel : Band)
		{
			Peak[0] = FMath::Max(Peak[0], Texel.R);
			Peak[1] = FMath::Max(Peak[1], Texel.G);
			Peak[2] = FMath::Max(Peak[2], Texel.B);
		}
	}

	const FString Report = FString::Printf(
		TEXT("Peak coverage: rubber %.4f, skid %.4f, marbles %.4f"),
		RacetrackSurfaceMapping::ExposureToCoverage(Peak[0]),
		RacetrackSurfaceMapping::ExposureToCoverage(Peak[1]),
		RacetrackSurfaceMapping::ExposureToCoverage(Peak[2]));

	UE_LOG(LogRacetrackSurface, Log,
		TEXT("%s over %d columns of the %d x %d atlas. Peak exposure: rubber %.6f, skid %.6f, "
			"marbles %.6f. Debug view is %s with mask %s"),
		*Report,
		ProbeCount,
		Ribbon.AtlasWidth(),
		Ribbon.AtlasHeight(),
		Peak[0],
		Peak[1],
		Peak[2],
		DebugViewName(DebugView),
		*DebugViewChannelMask(DebugView).ToString());

	RacetrackCircuitDisplay::AnnounceOnScreen(this, TEXT("RubberAccumDemo.SurfaceReport"), Report);
}

void ARacetrackCircuit::ApplyDebugViewToRoad()
{
	if (RoadSurfaceMaterial == nullptr)
	{
		return;
	}

	RoadSurfaceMaterial->SetVectorParameterValue(
		RacetrackCircuitDisplay::DebugChannelMaskParameterName,
		DebugViewChannelMask(DebugView));
}

void ARacetrackCircuit::ToggleSurfaceLook()
{
	SetSurfaceLook(SurfaceLook == ESurfaceLook::Debug ? ESurfaceLook::Asphalt : ESurfaceLook::Debug);
}

const TCHAR* ARacetrackCircuit::SurfaceLookName(ESurfaceLook Look)
{
	return Look == ESurfaceLook::Asphalt ? TEXT("lit asphalt") : TEXT("debug view");
}

void ARacetrackCircuit::SetSurfaceLook(ESurfaceLook Look)
{
	if (Look == ESurfaceLook::Asphalt && AsphaltSurfaceMaterial == nullptr)
	{
		UE_LOG(LogRacetrackSurface, Warning,
			TEXT("%s has no asphalt material bound, so the road stays on the debug view"),
			*GetName());

		RacetrackCircuitDisplay::AnnounceOnScreen(
			this,
			TEXT("RubberAccumDemo.SurfaceLook"),
			TEXT("No asphalt material is bound, so the road stays on the debug view"));
		return;
	}

	SurfaceLook = Look;

	ApplySurfaceLookToRoad();

	UE_LOG(LogRacetrackSurface, Log,
		TEXT("%s now shows the %s, with local exposure and bloom %s"),
		*GetName(),
		SurfaceLookName(SurfaceLook),
		SurfaceLook == ESurfaceLook::Asphalt ? TEXT("left to the scene") : TEXT("neutralised per texel"));

	RacetrackCircuitDisplay::AnnounceOnScreen(
		this,
		TEXT("RubberAccumDemo.SurfaceLook"),
		FString::Printf(TEXT("Surface look: %s"), SurfaceLookName(SurfaceLook)));
}

void ARacetrackCircuit::ApplySurfaceLookToRoad()
{
	const bool bShowAsphalt = SurfaceLook == ESurfaceLook::Asphalt;

	PerTexelDebugView->bEnabled = !bShowAsphalt;

	UMaterialInstanceDynamic* Shown = bShowAsphalt ? AsphaltSurfaceMaterial : RoadSurfaceMaterial;

	if (Shown != nullptr)
	{
		Road->SetMaterial(0, Shown);
	}
}

void ARacetrackCircuit::SpawnField()
{
	UWorld* World = GetWorld();

	if (World == nullptr || CarClass == nullptr || CarCount <= 0)
	{
		return;
	}

	const double Length = GetTrackLength();

	if (Length <= UE_DOUBLE_SMALL_NUMBER)
	{
		return;
	}

	SpawnedField.Reset(CarCount);

	for (int32 Index = 0; Index < CarCount; ++Index)
	{
		const double BaselineAlpha = GetDriverBaselineLateralOffsetAlpha(Index);
		const double LineCommitment = GetDriverLineCommitment(Index);

		const double PackedSpacing = FMath::Min(GridSpacingCentimetres, Length / CarCount);
		const double StartingDistance = Length - PackedSpacing * Index;
		const FTransform SpawnTransform(
			GetWorldDirectionAtDistance(StartingDistance).Rotation(),
			GetWorldPositionAtDistance(StartingDistance, BaselineAlpha));

		ASplineFollowingCar* Car = World->SpawnActorDeferred<ASplineFollowingCar>(
			CarClass,
			SpawnTransform,
			this,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

		if (Car == nullptr)
		{
			continue;
		}

		Car->ConfigureDriver(
			this, StartingDistance, BaselineAlpha, LineCommitment, GetDriverSeedForGridIndex(Index));
		Car->FinishSpawning(SpawnTransform);

		SpawnedField.Add(Car);
	}

	const ASplineFollowingCar* Reference = CarClass.GetDefaultObject();
	const double Spacing = GetAdjacentDriverSpacingCentimetres();

	UE_LOG(LogRacetrackSurface, Log,
		TEXT("%s spawned a field of %d cars from seed %d, spread across %.2f of the track width, which puts "
			"adjacent baselines %.2f cm apart against a %.1f cm contact patch"),
		*GetName(),
		CarCount,
		FieldRandomSeed,
		DriverLateralSpread * 2.0,
		Spacing,
		Reference != nullptr ? Reference->GetContactWidth() : 0.0f);

	ReportLapsToSaturation();
}

double ARacetrackCircuit::GetDriverBaselineLateralOffsetAlpha(int32 GridIndex) const
{
	const double GridFraction = CarCount > 1 ? static_cast<double>(GridIndex) / (CarCount - 1) : 0.5;

	return FMath::Lerp(-DriverLateralSpread, DriverLateralSpread, GridFraction);
}

double ARacetrackCircuit::GetDriverLineCommitment(int32 GridIndex) const
{
	const double GridFraction = CarCount > 1 ? static_cast<double>(GridIndex) / (CarCount - 1) : 0.5;

	return FMath::Lerp(
		DriverLineCommitmentAtCentre,
		DriverLineCommitmentAtEdge,
		FMath::Abs(GridFraction * 2.0 - 1.0));
}

double ARacetrackCircuit::GetAdjacentDriverSpacingCentimetres() const
{
	if (CarCount <= 1)
	{
		return 0.0;
	}

	return (2.0 * DriverLateralSpread / (CarCount - 1)) * TrackWidth * 0.5;
}

void ARacetrackCircuit::ReportLapsToSaturation()
{
	const ASplineFollowingCar* Reference = CarClass.GetDefaultObject();

	if (Reference == nullptr || CarCount <= 0)
	{
		return;
	}

	const float DepositionScale = USurfaceAccumulationSubsystem::GetDepositionScale();

	const float ApexPerContact = RacetrackSurfaceMapping::SlipToExposure(
		Reference->GetCorneringSlipAmount(), DepositionScale);

	const float StraightPerContact = RacetrackSurfaceMapping::SlipToExposure(
		Reference->GetStraightSlipAmount(), DepositionScale);

	if (ApexPerContact <= UE_SMALL_NUMBER || StraightPerContact <= UE_SMALL_NUMBER)
	{
		return;
	}

	const double Spacing = GetAdjacentDriverSpacingCentimetres();
	const double MarkWidth = Reference->GetContactWidth();
	const double WheelsOverATexel = ASplineFollowingCar::WheelsSharingATrack;

	const double CarsOverAStraightTexel = Spacing > UE_DOUBLE_SMALL_NUMBER
		? FMath::Clamp(MarkWidth / Spacing, 1.0, static_cast<double>(CarCount))
		: CarCount;

	const double ApexPerLap = CarCount * WheelsOverATexel * ApexPerContact;
	const double StraightPerLap = CarsOverAStraightTexel * WheelsOverATexel * StraightPerContact;

	UE_LOG(LogRacetrackSurface, Log,
		TEXT("One cornering contact deposits %.6f exposure. With %d cars putting %.0f wheels each over one "
			"apex texel that is %.4f per lap, so the apex reaches 63 %% coverage in about %.0f laps and 95 %% "
			"in about %.0f. That assumes the whole field crosses one texel, so read it as an upper bound"),
		ApexPerContact,
		CarCount,
		WheelsOverATexel,
		ApexPerLap,
		1.0 / (3.0 * ApexPerLap),
		1.0 / ApexPerLap);

	UE_LOG(LogRacetrackSurface, Log,
		TEXT("One straight-line contact deposits %.6f. Baselines %.2f cm apart against a %.1f cm mark put "
			"%.2f cars over one straight texel, each laying %.0f coincident wheels, that is %.6f per lap, so "
			"a straight reaches 63 %% in about %.0f laps and 95 %% in about %.0f, a contrast of %.1f to 1 "
			"against the apex"),
		StraightPerContact,
		Spacing,
		MarkWidth,
		CarsOverAStraightTexel,
		WheelsOverATexel,
		StraightPerLap,
		1.0 / (3.0 * StraightPerLap),
		1.0 / StraightPerLap,
		ApexPerLap / StraightPerLap);
}

void ARacetrackCircuit::BindAccumulationTargetToRoad()
{
	UWorld* World = GetWorld();

	if (World == nullptr || Road == nullptr || RoadMaterial == nullptr)
	{
		return;
	}

	USurfaceAccumulationSubsystem* Subsystem = World->GetSubsystem<USurfaceAccumulationSubsystem>();

	if (Subsystem == nullptr)
	{
		return;
	}

	UTextureRenderTarget2D* Target = Subsystem->GetOrCreateAccumulationTarget();

	if (Target == nullptr)
	{
		UE_LOG(LogRacetrackSurface, Warning,
			TEXT("%s could not obtain an accumulation target, so the road will not show the surface"),
			*GetName());
		return;
	}

	RoadSurfaceMaterial = Road->CreateDynamicMaterialInstance(0, RoadMaterial);

	if (RoadSurfaceMaterial == nullptr)
	{
		return;
	}

	RoadSurfaceMaterial->SetTextureParameterValue(
		RacetrackSurfaceMapping::AccumulationTargetParameterName,
		Target);

	ApplyDebugViewToRoad();

	UE_LOG(LogRacetrackSurface, Log,
		TEXT("%s bound a %d x %d accumulation target into the road material"),
		*GetName(),
		Target->SizeX,
		Target->SizeY);

	if (AsphaltMaterial != nullptr)
	{
		const float TilesAlongLap = static_cast<float>(
			FMath::Max(FMath::RoundToDouble(GetTrackLength() / AsphaltScanCentimetres), 1.0));
		const float TilesAcross = static_cast<float>(TrackWidth / AsphaltScanCentimetres);

		AsphaltSurfaceMaterial = UMaterialInstanceDynamic::Create(AsphaltMaterial, this);

		AsphaltSurfaceMaterial->SetTextureParameterValue(
			RacetrackSurfaceMapping::AccumulationTargetParameterName,
			Target);
		AsphaltSurfaceMaterial->SetScalarParameterValue(AsphaltTilesAlongLapParameterName, TilesAlongLap);
		AsphaltSurfaceMaterial->SetScalarParameterValue(AsphaltTilesAcrossParameterName, TilesAcross);

		UE_LOG(LogRacetrackSurface, Log,
			TEXT("%s bound the same target into the asphalt, tiled %.0f times along the %.1f cm lap and "
				"%.2f times across the %.1f cm width"),
			*GetName(),
			TilesAlongLap,
			GetTrackLength(),
			TilesAcross,
			TrackWidth);
	}

	ApplySurfaceLookToRoad();
}

void ARacetrackCircuit::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	GenerateTrackSpline();
	BuildCorneringProfile();
	GenerateRoadMesh();
	PublishTrackRibbon();
}

void ARacetrackCircuit::GenerateRoadMesh()
{
	if (Spline == nullptr || Road == nullptr)
	{
		return;
	}

	const double Length = GetTrackLength();

	if (Length <= UE_DOUBLE_SMALL_NUMBER)
	{
		return;
	}

	const int32 SegmentCount = FMath::Max(
		FMath::RoundToInt32(Length / FMath::Max(RoadSegmentLength, 1.0)), 3);

	const int32 RingCount = SegmentCount + 1;
	const double HalfWidth = TrackWidth * 0.5;

	TArray<FVector> Vertices;
	TArray<FVector2D> UVs;
	TArray<FVector> Normals;
	TArray<int32> Triangles;
	TArray<FLinearColor> VertexColours;
	TArray<FProcMeshTangent> Tangents;
	TArray<FVector> LeftEdges;
	TArray<FVector> RightEdges;
	TArray<FVector> EdgeUps;

	Vertices.Reserve(RingCount * 2);
	LeftEdges.Reserve(RingCount);
	RightEdges.Reserve(RingCount);
	EdgeUps.Reserve(RingCount);
	UVs.Reserve(RingCount * 2);
	Normals.Reserve(RingCount * 2);
	Triangles.Reserve(SegmentCount * 6);
	Tangents.Reserve(RingCount * 2);

	const FTransform WorldToLocal = Road->GetComponentTransform().Inverse();

	for (int32 Ring = 0; Ring < RingCount; ++Ring)
	{
		const double Distance = Length * Ring / SegmentCount;
		const float DistanceFloat = static_cast<float>(FMath::Min(Distance, Length));

		const FVector Centre = Spline->GetLocationAtDistanceAlongSpline(DistanceFloat, ESplineCoordinateSpace::World);
		const FVector Right = Spline->GetRightVectorAtDistanceAlongSpline(DistanceFloat, ESplineCoordinateSpace::World);
		const FVector Up = Spline->GetUpVectorAtDistanceAlongSpline(DistanceFloat, ESplineCoordinateSpace::World);
		const FVector Forward = Spline->GetDirectionAtDistanceAlongSpline(DistanceFloat, ESplineCoordinateSpace::World);

		const FVector Raised = Centre + Up * RoadHeightAboveSpline;
		const FVector LeftEdge = Raised - Right * HalfWidth;
		const FVector RightEdge = Raised + Right * HalfWidth;
		const double AlongTrack = Distance / Length;

		Vertices.Add(WorldToLocal.TransformPosition(LeftEdge));
		Vertices.Add(WorldToLocal.TransformPosition(RightEdge));

		LeftEdges.Add(LeftEdge);
		RightEdges.Add(RightEdge);
		EdgeUps.Add(Up);

		UVs.Add(FVector2D(AlongTrack, 0.0));
		UVs.Add(FVector2D(AlongTrack, 1.0));

		const FVector LocalUp = WorldToLocal.TransformVectorNoScale(Up);
		Normals.Add(LocalUp);
		Normals.Add(LocalUp);

		const FProcMeshTangent Tangent(WorldToLocal.TransformVectorNoScale(Forward), false);
		Tangents.Add(Tangent);
		Tangents.Add(Tangent);
	}

	for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
	{
		const int32 Base = Segment * 2;

		Triangles.Add(Base);
		Triangles.Add(Base + 1);
		Triangles.Add(Base + 2);

		Triangles.Add(Base + 1);
		Triangles.Add(Base + 3);
		Triangles.Add(Base + 2);
	}

	Road->ClearAllMeshSections();
	Road->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, VertexColours, Tangents, true);

	if (RoadMaterial != nullptr)
	{
		Road->SetMaterial(0, RoadMaterial);
	}

	GenerateEdgeWalls(LeftEdges, RightEdges, EdgeUps);
}

void ARacetrackCircuit::GenerateEdgeWalls(
	const TArray<FVector>& LeftEdges,
	const TArray<FVector>& RightEdges,
	const TArray<FVector>& EdgeUps)
{
	if (EdgeWalls == nullptr)
	{
		return;
	}

	EdgeWalls->ClearAllMeshSections();

	const int32 RingCount = LeftEdges.Num();

	if (RingCount < 2 || RightEdges.Num() != RingCount || EdgeUps.Num() != RingCount)
	{
		return;
	}

	const FTransform WorldToLocal = EdgeWalls->GetComponentTransform().Inverse();

	TArray<FVector> Vertices;
	TArray<int32> Triangles;

	Vertices.Reserve(RingCount * 4);
	Triangles.Reserve((RingCount - 1) * 12);

	for (int32 Ring = 0; Ring < RingCount; ++Ring)
	{
		const FVector Rise = EdgeUps[Ring] * EdgeWallHeight;

		Vertices.Add(WorldToLocal.TransformPosition(LeftEdges[Ring]));
		Vertices.Add(WorldToLocal.TransformPosition(LeftEdges[Ring] + Rise));
		Vertices.Add(WorldToLocal.TransformPosition(RightEdges[Ring]));
		Vertices.Add(WorldToLocal.TransformPosition(RightEdges[Ring] + Rise));
	}

	for (int32 Segment = 0; Segment < RingCount - 1; ++Segment)
	{
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const int32 Foot = Segment * 4 + Side * 2;
			const int32 Top = Foot + 1;
			const int32 NextFoot = Foot + 4;
			const int32 NextTop = Top + 4;

			Triangles.Append({ Foot, NextFoot, Top, Top, NextFoot, NextTop });
		}
	}

	EdgeWalls->CreateMeshSection_LinearColor(
		0,
		Vertices,
		Triangles,
		TArray<FVector>(),
		TArray<FVector2D>(),
		TArray<FLinearColor>(),
		TArray<FProcMeshTangent>(),
		true);
}

void ARacetrackCircuit::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	BuildCorneringProfile();
	PublishTrackRibbon();
}

void ARacetrackCircuit::PublishTrackRibbon()
{
	UWorld* World = GetWorld();

	if (World == nullptr || Spline == nullptr)
	{
		return;
	}

	USurfaceAccumulationSubsystem* Subsystem = World->GetSubsystem<USurfaceAccumulationSubsystem>();

	if (Subsystem == nullptr)
	{
		return;
	}

	FSurfaceTrackRibbon Ribbon;
	Ribbon.TrackLength = GetTrackLength();
	Ribbon.TrackWidth = TrackWidth;
	Ribbon.CentimetresPerTexel = SurfaceCentimetresPerTexel;
	Ribbon.bClosedLoop = Spline->IsClosedLoop();

	Subsystem->ConfigureTrackRibbon(Spline, Ribbon);

	const double LapSeconds = GetLapSeconds();

	if (LapSeconds > UE_DOUBLE_SMALL_NUMBER)
	{
		Subsystem->SetDecayHalfLifeSeconds(static_cast<float>(DecayHalfLifeInLaps * LapSeconds));
	}
}

double ARacetrackCircuit::GetLapSeconds() const
{
	const double Length = GetTrackLength();
	const ASplineFollowingCar* Reference = CarClass.GetDefaultObject();

	if (Reference == nullptr || CorneringProfile.Num() == 0 || Length <= UE_DOUBLE_SMALL_NUMBER)
	{
		return 0.0;
	}

	const double StraightSpeed = Reference->GetStraightLineSpeedCentimetresPerSecond();
	const double CorneringSpeed = StraightSpeed * Reference->GetCorneringSpeedFraction();

	if (StraightSpeed <= UE_DOUBLE_SMALL_NUMBER)
	{
		return 0.0;
	}

	const double Spacing = Length / CorneringProfile.Num();

	double Seconds = 0.0;

	for (const double SignedCornering : CorneringProfile)
	{
		const double Speed = FMath::Lerp(StraightSpeed, CorneringSpeed, FMath::Abs(SignedCornering));

		Seconds += Spacing / FMath::Max(Speed, UE_DOUBLE_SMALL_NUMBER);
	}

	return Seconds;
}

void ARacetrackCircuit::BuildStadiumPoints(TArray<FVector>& OutPoints) const
{
	const int32 PointsPerHalfCorner = FMath::Max(PointsPerCorner, 1);
	const double HalfLength = StraightLength * 0.5;

	OutPoints.Reserve(2 * PointsPerHalfCorner + 3);

	OutPoints.Add(FVector(-HalfLength, CornerRadius, 0.0));
	OutPoints.Add(FVector(HalfLength, CornerRadius, 0.0));

	const FVector RightCornerCentre(HalfLength, 0.0, 0.0);
	for (int32 Step = 1; Step <= PointsPerHalfCorner; ++Step)
	{
		const double Angle = FMath::DegreesToRadians(90.0 - 180.0 * Step / PointsPerHalfCorner);
		OutPoints.Add(RightCornerCentre + FVector(CornerRadius * FMath::Cos(Angle), CornerRadius * FMath::Sin(Angle), 0.0));
	}

	OutPoints.Add(FVector(-HalfLength, -CornerRadius, 0.0));

	const FVector LeftCornerCentre(-HalfLength, 0.0, 0.0);
	for (int32 Step = 1; Step < PointsPerHalfCorner; ++Step)
	{
		const double Angle = FMath::DegreesToRadians(-90.0 - 180.0 * Step / PointsPerHalfCorner);
		OutPoints.Add(LeftCornerCentre + FVector(CornerRadius * FMath::Cos(Angle), CornerRadius * FMath::Sin(Angle), 0.0));
	}
}

void ARacetrackCircuit::BuildAuthoredLayoutPoints(TArray<FVector>& OutPoints) const
{
	OutPoints.Reserve(UE_ARRAY_COUNT(RacetrackLayouts::SpaCentreline));

	for (const FVector& Point : RacetrackLayouts::SpaCentreline)
	{
		OutPoints.Add(FVector(-Point.Y, Point.X, Point.Z * ElevationExaggeration));
	}
}

void ARacetrackCircuit::ApplySplinePoints(const TArray<FVector>& Points)
{
	Spline->ClearSplinePoints(false);

	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		Spline->AddSplinePoint(Points[Index], ESplineCoordinateSpace::Local, false);
		Spline->SetSplinePointType(Index, ESplinePointType::Curve, false);
	}

	Spline->SetClosedLoop(true, true);
}

void ARacetrackCircuit::GenerateTrackSpline()
{
	if (Spline == nullptr)
	{
		return;
	}

	TArray<FVector> Points;

	if (Layout == ETrackLayout::Stadium)
	{
		BuildStadiumPoints(Points);
		ApplySplinePoints(Points);
		return;
	}

	BuildAuthoredLayoutPoints(Points);
	ApplySplinePoints(Points);

	const double AuthoredLength = GetTrackLength();

	if (AuthoredLength <= UE_DOUBLE_SMALL_NUMBER)
	{
		return;
	}

	const double Scale = AuthoredLayoutLapLength / AuthoredLength;
	FVector Centre = FVector::ZeroVector;

	for (const FVector& Point : Points)
	{
		Centre += Point;
	}

	Centre /= Points.Num();
	Centre.Z = 0.0;

	for (FVector& Point : Points)
	{
		Point = (Point - Centre) * Scale;
	}

	ApplySplinePoints(Points);
}

void ARacetrackCircuit::SetLayout(ETrackLayout NewLayout, double NewLapLength)
{
	Layout = NewLayout;
	AuthoredLayoutLapLength = NewLapLength;

	GenerateTrackSpline();
	BuildCorneringProfile();
	GenerateRoadMesh();
	PublishTrackRibbon();
}

void ARacetrackCircuit::BuildCorneringProfile()
{
	CorneringProfile.Reset();

	const double Length = GetTrackLength();

	if (Spline == nullptr || Length <= UE_DOUBLE_SMALL_NUMBER)
	{
		return;
	}

	const int32 SampleCount = FMath::Clamp(
		FMath::RoundToInt32(Length / CorneringSampleSpacing), 16, MaximumCorneringSamples);

	const double Spacing = Length / SampleCount;

	CorneringProfile.Reserve(SampleCount);

	TightestCurvature = 0.0;
	TotalTurningRadians = 0.0;

	for (int32 Sample = 0; Sample < SampleCount; ++Sample)
	{
		const double Distance = Spacing * Sample;

		const FVector Behind = Spline->GetDirectionAtDistanceAlongSpline(
			static_cast<float>(WrapDistance(Distance - Spacing)), ESplineCoordinateSpace::Local);

		const FVector Ahead = Spline->GetDirectionAtDistanceAlongSpline(
			static_cast<float>(WrapDistance(Distance + Spacing)), ESplineCoordinateSpace::Local);

		const double TurnAngle = FMath::Atan2(
			FVector::CrossProduct(Behind, Ahead).Z, FVector::DotProduct(Behind, Ahead));

		const double SignedCurvature = TurnAngle / (2.0 * Spacing);

		CorneringProfile.Add(SignedCurvature);
		TightestCurvature = FMath::Max(TightestCurvature, FMath::Abs(SignedCurvature));
		TotalTurningRadians += TurnAngle * 0.5;
	}

	if (TightestCurvature <= UE_DOUBLE_SMALL_NUMBER)
	{
		return;
	}

	TArray<double> Ranked;
	Ranked.Reserve(CorneringProfile.Num());

	for (const double Entry : CorneringProfile)
	{
		Ranked.Add(FMath::Abs(Entry));
	}

	Ranked.Sort();

	const int32 PercentileIndex = FMath::Clamp(
		FMath::FloorToInt32(CorneringNormalisationPercentile * Ranked.Num()), 0, Ranked.Num() - 1);

	const double SustainedCurvature = Ranked[PercentileIndex] > UE_DOUBLE_SMALL_NUMBER
		? Ranked[PercentileIndex]
		: TightestCurvature;

	for (double& Entry : CorneringProfile)
	{
		Entry = FMath::Clamp(Entry / SustainedCurvature, -1.0, 1.0);
	}

	UE_LOG(LogRacetrackSurface, Log,
		TEXT("%s sampled %d cornering points over %.0f cm, tightest curvature %.8f per cm, normalised "
			"against the %.0fth percentile at %.8f"),
		*GetName(),
		CorneringProfile.Num(),
		Length,
		TightestCurvature,
		CorneringNormalisationPercentile * 100.0,
		SustainedCurvature);
}

double ARacetrackCircuit::GetTrackLength() const
{
	return Spline != nullptr ? static_cast<double>(Spline->GetSplineLength()) : 0.0;
}

double ARacetrackCircuit::WrapDistance(double DistanceAlongTrack) const
{
	const double Length = GetTrackLength();

	if (Length <= UE_DOUBLE_SMALL_NUMBER)
	{
		return 0.0;
	}

	const double Wrapped = FMath::Fmod(DistanceAlongTrack, Length);

	return Wrapped < 0.0 ? Wrapped + Length : Wrapped;
}

FVector ARacetrackCircuit::GetWorldPositionAtDistance(double DistanceAlongTrack, double LateralOffsetAlpha) const
{
	const double Length = GetTrackLength();

	if (Spline == nullptr || Length <= UE_DOUBLE_SMALL_NUMBER)
	{
		return GetActorLocation();
	}

	const float DistanceFloat = static_cast<float>(WrapDistance(DistanceAlongTrack));
	const FVector CentreLocation = Spline->GetLocationAtDistanceAlongSpline(DistanceFloat, ESplineCoordinateSpace::World);
	const FVector RightVector = Spline->GetRightVectorAtDistanceAlongSpline(DistanceFloat, ESplineCoordinateSpace::World);
	const double ClampedAlpha = FMath::Clamp(LateralOffsetAlpha, -1.0, 1.0);

	return CentreLocation + RightVector * (ClampedAlpha * TrackWidth * 0.5);
}

FVector ARacetrackCircuit::GetWorldDirectionAtDistance(double DistanceAlongTrack) const
{
	const double Length = GetTrackLength();

	if (Spline == nullptr || Length <= UE_DOUBLE_SMALL_NUMBER)
	{
		return GetActorForwardVector();
	}

	return Spline->GetDirectionAtDistanceAlongSpline(
		static_cast<float>(WrapDistance(DistanceAlongTrack)), ESplineCoordinateSpace::World);
}

FVector ARacetrackCircuit::GetWorldRightVectorAtDistance(double DistanceAlongTrack) const
{
	const double Length = GetTrackLength();

	if (Spline == nullptr || Length <= UE_DOUBLE_SMALL_NUMBER)
	{
		return GetActorRightVector();
	}

	return Spline->GetRightVectorAtDistanceAlongSpline(
		static_cast<float>(WrapDistance(DistanceAlongTrack)), ESplineCoordinateSpace::World);
}

FVector ARacetrackCircuit::GetWorldUpVectorAtDistance(double DistanceAlongTrack) const
{
	const double Length = GetTrackLength();

	if (Spline == nullptr || Length <= UE_DOUBLE_SMALL_NUMBER)
	{
		return GetActorUpVector();
	}

	return Spline->GetUpVectorAtDistanceAlongSpline(
		static_cast<float>(WrapDistance(DistanceAlongTrack)), ESplineCoordinateSpace::World);
}

double ARacetrackCircuit::GetSignedCorneringAtDistance(double DistanceAlongTrack) const
{
	const double Length = GetTrackLength();

	if (CorneringProfile.Num() == 0 || Length <= UE_DOUBLE_SMALL_NUMBER)
	{
		return 0.0;
	}

	const double Position = WrapDistance(DistanceAlongTrack) / Length * CorneringProfile.Num();
	const int32 Index = FMath::Clamp(FMath::FloorToInt32(Position), 0, CorneringProfile.Num() - 1);
	const int32 NextIndex = (Index + 1) % CorneringProfile.Num();

	return FMath::Lerp(CorneringProfile[Index], CorneringProfile[NextIndex], Position - Index);
}

double ARacetrackCircuit::GetCorneringAmountAtDistance(double DistanceAlongTrack) const
{
	return FMath::Abs(GetSignedCorneringAtDistance(DistanceAlongTrack));
}
