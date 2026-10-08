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

	constexpr double WallPlacementTolerance = 0.5;

	constexpr double WheelSizedSweepRadius = 35.0;

	constexpr double WheelClearanceAboveRoad = 10.0;

	constexpr double SweepPastTheEdge = 200.0;

	constexpr double PolygonalEdgeAllowance = 2.0;

	constexpr double LocalRadiusHalfSpan = 10.0;

	constexpr double FoldScanStep = 10.0;

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

	void TestWallsStandOnTheEdges(FAutomationTestBase& Test, const ARacetrackCircuit& Track, const TCHAR* LayoutName)
	{
		UProceduralMeshComponent* Walls = Track.GetEdgeWalls();
		const FProcMeshSection* Section = Walls != nullptr ? Walls->GetProcMeshSection(0) : nullptr;

		if (Section == nullptr || Section->ProcVertexBuffer.Num() < 8 || Section->ProcVertexBuffer.Num() % 4 != 0)
		{
			Test.AddError(FString::Printf(TEXT("%s: the walls have no section of whole rings."), LayoutName));
			return;
		}

		const FTransform LocalToWorld = Walls->GetComponentTransform();
		const TArray<FProcMeshVertex>& WallVertices = Section->ProcVertexBuffer;
		const int32 RingCount = WallVertices.Num() / 4;
		const int32 SegmentCount = RingCount - 1;
		const double Length = Track.GetTrackLength();
		const double HalfWidth = Track.GetTrackWidth() * 0.5;
		const double FootHeight = Track.GetRoadHeightAboveSpline();
		const double TopHeight = FootHeight + Track.GetEdgeWallHeight();

		double WorstLateralError = 0.0;
		double WorstHeightError = 0.0;

		for (int32 Ring = 0; Ring < RingCount; ++Ring)
		{
			const double Distance = Length * Ring / SegmentCount;
			const FVector Centre = Track.GetWorldPositionAtDistance(Distance, 0.0);
			const FVector Right = Track.GetWorldRightVectorAtDistance(Distance);
			const FVector Up = Track.GetWorldUpVectorAtDistance(Distance);

			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				const FVector Offset = LocalToWorld.TransformPosition(WallVertices[Ring * 4 + Corner].Position) - Centre;
				const double ExpectedLateral = Corner < 2 ? -HalfWidth : HalfWidth;
				const double ExpectedHeight = Corner % 2 == 0 ? FootHeight : TopHeight;

				WorstLateralError = FMath::Max(
					WorstLateralError, FMath::Abs(FVector::DotProduct(Offset, Right) - ExpectedLateral));
				WorstHeightError = FMath::Max(
					WorstHeightError, FMath::Abs(FVector::DotProduct(Offset, Up) - ExpectedHeight));
			}
		}

		double WidestSeamGap = 0.0;

		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			WidestSeamGap = FMath::Max(
				WidestSeamGap,
				FVector::Distance(
					LocalToWorld.TransformPosition(WallVertices[Corner].Position),
					LocalToWorld.TransformPosition(WallVertices[SegmentCount * 4 + Corner].Position)));
		}

		Test.AddInfo(FString::Printf(
			TEXT("%s: %d wall rings, worst %.4f cm off the track edge, worst %.4f cm off the foot or top height, "
				"%.4f cm between the first and last ring"),
			LayoutName,
			RingCount,
			WorstLateralError,
			WorstHeightError,
			WidestSeamGap));

		Test.TestTrue(
			FString::Printf(TEXT("%s: every wall vertex is at plus or minus half the track width"), LayoutName),
			WorstLateralError <= WallPlacementTolerance);

		Test.TestTrue(
			FString::Printf(TEXT("%s: every wall foot is on the road and every top is a wall height above it"),
				LayoutName),
			WorstHeightError <= WallPlacementTolerance);

		Test.TestTrue(
			FString::Printf(TEXT("%s: the last ring closes onto the first, so there is no gap at the lap seam"),
				LayoutName),
			WidestSeamGap <= WallPlacementTolerance);

		Test.TestEqual(
			FString::Printf(TEXT("%s: two triangles per side per segment, none dropped as degenerate"), LayoutName),
			Section->ProcIndexBuffer.Num(),
			SegmentCount * 12);

		const FProcMeshSection* RoadSection = Track.GetRoad()->GetProcMeshSection(0);

		if (RoadSection == nullptr || RoadSection->ProcIndexBuffer.Num() < 3)
		{
			Test.AddError(FString::Printf(TEXT("%s: the road has no triangles to compare winding with."), LayoutName));
			return;
		}

		const FTransform RoadToWorld = Track.GetRoad()->GetComponentTransform();
		const TArray<FProcMeshVertex>& RoadVertices = RoadSection->ProcVertexBuffer;
		const TArray<uint32>& RoadIndices = RoadSection->ProcIndexBuffer;

		auto WorldNormal = [](const FVector& A, const FVector& B, const FVector& C)
		{
			return FVector::CrossProduct(B - A, C - A);
		};

		const double RoadFacing = FMath::Sign(FVector::DotProduct(
			WorldNormal(
				RoadToWorld.TransformPosition(RoadVertices[RoadIndices[0]].Position),
				RoadToWorld.TransformPosition(RoadVertices[RoadIndices[1]].Position),
				RoadToWorld.TransformPosition(RoadVertices[RoadIndices[2]].Position)),
			Track.GetWorldUpVectorAtDistance(0.0)));

		const TArray<uint32>& WallIndices = Section->ProcIndexBuffer;
		int32 FacingTheTrack = 0;

		for (int32 Triangle = 0; Triangle * 3 < WallIndices.Num(); ++Triangle)
		{
			const int32 Segment = Triangle / 4;
			const double TowardsTheTrack = (Triangle / 2) % 2 == 0 ? 1.0 : -1.0;
			const FVector Right = Track.GetWorldRightVectorAtDistance(Length * (Segment + 0.5) / SegmentCount);

			const FVector Normal = WorldNormal(
				LocalToWorld.TransformPosition(WallVertices[WallIndices[Triangle * 3]].Position),
				LocalToWorld.TransformPosition(WallVertices[WallIndices[Triangle * 3 + 1]].Position),
				LocalToWorld.TransformPosition(WallVertices[WallIndices[Triangle * 3 + 2]].Position));

			if (FMath::Sign(FVector::DotProduct(Normal, Right * TowardsTheTrack)) == RoadFacing)
			{
				++FacingTheTrack;
			}
		}

		Test.TestEqual(
			FString::Printf(
				TEXT("%s: every wall triangle faces the track the way the road's triangles face up, so the one-sided "
					"physics collision stops the car at both walls"),
				LayoutName),
			FacingTheTrack,
			WallIndices.Num() / 3);
	}

	double LocalCentrelineRadius(const ARacetrackCircuit& Track, double Distance)
	{
		const double Turn = FMath::Acos(FMath::Clamp(FVector::DotProduct(
			Track.GetWorldDirectionAtDistance(Distance - LocalRadiusHalfSpan),
			Track.GetWorldDirectionAtDistance(Distance + LocalRadiusHalfSpan)), -1.0, 1.0));

		return Turn > UE_DOUBLE_SMALL_NUMBER ? 2.0 * LocalRadiusHalfSpan / Turn : TNumericLimits<double>::Max();
	}

	void ReportWhereTheEdgeFolds(FAutomationTestBase& Test, const ARacetrackCircuit& Track, const TCHAR* LayoutName)
	{
		const double Length = Track.GetTrackLength();
		const double HalfWidth = Track.GetTrackWidth() * 0.5;

		double TightestRadius = TNumericLimits<double>::Max();
		double TightestAt = 0.0;
		int32 FoldedSamples = 0;
		int32 Samples = 0;

		for (double Distance = 0.0; Distance < Length; Distance += FoldScanStep)
		{
			const double Radius = LocalCentrelineRadius(Track, Distance);
			++Samples;

			if (Radius < TightestRadius)
			{
				TightestRadius = Radius;
				TightestAt = Distance;
			}

			if (Radius < HalfWidth)
			{
				++FoldedSamples;
			}
		}

		Test.AddInfo(FString::Printf(
			TEXT("%s: scanned every %.0f cm round the %.0f cm lap, the tightest centreline radius is %.0f cm at %.0f "
				"cm, and %d of %d samples are tighter than the %.0f cm half width, where the inner edge of the road "
				"and its wall fold back on themselves"),
			LayoutName,
			FoldScanStep,
			Length,
			TightestRadius,
			TightestAt,
			FoldedSamples,
			Samples,
			HalfWidth));
	}

	void TestSweepsStopAtTheWalls(
		FAutomationTestBase& Test,
		const UWorld& World,
		const ARacetrackCircuit& Track,
		const TCHAR* LayoutName)
	{
		const UProceduralMeshComponent* Walls = Track.GetEdgeWalls();
		const double Length = Track.GetTrackLength();
		const double HalfWidth = Track.GetTrackWidth() * 0.5;
		const FCollisionShape WheelSphere = FCollisionShape::MakeSphere(WheelSizedSweepRadius);

		TArray<double> Distances = RoadSampleDistances(Length);
		Distances.Add(Length - Track.GetRoadSegmentLength() * 0.5);

		int32 Sweeps = 0;
		int32 StoppedByTheWalls = 0;
		int32 SweepsWhereTheEdgeIsUnfolded = 0;
		int32 StoppedAtThePaintedEdge = 0;
		double SphereNearestTheEdge = 0.0;
		double SphereFurthestFromTheEdge = HalfWidth;

		for (const double Distance : Distances)
		{
			const FVector Right = Track.GetWorldRightVectorAtDistance(Distance);
			const FVector Up = Track.GetWorldUpVectorAtDistance(Distance);
			const FVector Start = Track.GetWorldPositionAtDistance(Distance, 0.0)
				+ Up * (Track.GetRoadHeightAboveSpline() + WheelSizedSweepRadius + WheelClearanceAboveRoad);
			const double Radius = LocalCentrelineRadius(Track, Distance);
			const bool bEdgeFolds = Radius < HalfWidth;

			for (const double Side : { -1.0, 1.0 })
			{
				const FVector End = Start + Right * (Side * (HalfWidth + SweepPastTheEdge));
				FHitResult Hit;
				++Sweeps;

				const bool bStoppedByTheWalls = World.SweepSingleByChannel(
					Hit, Start, End, FQuat::Identity, ECC_Vehicle, WheelSphere) && Hit.GetComponent() == Walls;

				const double SphereOuterEdge =
					FVector::DotProduct(Hit.Location - Start, Right) * Side + WheelSizedSweepRadius;

				if (bStoppedByTheWalls)
				{
					++StoppedByTheWalls;
					SphereNearestTheEdge = FMath::Max(SphereNearestTheEdge, SphereOuterEdge);
					SphereFurthestFromTheEdge = FMath::Min(SphereFurthestFromTheEdge, SphereOuterEdge);
				}

				if (!bEdgeFolds)
				{
					++SweepsWhereTheEdgeIsUnfolded;

					if (bStoppedByTheWalls && SphereOuterEdge <= HalfWidth + PolygonalEdgeAllowance)
					{
						++StoppedAtThePaintedEdge;
					}
				}

				if (!bStoppedByTheWalls || bEdgeFolds || SphereOuterEdge > HalfWidth + PolygonalEdgeAllowance)
				{
					Test.AddInfo(FString::Printf(
						TEXT("%s: the sweep at %.1f cm towards the %s %s, the sphere's outer edge %.2f cm from the "
							"centre, local centreline radius %.0f cm%s"),
						LayoutName,
						Distance,
						Side < 0.0 ? TEXT("left") : TEXT("right"),
						bStoppedByTheWalls ? TEXT("stopped on the walls") : TEXT("did not stop on the walls"),
						SphereOuterEdge,
						Radius,
						bEdgeFolds ? TEXT(", tighter than the half width, so the inner edge folds here") : TEXT("")));
				}
			}
		}

		Test.AddInfo(FString::Printf(
			TEXT("%s: %d of %d wheel-sized sweeps outward from the centreline stopped on the walls, the sphere's "
				"outer edge between %.2f and %.2f cm from the centre against a %.2f cm half width; %d of the %d "
				"where the edge does not fold stopped within %.1f cm of it"),
			LayoutName,
			StoppedByTheWalls,
			Sweeps,
			SphereFurthestFromTheEdge,
			SphereNearestTheEdge,
			HalfWidth,
			StoppedAtThePaintedEdge,
			SweepsWhereTheEdgeIsUnfolded,
			PolygonalEdgeAllowance));

		Test.TestEqual(
			FString::Printf(
				TEXT("%s: every wheel-sized sweep on the Vehicle channel is stopped by the walls, on both sides and "
					"across the lap seam"),
				LayoutName),
			StoppedByTheWalls,
			Sweeps);

		Test.TestEqual(
			FString::Printf(
				TEXT("%s: wherever the edge does not fold, the sweep stops at the painted edge, allowing for the "
					"straight chords between rings"),
				LayoutName),
			StoppedAtThePaintedEdge,
			SweepsWhereTheEdgeIsUnfolded);

		ReportWhereTheEdgeFolds(Test, Track, LayoutName);
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

	const FVector OnTheSpline = Track->GetWorldPositionAtDistance(0.0, 0.0);
	const FVector Up = Track->GetWorldUpVectorAtDistance(0.0);

	TimeSectionCook(
		*this, *Track, *Track->GetRoad(), TEXT("road"), OnTheSpline + Up * TraceReach, OnTheSpline - Up * TraceReach);

	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVehicleWallCollisionCook,
	"RubberAccumDemo.Performance.WallCollisionCook",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVehicleWallCollisionCook::RunTest(const FString& Parameters)
{
	using namespace RubberAccumDemoVehicleTest;

	UWorld* World = FindVehicleTestWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No world available."));
		return false;
	}

	ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>(ClearOfTheHarnessTrack, FRotator::ZeroRotator);

	if (Track == nullptr || Track->GetEdgeWalls() == nullptr)
	{
		AddError(TEXT("Failed to spawn a circuit with edge walls."));
		return false;
	}

	Track->SetLayout(ETrackLayout::Spa, DemoLapLength);

	const FVector Up = Track->GetWorldUpVectorAtDistance(0.0);
	const FVector AcrossTheTrack = Track->GetWorldRightVectorAtDistance(0.0) * (Track->GetTrackWidth() + SweepPastTheEdge);
	const FVector HalfWayUpTheWall =
		Track->GetWorldPositionAtDistance(0.0, 0.0) + Up * (Track->GetEdgeWallHeight() * 0.5);

	TimeSectionCook(
		*this, *Track, *Track->GetEdgeWalls(), TEXT("edge walls"), HalfWayUpTheWall, HalfWayUpTheWall + AcrossTheTrack);

	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVehicleWallsStandOnTheTrackEdges,
	"RubberAccumDemo.Vehicle.WallsStandOnTheTrackEdges",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVehicleWallsStandOnTheTrackEdges::RunTest(const FString& Parameters)
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

	TestWallsStandOnTheEdges(*this, *Track, TEXT("Stadium"));

	Track->SetLayout(ETrackLayout::Spa, DemoLapLength);

	TestWallsStandOnTheEdges(*this, *Track, TEXT("Spa"));

	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVehicleWallsBlockASweepAcrossTheTrack,
	"RubberAccumDemo.Vehicle.WallsBlockASweepAcrossTheTrack",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVehicleWallsBlockASweepAcrossTheTrack::RunTest(const FString& Parameters)
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

	TestSweepsStopAtTheWalls(*this, *World, *Track, TEXT("Stadium"));

	Track->SetLayout(ETrackLayout::Spa, DemoLapLength);

	TestSweepsStopAtTheWalls(*this, *World, *Track, TEXT("Spa"));

	Track->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVehicleWallsStopOnlyVehicles,
	"RubberAccumDemo.Vehicle.WallsStopOnlyVehicles",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVehicleWallsStopOnlyVehicles::RunTest(const FString& Parameters)
{
	using namespace RubberAccumDemoVehicleTest;

	UWorld* World = FindVehicleTestWorld();

	if (World == nullptr)
	{
		AddError(TEXT("No world available."));
		return false;
	}

	ARacetrackCircuit* Track = World->SpawnActor<ARacetrackCircuit>(ClearOfTheHarnessTrack, FRotator::ZeroRotator);

	if (Track == nullptr || Track->GetEdgeWalls() == nullptr)
	{
		AddError(TEXT("Failed to spawn a circuit with edge walls."));
		return false;
	}

	Track->SetLayout(ETrackLayout::Spa, DemoLapLength);

	const UProceduralMeshComponent* Walls = Track->GetEdgeWalls();

	TestFalse(TEXT("The walls are invisible"), Walls->GetVisibleFlag());

	TestTrue(TEXT("The walls block the Vehicle channel, so the car's body stops at them"),
		Walls->GetCollisionResponseToChannel(ECC_Vehicle) == ECR_Block);

	TestTrue(TEXT("The walls ignore the Pawn channel, so the fly camera passes through them"),
		Walls->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore);

	TestTrue(TEXT("The walls ignore the Camera channel, so the car's spring arm does not pull in at the edge"),
		Walls->GetCollisionResponseToChannel(ECC_Camera) == ECR_Ignore);

	TestTrue(TEXT("The walls ignore the WorldDynamic channel, so the wheels' suspension traces do not land on them"),
		Walls->GetCollisionResponseToChannel(ECC_WorldDynamic) == ECR_Ignore);

	TestTrue(TEXT("The walls ignore the Visibility channel, so nothing that looks for visible surfaces finds them"),
		Walls->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Ignore);

	FActorSpawnParameters AlwaysSpawn;
	AlwaysSpawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const double Distance = Track->GetTrackLength() * 0.25;
	const FVector Up = Track->GetWorldUpVectorAtDistance(Distance);
	const FVector HalfWayUpTheWall = Up * (Track->GetRoadHeightAboveSpline() + Track->GetEdgeWallHeight() * 0.5);
	const FVector Inside = Track->GetWorldPositionAtDistance(Distance, 0.5) + HalfWayUpTheWall;
	const FVector Outside = Inside
		+ Track->GetWorldRightVectorAtDistance(Distance) * (Track->GetTrackWidth() * 0.25 + SweepPastTheEdge);

	ADefaultPawn* FlyCamera = World->SpawnActor<ADefaultPawn>(Inside, FRotator::ZeroRotator, AlwaysSpawn);

	if (FlyCamera == nullptr)
	{
		AddError(TEXT("Failed to spawn a fly camera."));
		Track->Destroy();
		return false;
	}

	FHitResult FlyCameraHit;
	FlyCamera->SetActorLocation(Outside, true, &FlyCameraHit);

	TestFalse(TEXT("The fly camera's sweep across the track edge is not blocked by the wall"),
		FlyCameraHit.bBlockingHit);

	TestTrue(TEXT("The fly camera ends up outside the track, where it was sent"),
		FlyCamera->GetActorLocation().Equals(Outside, 0.1));

	FlyCamera->Destroy();
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
