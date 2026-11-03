// Edgegap SDK: dedicated-server helpers.

#include "EdgegapServerLibrary.h"
#include "EdgegapJson.h"
#include "EdgegapSettings.h"
#include "HttpManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/CoreDelegates.h"

namespace
{
	TMap<FString, FEdgegapIdList> ParseIdMapping(const FString& Json)
	{
		TMap<FString, FEdgegapIdList> Out;
		TMap<FString, TArray<FString>> Parsed;
		if (EdgegapJson::FromValue(EdgegapJson::Parse(Json), Parsed))
		{
			for (TPair<FString, TArray<FString>>& Pair : Parsed) { Out.Add(Pair.Key).Ids = MoveTemp(Pair.Value); }
		}
		return Out;
	}

	int32 PortFromEnv(const FString& PortName, const TCHAR* Kind)
	{
		int32 Port = 0;
		LexTryParseString(Port, *UEdgegapServerLibrary::Env(FString::Printf(TEXT("ARBITRIUM_PORT_%s_%s"), *PortName.ToUpper().Replace(TEXT("-"), TEXT("_")), Kind)));
		return Port;
	}

	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> MakeSelfStopRequest()
	{
		const FString Url = UEdgegapServerLibrary::Env(TEXT("ARBITRIUM_DELETE_URL"));
		const FString Token = UEdgegapServerLibrary::Env(TEXT("ARBITRIUM_DELETE_TOKEN"));
		if (Url.IsEmpty() || Token.IsEmpty())
		{
			UE_LOG(LogEdgegap, Warning, TEXT("SelfStop: ARBITRIUM_DELETE_URL/TOKEN not set (not running on Edgegap)."));
			return nullptr;
		}
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
		Request->SetVerb(TEXT("DELETE"));
		Request->SetURL(Url);
		Request->SetHeader(TEXT("Authorization"), Token);
		Request->SetTimeout(10.0f);
		return Request;
	}
}

FString UEdgegapServerLibrary::Env(const FString& Name)
{
	return FPlatformMisc::GetEnvironmentVariable(*Name);
}

bool UEdgegapServerLibrary::IsRunningOnEdgegap() { return !GetDeploymentId().IsEmpty(); }
bool UEdgegapServerLibrary::IsMatchmakerDeployment() { return !GetMatchId().IsEmpty(); }
FString UEdgegapServerLibrary::GetDeploymentId() { return Env(TEXT("ARBITRIUM_REQUEST_ID")); }
int32 UEdgegapServerLibrary::GetExternalPort(const FString& PortName) { return PortFromEnv(PortName, TEXT("EXTERNAL")); }
int32 UEdgegapServerLibrary::GetInternalPort(const FString& PortName) { return PortFromEnv(PortName, TEXT("INTERNAL")); }
FString UEdgegapServerLibrary::GetEnvironmentVariable(const FString& Name) { return Env(Name); }
FString UEdgegapServerLibrary::GetMatchId() { return Env(TEXT("MM_MATCH_ID")); }
FString UEdgegapServerLibrary::GetMatchProfile() { return Env(TEXT("MM_MATCH_PROFILE")); }
FString UEdgegapServerLibrary::GetTicketJson(const FString& TicketId) { return Env(TEXT("MM_TICKET_") + TicketId); }
TMap<FString, FEdgegapIdList> UEdgegapServerLibrary::GetGroupTicketMapping() { return ParseIdMapping(Env(TEXT("MM_GROUPS"))); }
TMap<FString, FEdgegapIdList> UEdgegapServerLibrary::GetTeamGroupMapping() { return ParseIdMapping(Env(TEXT("MM_TEAMS"))); }
FString UEdgegapServerLibrary::GetEqualityJson() { return Env(TEXT("MM_EQUALITY")); }
FString UEdgegapServerLibrary::GetIntersectionJson() { return Env(TEXT("MM_INTERSECTION")); }
TArray<FString> UEdgegapServerLibrary::GetTicketIds() { return JsonArrayToStringArray(Env(TEXT("MM_TICKET_IDS"))); }

FString UEdgegapServerLibrary::GetExpansionStage()
{
	const FString Value = Env(TEXT("MM_EXPANSION"));
	return Value.IsEmpty() ? Env(TEXT("MM_EXPANSION_STAGE")) : Value;
}

TArray<FString> UEdgegapServerLibrary::JsonArrayToStringArray(const FString& Json)
{
	TArray<FString> Out;
	EdgegapJson::FromValue(EdgegapJson::Parse(Json), Out);
	return Out;
}

void UEdgegapServerLibrary::SelfStop()
{
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request = MakeSelfStopRequest();
	if (!Request.IsValid()) { return; }
	Request->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr, FHttpResponsePtr Response, bool)
	{
		UE_LOG(LogEdgegap, Log, TEXT("SelfStop: HTTP %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
	});
	Request->ProcessRequest();
}

bool UEdgegapServerLibrary::SelfStopBlocking(float TimeoutSeconds)
{
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request = MakeSelfStopRequest();
	if (!Request.IsValid()) { return false; }
	Request->ProcessRequest();
	const double End = FPlatformTime::Seconds() + TimeoutSeconds;
	while (FPlatformTime::Seconds() < End && !EHttpRequestStatus::IsFinished(Request->GetStatus()))
	{
		FHttpModule::Get().GetHttpManager().Tick(0.05f);
		FPlatformProcess::Sleep(0.05f);
	}
	const int32 Code = Request->GetResponse().IsValid() ? Request->GetResponse()->GetResponseCode() : 0;
	UE_LOG(LogEdgegap, Log, TEXT("SelfStop on exit: HTTP %d"), Code);
	return Code >= 200 && Code < 300;
}

bool UEdgegapServerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return IsRunningDedicatedServer() && UEdgegapServerLibrary::IsRunningOnEdgegap() && GetDefault<UEdgegapSettings>()->bSelfStopOnExit;
}

void UEdgegapServerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogEdgegap, Log, TEXT("Edgegap deployment %s: self-stop on exit is on."), *UEdgegapServerLibrary::GetDeploymentId());
	PreExitHandle = FCoreDelegates::OnPreExit.AddUObject(this, &UEdgegapServerSubsystem::StopOnce, TEXT("exit"));
	SystemErrorHandle = FCoreDelegates::OnHandleSystemError.AddUObject(this, &UEdgegapServerSubsystem::StopOnce, TEXT("crash"));
}

void UEdgegapServerSubsystem::Deinitialize()
{
	FCoreDelegates::OnPreExit.Remove(PreExitHandle);
	FCoreDelegates::OnHandleSystemError.Remove(SystemErrorHandle);
	if (IsEngineExitRequested()) { StopOnce(TEXT("shutdown")); }
	Super::Deinitialize();
}

void UEdgegapServerSubsystem::StopOnce(const TCHAR* Reason)
{
	if (bStopped) { return; }
	bStopped = true;
	UE_LOG(LogEdgegap, Log, TEXT("Server %s: stopping the Edgegap deployment."), Reason);
	UEdgegapServerLibrary::SelfStopBlocking(3.0f);
}
