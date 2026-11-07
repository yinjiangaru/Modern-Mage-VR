// Copyright BMK Games LLC. All Rights Reserved.

#include "SurfaceNavAIDataProvider.h"

#include "CoreFwd.h"
#include "UObject/CoreObjectVersion.h"
#include "Math/IntVector.h"
#include "Containers/UnrealString.h"


//////////////////////////////////////////////////////////////////////////
// FAIDataProviderFIntVectorValue

FAIDataProviderFIntVectorValue::FAIDataProviderFIntVectorValue()
	: DefaultValue(FIntVector::ZeroValue)
{
	PropertyType = FIntProperty::StaticClass();
}

FIntVector FAIDataProviderFIntVectorValue::GetValue() const
{
	FIntVector* PropValue = GetRawValuePtr<FIntVector>();
	return PropValue ? *PropValue : DefaultValue;
}

FString FAIDataProviderFIntVectorValue::ValueToString() const
{
	return TTypeToString<FString>().ToSanitizedString(DefaultValue.ToString());
}

//////////////////////////////////////////////////////////////////////////
// FAIDataProviderFVectorValue

FAIDataProviderFVectorValue::FAIDataProviderFVectorValue()
	: DefaultValue(FVector::ZeroVector)
{
	PropertyType = FFloatProperty::StaticClass();
}

FVector FAIDataProviderFVectorValue::GetValue() const
{
	FVector* PropValue = GetRawValuePtr<FVector>();
	return PropValue ? *PropValue : DefaultValue;
}

FString FAIDataProviderFVectorValue::ValueToString() const
{
	return TTypeToString<FString>().ToSanitizedString(DefaultValue.ToString());
}