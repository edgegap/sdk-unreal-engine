// Edgegap SDK (server): game modes that refuse joins without a valid matchmaker ticket before login.

#include "EdgegapGameMode.h"
#include "EdgegapTicketGate.h"
#include "Kismet/GameplayStatics.h"

void EdgegapGameMode::CheckPreLogin(AGameModeBase* GameMode, const FString& Options, const FString& Address, FString& ErrorMessage)
{
	if (!ErrorMessage.IsEmpty()) { return; }
	if (UEdgegapTicketGate* Gate = UEdgegapTicketGate::Get(GameMode))
	{
		FString TicketId;
		Gate->CheckJoin(Options, Address, TicketId, ErrorMessage);
	}
}

void AEdgegapGameModeBase::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	EdgegapGameMode::CheckPreLogin(this, Options, Address, ErrorMessage);
	if (ErrorMessage.IsEmpty()) { ValidateJoin(Options, Address, UGameplayStatics::ParseOption(Options, UEdgegapTicketGate::TicketOption), ErrorMessage); }
}

void AEdgegapGameModeBase::ValidateJoin_Implementation(const FString& Options, const FString& Address, const FString& TicketId, FString& ErrorMessage)
{
}

void AEdgegapGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	EdgegapGameMode::CheckPreLogin(this, Options, Address, ErrorMessage);
	if (ErrorMessage.IsEmpty()) { ValidateJoin(Options, Address, UGameplayStatics::ParseOption(Options, UEdgegapTicketGate::TicketOption), ErrorMessage); }
}

void AEdgegapGameMode::ValidateJoin_Implementation(const FString& Options, const FString& Address, const FString& TicketId, FString& ErrorMessage)
{
}
