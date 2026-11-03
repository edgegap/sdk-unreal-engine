// Edgegap SDK: JSON helpers used by the generated models.
// Generated structs implement ReadJson(const FJsonObject&) and WriteJson() and plug into
// the FromValue/ToValue overloads below, so arrays and maps of them work too.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace EdgegapJson
{
	inline bool FromValue(const TSharedPtr<FJsonValue>& V, FString& Out)
	{
		if (!V.IsValid() || V->IsNull()) { return false; }
		if (V->Type == EJson::String) { Out = V->AsString(); return true; }
		if (V->Type == EJson::Number) { Out = FString::SanitizeFloat(V->AsNumber(), 0); return true; }
		if (V->Type == EJson::Boolean) { Out = V->AsBool() ? TEXT("true") : TEXT("false"); return true; }
		return false;
	}

	inline bool FromValue(const TSharedPtr<FJsonValue>& V, double& Out)
	{
		if (!V.IsValid() || V->IsNull()) { return false; }
		if (V->Type == EJson::Number) { Out = V->AsNumber(); return true; }
		if (V->Type == EJson::String) { return LexTryParseString(Out, *V->AsString()); }
		return false;
	}

	inline bool FromValue(const TSharedPtr<FJsonValue>& V, int32& Out)
	{
		double D = 0.0;
		if (!FromValue(V, D)) { return false; }
		Out = static_cast<int32>(D);
		return true;
	}

	inline bool FromValue(const TSharedPtr<FJsonValue>& V, int64& Out)
	{
		double D = 0.0;
		if (!FromValue(V, D)) { return false; }
		Out = static_cast<int64>(D);
		return true;
	}

	inline bool FromValue(const TSharedPtr<FJsonValue>& V, bool& Out)
	{
		if (!V.IsValid() || V->Type != EJson::Boolean) { return false; }
		Out = V->AsBool();
		return true;
	}

	inline bool FromValue(const TSharedPtr<FJsonValue>& V, FDateTime& Out)
	{
		FString S;
		return FromValue(V, S) && FDateTime::ParseIso8601(*S, Out);
	}

	template <typename T>
	auto FromValue(const TSharedPtr<FJsonValue>& V, T& Out) -> decltype(Out.ReadJson(DeclVal<const FJsonObject&>()), bool())
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!V.IsValid() || !V->TryGetObject(Obj) || !Obj || !Obj->IsValid()) { return false; }
		Out.ReadJson(**Obj);
		return true;
	}

	template <typename T>
	bool FromValue(const TSharedPtr<FJsonValue>& V, TArray<T>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
		if (!V.IsValid() || !V->TryGetArray(Items) || !Items) { return false; }
		Out.Reset(Items->Num());
		for (const TSharedPtr<FJsonValue>& Item : *Items)
		{
			T Value{};
			if (FromValue(Item, Value)) { Out.Add(MoveTemp(Value)); }
		}
		return true;
	}

	template <typename T>
	bool FromValue(const TSharedPtr<FJsonValue>& V, TMap<FString, T>& Out)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!V.IsValid() || !V->TryGetObject(Obj) || !Obj || !Obj->IsValid()) { return false; }
		Out.Reset();
		for (const auto& Pair : (*Obj)->Values)
		{
			T Value{};
			if (FromValue(Pair.Value, Value)) { Out.Add(FString(*Pair.Key), MoveTemp(Value)); }
		}
		return true;
	}

	inline TSharedPtr<FJsonValue> ToValue(const FString& V) { return MakeShared<FJsonValueString>(V); }
	inline TSharedPtr<FJsonValue> ToValue(double V) { return MakeShared<FJsonValueNumber>(V); }
	inline TSharedPtr<FJsonValue> ToValue(int32 V) { return MakeShared<FJsonValueNumber>(V); }
	inline TSharedPtr<FJsonValue> ToValue(int64 V) { return MakeShared<FJsonValueNumber>(static_cast<double>(V)); }
	inline TSharedPtr<FJsonValue> ToValue(bool V) { return MakeShared<FJsonValueBoolean>(V); }
	inline TSharedPtr<FJsonValue> ToValue(const FDateTime& V) { return MakeShared<FJsonValueString>(V.ToIso8601()); }

	template <typename T>
	auto ToValue(const T& V) -> decltype(V.WriteJson(), TSharedPtr<FJsonValue>())
	{
		return MakeShared<FJsonValueObject>(V.WriteJson());
	}

	template <typename T>
	TSharedPtr<FJsonValue> ToValue(const TArray<T>& V)
	{
		TArray<TSharedPtr<FJsonValue>> Items;
		Items.Reserve(V.Num());
		for (const T& Item : V) { Items.Add(ToValue(Item)); }
		return MakeShared<FJsonValueArray>(Items);
	}

	template <typename T>
	TSharedPtr<FJsonValue> ToValue(const TMap<FString, T>& V)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		for (const TPair<FString, T>& Pair : V) { Obj->SetField(Pair.Key, ToValue(Pair.Value)); }
		return MakeShared<FJsonValueObject>(Obj);
	}

	template <typename T>
	void Read(const FJsonObject& Obj, const TCHAR* Field, T& Out)
	{
		FromValue(Obj.TryGetField(Field), Out);
	}

	template <typename T>
	void Write(FJsonObject& Obj, const TCHAR* Field, const T& Value)
	{
		Obj.SetField(Field, ToValue(Value));
	}

	/** Fields the spec leaves untyped (e.g. free-form attributes) are kept as JSON text. */
	inline FString Stringify(const TSharedPtr<FJsonValue>& V)
	{
		if (!V.IsValid()) { return FString(); }
		FString Out;
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(V, FString(), Writer);
		return Out;
	}

	inline TSharedPtr<FJsonValue> Parse(const FString& Text)
	{
		if (Text.TrimStartAndEnd().IsEmpty()) { return nullptr; }
		TSharedPtr<FJsonValue> Value;
		TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Text);
		return FJsonSerializer::Deserialize(Reader, Value) ? Value : nullptr;
	}

	inline void ReadRaw(const FJsonObject& Obj, const TCHAR* Field, FString& Out)
	{
		const TSharedPtr<FJsonValue> V = Obj.TryGetField(Field);
		Out = (V.IsValid() && !V->IsNull()) ? Stringify(V) : FString();
	}

	/**
	 * Writes JSON text as a nested value. Text that is not JSON is sent as a string (untyped
	 * "string or null" fields); an empty string writes {} so required objects stay valid.
	 */
	inline void WriteRaw(FJsonObject& Obj, const TCHAR* Field, const FString& JsonText)
	{
		if (JsonText.TrimStartAndEnd().IsEmpty())
		{
			Obj.SetField(Field, MakeShared<FJsonValueObject>(MakeShared<FJsonObject>()));
			return;
		}
		TSharedPtr<FJsonValue> V = Parse(JsonText);
		Obj.SetField(Field, V.IsValid() ? V : MakeShared<FJsonValueString>(JsonText));
	}

	inline FString ObjectToString(const TSharedRef<FJsonObject>& Obj)
	{
		return Stringify(MakeShared<FJsonValueObject>(Obj));
	}

	inline TSharedPtr<FJsonObject> ParseObject(const FString& Text)
	{
		const TSharedPtr<FJsonValue> V = Parse(Text);
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		return (V.IsValid() && V->TryGetObject(Obj) && Obj) ? *Obj : nullptr;
	}
}
