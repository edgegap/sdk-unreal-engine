// Edgegap SDK (server): game modes that refuse joins without a valid matchmaker ticket before login.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/GameModeBase.h"
#include "EdgegapGameMode.generated.h"

namespace EdgegapGameMode
{
	/** Shared PreLogin check; for your own game mode, call this after Super::PreLogin. */
	EDGEGAPSERVER_API void CheckPreLogin(AGameModeBase* GameMode, const FString& Options, const FString& Address, FString& ErrorMessage);
}

/** AGameModeBase that checks the Edgegap matchmaker ticket in PreLogin (see UEdgegapTicketGate). */
UCLASS()
class EDGEGAPSERVER_API AEdgegapGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

	/** Extra checks of your own after the ticket check passed; set ErrorMessage to refuse. */
	UFUNCTION(BlueprintNativeEvent, Category = "Edgegap|Server|Tickets")
	void ValidateJoin(const FString& Options, const FString& Address, const FString& TicketId, FString& ErrorMessage);
};

/** AGameMode (match states) that checks the Edgegap matchmaker ticket in PreLogin (see UEdgegapTicketGate). */
UCLASS()
class EDGEGAPSERVER_API AEdgegapGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

	/** Extra checks of your own after the ticket check passed; set ErrorMessage to refuse. */
	UFUNCTION(BlueprintNativeEvent, Category = "Edgegap|Server|Tickets")
	void ValidateJoin(const FString& Options, const FString& Address, const FString& TicketId, FString& ErrorMessage);
};
