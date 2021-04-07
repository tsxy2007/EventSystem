// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#pragma once

#if !defined(UNREAL_COMPATIBILITY_GUARD_H)
#define UNREAL_COMPATIBILITY_GUARD_H

#include "Runtime/Launch/Resources/Version.h"
#include "UObject/UnrealType.h"

struct FEdGraphPinType;
template<typename T,typename A,typename... TArgs>
inline auto& Add_GetRef(TArray<T, A>& Arr, TArgs&&... Args)
{
	const auto Index = Arr.AddUninitialized(1);
	T* Ptr = Arr.GetData() + Index;
	new (Ptr)T(Forward<TArgs>(Args)...);
	return *Ptr;
}
#endif  // !defined(UNREAL_COMPATIBILITY_GUARD_H)