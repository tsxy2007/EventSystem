// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#include "EventAssetInterface.h"

UEventAssetInterface::UEventAssetInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool IEventAssetInterface::HasMatchingEvent(FEventInfo TagToCheck) const
{
	FEventContainer OwnedTags;
	GetOwnedEvents(OwnedTags);

	return OwnedTags.HasTag(TagToCheck);
}

bool IEventAssetInterface::HasAllMatchingEvents(const FEventContainer& TagContainer) const
{
	FEventContainer OwnedTags;
	GetOwnedEvents(OwnedTags);

	return OwnedTags.HasAll(TagContainer);
}

bool IEventAssetInterface::HasAnyMatchingEvents(const FEventContainer& TagContainer) const
{
	FEventContainer OwnedTags;
	GetOwnedEvents(OwnedTags);

	return OwnedTags.HasAny(TagContainer);
}
