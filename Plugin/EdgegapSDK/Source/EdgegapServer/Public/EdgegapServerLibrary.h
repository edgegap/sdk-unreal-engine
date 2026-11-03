// Edgegap SDK: dedicated-server helpers (deployment and matchmaker environment, self-stop).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EdgegapCore.h"
#include "EdgegapServerLibrary.generated.h"

/** IDs grouped by a parent ID (MM_GROUPS: group -> tickets, MM_TEAMS: team -> groups). */
USTRUCT(BlueprintType)
struct EDGEGAPSERVER_API FEdgegapIdList
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Edgegap|Server")
	TArray<FString> Ids;
};

UCLASS()
class EDGEGAPSERVER_API UEdgegapServerLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** True inside an Edgegap deployment (ARBITRIUM_REQUEST_ID is set). */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Server")
	static bool IsRunningOnEdgegap();

	/** True when a matchmaker started this server (MM_MATCH_ID is set). */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Server")
	static bool IsMatchmakerDeployment();

	/** ARBITRIUM_REQUEST_ID: this deployment's ID. */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Server")
	static FString GetDeploymentId();

	/** ARBITRIUM_PORT_<NAME>_EXTERNAL: the public port of a named port (0 when unknown). */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Server")
	static int32 GetExternalPort(const FString& PortName);

	/** ARBITRIUM_PORT_<NAME>_INTERNAL: the port to listen on (0 when unknown). */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Server")
	static int32 GetInternalPort(const FString& PortName);

	/** Any environment variable. */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Server")
	static FString GetEnvironmentVariable(const FString& Name);

	/** MM_MATCH_ID. */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Server|Matchmaker")
	static FString GetMatchId();

	/** MM_MATCH_PROFILE. */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Server|Matchmaker")
	static FString GetMatchProfile();

	/** MM_EXPANSION: expansion stage that produced the match ("initial" or seconds). */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Server|Matchmaker")
	static FString GetExpansionStage();

	/** MM_TICKET_IDS: ticket IDs of the matched players. */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Server|Matchmaker")
	static TArray<FString> GetTicketIds();

	/** MM_TICKET_<id>: that ticket as JSON (player_ip, group_id, team_id, attributes). */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Server|Matchmaker")
	static FString GetTicketJson(const FString& TicketId);

	/** MM_GROUPS: group ID -> its ticket IDs. */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Server|Matchmaker")
	static TMap<FString, FEdgegapIdList> GetGroupTicketMapping();

	/** MM_TEAMS: team ID -> its group IDs. */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Server|Matchmaker")
	static TMap<FString, FEdgegapIdList> GetTeamGroupMapping();

	/** MM_EQUALITY: values shared by the whole match for "string_equality" rules (JSON). */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Server|Matchmaker")
	static FString GetEqualityJson();

	/** MM_INTERSECTION: values common to all players for "intersection" rules (JSON). */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Server|Matchmaker")
	static FString GetIntersectionJson();

	/** Parse a JSON array of strings (e.g. an environment variable). */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Server")
	static TArray<FString> JsonArrayToStringArray(const FString& Json);

	/** Stop this deployment through ARBITRIUM_DELETE_URL (call when the match is over). */
	UFUNCTION(BlueprintCallable, Category = "Edgegap|Server")
	static void SelfStop();

	/** C++: self-stop and block up to TimeoutSeconds for the answer (for exit and crash paths). */
	static bool SelfStopBlocking(float TimeoutSeconds);

	static FString Env(const FString& Name);
};

/**
 * Created only on dedicated servers running on Edgegap. When the setting "Self Stop On Exit" is on,
 * stops the deployment when the process exits or crashes, so Edgegap does not restart or bill it.
 */
UCLASS()
class EDGEGAPSERVER_API UEdgegapServerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void StopOnce(const TCHAR* Reason);

	FDelegateHandle PreExitHandle;
	FDelegateHandle SystemErrorHandle;
	bool bStopped = false;
};
