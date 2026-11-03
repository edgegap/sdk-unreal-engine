// Edgegap SDK: matchmaking helpers.

#include "EdgegapMatchmakingLibrary.h"
#include "EdgegapJson.h"
#include "EdgegapSettings.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

// ---------------------------------------------------------------- attributes

namespace
{
	FEdgegapAttribute MakeAttribute(const FString& Name, EEdgegapAttributeType Type)
	{
		FEdgegapAttribute Attribute;
		Attribute.Name = Name;
		Attribute.Type = Type;
		return Attribute;
	}
}

FEdgegapAttribute UEdgegapMatchmakingLibrary::MakeStringAttribute(const FString& Name, const FString& Value)
{
	FEdgegapAttribute A = MakeAttribute(Name, EEdgegapAttributeType::String);
	A.StringValue = Value;
	return A;
}

FEdgegapAttribute UEdgegapMatchmakingLibrary::MakeNumberAttribute(const FString& Name, double Value)
{
	FEdgegapAttribute A = MakeAttribute(Name, EEdgegapAttributeType::Number);
	A.NumberValue = Value;
	return A;
}

FEdgegapAttribute UEdgegapMatchmakingLibrary::MakeBooleanAttribute(const FString& Name, bool Value)
{
	FEdgegapAttribute A = MakeAttribute(Name, EEdgegapAttributeType::Boolean);
	A.bBooleanValue = Value;
	return A;
}

FEdgegapAttribute UEdgegapMatchmakingLibrary::MakeStringArrayAttribute(const FString& Name, const TArray<FString>& Values)
{
	FEdgegapAttribute A = MakeAttribute(Name, EEdgegapAttributeType::StringArray);
	A.StringArrayValue = Values;
	return A;
}

FEdgegapAttribute UEdgegapMatchmakingLibrary::MakeLatenciesAttribute(const TMap<FString, double>& Latencies, const FString& Name)
{
	FEdgegapAttribute A = MakeAttribute(Name, EEdgegapAttributeType::NumberMap);
	A.NumberMapValue = Latencies;
	return A;
}

bool UEdgegapMatchmakingLibrary::AttributesToJson(const TArray<FEdgegapAttribute>& Attributes, FString& Json, FString& ErrorMessage)
{
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	for (const FEdgegapAttribute& A : Attributes)
	{
		const FString Name = A.Name.TrimStartAndEnd();
		if (Name.IsEmpty()) { ErrorMessage = TEXT("An attribute has no name."); return false; }
		if (Obj->HasField(Name)) { ErrorMessage = FString::Printf(TEXT("Attribute '%s' is set twice."), *Name); return false; }
		switch (A.Type)
		{
		case EEdgegapAttributeType::String: EdgegapJson::Write(*Obj, *Name, A.StringValue); break;
		case EEdgegapAttributeType::Number: EdgegapJson::Write(*Obj, *Name, A.NumberValue); break;
		case EEdgegapAttributeType::Boolean: EdgegapJson::Write(*Obj, *Name, A.bBooleanValue); break;
		case EEdgegapAttributeType::StringArray: EdgegapJson::Write(*Obj, *Name, A.StringArrayValue); break;
		case EEdgegapAttributeType::NumberMap: EdgegapJson::Write(*Obj, *Name, A.NumberMapValue); break;
		}
	}
	Json = EdgegapJson::ObjectToString(Obj);
	ErrorMessage.Reset();
	return true;
}

// ---------------------------------------------------------------- connection and travel

void UEdgegapMatchmakingLibrary::SetConnection(EEdgegapService Service, const FString& Url, const FString& Credential)
{
	FEdgegapHttp::SetConnection(Service, Url, Credential);
}

FString UEdgegapMatchmakingLibrary::MakeAssignmentAddress(const FEdgegapMMTicketAssignmentResponse& Assignment, const FString& PortName)
{
	const FString Name = PortName.IsEmpty() ? GetDefault<UEdgegapSettings>()->GamePortName : PortName;
	const FEdgegapMMAssignmentPortResponse* Port = Assignment.Ports.Find(Name);
	if (!Port && Assignment.Ports.Num() == 1)
	{
		for (const TPair<FString, FEdgegapMMAssignmentPortResponse>& Pair : Assignment.Ports) { Port = &Pair.Value; }
	}
	if (!Port)
	{
		TArray<FString> Names;
		Assignment.Ports.GetKeys(Names);
		UE_LOG(LogEdgegap, Warning, TEXT("Assignment has no port named '%s' (ports: %s). Set Game Port Name in Project Settings > Edgegap."),
			*Name, *FString::Join(Names, TEXT(", ")));
		return Assignment.PublicIp;
	}
	return FString::Printf(TEXT("%s:%d"), *Assignment.PublicIp, Port->External);
}

FString UEdgegapMatchmakingLibrary::MakeJoinUrl(const FEdgegapMMTicketAssignmentResponse& Assignment, const FString& TicketId)
{
	const FString Address = MakeAssignmentAddress(Assignment, FString());
	return TicketId.IsEmpty() ? Address : FString::Printf(TEXT("%s?edgegap_mm_ticket_id=%s"), *Address, *TicketId);
}

bool UEdgegapMatchmakingLibrary::TravelPlayerToMatch(APlayerController* PlayerController, const FString& Address)
{
	if (!PlayerController || Address.IsEmpty()) { return false; }
	UE_LOG(LogEdgegap, Log, TEXT("Travelling %s to %s"), *PlayerController->GetName(), *Address);
	PlayerController->ClientTravel(Address, TRAVEL_Absolute);
	return true;
}

bool UEdgegapMatchmakingLibrary::TravelPlayerToMatchAssignment(APlayerController* PlayerController, const FEdgegapMMTicketAssignmentResponse& Assignment, const FString& TicketId)
{
	return TravelPlayerToMatch(PlayerController, MakeJoinUrl(Assignment, TicketId));
}

int32 UEdgegapMatchmakingLibrary::TravelPartyToMatch(const UObject* WorldContextObject, const FString& Address, float LeaderDelaySeconds)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World || Address.IsEmpty()) { return 0; }

	if (World->GetNetMode() != NM_ListenServer && World->GetNetMode() != NM_DedicatedServer)
	{
		return TravelPlayerToMatch(World->GetFirstPlayerController(), Address) ? 1 : 0;
	}

	int32 Sent = 0;
	TWeakObjectPtr<APlayerController> Host;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) { continue; }
		if (PC->IsLocalController()) { Host = PC; continue; }
		Sent += TravelPlayerToMatch(PC, Address) ? 1 : 0;
	}
	if (Host.IsValid())
	{
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Host, Address](float)
		{
			if (APlayerController* PC = Host.Get()) { TravelPlayerToMatch(PC, Address); }
			return false;
		}), FMath::Max(0.0f, LeaderDelaySeconds));
		++Sent;
	}
	return Sent;
}

int32 UEdgegapMatchmakingLibrary::TravelPartyToMatchAssignment(const UObject* WorldContextObject, const FEdgegapMMTicketAssignmentResponse& Assignment, float LeaderDelaySeconds)
{
	return TravelPartyToMatch(WorldContextObject, MakeAssignmentAddress(Assignment, FString()), LeaderDelaySeconds);
}

int32 UEdgegapMatchmakingLibrary::TravelAllConnectedPlayersToMatchAssignment(const UObject* WorldContextObject, const FEdgegapMMTicketAssignmentResponse& Assignment, float HostDelaySeconds)
{
	return TravelPartyToMatch(WorldContextObject, MakeAssignmentAddress(Assignment, FString()), HostDelaySeconds);
}

FString UEdgegapMatchmakingLibrary::Conv_EdgegapErrorToString(const FEdgegapError& Error)
{
	return Error.bFailed ? FString::Printf(TEXT("Edgegap error %d: %s"), Error.HttpStatus, *Error.Message) : TEXT("OK");
}

// ---------------------------------------------------------------- validation against the live spec

namespace
{
	struct FSpecCache
	{
		TSharedPtr<FJsonObject> Spec;
		double FetchedAt = 0.0;
	};

	TMap<FString, FSpecCache>& SpecCache()
	{
		static TMap<FString, FSpecCache> Cache;
		return Cache;
	}

	TSharedPtr<FJsonObject> Child(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		const TSharedPtr<FJsonObject>* Out = nullptr;
		return Obj.IsValid() && Obj->TryGetObjectField(Field, Out) && Out ? *Out : nullptr;
	}

	TSharedPtr<FJsonObject> Resolve(const TSharedPtr<FJsonObject>& Root, const TSharedPtr<FJsonObject>& Schema)
	{
		FString Ref;
		if (!Schema.IsValid() || !Schema->TryGetStringField(TEXT("$ref"), Ref)) { return Schema; }
		FString Name;
		Ref.Split(TEXT("/"), nullptr, &Name, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		return Child(Child(Child(Root, TEXT("components")), TEXT("schemas")), *Name);
	}

	bool IsNumber(const TSharedPtr<FJsonValue>& V)
	{
		double D = 0.0;
		return V.IsValid() && (V->Type == EJson::Number || (V->Type == EJson::String && LexTryParseString(D, *V->AsString())));
	}

	void CheckValue(const TSharedPtr<FJsonObject>& Root, const FString& Name, const TSharedPtr<FJsonObject>& InSchema, const TSharedPtr<FJsonValue>& Value, TArray<FString>& Problems)
	{
		const TSharedPtr<FJsonObject> Schema = Resolve(Root, InSchema);
		FString Type;
		if (!Schema.IsValid() || !Schema->TryGetStringField(TEXT("type"), Type)) { return; }
		const bool bNumericString = Type == TEXT("string") && (Schema->HasField(TEXT("minimum")) || Schema->HasField(TEXT("maximum")));
		bool bOk = true;
		if (Type == TEXT("number") || Type == TEXT("integer") || bNumericString) { bOk = IsNumber(Value); Type = TEXT("number"); }
		else if (Type == TEXT("string")) { bOk = Value->Type == EJson::String; }
		else if (Type == TEXT("boolean")) { bOk = Value->Type == EJson::Boolean; }
		else if (Type == TEXT("array"))
		{
			bOk = Value->Type == EJson::Array;
			const TSharedPtr<FJsonObject> Items = Child(Schema, TEXT("items"));
			if (bOk && Items.IsValid())
			{
				for (const TSharedPtr<FJsonValue>& Item : Value->AsArray()) { CheckValue(Root, Name + TEXT("[]"), Items, Item, Problems); }
			}
		}
		else if (Type == TEXT("object"))
		{
			bOk = Value->Type == EJson::Object;
			const TSharedPtr<FJsonObject> Extra = Child(Schema, TEXT("additionalProperties"));
			if (bOk && Extra.IsValid() && Extra->Values.Num() > 0)
			{
				int32 MinProperties = 0;
				if (Schema->TryGetNumberField(TEXT("minProperties"), MinProperties) && Value->AsObject()->Values.Num() < MinProperties)
				{
					Problems.Add(FString::Printf(TEXT("'%s' needs at least %d entries."), *Name, MinProperties));
				}
				for (const auto& Pair : Value->AsObject()->Values) { CheckValue(Root, Name + TEXT(".") + FString(*Pair.Key), Extra, Pair.Value, Problems); }
			}
		}
		if (!bOk) { Problems.Add(FString::Printf(TEXT("'%s' should be a %s."), *Name, *Type)); }
	}

	void ValidateAgainst(const TSharedPtr<FJsonObject>& Root, const FString& Profile, const TSharedPtr<FJsonObject>& Attributes, TArray<FString>& Problems)
	{
		// Group Up (POST /groups) is what Find Match uses; fall back to POST /tickets.
		TSharedPtr<FJsonObject> Body;
		for (const TCHAR* Path : { TEXT("/groups"), TEXT("/tickets") })
		{
			Body = Child(Child(Child(Child(Child(Child(Root, TEXT("paths")), Path), TEXT("post")), TEXT("requestBody")), TEXT("content")), TEXT("application/json"));
			if (Body.IsValid()) { break; }
		}
		const TArray<TSharedPtr<FJsonValue>>* Variants = nullptr;
		const TSharedPtr<FJsonObject> Schema = Child(Body, TEXT("schema"));
		if (!Schema.IsValid() || !Schema->TryGetArrayField(TEXT("oneOf"), Variants) || !Variants)
		{
			Problems.Add(TEXT("The matchmaker spec has no per-profile schemas to check against (matchmaker older than 3.x?)."));
			return;
		}

		TArray<FString> Profiles;
		for (const TSharedPtr<FJsonValue>& VariantValue : *Variants)
		{
			const TSharedPtr<FJsonObject> Variant = Resolve(Root, VariantValue->AsObject());
			const TSharedPtr<FJsonObject> Props = Child(Variant, TEXT("properties"));
			const TArray<TSharedPtr<FJsonValue>>* Enum = nullptr;
			const TSharedPtr<FJsonObject> ProfileProp = Child(Props, TEXT("profile"));
			if (!ProfileProp.IsValid() || !ProfileProp->TryGetArrayField(TEXT("enum"), Enum) || !Enum || Enum->Num() != 1) { continue; }
			const FString Name = (*Enum)[0]->AsString();
			Profiles.Add(Name);
			if (Name != Profile) { continue; }

			const TSharedPtr<FJsonObject> AttrSchema = Resolve(Root, Child(Props, TEXT("attributes")));
			const TSharedPtr<FJsonObject> AttrProps = Child(AttrSchema, TEXT("properties"));
			const TArray<TSharedPtr<FJsonValue>>* Required = nullptr;
			if (AttrSchema.IsValid() && AttrSchema->TryGetArrayField(TEXT("required"), Required) && Required)
			{
				for (const TSharedPtr<FJsonValue>& R : *Required)
				{
					if (!Attributes->HasField(R->AsString())) { Problems.Add(FString::Printf(TEXT("Missing required attribute '%s'."), *R->AsString())); }
				}
			}
			bool bAllowsExtra = true;
			if (AttrSchema.IsValid()) { AttrSchema->TryGetBoolField(TEXT("additionalProperties"), bAllowsExtra); }
			for (const auto& Pair : Attributes->Values)
			{
				const FString Key(*Pair.Key);
				const TSharedPtr<FJsonObject> PropSchema = Child(AttrProps, *Key);
				if (!PropSchema.IsValid())
				{
					if (!bAllowsExtra) { Problems.Add(FString::Printf(TEXT("Attribute '%s' is not in profile '%s'."), *Key, *Profile)); }
					continue;
				}
				CheckValue(Root, Key, PropSchema, Pair.Value, Problems);
			}
			return;
		}
		Problems.Add(FString::Printf(TEXT("Profile '%s' does not exist. Profiles: %s."), *Profile, *FString::Join(Profiles, TEXT(", "))));
	}
}

UEdgegapValidateAttributesNode* UEdgegapValidateAttributesNode::ValidateAttributes(const FString& Profile, const FString& AttributesJson)
{
	UEdgegapValidateAttributesNode* Node = NewObject<UEdgegapValidateAttributesNode>();
	Node->Profile = Profile;
	Node->AttributesJson = AttributesJson;
	return Node;
}

void UEdgegapValidateAttributesNode::Activate()
{
	AddToRoot();
	Validate(Profile, AttributesJson, [this](const TArray<FString>& Problems)
	{
		(Problems.IsEmpty() ? OnValid : OnInvalid).Broadcast(Problems);
		RemoveFromRoot();
		SetReadyToDestroy();
	});
}

void UEdgegapValidateAttributesNode::Validate(const FString& Profile, const FString& AttributesJson, TFunction<void(const TArray<FString>&)> Done)
{
	const TSharedPtr<FJsonObject> Attributes = AttributesJson.TrimStartAndEnd().IsEmpty() ? MakeShared<FJsonObject>() : EdgegapJson::ParseObject(AttributesJson);
	const FString BaseUrl = FEdgegapHttp::GetBaseUrl(EEdgegapService::Matchmaker);
	auto Finish = [Done](const TArray<FString>& Problems)
	{
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Done, Problems](float) { Done(Problems); return false; }));
	};
	if (!Attributes.IsValid()) { Finish({ TEXT("Attributes are not a JSON object.") }); return; }
	if (BaseUrl.IsEmpty()) { Finish({ TEXT("Matchmaker URL is not set.") }); return; }

	const FSpecCache* Cached = SpecCache().Find(BaseUrl);
	if (Cached && Cached->Spec.IsValid() && FPlatformTime::Seconds() - Cached->FetchedAt < 60.0)
	{
		TArray<FString> Problems;
		ValidateAgainst(Cached->Spec, Profile, Attributes, Problems);
		Finish(Problems);
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetVerb(TEXT("GET"));
	Request->SetURL(BaseUrl + TEXT("/swagger/v1/swagger.json"));
	Request->SetTimeout(GetDefault<UEdgegapSettings>()->RequestTimeoutSeconds);
	Request->OnProcessRequestComplete().BindLambda([BaseUrl, Profile, Attributes, Done](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
	{
		TArray<FString> Problems;
		const TSharedPtr<FJsonObject> Spec = bConnected && Response.IsValid() && Response->GetResponseCode() == 200
			? EdgegapJson::ParseObject(Response->GetContentAsString()) : nullptr;
		if (!Spec.IsValid())
		{
			Problems.Add(TEXT("Could not read the matchmaker spec (/swagger/v1/swagger.json) to validate attributes."));
		}
		else
		{
			FSpecCache& Entry = SpecCache().FindOrAdd(BaseUrl);
			Entry.Spec = Spec;
			Entry.FetchedAt = FPlatformTime::Seconds();
			ValidateAgainst(Spec, Profile, Attributes.ToSharedRef(), Problems);
		}
		Done(Problems);
	});
	Request->ProcessRequest();
}
