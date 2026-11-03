// Edgegap SDK: "Find Match" flow.

#include "EdgegapMatchmakerSubsystem.h"
#include "EdgegapBeacons.h"
#include "EdgegapMatchmakingLibrary.h"
#include "EdgegapSettings.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void UEdgegapMatchmakerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("EdgegapMM.FindMatch"), TEXT("EdgegapMM.FindMatch <profile> [attributes JSON]: search, measuring 'beacons'."),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			if (Args.Num() < 1) { UE_LOG(LogEdgegap, Warning, TEXT("Usage: EdgegapMM.FindMatch <profile> [attributes JSON]")); return; }
			TArray<FString> JsonParts(Args);
			JsonParts.RemoveAt(0);
			TArray<FString> Latency;
			Latency.Add(TEXT("beacons"));
			FindMatch(Args[0], FString::Join(JsonParts, TEXT(" ")), Latency);
		}), ECVF_Default));
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("EdgegapMM.Cancel"), TEXT("Leave the matchmaking queue."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]() { CancelMatch(); }), ECVF_Default));

	FString Profile;
	if (FParse::Value(FCommandLine::Get(), TEXT("EdgegapFindMatch="), Profile) && !Profile.IsEmpty())
	{
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this, Profile](float)
		{
			TArray<FString> Latency;
			Latency.Add(TEXT("beacons"));
			FindMatch(Profile, FString(), Latency);
			return false;
		}), 1.0f);
	}
}

void UEdgegapMatchmakerSubsystem::Deinitialize()
{
	if (bSearching) { CancelMatch(); }
	for (IConsoleObject* Command : ConsoleCommands) { IConsoleManager::Get().UnregisterConsoleObject(Command); }
	ConsoleCommands.Reset();
	Super::Deinitialize();
}

void UEdgegapMatchmakerSubsystem::FindMatch(const FString& Profile, const FString& AttributesJson, const TArray<FString>& MeasureLatencyInto)
{
	if (bSearching)
	{
		UE_LOG(LogEdgegap, Warning, TEXT("FindMatch ignored: already searching. Call CancelMatch first."));
		return;
	}

	TSharedPtr<FJsonObject> Attributes = AttributesJson.TrimStartAndEnd().IsEmpty() ? MakeShared<FJsonObject>() : EdgegapJson::ParseObject(AttributesJson);
	if (!Attributes.IsValid())
	{
		Fail(FEdgegapError::Make(0, FString::Printf(TEXT("Attributes are not a JSON object: %s"), *AttributesJson)));
		return;
	}

	bSearching = true;
	const int32 Search = ++SearchId;
	SearchStarted = FPlatformTime::Seconds();

	TArray<FString> Missing;
	for (const FString& Name : MeasureLatencyInto)
	{
		const TSharedPtr<FJsonObject>* Existing = nullptr;
		if (!Attributes->TryGetObjectField(Name, Existing) || !Existing || (*Existing)->Values.Num() == 0) { Missing.Add(Name); }
	}

	const TSharedRef<FJsonObject> AttributesRef = Attributes.ToSharedRef();
	if (Missing.IsEmpty())
	{
		CreateGroup(Search, Profile, AttributesRef);
		return;
	}

	SetStatus(TEXT("Measuring beacons"));
	MeasureBeacons(Search, [this, Search, Profile, AttributesRef, Missing](const TMap<FString, double>& Latencies)
	{
		for (const FString& Name : Missing) { EdgegapJson::Write(*AttributesRef, *Name, Latencies); }
		CreateGroup(Search, Profile, AttributesRef);
	});
}

void UEdgegapMatchmakerSubsystem::MeasureBeacons(int32 Search, TFunction<void(const TMap<FString, double>&)> Done)
{
	TWeakObjectPtr<UEdgegapMatchmakerSubsystem> WeakThis(this);
	EdgegapBeacons::Measure([WeakThis, Search, Done](const TMap<FString, double>& Latencies, const TArray<FEdgegapBeaconResult>&, const FEdgegapError& Error)
	{
		UEdgegapMatchmakerSubsystem* Self = WeakThis.Get();
		if (!Self || Search != Self->SearchId) { return; }
		if (Error.bFailed) { Self->Fail(Error); return; }
		Self->BeaconLatencies = Latencies;
		Done(Latencies);
	});
}

void UEdgegapMatchmakerSubsystem::CreateGroup(int32 Search, const FString& Profile, const TSharedRef<FJsonObject>& Attributes)
{
	SetStatus(TEXT("Joining the queue"));
	const FString AttributesJson = EdgegapJson::ObjectToString(Attributes);

#if !UE_BUILD_SHIPPING
	// Development aid: compare against the live matchmaker spec without delaying the search.
	UEdgegapValidateAttributesNode::Validate(Profile, AttributesJson, [Profile](const TArray<FString>& Problems)
	{
		for (const FString& Problem : Problems) { UE_LOG(LogEdgegap, Warning, TEXT("Profile '%s' attributes: %s"), *Profile, *Problem); }
	});
#endif

	FEdgegapMMCreatePartyTicket Body;
	Body.Profile = Profile;
	Body.Attributes = AttributesJson;
	Body.bIsReady = true;  // a solo group is ready right away; parties use the Group nodes directly

	TWeakObjectPtr<UEdgegapMatchmakerSubsystem> WeakThis(this);
	FEdgegapMMApi::CreateGroup(Body, [WeakThis, Search](const FEdgegapMMReadPartyMemberResponse& Member, const FEdgegapError& Error)
	{
		UEdgegapMatchmakerSubsystem* Self = WeakThis.Get();
		if (!Self) { return; }
		if (Search != Self->SearchId)
		{
			// Cancelled while the request was in flight: do not leave the group queued.
			if (!Error.bFailed) { FEdgegapMMApi::DeleteGroup(Member.GroupId, [](const FEdgegapError&) {}); }
			return;
		}
		if (Error.bFailed) { Self->Fail(Error); return; }
		Self->GroupId = Member.GroupId;
		Self->MemberId = Member.MemberId;
		UE_LOG(LogEdgegap, Log, TEXT("Queued: group %s, member %s"), *Self->GroupId, *Self->MemberId);
		Self->SetStatus(TEXT("Searching"));
		Self->SchedulePoll(Search);
	});
}

void UEdgegapMatchmakerSubsystem::SchedulePoll(int32 Search)
{
	const float Interval = FMath::Max(1.0f, GetDefault<UEdgegapSettings>()->PollIntervalSeconds);
	PollHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this, Search](float)
	{
		PollHandle.Reset();
		Poll(Search);
		return false;
	}), Interval);
}

void UEdgegapMatchmakerSubsystem::Poll(int32 Search)
{
	if (Search != SearchId) { return; }
	const float Timeout = GetDefault<UEdgegapSettings>()->SearchTimeoutSeconds;
	if (Timeout > 0.0f && FPlatformTime::Seconds() - SearchStarted > Timeout)
	{
		CancelMatch();
		Fail(FEdgegapError::Make(0, TEXT("No match found before the search timeout.")));
		return;
	}

	TWeakObjectPtr<UEdgegapMatchmakerSubsystem> WeakThis(this);
	FEdgegapMMApi::GetGroupMember(GroupId, MemberId, [WeakThis, Search](const FEdgegapMMReadPartyMemberResponse& Member, const FEdgegapError& Error)
	{
		UEdgegapMatchmakerSubsystem* Self = WeakThis.Get();
		if (!Self || Search != Self->SearchId) { return; }
		if (Error.bFailed)
		{
			if (Error.HttpStatus == 404) { Self->Fail(FEdgegapError::Make(404, TEXT("The matchmaker removed the ticket: no match before it expired."))); }
			else { Self->Fail(Error); }
			return;
		}
		if (Member.bHasAssignment && !Member.Assignment.PublicIp.IsEmpty())
		{
			Self->Succeed(Member.Assignment, Member.TicketId);
			return;
		}
		switch (Member.Status)
		{
		case EEdgegapMMTicketSteps::Cancelled:
			Self->Fail(FEdgegapError::Make(0, TEXT("The matchmaker cancelled the ticket.")));
			return;
		case EEdgegapMMTicketSteps::MatchFound:
			Self->SetStatus(TEXT("Match found, starting the server"));
			break;
		default:
			Self->SetStatus(TEXT("Searching"));
			break;
		}
		Self->SchedulePoll(Search);
	});
}

FString UEdgegapMatchmakerSubsystem::GetConnectAddress(const FEdgegapMMTicketAssignmentResponse& Assignment)
{
	return UEdgegapMatchmakingLibrary::MakeAssignmentAddress(Assignment, FString());
}

void UEdgegapMatchmakerSubsystem::Succeed(const FEdgegapMMTicketAssignmentResponse& Assignment, const FString& TicketId)
{
	const FString Address = UEdgegapMatchmakingLibrary::MakeJoinUrl(Assignment, TicketId);
	UE_LOG(LogEdgegap, Log, TEXT("MATCH FOUND: %s (%s, %s) group %s"), *Address, *Assignment.Fqdn, *Assignment.Location.City, *GroupId);
	Reset();
	SetStatus(TEXT("Match found"));
	OnMatchFound.Broadcast(Assignment, Address, TicketId);

	if (FParse::Param(FCommandLine::Get(), TEXT("EdgegapQuitOnMatch")))
	{
		RequestEngineExit(TEXT("EdgegapQuitOnMatch"));
		return;
	}
	if (GetDefault<UEdgegapSettings>()->bTravelOnMatch && !FParse::Param(FCommandLine::Get(), TEXT("EdgegapNoTravel")))
	{
		UEdgegapMatchmakingLibrary::TravelPlayerToMatch(GetGameInstance()->GetFirstLocalPlayerController(), Address);
	}
}

void UEdgegapMatchmakerSubsystem::CancelMatch()
{
	if (!GroupId.IsEmpty())
	{
		UE_LOG(LogEdgegap, Log, TEXT("Leaving the queue (group %s)"), *GroupId);
		FEdgegapMMApi::DeleteGroup(GroupId, [](const FEdgegapError&) {});
	}
	++SearchId;
	Reset();
	SetStatus(TEXT("Cancelled"));
}

void UEdgegapMatchmakerSubsystem::Fail(const FEdgegapError& Error)
{
	UE_LOG(LogEdgegap, Warning, TEXT("Matchmaking failed: %s"), *Error.Message);
	++SearchId;
	Reset();
	OnFailed.Broadcast(Error);
}

void UEdgegapMatchmakerSubsystem::SetStatus(const FString& Status)
{
	UE_LOG(LogEdgegap, Verbose, TEXT("Status: %s"), *Status);
	OnStatus.Broadcast(Status);
}

void UEdgegapMatchmakerSubsystem::Reset()
{
	if (PollHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PollHandle);
		PollHandle.Reset();
	}
	bSearching = false;
	GroupId.Reset();
	MemberId.Reset();
}
