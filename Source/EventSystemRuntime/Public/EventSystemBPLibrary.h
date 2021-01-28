// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Systems/GIEventSubsystem.h"
#include "EventSystemBPLibrary.generated.h"

/* 
*	Function library class.
*	Each function in it is expected to be static and represents blueprint node that can be called in any blueprint.
*
*	When declaring function you can define metadata for the node. Key function specifiers will be BlueprintPure and BlueprintCallable.
*	BlueprintPure - means the function does not affect the owning object in any way and thus creates a node without Exec pins.
*	BlueprintCallable - makes a function which can be executed in Blueprints - Thus it has Exec pins.
*	DisplayName - full name of the node, shown when you mouse over the node and in the blueprint drop down menu.
*				Its lets you name the node using characters not allowed in C++ function names.
*	CompactNodeTitle - the word(s) that appear on the node.
*	Keywords -	the list of keywords that helps you to find node when you search for it using Blueprint drop-down menu. 
*				Good example is "Print String" node which you can find also by using keyword "log".
*	Category -	the category your node will be under in the Blueprint drop-down menu.
*
*	For more info on custom blueprint nodes visit documentation:
*	https://wiki.unrealengine.com/Custom_Blueprint_Node_Creation
*/
UCLASS()
class UEventSystemBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Bind Event by Event", ScriptName = "SetEvent", DefaultToSelf = "Object", AdvancedDisplay = "InitialStartDelay, InitialStartDelayVariance"), Category = "Utilities|EventSystem")
	static FEventHandle BindEventToSystemDelegate( FString MsgType, FEventSystemDelegate Delegate);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Bind Event by Function Name", ScriptName = "SetEvent", DefaultToSelf = "Object", AdvancedDisplay = "InitialStartDelay, InitialStartDelayVariance"), Category = "Utilities|EventSystem")
	static FEventHandle BindEventToSystem(FString MsgType, UObject* Object, FString FunctionName);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "UnBind Event By Handle", ScriptName = "UnBindEvent", DefaultToSelf = "Object"), Category = "Utilities|EventSystem")
	static void UnBindEventByHandle(const UObject* WorldContextObject, const FEventHandle& Handle);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "UnBind MsgType Event By Object", ScriptName = "UnBindEvent", DefaultToSelf = "Object"), Category = "Utilities|EventSystem")
		static void UnBindEventByObject(const UObject* WorldContextObject, FString MsgType,UObject* Object);


	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Broadcast Event", ScriptName = "UnBindEvent", DefaultToSelf = "Object"), Category = "Utilities|EventSystem")
	static void Broadcast(const UObject* WorldContextObject, FString MsgType, const FEventBase& Event);



	UFUNCTION(BlueprintCallable, CustomThunk, meta = (DisplayName = "Post Message", CompactNodeTitle = "POSTMSG"), Category = "Utilities|EventSystem")
	static int32 PostMessage(const FString& MsgType, const int32& NewItem);

	DECLARE_FUNCTION(execPostMessage)
	{
	
	}

};
