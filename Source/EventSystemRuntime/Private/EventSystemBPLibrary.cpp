// Copyright Epic Games, Inc. All Rights Reserved.

#include "EventSystemBPLibrary.h"
#include "EdGraphSchema_K2.h"
#include "Engine/UserDefinedEnum.h"
extern ENGINE_API FString GetPathPostfix(const UObject* ForObject);
UEventSystemBPLibrary::UEventSystemBPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

FString UEventSystemBPLibrary::GetParameterType(const FEdGraphPinType& Type)
{// A temp uproperty should be generated?
	auto PinTypeToNativeTypeInner = [](const FEdGraphPinType& InType) -> FString
	{
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
				const FString FullyQualifiedEnumName = (!Enum->CppType.IsEmpty()) ? Enum->CppType : UEventSystemBPLibrary::GetCppName(Enum);
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
				return UEventSystemBPLibrary::GetCppName(Struct);//InType.PinCategory.ToString();//
			}
		}
		else if (UEdGraphSchema_K2::PC_Class == InType.PinCategory)
		{
			if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			{
				return FString::Printf(TEXT("TSubclassOf<%s>"), *UEventSystemBPLibrary::GetCppName(Class));
			}
		}
		else if (UEdGraphSchema_K2::PC_SoftClass == InType.PinCategory)
		{
			if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			{
				return FString::Printf(TEXT("TSoftClassPtr<%s>"), *UEventSystemBPLibrary::GetCppName(Class));
			}
		}
		else if (UEdGraphSchema_K2::PC_Interface == InType.PinCategory)
		{
			if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			{
				return FString::Printf(TEXT("TScriptInterface<%s>"), *UEventSystemBPLibrary::GetCppName(Class));
			}
		}
		else if (UEdGraphSchema_K2::PC_SoftObject == InType.PinCategory)
		{
			if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			{
				return FString::Printf(TEXT("TSoftObjectPtr<%s>"), *UEventSystemBPLibrary::GetCppName(Class));
			}
		}
		else if (UEdGraphSchema_K2::PC_Object == InType.PinCategory)
		{
			if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			{
				return FString::Printf(TEXT("%s"), *UEventSystemBPLibrary::GetCppName(Class));
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
		//UE_LOG(LogK2Compiler, Error, TEXT("FEmitHelper::DefaultValue cannot generate an array type"));
		return FString{};
	};

	FString InnerTypeName = PinTypeToNativeTypeInner(Type);

	if (Type.IsArray())
	{
		return Type.IsArray() ? FString::Printf(TEXT("TArray<%s>"), *InnerTypeName) : InnerTypeName;
	}
	else if (Type.IsSet())
	{
		return Type.IsSet() ? FString::Printf(TEXT("TSet<%s>"), *InnerTypeName) : InnerTypeName;
	}
	else if (Type.IsMap())
	{
		FEdGraphPinType ValuePinType;
		ValuePinType.PinCategory = Type.PinValueType.TerminalCategory;
		ValuePinType.PinSubCategory = Type.PinValueType.TerminalSubCategory;
		ValuePinType.PinSubCategoryObject = Type.PinValueType.TerminalSubCategoryObject;
		ValuePinType.bIsWeakPointer = Type.PinValueType.bTerminalIsWeakPointer;
		auto KeyType = Type;
		KeyType.ContainerType = EPinContainerType::None;
		return FString::Printf(TEXT("TMap<%s,%s>"), *PinTypeToNativeTypeInner(KeyType),*PinTypeToNativeTypeInner(ValuePinType));
	}
	return InnerTypeName;
}

FString UEventSystemBPLibrary::GetCppName(FFieldVariant Field, bool bUInterface /*= false*/, bool bForceParameterNameModification /*= false*/)
{
	check(Field);
	const UClass* AsClass = Field.Get<UClass>();
	const UScriptStruct* AsScriptStruct = Field.Get<UScriptStruct>();
	if (AsClass || AsScriptStruct)
	{
		if (AsClass)
		{
			return *AsClass->GetName();
		}
		else
		{
			return *AsScriptStruct->GetName();
		}
		if (AsClass && AsClass->HasAnyClassFlags(CLASS_Interface))
		{
			ensure(AsClass->IsChildOf<UInterface>());
			return FString::Printf(TEXT("%s%s")
				, bUInterface ? TEXT("U") : TEXT("I")
				, *AsClass->GetName());
		}
		const UStruct* AsStruct = Field.Get<UStruct>();
		check(AsStruct);
		if (AsStruct->IsNative())
		{
			return FString::Printf(TEXT("%s%s"), AsStruct->GetPrefixCPP(), *AsStruct->GetName());
		}
		else
		{
			return ::UnicodeToCPPIdentifier(*AsStruct->GetName(), false, AsStruct->GetPrefixCPP()) + GetPathPostfix(AsStruct);
		}
	}
	else if (const FProperty* AsProperty = Field.Get<FProperty>())
	{
		const UStruct* Owner = AsProperty->GetOwnerStruct();
		const bool bModifyName = ensure(Owner)
			&& (Cast<UBlueprintGeneratedClass>(Owner) || !Owner->IsNative() || bForceParameterNameModification);
		if (bModifyName)
		{
			FString VarPrefix;

			const bool bIsUberGraphVariable = Owner->IsA<UBlueprintGeneratedClass>() && AsProperty->HasAllPropertyFlags(CPF_Transient | CPF_DuplicateTransient);
			const bool bIsParameter = AsProperty->HasAnyPropertyFlags(CPF_Parm);
			const bool bFunctionLocalVariable = Owner->IsA<UFunction>();
			if (bIsUberGraphVariable)
			{
				int32 InheritenceLevel = GetInheritenceLevel(Owner);
				VarPrefix = FString::Printf(TEXT("b%dl__"), InheritenceLevel);
			}
			else if (bIsParameter)
			{
				VarPrefix = TEXT("bpp__");
			}
			else if (bFunctionLocalVariable)
			{
				VarPrefix = TEXT("bpfv__");
			}
			else
			{
				VarPrefix = TEXT("bpv__");
			}
			return ::UnicodeToCPPIdentifier(AsProperty->GetName(), AsProperty->HasAnyPropertyFlags(CPF_Deprecated), *VarPrefix);
		}
		return AsProperty->GetNameCPP();
	}

	if (Field.IsA<UUserDefinedEnum>())
	{
		return ::UnicodeToCPPIdentifier(Field.GetName(), false, TEXT("E__"));
	}

	if (!Field.IsNative())
	{
		return ::UnicodeToCPPIdentifier(Field.GetName(), false, TEXT("bpf__"));
	}
	return Field.GetName();
}

int32 UEventSystemBPLibrary::GetInheritenceLevel(const UStruct* Struct)
{
	const UStruct* StructIt = Struct ? Struct->GetSuperStruct() : nullptr;
	int32 InheritenceLevel = 0;
	while ((StructIt != nullptr) && !StructIt->IsNative())
	{
		++InheritenceLevel;
		StructIt = StructIt->GetSuperStruct();
	}
	return InheritenceLevel;
}