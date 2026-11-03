// Edgegap SDK: HTTP core.

#include "EdgegapCore.h"
#include "EdgegapJson.h"
#include "EdgegapSettings.h"
#include "Containers/Ticker.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogEdgegap);

IMPLEMENT_MODULE(FDefaultModuleImpl, EdgegapSDK);

namespace
{
	struct FOverride
	{
		FString Url;
		FString Credential;
	};

	TMap<EEdgegapService, FOverride>& Overrides()
	{
		static TMap<EEdgegapService, FOverride> Map;
		return Map;
	}

	FString FirstNonEmpty(std::initializer_list<FString> Values)
	{
		for (const FString& Value : Values)
		{
			const FString Trimmed = Value.TrimStartAndEnd();
			if (!Trimmed.IsEmpty()) { return Trimmed; }
		}
		return FString();
	}

	FString CommandLineValue(const TCHAR* Key)
	{
		FString Value;
		FParse::Value(FCommandLine::Get(), Key, Value);
		return Value;
	}

	FString Env(const TCHAR* Name)
	{
		return FPlatformMisc::GetEnvironmentVariable(Name);
	}

	struct FAttempt : TSharedFromThis<FAttempt>
	{
		FString Verb;
		FString Url;
		FString Authorization;
		FString Body;
		TMap<FString, FString> Headers;
		TFunction<void(const FEdgegapResponse&)> Done;
		int32 Retries = 0;
		int32 MaxRetries = 3;
		float Timeout = 10.0f;

		bool ShouldRetry(int32 Status) const
		{
			if (Retries >= MaxRetries) { return false; }
			if (Status == 429) { return true; }
			// A POST that timed out may already have created a ticket or deployment: never resend it.
			return Verb != TEXT("POST") && (Status == 0 || Status >= 500);
		}

		void Start()
		{
			TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
			Request->SetVerb(Verb);
			Request->SetURL(Url);
			Request->SetTimeout(Timeout);
			if (!Authorization.IsEmpty()) { Request->SetHeader(TEXT("Authorization"), Authorization); }
			Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
			for (const TPair<FString, FString>& Header : Headers) { Request->SetHeader(Header.Key, Header.Value); }
			if (!Body.IsEmpty())
			{
				Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
				Request->SetContentAsString(Body);
			}

			TSharedRef<FAttempt> Self = AsShared();
			Request->OnProcessRequestComplete().BindLambda(
				[Self](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
				{
					Self->OnComplete(Response, bConnected);
				});
			Request->ProcessRequest();
		}

		void OnComplete(FHttpResponsePtr Response, bool bConnected)
		{
			const int32 Status = (bConnected && Response.IsValid()) ? Response->GetResponseCode() : 0;
			const FString Content = Response.IsValid() ? Response->GetContentAsString() : FString();
			const bool bOk = Status >= 200 && Status < 300;

			if (!bOk && ShouldRetry(Status))
			{
				float Delay = FMath::Min(10.0f, FMath::Pow(2.0f, static_cast<float>(Retries)));
				float RetryAfter = 0.0f;
				if (Response.IsValid() && LexTryParseString(RetryAfter, *Response->GetHeader(TEXT("Retry-After"))) && RetryAfter >= 0.0f)
				{
					Delay = FMath::Min(RetryAfter, 60.0f);
				}
				Delay += FMath::FRandRange(0.0f, 0.25f);
				++Retries;
				UE_LOG(LogEdgegap, Warning, TEXT("%s %s -> %d, retry %d/%d in %.1fs"), *Verb, *Url, Status, Retries, MaxRetries, Delay);
				TSharedRef<FAttempt> Self = AsShared();
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self](float)
				{
					Self->Start();
					return false;
				}), Delay);
				return;
			}

			FEdgegapResponse Result;
			Result.Status = Status;
			Result.Body = Content;
			if (!bOk)
			{
				Result.Error = FEdgegapError::Make(Status, FEdgegapHttp::ExtractErrorMessage(Status, Content));
				UE_LOG(LogEdgegap, Warning, TEXT("%s %s failed: %s"), *Verb, *Url, *Result.Error.Message);
			}
			Done(Result);
		}
	};
}

void FEdgegapHttp::SetConnection(EEdgegapService Service, const FString& Url, const FString& Credential)
{
	FOverride& Override = Overrides().FindOrAdd(Service);
	Override.Url = Url;
	Override.Credential = Credential;
}

FString FEdgegapHttp::GetBaseUrl(EEdgegapService Service)
{
	const UEdgegapSettings* Settings = GetDefault<UEdgegapSettings>();
	const FOverride* Override = Overrides().Find(Service);
	const FString Over = Override ? Override->Url : FString();
	FString Url;
	switch (Service)
	{
	case EEdgegapService::Matchmaker:
		Url = FirstNonEmpty({ Over, CommandLineValue(TEXT("EdgegapMMUrl=")), Env(TEXT("EDGEGAP_MM_URL")), Settings->MatchmakerUrl });
		break;
	case EEdgegapService::Api:
		Url = FirstNonEmpty({ Over, CommandLineValue(TEXT("EdgegapApiUrl=")), Env(TEXT("EDGEGAP_API_URL")), Settings->ApiUrl, TEXT("https://api.edgegap.com") });
		break;
	case EEdgegapService::ServerBrowser:
		Url = FirstNonEmpty({ Over, CommandLineValue(TEXT("EdgegapSBUrl=")), Env(TEXT("EDGEGAP_SB_URL")), Settings->ServerBrowserUrl });
		break;
	}
	while (Url.EndsWith(TEXT("/"))) { Url.LeftChopInline(1); }
	return Url;
}

FString FEdgegapHttp::GetAuthorization(EEdgegapService Service)
{
	const UEdgegapSettings* Settings = GetDefault<UEdgegapSettings>();
	const UEdgegapUserSecrets* Secrets = GetDefault<UEdgegapUserSecrets>();
	const FOverride* Override = Overrides().Find(Service);
	const FString Over = Override ? Override->Credential : FString();
	switch (Service)
	{
	case EEdgegapService::Matchmaker:
		return FirstNonEmpty({ Over, CommandLineValue(TEXT("EdgegapMMToken=")), Env(TEXT("EDGEGAP_MM_TOKEN")), Settings->MatchmakerToken });
	case EEdgegapService::Api:
	{
		FString Token = FirstNonEmpty({ Over, Env(TEXT("EDGEGAP_API_TOKEN")),
#if WITH_EDITOR
			Secrets->ApiToken,
#endif
			Settings->bIncludeApiTokenInPackagedBuilds ? Settings->PackagedApiToken : FString() });
		if (!Token.IsEmpty() && !Token.StartsWith(TEXT("token "), ESearchCase::IgnoreCase)) { Token = TEXT("token ") + Token; }
		return Token;
	}
	case EEdgegapService::ServerBrowser:
	{
		const FString ServerToken = FirstNonEmpty({ Env(TEXT("EDGEGAP_SB_SERVER_TOKEN")),
#if WITH_EDITOR
			Secrets->ServerBrowserServerToken,
#endif
			FString() });
		const FString ClientToken = FirstNonEmpty({ CommandLineValue(TEXT("EdgegapSBToken=")), Env(TEXT("EDGEGAP_SB_CLIENT_TOKEN")), Settings->ServerBrowserClientToken });
		return FirstNonEmpty({ Over, IsRunningDedicatedServer() ? ServerToken : FString(), ClientToken, ServerToken });
	}
	}
	return FString();
}

void FEdgegapHttp::FailLater(const FString& Message, TFunction<void(const FEdgegapResponse&)> Done)
{
	FEdgegapResponse Result;
	Result.Error = FEdgegapError::Make(0, Message);
	UE_LOG(LogEdgegap, Warning, TEXT("Request not sent: %s"), *Message);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Done, Result](float) { Done(Result); return false; }));
}

void FEdgegapHttp::Send(EEdgegapService Service, const FString& Verb, const FString& Path, const FString& Body,
	const TMap<FString, FString>& Headers, TFunction<void(const FEdgegapResponse&)> Done)
{
	static const TCHAR* Names[] = { TEXT("Matchmaker"), TEXT("Edgegap API"), TEXT("Server browser") };
	const TCHAR* Name = Names[static_cast<int32>(Service)];

	const FString BaseUrl = GetBaseUrl(Service);
	if (!BaseUrl.StartsWith(TEXT("http://")) && !BaseUrl.StartsWith(TEXT("https://")))
	{
		FailLater(FString::Printf(TEXT("%s URL is not set (Project Settings > Plugins > Edgegap, or its environment variable)."), Name), MoveTemp(Done));
		return;
	}
	const FString Authorization = GetAuthorization(Service);
	if (Authorization.IsEmpty() && !Headers.Contains(TEXT("Authorization")))
	{
		FailLater(FString::Printf(TEXT("%s credential is not set (see Project Settings > Plugins > Edgegap)."), Name), MoveTemp(Done));
		return;
	}

	const UEdgegapSettings* Settings = GetDefault<UEdgegapSettings>();
	TSharedRef<FAttempt> Attempt = MakeShared<FAttempt>();
	Attempt->Verb = Verb;
	Attempt->Url = BaseUrl + Path;
	Attempt->Authorization = Authorization;
	Attempt->Body = Body;
	Attempt->Headers = Headers;
	Attempt->Done = MoveTemp(Done);
	Attempt->MaxRetries = Settings->MaxRetries;
	Attempt->Timeout = Settings->RequestTimeoutSeconds;
	Attempt->Start();
}

FString FEdgegapHttp::ExtractErrorMessage(int32 Status, const FString& Body)
{
	if (Status == 0) { return TEXT("Could not reach the service (offline, timed out or wrong URL)."); }

	TArray<FString> Messages;
	if (const TSharedPtr<FJsonObject> Obj = EdgegapJson::ParseObject(Body))
	{
		for (const TCHAR* Key : { TEXT("message"), TEXT("detail"), TEXT("error"), TEXT("title") })
		{
			FString Message;
			if (Obj->TryGetStringField(Key, Message) && !Message.IsEmpty()) { Messages.AddUnique(Message); }
		}
		const TSharedPtr<FJsonObject>* Errors = nullptr;
		if (Obj->TryGetObjectField(TEXT("errors"), Errors) && Errors)
		{
			for (const auto& Pair : (*Errors)->Values)
			{
				Messages.Add(FString::Printf(TEXT("%s: %s"), *FString(*Pair.Key), *EdgegapJson::Stringify(Pair.Value)));
			}
		}
	}
	if (Messages.IsEmpty()) { Messages.Add(Body.Left(300)); }

	const TCHAR* Hint = Status == 401 || Status == 403 ? TEXT(" (check the token)")
		: Status == 404 ? TEXT(" (not found or expired, or wrong URL)")
		: Status == 409 ? TEXT(" (conflict: the resource moved to a state that refuses this)")
		: Status == 429 ? TEXT(" (rate limited)") : TEXT("");
	return FString::Printf(TEXT("HTTP %d%s: %s"), Status, Hint, *FString::Join(Messages, TEXT("; ")));
}
