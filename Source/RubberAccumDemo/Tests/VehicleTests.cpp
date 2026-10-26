// Copyright 2026 Balázs Fábián. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "HAL/PlatformTime.h"
#include "ProceduralMeshComponent.h"
#include "SurfaceAccumulationMapping.h"
#include "../RacetrackCircuit.h"
#include "../SplineFollowingCar.h"
#include "../WheelSurfaceSlip.h"

namespace RubberAccumDemoVehicleTest
{
	constexpr double DemoLapLength = 180000.0;

	const FVector ClearOfTheHarnessTrack(0.0, 0.0, -300000.0);

	constexpr int32 RoadSamplesPerLap = 64;

	constexpr double RoadSampleAlphas[] = { -0.95, 0.0, 0.95 };

	constexpr double TraceReach = 500.0;

	constexpr double RoadSurfaceTolerance = 5.0;

	constexpr double FlyCameraDrop = 300.0;

	constexpr int32 CookSamples = 64;

	UWorld* FindVehicleTestWorld()
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

	TArray<double> RoadSampleDistances(double Length)
	{
		TArray<double> Distances;

		for (int32 Sample = 0; Sample < RoadSamplesPerLap; ++Sample)
		{
			Distances.Add(Length * Sample / RoadSamplesPerLap);
		}

		Distances.Add(Length - 1.0);

		return Distances;
	}

	void TestRoadUnderEverySample(
		FAutomationTestBase& Test,
		const UWorld& World,
		const ARacetrackCircuit& Track,
		const TCHAR* LayoutName)
	{
		const UProceduralMeshComponent* Road = Track.GetRoad();
		int32 Traces = 0;
		int32 RoadHits = 0;
		double WorstHeightOffTheSpline = 0.0;

		for (const double Distance : RoadSampleDistances(Track.GetTrackLength()))
		{
			const FVector Up = Track.GetWorldUpVectorAtDistance(Distance);

			for (const double Alpha : RoadSampleAlphas)
			{
				const FVector OnTheSpline = Track.GetWorldPositionAtDistance(Distance, Alpha);
				FHitResult Hit;
				++Traces;

				const bool bHit = World.LineTraceSingleByChannel(
					Hit,
					OnTheSpline + Up * TraceReach,
					OnTheSpline - Up * TraceReach,
					ECC_WorldDynamic);

				if (bHit && Hit.GetComponent() == Road)
				{
					++RoadHits;
					WorstHeightOffTheSpline = FMath::Max(
						WorstHeightOffTheSpline,
						FMath::Abs(FVector::DotProduct(Hit.ImpactPoint - OnTheSpline, Up)));
				}
			}
		}

		Test.AddInfo(FString::Printf(
			TEXT("%s: %d of %d downward traces hit the road, the worst %.2f cm off the spline along its up vector"),
			LayoutName,
			RoadHits,
			Traces,
			WorstHeightOffTheSpline));

		Test.TestEqual(
			FString::Printf(
				TEXT("%s: every downward trace on the wheels' WorldDynamic channel hits the road, on both sides of "
					"the lap seam"),
				LayoutName),
			RoadHits,
			Traces);

		Test.TestTrue(
			FString::Printf(TEXT("%s: the hits sit on the road surface, within %.0f cm of the spline"),
				LayoutName,
				RoadSurfaceTolerance),
			WorstHeightOffTheSpline <= RoadSurfaceTolerance);
	}

	double Median(TArray<double> Samples)
	{
		Samples.Sort();

		const int32 Middle = Samples.Num() / 2;

		return Samples.Num() % 2 == 1 ? Samples[Middle] : 0.5 * (Samples[Middle - 1] + Samples[Middle]);
	}

	void TimeSectionCook(
		FAutomationTestBase& Test,
		ARacetrackCircuit& Track,
		UProceduralMeshComponent& Source,
		const TCHAR* SectionName,
		const FVector& ProbeTraceStart,
		const FVector& ProbeTraceEnd)
	{
		const FProcMeshSection* SourceSection = Source.GetProcMeshSection(0);

		if (SourceSection == nullptr)
		{
			Test.AddError(FString::Printf(TEXT("The %s has no section to copy."), SectionName));
			return;
		}

		TArray<FVector> Vertices;
		TArray<int32> Triangles;

		for (const FProcMeshVertex& Vertex : SourceSection->ProcVertexBuffer)
		{
			Vertices.Add(Vertex.Position);
		}

		for (const uint32 Index : SourceSection->ProcIndexBuffer)
		{
			Triangles.Add(static_cast<int32>(Index));
		}

		UProceduralMeshComponent* Probe = NewObject<UProceduralMeshComponent>(&Track);
		Probe->bUseAsyncCooking = false;
		Probe->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Probe->SetWorldTransform(Source.GetComponentTransform());
		Probe->RegisterComponent();

		const TArray<FVector> NoNormals;
		const TArray<FVector2D> NoUVs;
		const TArray<FLinearColor> NoColours;
		const TArray<FProcMeshTangent> NoTangents;

		TArray<double> WithCollisionMilliseconds;
		TArray<double> WithoutCollisionMilliseconds;
		bool bProbeAlwaysCollided = true;

		for (int32 Sample = 0; Sample < CookSamples; ++Sample)
		{
			const double WithStart = FPlatformTime::Seconds();
			Probe->CreateMeshSection_LinearColor(0, Vertices, Triangles, NoNormals, NoUVs, NoColours, NoTangents, true);
			WithCollisionMilliseconds.Add((FPlatformTime::Seconds() - WithStart) * 1000.0);

			FHitResult ProbeHit;
			bProbeAlwaysCollided &= Probe->LineTraceComponent(
				ProbeHit, ProbeTraceStart, ProbeTraceEnd, FCollisionQueryParams::DefaultQueryParam);

			const double WithoutStart = FPlatformTime::Seconds();
			Probe->CreateMeshSection_LinearColor(0, Vertices, Triangles, NoNormals, NoUVs, NoColours, NoTangents, false);
			WithoutCollisionMilliseconds.Add((FPlatformTime::Seconds() - WithoutStart) * 1000.0);
		}

		Probe->DestroyComponent();

		const double MedianWith = Median(WithCollisionMilliseconds);
		const double MedianWithout = Median(WithoutCollisionMilliseconds);

		WithCollisionMilliseconds.Sort();

		Test.AddInfo(FString::Printf(
			TEXT("Spa %s at %.0f cm, %d vertices and %d triangles: creating the section takes %.4f ms with "
				"collision and %.4f ms without, median of %d interleaved samples each, so the collision cook and body "
				"creation cost %.4f ms. Fastest and slowest with collision %.4f and %.4f ms."),
			SectionName,
			Track.GetTrackLength(),
			Vertices.Num(),
			Triangles.Num() / 3,
			MedianWith,
			MedianWithout,
			CookSamples,
			MedianWith - MedianWithout,
			WithCollisionMilliseconds[0],
			WithCollisionMilliseconds.Last()));

		Test.TestTrue(
			FString::Printf(
				TEXT("Every timed %s section with collision really could be traced, so the cook was not skipped"),
				SectionName),
			bProbeAlwaysCollided);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVehicleFieldCarsCollideWithNothing,
	"RubberAccumDemo.Vehicle.FieldCarsCollideWithNothing",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVehicleFieldCarsCollideWithNothing::RunTest(const FString& Parameters)
{
	using namespace RubberAccumDemoVehicleTest;

	UWorld* World = FindVehicleTestWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No world available."));
		return false;
	}

	ASplineFollowingCar* Car = World->SpawnActor<ASplineFollowingCar>();

	if (Car == nullptr)
	{
		AddError(TEXT("Failed to spawn ASplineFollowingCar."));
		return false;
	}

	TArray<UPrimitiveComponent*> Primitives;
	Car->GetComponents(Primitives);

	TestTrue(TEXT("The car has a primitive component to check"), Primitives.Num() > 0);

	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		TestTrue(
			FString::Printf(
				TEXT("%s collides with nothing, so the player's car and the fly camera pass through the field"),
				*Primitive->GetName()),
			Primitive->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	}

	Car->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVehicleRoadHasCollision,
	"RubberAccumDemo.Vehicle.RoadHasCollision",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVehicleRoadHasCollision::RunTest(const FString& Parameters)
{
	using namespace RubberAccumDemoVehicleTest;

	UWorld* World = FindVehicleTestWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No world available."));
		return false;
	}

	ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>(ClearOfTheHarnessTrack, FRotator::ZeroRotator);

	if (Track == nullptr || Track->GetRoad() == nullptr)
	{
		AddError(TEXT("Failed to spawn a circuit with a road."));
		return false;
	}

	Track->SetLayout(ETrackLayout::Spa, DemoLapLength);

	TestRoadUnderEverySample(*this, *World, *Track, TEXT("Spa"));

	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVehicleFlyCameraPassesThroughTheRoad,
	"RubberAccumDemo.Vehicle.FlyCameraPassesThroughTheRoad",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVehicleFlyCameraPassesThroughTheRoad::RunTest(const FString& Parameters)
{
	using namespace RubberAccumDemoVehicleTest;

	UWorld* World = FindVehicleTestWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No world available."));
		return false;
	}

	ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>(ClearOfTheHarnessTrack, FRotator::ZeroRotator);

	if (Track == nullptr)
	{
		AddError(TEXT("Failed to spawn a circuit."));
		return false;
	}

	Track->SetLayout(ETrackLayout::Spa, DemoLapLength);

	FActorSpawnParameters AlwaysSpawn;
	AlwaysSpawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADefaultPawn* FlyCamera = World->SpawnActor<ADefaultPawn>(
		Track->GetWorldPositionAtDistance(0.0, 0.0) + FVector::UpVector * FlyCameraDrop,
		FRotator::ZeroRotator,
		AlwaysSpawn);

	if (FlyCamera == nullptr || FlyCamera->GetCollisionComponent() == nullptr)
	{
		AddError(TEXT("Failed to spawn a fly camera with its collision sphere."));
		Track->Destroy();
		return false;
	}

	const FCollisionShape FlyCameraSphere =
		FCollisionShape::MakeSphere(FlyCamera->GetCollisionComponent()->GetScaledSphereRadius());

	FCollisionQueryParams IgnoringTheFlyCamera;
	IgnoringTheFlyCamera.AddIgnoredActor(FlyCamera);

	const double Length = Track->GetTrackLength();

	for (int32 Quarter = 0; Quarter < 4; ++Quarter)
	{
		const double Distance = Length * Quarter / 4.0;
		const FVector OnTheSpline = Track->GetWorldPositionAtDistance(Distance, 0.0);
		const FVector Up = Track->GetWorldUpVectorAtDistance(Distance);
		const FVector Above = OnTheSpline + Up * FlyCameraDrop;
		const FVector Below = OnTheSpline - Up * FlyCameraDrop;

		FHitResult WheelChannelHit;
		const bool bSolidToTheWheels = World->SweepSingleByChannel(
			WheelChannelHit, Above, Below, FQuat::Identity, ECC_WorldDynamic, FlyCameraSphere, IgnoringTheFlyCamera);

		TestTrue(
			FString::Printf(
				TEXT("At %.0f cm the same sphere swept on the wheels' channel stops on the road"), Distance),
			bSolidToTheWheels && WheelChannelHit.GetComponent() == Track->GetRoad());

		FlyCamera->SetActorLocation(Above, false, nullptr, ETeleportType::TeleportPhysics);

		FHitResult FlyCameraHit;
		FlyCamera->SetActorLocation(Below, true, &FlyCameraHit);

		TestFalse(
			FString::Printf(TEXT("At %.0f cm the fly camera's sweep through the road is not blocked"), Distance),
			FlyCameraHit.bBlockingHit);

		TestTrue(
			FString::Printf(TEXT("At %.0f cm the fly camera ends up below the road, where it was sent"), Distance),
			FlyCamera->GetActorLocation().Equals(Below, 0.1));
	}

	FlyCamera->Destroy();
	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVehicleRoadCollisionCook,
	"RubberAccumDemo.Performance.RoadCollisionCook",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVehicleRoadCollisionCook::RunTest(const FString& Parameters)
{
	using namespace RubberAccumDemoVehicleTest;

	UWorld* World = FindVehicleTestWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No world available."));
		return false;
	}

	ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>(ClearOfTheHarnessTrack, FRotator::ZeroRotator);

	if (Track == nullptr || Track->GetRoad() == nullptr)
	{
		AddError(TEXT("Failed to spawn a circuit with a road."));
		return false;
	}

	Track->SetLayout(ETrackLayout::Spa, DemoLapLength);

	const FVector OnTheSpline = Track->GetWorldPositionAtDistance(0.0, 0.0);
	const FVector Up = Track->GetWorldUpVectorAtDistance(0.0);

	TimeSectionCook(
		*this, *Track, *Track->GetRoad(), TEXT("road"), OnTheSpline + Up * TraceReach, OnTheSpline - Up * TraceReach);

	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVehicleWheelSlipMapsToSurfaceSlip,
	"RubberAccumDemo.Vehicle.WheelSlipMapsToSurfaceSlip",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVehicleWheelSlipMapsToSurfaceSlip::RunTest(const FString& Parameters)
{
	FWheelSlipCalibration Calibration;
	Calibration.FullRubberSlipRatio = 0.2f;
	Calibration.FullRubberSlipAngle = 0.15f;
	Calibration.LockedWheelSpeedFraction = 0.1f;
	Calibration.SlowestSkiddingGroundSpeed = 100.0f;

	auto Reading = [](float BrakeTorque, float SlipMagnitude, float SlipAngle, float GroundSpeed)
	{
		FWheelSlipReading Wheel;
		Wheel.BrakeTorque = BrakeTorque;
		Wheel.SlipMagnitude = SlipMagnitude;
		Wheel.SlipAngle = SlipAngle;
		Wheel.GroundSpeed = GroundSpeed;
		return Wheel;
	};

	auto Slip = [&Calibration](const FWheelSlipReading& Wheel)
	{
		return WheelSurfaceSlip::SurfaceSlipFromWheel(Wheel, Calibration);
	};

	TestEqual(TEXT("A braked wheel turning at 2.5 % of road speed is locked and gives 1"),
		Slip(Reading(1500.0f, -1950.0f, 0.0f, 2000.0f)), 1.0f);

	TestEqual(TEXT("A free-rolling wheel gives 0"),
		Slip(Reading(0.0f, 0.0f, 0.0f, 2000.0f)), 0.0f);

	const float Cornering = Slip(Reading(0.0f, 0.0f, 0.05f, 2000.0f));

	TestTrue(TEXT("A cornering wheel lays rubber strictly between free rolling and a lockup"),
		Cornering > 0.0f && Cornering < 1.0f);

	TestFalse(TEXT("A braked wheel still turning at 20 % of road speed is not a lockup, however hard it slips"),
		RacetrackSurfaceMapping::IsLockup(Slip(Reading(1500.0f, -1600.0f, 0.0f, 2000.0f))));

	TestFalse(TEXT("A braked wheel below the slowest skidding speed is not a lockup"),
		RacetrackSurfaceMapping::IsLockup(Slip(Reading(1500.0f, -50.0f, 0.0f, 50.0f))));

	int32 UnbrakedReadings = 0;
	int32 UnbrakedLockups = 0;

	for (float SlipMagnitude = -3000.0f; SlipMagnitude <= 3000.0f; SlipMagnitude += 250.0f)
	{
		for (float SlipAngle = 0.0f; SlipAngle <= HALF_PI; SlipAngle += 0.1f)
		{
			for (const float GroundSpeed : { 0.0f, 50.0f, 500.0f, 2000.0f, 5000.0f })
			{
				++UnbrakedReadings;
				UnbrakedLockups += RacetrackSurfaceMapping::IsLockup(Slip(Reading(0.0f, SlipMagnitude, SlipAngle, GroundSpeed)))
					? 1
					: 0;
			}
		}
	}

	TestEqual(
		FString::Printf(TEXT("None of %d readings without brake torque is ever a lockup"), UnbrakedReadings),
		UnbrakedLockups,
		0);

	return true;
}

#endif
