// Edgegap SDK: ping beacon latency measurement.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "EdgegapCore.h"
#include "EdgegapBeacons.generated.h"

/** One beacon's result. */
USTRUCT(BlueprintType)
struct EDGEGAPCLIENT_API FEdgegapBeaconResult
{
	GENERATED_BODY()

	/** Location key used by matchmaker latency rules (the beacon's city). */
	UPROPERTY(BlueprintReadOnly, Category = "Edgegap|Beacons")
	FString City;

	UPROPERTY(BlueprintReadOnly, Category = "Edgegap|Beacons")
	FString Country;

	UPROPERTY(BlueprintReadOnly, Category = "Edgegap|Beacons")
	FString PublicIp;

	/** Average round trip in ms; -1 when the beacon never answered. */
	UPROPERTY(BlueprintReadOnly, Category = "Edgegap|Beacons")
	double LatencyMs = -1.0;

	/** "UDP" or "TCP" (TCP is used when the UDP echo got no answer). */
	UPROPERTY(BlueprintReadOnly, Category = "Edgegap|Beacons")
	FString Protocol;
};

namespace EdgegapBeacons
{
	/**
	 * Lists the matchmaker's beacons and pings them all in parallel off the game thread.
	 * Latencies maps city -> ms (best beacon per city), ready for a "latencies" rule.
	 */
	EDGEGAPCLIENT_API void Measure(TFunction<void(const TMap<FString, double>& Latencies, const TArray<FEdgegapBeaconResult>& Results, const FEdgegapError& Error)> Done);
}

/** Measure Beacons output. */
USTRUCT(BlueprintType)
struct EDGEGAPCLIENT_API FEdgegapBeaconMeasurement
{
	GENERATED_BODY()

	/** City -> ms, ready for a "latencies" attribute. */
	UPROPERTY(BlueprintReadOnly, Category = "Edgegap|Beacons")
	TMap<FString, double> Latencies;

	UPROPERTY(BlueprintReadOnly, Category = "Edgegap|Beacons")
	TArray<FEdgegapBeaconResult> Results;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEdgegapOnBeaconsMeasured, const FEdgegapBeaconMeasurement&, Measurement, const FEdgegapError&, Error);

/** Blueprint: Measure Beacons. */
UCLASS()
class EDGEGAPCLIENT_API UEdgegapMeasureBeaconsNode : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Ping every matchmaker beacon (UDP echo, TCP fallback; see Project Settings > Edgegap > Beacons). */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Measure Beacons"), Category = "Edgegap|Matchmaker")
	static UEdgegapMeasureBeaconsNode* MeasureBeacons();

	UPROPERTY(BlueprintAssignable)
	FEdgegapOnBeaconsMeasured OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FEdgegapOnBeaconsMeasured OnFailure;

	virtual void Activate() override;
};
