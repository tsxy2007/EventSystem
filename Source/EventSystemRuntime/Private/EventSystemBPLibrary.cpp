// Copyright Epic Games, Inc. All Rights Reserved.

#include "EventSystemBPLibrary.h"

UEventSystemBPLibrary::UEventSystemBPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

FName UEventSystemBPLibrary::GetParameterType(const FEdGraphPinType& Type)
{// A temp uproperty should be generated?
	auto PinTypeToNativeTypeInner = [](const FEdGraphPinType& InType) -> FString
	{
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		if (UEdGraphSchema_K2::PC_String == InType.PinCategory)
		{
			return TEXT("FString");
		}
		else if (UEdGraphSchema_K2::PC_Boolean == InType.PinCategory)
		{
			return TEXT("bool");
		}
		else if ((UEdGraphSchema_K2::PC_Byte == InType.PinCategory) || (UEdGraphSchema_K2::PC_Enum == InType.PinCategory))
		{
			if (UEnum* Enum = Cast<UEnum>(InType.PinSubCategoryObject.Get()))
			{
				const bool bEnumClassForm = Enum->GetCppForm() == UEnum::ECppForm::EnumClass;
				const bool bNonNativeEnum = Enum->GetClass() != UEnum::StaticClass();
				ensure(!bNonNativeEnum || Enum->CppType.IsEmpty());
				const FString FullyQualifiedEnumName = (!Enum->CppType.IsEmpty()) ? Enum->CppType : FEmitHelper::GetCppName(Enum);
				// TODO: sometimes we need unwrapped type for enums without size specified. For example when native function has a raw ref param.
				return (bEnumClassForm || bNonNativeEnum) ? FullyQualifiedEnumName : FString::Printf(TEXT("TEnumAsByte<%s>"), *FullyQualifiedEnumName);
			}
			return TEXT("uint8");
		}
		else if (UEdGraphSchema_K2::PC_Int == InType.PinCategory)
		{
			return TEXT("int32");
		}
		else if (UEdGraphSchema_K2::PC_Int64 == InType.PinCategory)
		{
			return TEXT("int64");
		}
		else if (UEdGraphSchema_K2::PC_Float == InType.PinCategory)
		{
			return TEXT("float");
		}
		else if (UEdGraphSchema_K2::PC_Name == InType.PinCategory)
		{
			return TEXT("FName");
		}
		else if (UEdGraphSchema_K2::PC_Text == InType.PinCategory)
		{
			return TEXT("FText");
		}
		else if (UEdGraphSchema_K2::PC_Struct == InType.PinCategory)
		{
			if (UScriptStruct* Struct = Cast<UScriptStruct>(InType.PinSubCategoryObject.Get()))
			{
				return FEmitHelper::GetCppName(Struct);
			}
		}
		else if (UEdGraphSchema_K2::PC_Class == InType.PinCategory)
		{
			if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			{
				return FString::Printf(TEXT("TSubclassOf<%s>"), *FEmitHelper::GetCppName(Class));
			}
		}
		else if (UEdGraphSchema_K2::PC_SoftClass == InType.PinCategory)
		{
			if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			{
				return FString::Printf(TEXT("TSoftClassPtr<%s>"), *FEmitHelper::GetCppName(Class));
			}
		}
		else if (UEdGraphSchema_K2::PC_Interface == InType.PinCategory)
		{
			if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			{
				return FString::Printf(TEXT("TScriptInterface<%s>"), *FEmitHelper::GetCppName(Class));
			}
		}
		else if (UEdGraphSchema_K2::PC_SoftObject == InType.PinCategory)
		{
			if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			{
				return FString::Printf(TEXT("TSoftObjectPtr<%s>"), *FEmitHelper::GetCppName(Class));
			}
		}
		else if (UEdGraphSchema_K2::PC_Object == InType.PinCategory)
		{
			if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			{
				return FString::Printf(TEXT("%s*"), *FEmitHelper::GetCppName(Class));
			}
		}
		else if (UEdGraphSchema_K2::PC_FieldPath == InType.PinCategory)
		{
			// @todo: FProp
			check(false);
			//if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			//{
			//	return FString::Printf(TEXT("TSoftObjectPtr<%s>"), *FEmitHelper::GetCppName(Class));
			//}
		}
		UE_LOG(LogK2Compiler, Error, TEXT("FEmitHelper::DefaultValue cannot generate an array type"));
		return FString{};
	};

	FString InnerTypeName = PinTypeToNativeTypeInner(Type);
	ensure(!Type.IsSet() && !Type.IsMap());
	return Type.IsArray() ? FString::Printf(TEXT("TArray<%s>"), *InnerTypeName) : InnerTypeName;
}

