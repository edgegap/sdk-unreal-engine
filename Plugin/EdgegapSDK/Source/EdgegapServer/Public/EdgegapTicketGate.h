// Edgegap SDK (server): only players the matchmaker sent here may join.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Generated/EdgegapMMModels.h"
#include "EdgegapTicketGate.generated.h"

class AController;
class AGameModeBase;
class APlayerController;
class UNetConnection;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEdgegapOnJoinRejected, const FString&, Address, const FString&, TicketId, const FString&, Reason);

/**
 * Ticket check for dedicated servers started by an Edgegap matchmaker.
 *
 * Clients join with "?edgegap_mm_ticket_id=<their ticket>" on the travel URL (the Edgegap client
 * module adds it). The server accepts a ticket only if the matchmaker assigned it to this server
 * (MM_TICKET_IDS, plus tickets added later with AllowTicket, e.g. from backfill), and only for one
 * connection at a time: a ticket stays in use while the connection that joined with it is open,
 * even if the game replaces that player's controller.
 *
 * Works with any GameMode: players with a bad ticket are kicked right after login. Use
 * AEdgegapGameMode / AEdgegapGameModeBase (or call CheckJoin from your PreLogin) to refuse them
 * before login instead, so they never spawn. Off when MM_TICKET_IDS is not set (local tests,
 * servers not started by a matchmaker) or when "Require Matchmaker Ticket" is off in settings.
 */
UCLASS()
class EDGEGAPSERVER_API UEdgegapTicketGate : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Travel URL option the client sends. */
	static const TCHAR* TicketOption;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UEdgegapTicketGate* Get(const UObject* WorldContextObject);

	/** True when joins are being checked. */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Server|Tickets")
	bool IsEnforcing() const;

	/** Let a ticket join (backfill, or a ticket you validated another way). */
	UFUNCTION(BlueprintCallable, Category = "Edgegap|Server|Tickets")
	void AllowTicket(const FString& TicketId);

	/** Allow every ticket of a backfill (call after Create/Get Backfill reports new players). */
	UFUNCTION(BlueprintCallable, Category = "Edgegap|Server|Tickets")
	void AllowBackfillTickets(const FEdgegapMMReadBackfillResponse& Backfill);

	/** Remove a ticket; a player already in keeps playing. */
	UFUNCTION(BlueprintCallable, Category = "Edgegap|Server|Tickets")
	void RevokeTicket(const FString& TicketId);

	UFUNCTION(BlueprintPure, Category = "Edgegap|Server|Tickets")
	TArray<FString> GetAllowedTickets() const;

	/** The ticket a connected player joined with (empty for the local host or before login). */
	UFUNCTION(BlueprintPure, Category = "Edgegap|Server|Tickets")
	FString GetPlayerTicket(const APlayerController* Player) const;

	/** Turn checking on or off at runtime (e.g. on for a test without a matchmaker). */
	UFUNCTION(BlueprintCallable, Category = "Edgegap|Server|Tickets")
	void SetEnforcing(bool bEnforce);

	/** Fired for each refused join. */
	UPROPERTY(BlueprintAssignable, Category = "Edgegap|Server|Tickets")
	FEdgegapOnJoinRejected OnJoinRejected;

	/**
	 * PreLogin check. Returns false with a reason ("403: ...") when the join must be refused.
	 * TicketId receives the ticket parsed from Options.
	 */
	bool CheckJoin(const FString& Options, const FString& Address, FString& TicketId, FString& OutError);

private:
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* Player);
	void HandleLogout(AGameModeBase* GameMode, AController* Exiting);
	void HoldTicket(const FString& TicketId, UNetConnection* Connection);
	/** Frees the tickets of Closing and of every connection that has closed since. */
	void ReleaseTickets(const UNetConnection* Closing);
	static bool IsOpen(const TWeakObjectPtr<UNetConnection>& Connection);

	friend class FEdgegapTicketGateInUseTest;

	TSet<FString> Allowed;
	/** Ticket -> the connection that joined with it (not its controller, which games may replace). */
	TMap<FString, TWeakObjectPtr<UNetConnection>> InUse;
	bool bEnforce = false;
	FDelegateHandle PostLoginHandle;
	FDelegateHandle LogoutHandle;
};
