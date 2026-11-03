// Edgegap SDK: hand-written HTTP core shared by every generated API.

#pragma once

#include "CoreMinimal.h"
#include "EdgegapCompat.h"
#include "EdgegapCore.generated.h"

EDGEGAPSDK_API DECLARE_LOG_CATEGORY_EXTERN(LogEdgegap, Log, All);

/** Which Edgegap service a request goes to. Each has its own URL and credential. */
UENUM(BlueprintType)
enum class EEdgegapService : uint8
{
	/** Your matchmaker (om-xxxx.edgegap.net), matchmaker token. Safe in clients. */
	Matchmaker,
	/** api.edgegap.com, organization API token. Editor, tools and servers only by default. */
	Api,
	/** Your server browser, client token on clients and server token on dedicated servers. */
	ServerBrowser,
};

/** Result of a failed call. HttpStatus is 0 when the request never got an answer (or was refused locally). */
USTRUCT(BlueprintType)
struct EDGEGAPSDK_API FEdgegapError
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Edgegap")
	bool bFailed = false;

	UPROPERTY(BlueprintReadOnly, Category = "Edgegap")
	int32 HttpStatus = 0;

	/** True when Edgegap kept answering 429 after every retry. */
	UPROPERTY(BlueprintReadOnly, Category = "Edgegap")
	bool bRateLimited = false;

	UPROPERTY(BlueprintReadOnly, Category = "Edgegap")
	FString Message;

	static FEdgegapError Make(int32 InStatus, const FString& InMessage)
	{
		FEdgegapError Error;
		Error.bFailed = true;
		Error.HttpStatus = InStatus;
		Error.bRateLimited = InStatus == 429;
		Error.Message = InMessage;
		return Error;
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEdgegapOnDone, const FEdgegapError&, Error);

/** Raw HTTP response handed to generated code. */
struct FEdgegapResponse
{
	int32 Status = 0;
	FString Body;
	FEdgegapError Error;
};

/**
 * HTTP client for all Edgegap services. Resolves each service's URL and credential,
 * and retries rate limits (429, honouring Retry-After) and, except for POST, network
 * errors and 5xx responses, with exponential backoff and jitter.
 */
class EDGEGAPSDK_API FEdgegapHttp
{
public:
	/** Override a service's URL and/or credential for this process (empty keeps the configured value). */
	static void SetConnection(EEdgegapService Service, const FString& Url, const FString& Credential);

	/** Base URL without trailing slash. Order: SetConnection > command line > environment > settings. */
	static FString GetBaseUrl(EEdgegapService Service);

	/** Full Authorization header value for a service (API tokens get the "token " prefix). */
	static FString GetAuthorization(EEdgegapService Service);

	/** Done always runs on the game thread, exactly once, and never inside Send. */
	static void Send(EEdgegapService Service, const FString& Verb, const FString& Path, const FString& Body,
		const TMap<FString, FString>& Headers, TFunction<void(const FEdgegapResponse&)> Done);

	/** Fail asynchronously without sending (local validation). */
	static void FailLater(const FString& Message, TFunction<void(const FEdgegapResponse&)> Done);

	/** Best-effort message from an Edgegap error body. */
	static FString ExtractErrorMessage(int32 Status, const FString& Body);
};
