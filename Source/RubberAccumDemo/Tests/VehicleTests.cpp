#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "HAL/PlatformTime.h"
#include "ProceduralMeshComponent.h"
#include "../RacetrackCircuit.h"
#include "../SplineFollowingCar.h"

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

	TestRoadUnderEverySample(*this, *World, *Track, TEXT("Stadium"));

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

	const FProcMeshSection* RoadSection = Track->GetRoad()->GetProcMeshSection(0);

	if (RoadSection == nullptr)
	{
		AddError(TEXT("The road has no section to copy."));
		Track->Destroy();
		return false;
	}

	TArray<FVector> Vertices;
	TArray<int32> Triangles;

	for (const FProcMeshVertex& Vertex : RoadSection->ProcVertexBuffer)
	{
		Vertices.Add(Vertex.Position);
	}

	for (const uint32 Index : RoadSection->ProcIndexBuffer)
	{
		Triangles.Add(static_cast<int32>(Index));
	}

	UProceduralMeshComponent* Probe = NewObject<UProceduralMeshComponent>(Track);
	Probe->bUseAsyncCooking = false;
	Probe->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Probe->SetWorldTransform(Track->GetRoad()->GetComponentTransform());
	Probe->RegisterComponent();

	const TArray<FVector> NoNormals;
	const TArray<FVector2D> NoUVs;
	const TArray<FLinearColor> NoColours;
	const TArray<FProcMeshTangent> NoTangents;

	TArray<double> WithCollisionMilliseconds;
	TArray<double> WithoutCollisionMilliseconds;
	bool bProbeAlwaysCollided = true;

	const FVector ProbeTraceStart = Track->GetWorldPositionAtDistance(0.0, 0.0) + FVector::UpVector * TraceReach;
	const FVector ProbeTraceEnd = Track->GetWorldPositionAtDistance(0.0, 0.0) - FVector::UpVector * TraceReach;

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

	AddInfo(FString::Printf(
		TEXT("Spa road at %.0f cm, %d vertices and %d triangles: creating the section takes %.4f ms with "
			"collision and %.4f ms without, median of %d interleaved samples each, so the collision cook and body "
			"creation cost %.4f ms. Fastest and slowest with collision %.4f and %.4f ms."),
		Track->GetTrackLength(),
		Vertices.Num(),
		Triangles.Num() / 3,
		MedianWith,
		MedianWithout,
		CookSamples,
		MedianWith - MedianWithout,
		WithCollisionMilliseconds[0],
		WithCollisionMilliseconds.Last()));

	TestTrue(TEXT("Every timed section with collision really could be traced, so the cook was not skipped"),
		bProbeAlwaysCollided);

	Track->Destroy();

	return true;
}

#endif
