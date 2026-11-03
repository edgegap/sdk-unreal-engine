// Edgegap SDK: "Find Match" flow (hand-written, built on the generated matchmaker API).

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Generated/EdgegapMMApi.h"
#include "EdgegapMatchmakerSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEdgegapMMOnStatus, const FString&, Status);
/** JoinUrl = "ip:port?edgegap_mm_ticket_id=<ticket>": pass it to ClientTravel / open. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEdgegapMMOnMatchFound, const FEdgegapMMTicketAssignmentResponse&, Assignment, const FString&, JoinUrl, const FString&, TicketId);

/**
 * Finds a match for the local player: measures beacon latencies, creates a solo Group Up
 * group, polls it until the matchmaker assigns a server, then (optionally) travels there.
 *
 * Command line (handy for tests): -EdgegapFindMatch=<profile> starts a search at launch,
 * -EdgegapNoTravel skips ClientTravel, -EdgegapQuitOnMatch exits once a server is assigned.
 * Console: EdgegapMM.FindMatch <profile> [attributes JSON], EdgegapMM.Cancel
 */
UCLASS()
class EDGEGAPCLIENT_API UEdgegapMatchmakerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Start searching. AttributesJson is the profile's attributes object (use the generated
	 * Find Match (<profile>) nodes for typed attributes). Each name in MeasureLatencyInto that is
	 * missing or empty gets a { city: milliseconds } map measured from the matchmaker's beacons.
	 */
	UFUNCTION(BlueprintCallable, Category = "Edgegap|Matchmaker")
	void FindMatch(const FString& Profile, const FString& AttributesJson, const TArray<FString>& MeasureLatencyInto);

	/** Leave the queue (deletes the group). */
	UFUNCTION(BlueprintCallable, Category = "Edgegap|Matchmaker")
	void CancelMatch();

	UFUNCTION(BlueprintPure, Category = "Edgegap|Matchmaker")
	bool IsSearching() const { return bSearching; }

	/** "ip:external port" of the settings' GamePortName, or of the only port when there is one. */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Matchmaker")
	static FString GetConnectAddress(const FEdgegapMMTicketAssignmentResponse& Assignment);

	/** Searching, measuring beacons, waiting for a server... */
	UPROPERTY(BlueprintAssignable, Category = "Edgegap|Matchmaker")
	FEdgegapMMOnStatus OnStatus;

	UPROPERTY(BlueprintAssignable, Category = "Edgegap|Matchmaker")
	FEdgegapMMOnMatchFound OnMatchFound;

	UPROPERTY(BlueprintAssignable, Category = "Edgegap|Matchmaker")
	FEdgegapOnDone OnFailed;

	/** Last measured beacon latencies (city -> ms). */
	UPROPERTY(BlueprintReadOnly, Category = "Edgegap|Matchmaker")
	TMap<FString, double> BeaconLatencies;

private:
	void MeasureBeacons(int32 Search, TFunction<void(const TMap<FString, double>&)> Done);
	void CreateGroup(int32 Search, const FString& Profile, const TSharedRef<FJsonObject>& Attributes);
	void SchedulePoll(int32 Search);
	void Poll(int32 Search);
	void Succeed(const FEdgegapMMTicketAssignmentResponse& Assignment, const FString& TicketId);
	void Fail(const FEdgegapError& Error);
	void SetStatus(const FString& Status);
	void Reset();

	bool bSearching = false;
	int32 SearchId = 0;       // bumps on every search/cancel so stale callbacks are ignored
	double SearchStarted = 0.0;
	FString GroupId;
	FString MemberId;
	FTSTicker::FDelegateHandle PollHandle;
	TArray<class IConsoleObject*> ConsoleCommands;
};
