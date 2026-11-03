// Edgegap SDK (server): only players the matchmaker sent here may join.

#include "EdgegapTicketGate.h"
#include "EdgegapServerLibrary.h"
#include "EdgegapSettings.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

const TCHAR* UEdgegapTicketGate::TicketOption = TEXT("edgegap_mm_ticket_id");

void UEdgegapTicketGate::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	for (const FString& Ticket : UEdgegapServerLibrary::GetTicketIds()) { Allowed.Add(Ticket); }
	bEnforce = GetDefault<UEdgegapSettings>()->bRequireMatchmakerTicket && Allowed.Num() > 0;
	if (bEnforce)
	{
		UE_LOG(LogEdgegap, Log, TEXT("Ticket check on: %d matchmaker ticket(s) may join."), Allowed.Num());
	}
	PostLoginHandle = FGameModeEvents::GameModePostLoginEvent.AddUObject(this, &UEdgegapTicketGate::HandlePostLogin);
	LogoutHandle = FGameModeEvents::GameModeLogoutEvent.AddUObject(this, &UEdgegapTicketGate::HandleLogout);
}

void UEdgegapTicketGate::Deinitialize()
{
	FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginHandle);
	FGameModeEvents::GameModeLogoutEvent.Remove(LogoutHandle);
	Super::Deinitialize();
}

UEdgegapTicketGate* UEdgegapTicketGate::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UEdgegapTicketGate>() : nullptr;
}

bool UEdgegapTicketGate::IsEnforcing() const { return bEnforce; }
void UEdgegapTicketGate::SetEnforcing(bool bInEnforce) { bEnforce = bInEnforce; }

void UEdgegapTicketGate::AllowTicket(const FString& TicketId)
{
	if (!TicketId.IsEmpty()) { Allowed.Add(TicketId); }
}

void UEdgegapTicketGate::AllowBackfillTickets(const FEdgegapMMReadBackfillResponse& Backfill)
{
	for (const TPair<FString, FEdgegapMMBackfillTicketMember>& Pair : Backfill.Tickets)
	{
		AllowTicket(Pair.Value.Id.IsEmpty() ? Pair.Key : Pair.Value.Id);
	}
}

void UEdgegapTicketGate::RevokeTicket(const FString& TicketId) { Allowed.Remove(TicketId); }
TArray<FString> UEdgegapTicketGate::GetAllowedTickets() const { return Allowed.Array(); }

FString UEdgegapTicketGate::GetPlayerTicket(const APlayerController* Player) const
{
	const UNetConnection* Connection = Player ? Player->GetNetConnection() : nullptr;
	if (!Connection) { return FString(); }
	for (const TPair<FString, TWeakObjectPtr<UNetConnection>>& Pair : InUse)
	{
		if (Pair.Value.Get() == Connection) { return Pair.Key; }
	}
	return FString();
}

bool UEdgegapTicketGate::IsOpen(const TWeakObjectPtr<UNetConnection>& Connection)
{
	return Connection.IsValid() && Connection->GetConnectionState() != USOCK_Closed;
}

void UEdgegapTicketGate::HoldTicket(const FString& TicketId, UNetConnection* Connection)
{
	InUse.Add(TicketId, Connection);
}

void UEdgegapTicketGate::ReleaseTickets(const UNetConnection* Closing)
{
	for (auto It = InUse.CreateIterator(); It; ++It)
	{
		if (It->Value.Get() == Closing || !IsOpen(It->Value)) { It.RemoveCurrent(); }
	}
}

bool UEdgegapTicketGate::CheckJoin(const FString& Options, const FString& Address, FString& TicketId, FString& OutError)
{
	TicketId = UGameplayStatics::ParseOption(Options, TicketOption);
	if (!bEnforce) { return true; }

	FString Reason;
	if (TicketId.IsEmpty())
	{
		Reason = TEXT("no matchmaking ticket");
	}
	else if (!Allowed.Contains(TicketId))  // exact match: a substring of the ID list must not pass
	{
		Reason = TEXT("ticket not assigned to this server");
	}
	else if (const TWeakObjectPtr<UNetConnection>* Holder = InUse.Find(TicketId))
	{
		if (IsOpen(*Holder)) { Reason = TEXT("ticket already in use"); }
	}
	if (Reason.IsEmpty()) { return true; }

	OutError = TEXT("403: Unauthorized (") + Reason + TEXT(")");
	UE_LOG(LogEdgegap, Warning, TEXT("Join refused from %s: %s (ticket '%s')."), *Address, *Reason, *TicketId);
	OnJoinRejected.Broadcast(Address, TicketId, Reason);
	return false;
}

void UEdgegapTicketGate::HandlePostLogin(AGameModeBase* GameMode, APlayerController* Player)
{
	if (!Player || !GameMode || GameMode->GetGameInstance() != GetGameInstance()) { return; }
	UNetConnection* Connection = Player->GetNetConnection();
	if (!Connection || Player->IsLocalController()) { return; }  // listen-server host

	// The login URL keeps the options the client traveled with; same check as PreLogin,
	// which also covers game modes that do not call CheckJoin themselves.
	const FString& Url = Connection->RequestURL;
	int32 OptionsStart = INDEX_NONE;
	const FString Options = Url.FindChar(TEXT('?'), OptionsStart) ? Url.Mid(OptionsStart) : FString();
	FString TicketId, Error;
	if (!CheckJoin(Options, Connection->LowLevelGetRemoteAddress(true), TicketId, Error))
	{
		TWeakObjectPtr<APlayerController> WeakPlayer = Player;
		TWeakObjectPtr<AGameModeBase> WeakMode = GameMode;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPlayer, WeakMode, Error](float)
		{
			if (WeakPlayer.IsValid() && WeakMode.IsValid() && WeakMode->GameSession)
			{
				WeakMode->GameSession->KickPlayer(WeakPlayer.Get(), FText::FromString(Error));
			}
			return false;
		}));
		return;
	}
	if (!TicketId.IsEmpty())
	{
		HoldTicket(TicketId, Connection);
		UE_LOG(LogEdgegap, Log, TEXT("Player joined with ticket %s."), *TicketId);
	}
}

void UEdgegapTicketGate::HandleLogout(AGameModeBase* GameMode, AController* Exiting)
{
	if (GameMode && GameMode->GetGameInstance() != GetGameInstance()) { return; }
	// A disconnecting player's connection is already closed here (the sweep frees its ticket) and
	// its controller no longer points to it. A controller the game replaced (SwapPlayerControllers)
	// still points to its player's open connection, so release by connection only when that
	// connection's current controller is the one leaving (a kick, for example).
	const APlayerController* Player = Cast<APlayerController>(Exiting);
	const UNetConnection* Connection = Player ? Player->GetNetConnection() : nullptr;
	ReleaseTickets(Connection && Connection->PlayerController == Player ? Connection : nullptr);
}
