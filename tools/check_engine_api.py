#!/usr/bin/env python3
"""Static compatibility check: does each Unreal release tag have the engine APIs the SDK uses?

Reads engine headers from GitHub (Epic's UnrealEngine repo or a fork you can see; needs the
gh CLI logged in with an account linked to Epic). This is not a compile: it catches removed or
renamed APIs, not every template or UHT subtlety. Real compiles: see README "Engine versions".

  python tools/check_engine_api.py [--repo EpicGames/UnrealEngine] [--tags 5.0.3-release ...]
"""

import argparse
import re
import subprocess
import sys

DEFAULT_TAGS = ["5.0.3-release", "5.1.1-release", "5.2.1-release", "5.3.2-release", "5.4.4-release",
                "5.5.4-release", "5.6.1-release", "5.7.4-release"]

R = "Engine/Source/Runtime/"
# (what the SDK relies on, header, regex that must match). "!" before a regex = must NOT match.
CHECKS = [
    ("FTSTicker (core ticker)", R + "Core/Public/Containers/Ticker.h", r"class\s+(CORE_API\s+)?FTSTicker"),
    ("DeclVal", R + "Core/Public/Templates/UnrealTemplate.h", r"DeclVal\s*\("),
    ("TArray::IsEmpty", R + "Core/Public/Containers/Array.h", r"bool\s+IsEmpty\s*\(\s*\)\s*const"),
    ("TMap::IsEmpty", R + "Core/Public/Containers/Map.h", r"bool\s+IsEmpty\s*\(\s*\)\s*const"),
    ("FString::LeftChopInline", R + "Core/Public/Containers/UnrealString.h", r"LeftChopInline|UnrealString\.h\.inl"),
    ("FCoreDelegates::OnPreExit", R + "Core/Public/Misc/CoreDelegates.h", r"OnPreExit"),
    ("FCoreDelegates::OnHandleSystemError", R + "Core/Public/Misc/CoreDelegates.h", r"OnHandleSystemError"),
    ("FMonitoredProcess::OnCanceled", R + "Core/Public/Misc/MonitoredProcess.h", r"OnCanceled\s*\("),
    ("FMonitoredProcess::Cancel(bool)", R + "Core/Public/Misc/MonitoredProcess.h", r"void\s+Cancel\s*\(\s*bool"),
    ("EAutomationTestFlags::ApplicationContextMask (used < 5.5)", R + "Core/Public/Misc/AutomationTest.h", r"ApplicationContextMask\s*="),
    ("FJsonObject::TryGetField", R + "Json/Public/Dom/JsonObject.h", r"TryGetField\s*\("),
    ("FJsonObject::TryGetNumberField(int32)", R + "Json/Public/Dom/JsonObject.h", r"TryGetNumberField\s*\([^)]*int32\s*&"),
    ("FJsonObject::TryGetBoolField", R + "Json/Public/Dom/JsonObject.h", r"TryGetBoolField\s*\("),
    ("FJsonObject::Values", R + "Json/Public/Dom/JsonObject.h", r"TMap<\s*\w+\s*,\s*TSharedPtr<FJsonValue>\s*>\s+Values"),
    ("FJsonValue::IsNull / TryGetObject / TryGetArray", R + "Json/Public/Dom/JsonValue.h", r"IsNull[\s\S]*TryGetArray[\s\S]*TryGetObject|IsNull[\s\S]*TryGetObject[\s\S]*TryGetArray"),
    ("FJsonSerializer::Serialize(value, identifier, writer)", R + "Json/Public/Serialization/JsonSerializer.h", r"Serialize\s*\(\s*const\s+(TSharedPtr<FJsonValue>|typename\s+Policy::FValue)\s*&\s*\w+\s*,\s*const\s+FString\s*&"),
    ("IHttpRequest::GetStatus / GetResponse", R + "Online/HTTP/Public/Interfaces/IHttpRequest.h|" + R + "Online/HTTP/Public/Interfaces/IHttpBase.h", r"GetStatus\s*\([\s\S]*GetResponse\s*\(|GetResponse\s*\([\s\S]*GetStatus\s*\("),
    ("EHttpRequestStatus::IsFinished", R + "Online/HTTP/Public/Interfaces/IHttpBase.h|" + R + "Online/HTTP/Public/Interfaces/IHttpRequest.h", r"IsFinished"),
    ("FHttpManager::Tick", R + "Online/HTTP/Public/HttpManager.h", r"Tick\s*\(\s*float"),
    ("FGenericPlatformHttp::UrlEncode", R + "Online/HTTP/Public/GenericPlatform/GenericPlatformHttp.h", r"UrlEncode"),
    ("FSocket::Wait(FTimespan)", R + "Sockets/Public/Sockets.h", r"Wait\s*\(\s*ESocketWaitConditions::Type[^)]*FTimespan"),
    ("UDeveloperSettings::GetCategoryName", R + "DeveloperSettings/Public/Engine/DeveloperSettings.h", r"GetCategoryName"),
    ("UGameInstanceSubsystem::ShouldCreateSubsystem", R + "Engine/Public/Subsystems/Subsystem.h", r"ShouldCreateSubsystem"),
    ("SVerticalBox slot MaxHeight", R + "SlateCore/Public/Widgets/SBoxPanel.h", r"MaxHeight"),
    ("SMultiLineEditableTextBox::ScrollTo", R + "Slate/Public/Widgets/Input/SMultiLineEditableTextBox.h", r"ScrollTo\s*\("),
    ("UToolMenus::RegisterStartupCallback returns a handle", "Engine/Source/Developer/ToolMenus/Public/ToolMenus.h", r"FDelegateHandle\s+RegisterStartupCallback"),
    ("UToolMenus::UnRegisterStartupCallback(FDelegateHandle)", "Engine/Source/Developer/ToolMenus/Public/ToolMenus.h", r"UnRegisterStartupCallback\s*\(\s*FDelegateHandle"),
    ("UToolMenu::FindSection / AddSection(name, label)", "Engine/Source/Developer/ToolMenus/Public/ToolMenu.h", r"FToolMenuSection&\s+AddSection\s*\(\s*const\s+FName\s+\w+\s*,\s*const\s+TAttribute[\s\S]*FToolMenuSection\*\s+FindSection\s*\(|FToolMenuSection\*\s+FindSection\s*\([\s\S]*FToolMenuSection&\s+AddSection\s*\(\s*const\s+FName\s+\w+\s*,\s*const\s+TAttribute"),
    ("Policies/CondensedJsonPrintPolicy.h", R + "Json/Public/Policies/CondensedJsonPrintPolicy.h", r"TCondensedJsonPrintPolicy"),
    ("FDetailsViewArgs::HideNameArea", "Engine/Source/Editor/PropertyEditor/Public/PropertyEditorDelegates.h|Engine/Source/Editor/PropertyEditor/Public/IDetailsView.h|Engine/Source/Editor/PropertyEditor/Public/DetailsViewArgs.h", r"HideNameArea"),
]


def fetch(repo, tag, path):
    out = subprocess.run(["gh", "api", "-H", "Accept: application/vnd.github.raw", f"repos/{repo}/contents/{path}?ref={tag}"],
                         capture_output=True)
    return out.stdout.decode("utf-8", "ignore") if out.returncode == 0 else None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--repo", default="EpicGames/UnrealEngine")
    parser.add_argument("--tags", nargs="*", default=DEFAULT_TAGS)
    args = parser.parse_args()

    cache, failures = {}, 0
    for tag in args.tags:
        missing = []
        for label, paths, pattern in CHECKS:
            text = None
            for path in paths.split("|"):
                key = (tag, path)
                if key not in cache:
                    cache[key] = fetch(args.repo, tag, path)
                if cache[key] is not None:
                    text = (text or "") + cache[key]
            if text is None:
                missing.append(f"{label} (header not found)")
            elif not re.search(pattern, text):
                missing.append(label)
        version = tag.replace("-release", "")
        # The SDK only uses the old automation mask before 5.5 (see EdgegapCompat.h).
        if version >= "5.5":
            missing = [m for m in missing if "used < 5.5" not in m]
        status = "OK" if not missing else "MISSING: " + "; ".join(missing)
        print(f"{tag:16} {status}")
        failures += bool(missing)
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
