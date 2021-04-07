// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Forward declarations
class UEdGraphPin;

/** Utility functions for gameplay tag-related pins */
namespace EventPinUtilities
{
	/**
	 * Given a editor graph pin representing a tag or container, extract the appropriate filter
	 * string from any associated metadata
	 * 
	 * @param InTagPin	Pin to extract from
	 * 
	 * @return Filter string, if any, to apply from the tag pin based on metadata
	 */
	FString ExtractTagFilterStringFromGraphPin(UEdGraphPin* InTagPin);
}
