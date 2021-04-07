// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#include "BlueprintEventLibrary.h"
#include "EventsRuntimeModule.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

UBlueprintEventLibrary::UBlueprintEventLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBlueprintEventLibrary::MatchesTag(FEventInfo TagOne, FEventInfo TagTwo, bool bExactMatch)
{
	if (bExactMatch)
	{
		return TagOne.MatchesTagExact(TagTwo);
	}

	return TagOne.MatchesTag(TagTwo);
}

bool UBlueprintEventLibrary::MatchesAnyTags(FEventInfo TagOne, const FEventContainer& OtherContainer, bool bExactMatch)
{
	if (bExactMatch)
	{
		return TagOne.MatchesAnyExact(OtherContainer);
	}
	return TagOne.MatchesAny(OtherContainer);
}

bool UBlueprintEventLibrary::EqualEqual_Event(FEventInfo A, FEventInfo B)
{
	return A == B;
}

bool UBlueprintEventLibrary::NotEqual_Event(FEventInfo A, FEventInfo B)
{
	return A != B;
}

bool UBlueprintEventLibrary::IsEventValid(FEventInfo Event)
{
	return Event.IsValid();
}

FName UBlueprintEventLibrary::GetTagName(const FEventInfo& Event)
{
	return Event.GetTagName();
}

FEventInfo UBlueprintEventLibrary::MakeLiteralEvent(FEventInfo Value)
{
	return Value;
}

int32 UBlueprintEventLibrary::GetNumEventsInContainer(const FEventContainer& TagContainer)
{
	return TagContainer.Num();
}

bool UBlueprintEventLibrary::HasTag(const FEventContainer& TagContainer, FEventInfo Tag, bool bExactMatch)
{
	if (bExactMatch)
	{
		return TagContainer.HasTagExact(Tag);
	}
	return TagContainer.HasTag(Tag);
}

bool UBlueprintEventLibrary::HasAnyTags(const FEventContainer& TagContainer, const FEventContainer& OtherContainer, bool bExactMatch)
{
	if (bExactMatch)
	{
		return TagContainer.HasAnyExact(OtherContainer);
	}
	return TagContainer.HasAny(OtherContainer);
}

bool UBlueprintEventLibrary::HasAllTags(const FEventContainer& TagContainer, const FEventContainer& OtherContainer, bool bExactMatch)
{
	if (bExactMatch)
	{
		return TagContainer.HasAllExact(OtherContainer);
	}
	return TagContainer.HasAll(OtherContainer);
}

bool UBlueprintEventLibrary::IsTagQueryEmpty(const FEventQuery& TagQuery)
{
	return TagQuery.IsEmpty();
}

bool UBlueprintEventLibrary::DoesContainerMatchTagQuery(const FEventContainer& TagContainer, const FEventQuery& TagQuery)
{
	return TagQuery.Matches(TagContainer);
}

void UBlueprintEventLibrary::GetAllActorsOfClassMatchingTagQuery(UObject* WorldContextObject,
																	   TSubclassOf<AActor> ActorClass,
																	   const FEventQuery& EventQuery,
																	   TArray<AActor*>& OutActors)
{
	OutActors.Empty();

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

	// We do nothing if not class provided, rather than giving ALL actors!
	if (ActorClass && World)
	{
		bool bHasLoggedMissingInterface = false;
		for (TActorIterator<AActor> It(World, ActorClass); It; ++It)
		{
			AActor* Actor = *It;
			check(Actor != nullptr);
			if (!Actor->IsPendingKill())
			{
				IEventAssetInterface* EventAssetInterface = Cast<IEventAssetInterface>(Actor);
				if (EventAssetInterface != nullptr)
				{
					FEventContainer OwnedEvents;
					EventAssetInterface->GetOwnedEvents(OwnedEvents);

					if (OwnedEvents.MatchesQuery(EventQuery))
					{
						OutActors.Add(Actor);
					}
				}
				else
				{
					if (!bHasLoggedMissingInterface)
					{
						UE_LOG(LogEvents, Warning,
							TEXT("At least one actor (%s) of class %s does not implement IGameplTagAssetInterface.  Unable to find owned tags, so cannot determine if actor matches gameplay tag query.  Presuming it does not."),
							*Actor->GetName(), *ActorClass->GetName());
						bHasLoggedMissingInterface = true;
					}
				}
			}
		}
	}
}

bool UBlueprintEventLibrary::EqualEqual_EventContainer(const FEventContainer& A, const FEventContainer& B)
{
	return A == B;
}

bool UBlueprintEventLibrary::NotEqual_EventContainer(const FEventContainer& A, const FEventContainer& B)
{
	return A != B;
}

FEventContainer UBlueprintEventLibrary::MakeLiteralEventContainer(FEventContainer Value)
{
	return Value;
}

FEventContainer UBlueprintEventLibrary::MakeEventContainerFromArray(const TArray<FEventInfo>& Events)
{
	return FEventContainer::CreateFromArray(Events);
}

FEventContainer UBlueprintEventLibrary::MakeEventContainerFromTag(FEventInfo SingleTag)
{
	return FEventContainer(SingleTag);
}

void UBlueprintEventLibrary::BreakEventContainer(const FEventContainer& EventContainer, TArray<FEventInfo>& Events)
{
	EventContainer.GetEventArray(Events);
}

FEventQuery UBlueprintEventLibrary::MakeEventQuery(FEventQuery TagQuery)
{
	return TagQuery;
}

bool UBlueprintEventLibrary::HasAllMatchingEvents(TScriptInterface<IEventAssetInterface> TagContainerInterface, const FEventContainer& OtherContainer)
{
	if (TagContainerInterface.GetInterface() == NULL)
	{
		return (OtherContainer.Num() == 0);
	}

	FEventContainer OwnedTags;
	TagContainerInterface->GetOwnedEvents(OwnedTags);
	return (OwnedTags.HasAll(OtherContainer));
}

bool UBlueprintEventLibrary::DoesTagAssetInterfaceHaveTag(TScriptInterface<IEventAssetInterface> TagContainerInterface, FEventInfo Tag)
{
	if (TagContainerInterface.GetInterface() == NULL)
	{
		return false;
	}

	FEventContainer OwnedTags;
	TagContainerInterface->GetOwnedEvents(OwnedTags);
	return (OwnedTags.HasTag(Tag));
}

void UBlueprintEventLibrary::AppendEventContainers(FEventContainer& InOutTagContainer, const FEventContainer& InTagContainer)
{
	InOutTagContainer.AppendTags(InTagContainer);
}

void UBlueprintEventLibrary::AddEvent(FEventContainer& TagContainer, FEventInfo Tag)
{
	TagContainer.AddTag(Tag);
}

bool UBlueprintEventLibrary::RemoveEvent(FEventContainer& TagContainer, FEventInfo Tag)
{
	return TagContainer.RemoveTag(Tag);
}

bool UBlueprintEventLibrary::NotEqual_TagTag(FEventInfo A, FString B)
{
	return A.ToString() != B;
}

bool UBlueprintEventLibrary::NotEqual_TagContainerTagContainer(FEventContainer A, FString B)
{
	FEventContainer TagContainer;

	const FString OpenParenthesesStr(TEXT("("));
	const FString CloseParenthesesStr(TEXT(")"));

	// Convert string to Tag Container before compare
	FString TagString = MoveTemp(B);
	if (TagString.StartsWith(OpenParenthesesStr, ESearchCase::CaseSensitive) && TagString.EndsWith(CloseParenthesesStr, ESearchCase::CaseSensitive))
	{
		TagString.LeftChopInline(1, false);
		TagString.RightChopInline(1, false);

		const FString EqualStr(TEXT("="));

		TagString.Split(EqualStr, nullptr, &TagString, ESearchCase::CaseSensitive);

		TagString.LeftChopInline(1, false);
		TagString.RightChopInline(1, false);

		FString ReadTag;
		FString Remainder;

		const FString CommaStr(TEXT(","));
		const FString QuoteStr(TEXT("\""));

		while (TagString.Split(CommaStr, &ReadTag, &Remainder, ESearchCase::CaseSensitive))
		{
			ReadTag.Split(EqualStr, nullptr, &ReadTag, ESearchCase::CaseSensitive);
			if (ReadTag.EndsWith(CloseParenthesesStr, ESearchCase::CaseSensitive))
			{
				ReadTag.LeftChopInline(1, false);
				if (ReadTag.StartsWith(QuoteStr, ESearchCase::CaseSensitive) && ReadTag.EndsWith(QuoteStr, ESearchCase::CaseSensitive))
				{
					ReadTag.LeftChopInline(1, false);
					ReadTag.RightChopInline(1, false);
				}
			}
			TagString = Remainder;

			const FEventInfo Tag = FEventInfo::RequestEvent(FName(*ReadTag));
			TagContainer.AddTag(Tag);
		}
		if (Remainder.IsEmpty())
		{
			Remainder = MoveTemp(TagString);
		}
		if (!Remainder.IsEmpty())
		{
			Remainder.Split(EqualStr, nullptr, &Remainder, ESearchCase::CaseSensitive);
			if (Remainder.EndsWith(CloseParenthesesStr, ESearchCase::CaseSensitive))
			{
				Remainder.LeftChopInline(1, false);
				if (Remainder.StartsWith(QuoteStr, ESearchCase::CaseSensitive) && Remainder.EndsWith(QuoteStr, ESearchCase::CaseSensitive))
				{
					Remainder.LeftChopInline(1, false);
					Remainder.RightChopInline(1, false);
				}
			}
			const FEventInfo Tag = FEventInfo::RequestEvent(FName(*Remainder));
			TagContainer.AddTag(Tag);
		}
	}

	return A != TagContainer;
}
FString UBlueprintEventLibrary::GetDebugStringFromEventContainer(const FEventContainer& TagContainer)
{
	return TagContainer.ToStringSimple();
}

FString UBlueprintEventLibrary::GetDebugStringFromEvent(FEventInfo Event)
{
	return Event.ToString();
}
