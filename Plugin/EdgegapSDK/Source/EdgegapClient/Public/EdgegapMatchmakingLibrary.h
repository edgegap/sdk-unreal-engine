// Edgegap SDK: matchmaking helpers that do not depend on a matchmaker's spec.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EdgegapCore.h"
#include "Generated/EdgegapMMModels.h"
#include "EdgegapMatchmakingLibrary.generated.h"

class APlayerController;

UENUM(BlueprintType)
enum class EEdgegapAttributeType : uint8
{
	String,
	Number,
	Boolean,
	/** For "intersection" rules. */
	StringArray,
	/** For "latencies" rules: city -> ms. */
	NumberMap,
};

/**
 * One matchmaking attribute without writing JSON. The generated per-profile structs
 * (Find Match (<profile>)) are the typed option; these are for code that must work with
 * any matchmaker configuration.
 */
USTRUCT(BlueprintType)
struct EDGEGAPCLIENT_API FEdgegapAttribute
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edgegap|Matchmaker")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edgegap|Matchmaker")
	EEdgegapAttributeType Type = EEdgegapAttributeType::String;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edgegap|Matchmaker")
	FString StringValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edgegap|Matchmaker")
	double NumberValue = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edgegap|Matchmaker")
	bool bBooleanValue = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edgegap|Matchmaker")
	TArray<FString> StringArrayValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edgegap|Matchmaker")
	TMap<FString, double> NumberMapValue;
};

UCLASS()
class EDGEGAPCLIENT_API UEdgegapMatchmakingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ---- Attributes

	UFUNCTION(BlueprintPure, Category = "Edgegap|Matchmaker|Attributes", meta = (DisplayName = "Make String Attribute"))
	static FEdgegapAttribute MakeStringAttribute(const FString& Name, const FString& Value);

	UFUNCTION(BlueprintPure, Category = "Edgegap|Matchmaker|Attributes", meta = (DisplayName = "Make Number Attribute"))
	static FEdgegapAttribute MakeNumberAttribute(const FString& Name, double Value);

	UFUNCTION(BlueprintPure, Category = "Edgegap|Matchmaker|Attributes", meta = (DisplayName = "Make Boolean Attribute"))
	static FEdgegapAttribute MakeBooleanAttribute(const FString& Name, bool Value);

	UFUNCTION(BlueprintPure, Category = "Edgegap|Matchmaker|Attributes", meta = (DisplayName = "Make String Array Attribute"))
	static FEdgegapAttribute MakeStringArrayAttribute(const FString& Name, const TArray<FString>& Values);

	/** Beacon latencies (city -> ms) for a "latencies" rule; leave empty to let Find Match measure them. */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Matchmaker|Attributes", meta = (DisplayName = "Make Beacon Latencies Attribute", AutoCreateRefTerm = "Latencies"))
	static FEdgegapAttribute MakeLatenciesAttribute(const TMap<FString, double>& Latencies, const FString& Name = TEXT("beacons"));

	/** Attributes object as JSON, for Find Match or the generic Create Group / Create Ticket nodes. False on an empty or duplicate name. */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Matchmaker|Attributes", meta = (DisplayName = "Attributes To JSON"))
	static bool AttributesToJson(const TArray<FEdgegapAttribute>& Attributes, FString& Json, FString& ErrorMessage);

	// ---- Connection

	/** Override a service's URL and/or credential at runtime (empty keeps the configured value). */
	UFUNCTION(BlueprintCallable, Category = "Edgegap")
	static void SetConnection(EEdgegapService Service, const FString& Url, const FString& Credential);

	// ---- Travel

	/** "ip:port" for an assignment's port (empty PortName = the setting's GamePortName, or the only port). */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Matchmaker|Travel")
	static FString MakeAssignmentAddress(const FEdgegapMMTicketAssignmentResponse& Assignment, const FString& PortName);

	/**
	 * Travel URL for a match: "ip:port?edgegap_mm_ticket_id=<ticket>". Servers using the Edgegap
	 * server module refuse players without the ticket the matchmaker assigned to them.
	 */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Matchmaker|Travel")
	static FString MakeJoinUrl(const FEdgegapMMTicketAssignmentResponse& Assignment, const FString& TicketId);

	/** ClientTravel one player to an address or join URL. */
	UFUNCTION(BlueprintCallable, Category = "Edgegap|Matchmaker|Travel")
	static bool TravelPlayerToMatch(APlayerController* PlayerController, const FString& Address);

	/** ClientTravel one player to an assignment with their ticket (from Get Group Member / Get Ticket). */
	UFUNCTION(BlueprintCallable, Category = "Edgegap|Matchmaker|Travel")
	static bool TravelPlayerToMatchAssignment(APlayerController* PlayerController, const FEdgegapMMTicketAssignmentResponse& Assignment, const FString& TicketId);

	/**
	 * Move a party that is together on a listen server (the leader hosts) to the match: remote
	 * members travel first, then the leader after LeaderDelaySeconds so their connections are not
	 * dropped by the host leaving. On a client this travels only the local player.
	 * With server ticket checks on, each member must join with their own ticket: give each one its
	 * Make Join Url instead (every member's Get Group Member returns its ticket_id).
	 */
	UFUNCTION(BlueprintCallable, Category = "Edgegap|Matchmaker|Travel", meta = (WorldContext = "WorldContextObject"))
	static int32 TravelPartyToMatch(const UObject* WorldContextObject, const FString& Address, float LeaderDelaySeconds = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Edgegap|Matchmaker|Travel", meta = (WorldContext = "WorldContextObject"))
	static int32 TravelPartyToMatchAssignment(const UObject* WorldContextObject, const FEdgegapMMTicketAssignmentResponse& Assignment, float LeaderDelaySeconds = 1.0f);

	/** Server: send every connected player (and a listen-server host, last) to the assignment. Returns how many were sent. */
	UFUNCTION(BlueprintCallable, Category = "Edgegap|Matchmaker|Travel", meta = (WorldContext = "WorldContextObject"))
	static int32 TravelAllConnectedPlayersToMatchAssignment(const UObject* WorldContextObject, const FEdgegapMMTicketAssignmentResponse& Assignment, float HostDelaySeconds = 1.0f);

	// ---- Debug strings

	UFUNCTION(BlueprintPure, Category = "Edgegap", meta = (DisplayName = "To String (Edgegap Error)", CompactNodeTitle = "->", BlueprintAutocast))
	static FString Conv_EdgegapErrorToString(const FEdgegapError& Error);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEdgegapOnValidated, const TArray<FString>&, Problems);

/**
 * Check attributes against the live matchmaker's spec before queuing: unknown profile, missing
 * required attributes, unknown names and wrong types. Find Match also runs this (log only) in
 * non-shipping builds.
 */
UCLASS()
class EDGEGAPCLIENT_API UEdgegapValidateAttributesNode : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Validate Matchmaking Attributes"), Category = "Edgegap|Matchmaker")
	static UEdgegapValidateAttributesNode* ValidateAttributes(const FString& Profile, const FString& AttributesJson);

	/** No problems found. */
	UPROPERTY(BlueprintAssignable)
	FEdgegapOnValidated OnValid;

	/** Problems found, or the spec could not be read (then Problems says so). */
	UPROPERTY(BlueprintAssignable)
	FEdgegapOnValidated OnInvalid;

	virtual void Activate() override;

	/** C++: Done receives the problems (empty = valid). Spec is cached per matchmaker URL for 60 s. */
	static void Validate(const FString& Profile, const FString& AttributesJson, TFunction<void(const TArray<FString>&)> Done);

private:
	FString Profile;
	FString AttributesJson;
};
