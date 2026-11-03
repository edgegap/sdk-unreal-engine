// Edgegap SDK: engine-version shims. Supported: UE 5.0 and later.

#pragma once

#include "Runtime/Launch/Resources/Version.h"

#define EDGEGAP_UE_AT_LEAST(Major, Minor) \
	(ENGINE_MAJOR_VERSION > (Major) || (ENGINE_MAJOR_VERSION == (Major) && ENGINE_MINOR_VERSION >= (Minor)))

#if ENGINE_MAJOR_VERSION < 5
	#error "The Edgegap SDK needs Unreal Engine 5.0 or later (Blueprint doubles, FTSTicker)."
#endif

// EAutomationTestFlags became an enum class with free-standing masks in 5.5.
#if EDGEGAP_UE_AT_LEAST(5, 5)
	#define EDGEGAP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
#else
	#define EDGEGAP_TEST_FLAGS (EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
#endif
