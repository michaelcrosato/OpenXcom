// Copyright 2026 UEGT contributors. MIT License.

#include "Content/ContentPackageJson.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace ContentPackageJsonPrivate
{
	void AddDiagnostic(
		FContentPackageParseResult& Result,
		const EContentDiagnosticSeverity Severity,
		const FName Code,
		const FString& Message)
	{
		FContentDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = Severity;
		Diagnostic.Code = Code;
		Diagnostic.PackageId = Result.Package.Descriptor.PackageId;
		Diagnostic.Message = Message;
	}

	bool HasErrors(const FContentPackageParseResult& Result)
	{
		return Result.Diagnostics.ContainsByPredicate(
			[](const FContentDiagnostic& Diagnostic)
			{
				return Diagnostic.Severity == EContentDiagnosticSeverity::Error;
			});
	}

	void WarnUnknownFields(
		const TSharedPtr<FJsonObject>& Object,
		const TSet<FString>& AllowedFields,
		const FString& Context,
		FContentPackageParseResult& Result)
	{
		TArray<FString> UnknownFields;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
		{
			if (!AllowedFields.Contains(Pair.Key))
			{
				UnknownFields.Add(Pair.Key);
			}
		}
		UnknownFields.Sort();
		for (const FString& Field : UnknownFields)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Warning, TEXT("unknown_field"), FString::Printf(TEXT("%s contains unknown field '%s'."), *Context, *Field));
		}
	}

	bool ReadRequiredString(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		FString& OutValue,
		const FString& Context,
		FContentPackageParseResult& Result)
	{
		if (!Object->HasField(Field))
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("missing_field"), FString::Printf(TEXT("%s is missing required string '%s'."), *Context, Field));
			return false;
		}
		if (!Object->TryGetStringField(Field, OutValue))
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.%s must be a string."), *Context, Field));
			return false;
		}
		if (OutValue.TrimStartAndEnd().IsEmpty())
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s.%s cannot be empty."), *Context, Field));
			return false;
		}
		return true;
	}

	bool ReadInteger(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		int32& OutValue,
		const bool bRequired,
		const FString& Context,
		FContentPackageParseResult& Result)
	{
		if (!Object->HasField(Field))
		{
			if (bRequired)
			{
				AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("missing_field"), FString::Printf(TEXT("%s is missing required integer '%s'."), *Context, Field));
				return false;
			}
			return true;
		}

		double Number = 0.0;
		if (!Object->TryGetNumberField(Field, Number)
			|| !FMath::IsFinite(Number)
			|| Number != FMath::TruncToDouble(Number)
			|| Number < static_cast<double>(MIN_int32)
			|| Number > static_cast<double>(MAX_int32))
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.%s must be a 32-bit integer."), *Context, Field));
			return false;
		}

		OutValue = static_cast<int32>(Number);
		return true;
	}

	bool ReadOptionalBool(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		bool& OutValue,
		const FString& Context,
		FContentPackageParseResult& Result)
	{
		if (!Object->HasField(Field))
		{
			return true;
		}
		const TSharedPtr<FJsonValue>* Value = Object->Values.Find(Field);
		if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::Boolean)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.%s must be a boolean."), *Context, Field));
			return false;
		}
		OutValue = (*Value)->AsBool();
		return true;
	}

	bool ReadRequiredBool(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		bool& OutValue,
		const FString& Context,
		FContentPackageParseResult& Result)
	{
		if (!Object->HasField(Field))
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("missing_field"), FString::Printf(TEXT("%s is missing required boolean '%s'."), *Context, Field));
			return false;
		}
		const TSharedPtr<FJsonValue>* Value = Object->Values.Find(Field);
		if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::Boolean)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.%s must be a boolean."), *Context, Field));
			return false;
		}
		OutValue = (*Value)->AsBool();
		return true;
	}

	bool ReadNameArray(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		TArray<FName>& OutValues,
		const FString& Context,
		FContentPackageParseResult& Result)
	{
		if (!Object->HasField(Field))
		{
			return true;
		}

		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object->TryGetArrayField(Field, Values) || Values == nullptr)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.%s must be an array of ids."), *Context, Field));
			return false;
		}

		bool bValid = true;
		TSet<FName> Seen;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			FString Value;
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetString(Value))
			{
				AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.%s[%d] must be an id string."), *Context, Field, Index));
				bValid = false;
				continue;
			}
			const FName Name(*Value);
			if (!FContentPackageResolver::IsValidPackageId(Name))
			{
				AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_rule_id"), FString::Printf(TEXT("%s.%s[%d] contains invalid id '%s'."), *Context, Field, Index, *Value));
				bValid = false;
				continue;
			}
			if (!Seen.Contains(Name))
			{
				Seen.Add(Name);
				OutValues.Add(Name);
			}
		}
		return bValid;
	}

	bool ReadManufacturingInputs(
		const TSharedPtr<FJsonObject>& Object,
		TArray<FManufacturingInputRule>& OutInputs,
		const FString& Context,
		FContentPackageParseResult& Result)
	{
		if (!Object->HasField(TEXT("manufactureInputs")))
		{
			return true;
		}
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object->TryGetArrayField(TEXT("manufactureInputs"), Values) || Values == nullptr)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_type"),
				FString::Printf(TEXT("%s.manufactureInputs must be an array of item quantities."), *Context));
			return false;
		}
		bool bValid = true;
		if (Values->Num() > 16)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"),
				FString::Printf(TEXT("%s.manufactureInputs supports at most 16 entries."), *Context));
			bValid = false;
		}
		TSet<FName> Seen;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* InputObject = nullptr;
			const FString InputContext = FString::Printf(TEXT("%s.manufactureInputs[%d]"), *Context, Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(InputObject)
				|| InputObject == nullptr || !InputObject->IsValid())
			{
				AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_type"),
					FString::Printf(TEXT("%s must be an object."), *InputContext));
				bValid = false;
				continue;
			}
			WarnUnknownFields(*InputObject, { TEXT("itemId"), TEXT("quantity") }, InputContext, Result);
			FString ItemIdText;
			int32 Quantity = 0;
			bool bInputValid = ReadRequiredString(
				*InputObject, TEXT("itemId"), ItemIdText, InputContext, Result);
			bInputValid &= ReadInteger(
				*InputObject, TEXT("quantity"), Quantity, true, InputContext, Result);
			const FName ItemId(*ItemIdText);
			if (bInputValid && !FContentPackageResolver::IsValidPackageId(ItemId))
			{
				AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_rule_id"),
					FString::Printf(TEXT("%s.itemId contains invalid id '%s'."), *InputContext, *ItemIdText));
				bInputValid = false;
			}
			if (bInputValid && Quantity <= 0)
			{
				AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"),
					FString::Printf(TEXT("%s.quantity must be positive."), *InputContext));
				bInputValid = false;
			}
			if (bInputValid && Seen.Contains(ItemId))
			{
				AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("duplicate_manufacturing_input"),
					FString::Printf(TEXT("%s repeats input item '%s'."), *Context, *ItemId.ToString()));
				bInputValid = false;
			}
			if (!bInputValid)
			{
				bValid = false;
				continue;
			}
			Seen.Add(ItemId);
			FManufacturingInputRule& Input = OutInputs.AddDefaulted_GetRef();
			Input.ItemId = ItemId;
			Input.Quantity = Quantity;
		}
		OutInputs.Sort([](const FManufacturingInputRule& Left, const FManufacturingInputRule& Right)
		{
			return Left.ItemId.LexicalLess(Right.ItemId);
		});
		return bValid;
	}

	bool ReadIdentity(
		const TSharedPtr<FJsonObject>& Object,
		FRuleIdentity& OutIdentity,
		const FString& Context,
		FContentPackageParseResult& Result)
	{
		FString RuleId;
		bool bValid = ReadRequiredString(Object, TEXT("id"), RuleId, Context, Result);
		if (bValid)
		{
			OutIdentity.RuleId = FName(*RuleId);
			if (!FContentPackageResolver::IsValidPackageId(OutIdentity.RuleId))
			{
				AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_rule_id"), FString::Printf(TEXT("%s.id '%s' is not a valid namespaced id."), *Context, *RuleId));
				bValid = false;
			}
		}
		bValid &= ReadOptionalBool(Object, TEXT("replace"), OutIdentity.bReplaceExisting, Context, Result);
		return bValid;
	}

	bool ValidateNonNegative(const int32 Value, const TCHAR* Field, const FString& Context, FContentPackageParseResult& Result)
	{
		if (Value < 0)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s.%s cannot be negative."), *Context, Field));
			return false;
		}
		return true;
	}

	bool ValidatePositive(const int32 Value, const TCHAR* Field, const FString& Context, FContentPackageParseResult& Result)
	{
		if (Value <= 0)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s.%s must be positive."), *Context, Field));
			return false;
		}
		return true;
	}

	int64 ComputeTacticalMapCellCount(const int32 Width, const int32 Height, const int32 Levels)
	{
		if (Width < 8 || Width > 64 || Height < 12 || Height > 96 || Levels < 1 || Levels > 4)
		{
			return MAX_int64;
		}
		return static_cast<int64>(Width) * static_cast<int64>(Height) * static_cast<int64>(Levels);
	}

	bool ReadTacticalDamageType(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		ETacticalDamageType& OutValue,
		const FString& Context,
		FContentPackageParseResult& Result)
	{
		if (!Object->HasField(Field))
		{
			return true;
		}
		FString Value;
		if (!ReadRequiredString(Object, Field, Value, Context, Result))
		{
			return false;
		}
		if (Value == TEXT("kinetic"))
		{
			OutValue = ETacticalDamageType::Kinetic;
			return true;
		}
		if (Value == TEXT("thermal"))
		{
			OutValue = ETacticalDamageType::Thermal;
			return true;
		}
		if (Value == TEXT("arc"))
		{
			OutValue = ETacticalDamageType::Arc;
			return true;
		}
		AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s.%s has unknown tactical damage type '%s'."), *Context, Field, *Value));
		return false;
	}

	bool ReadTacticalObjectiveType(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		ETacticalObjectiveType& OutValue,
		const FString& Context,
		FContentPackageParseResult& Result)
	{
		if (!Object->HasField(Field))
		{
			return true;
		}
		FString Value;
		if (!ReadRequiredString(Object, Field, Value, Context, Result))
		{
			return false;
		}
		if (Value == TEXT("disrupt"))
		{
			OutValue = ETacticalObjectiveType::Disrupt;
			return true;
		}
		if (Value == TEXT("recover"))
		{
			OutValue = ETacticalObjectiveType::Recover;
			return true;
		}
		if (Value == TEXT("control"))
		{
			OutValue = ETacticalObjectiveType::Control;
			return true;
		}
		AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s.%s has unknown tactical objective type '%s'."), *Context, Field, *Value));
		return false;
	}

	bool ReadTacticalMissionContext(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		ETacticalMissionContext& OutValue,
		const FString& Context,
		FContentPackageParseResult& Result)
	{
		if (!Object->HasField(Field))
		{
			return true;
		}
		FString Value;
		if (!ReadRequiredString(Object, Field, Value, Context, Result))
		{
			return false;
		}
		if (Value == TEXT("strategicSite"))
		{
			OutValue = ETacticalMissionContext::StrategicSite;
			return true;
		}
		if (Value == TEXT("baseDefense"))
		{
			OutValue = ETacticalMissionContext::BaseDefense;
			return true;
		}
		AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s.%s has unknown tactical mission context '%s'."), *Context, Field, *Value));
		return false;
	}

	bool ReadTacticalSiteType(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		ETacticalSiteType& OutValue,
		const FString& Context,
		FContentPackageParseResult& Result)
	{
		if (!Object->HasField(Field))
		{
			return true;
		}
		FString Value;
		if (!ReadRequiredString(Object, Field, Value, Context, Result))
		{
			return false;
		}
		if (Value == TEXT("wreckage"))
		{
			OutValue = ETacticalSiteType::Wreckage;
			return true;
		}
		if (Value == TEXT("landing"))
		{
			OutValue = ETacticalSiteType::Landing;
			return true;
		}
		AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s.%s has unknown tactical site type '%s'."), *Context, Field, *Value));
		return false;
	}

	bool ReadTacticalAiPosture(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		ETacticalAiPosture& OutValue,
		const FString& Context,
		FContentPackageParseResult& Result)
	{
		if (!Object->HasField(Field))
		{
			return true;
		}
		FString Value;
		if (!ReadRequiredString(Object, Field, Value, Context, Result))
		{
			return false;
		}
		if (Value == TEXT("assault"))
		{
			OutValue = ETacticalAiPosture::Assault;
			return true;
		}
		if (Value == TEXT("signalPressure"))
		{
			OutValue = ETacticalAiPosture::SignalPressure;
			return true;
		}
		if (Value == TEXT("objectivePush"))
		{
			OutValue = ETacticalAiPosture::ObjectivePush;
			return true;
		}
		if (Value == TEXT("sentinel"))
		{
			OutValue = ETacticalAiPosture::Sentinel;
			return true;
		}
		AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s.%s has unknown tactical AI posture '%s'."), *Context, Field, *Value));
		return false;
	}

	bool ParseItem(const TSharedPtr<FJsonObject>& Object, const FString& Context, FItemRule& OutRule, FContentPackageParseResult& Result)
	{
		WarnUnknownFields(Object, { TEXT("id"), TEXT("replace"), TEXT("displayName"), TEXT("category"), TEXT("purchaseCost"), TEXT("sellValue"), TEXT("mass"), TEXT("power"), TEXT("manufactureCost"), TEXT("manufactureHours"), TEXT("manufactureInputs"), TEXT("requires"), TEXT("tacticalRange"), TEXT("tacticalAccuracyModifier"), TEXT("tacticalActionPointCost"), TEXT("tacticalDamageType"), TEXT("tacticalAmmunitionItemId"), TEXT("tacticalMagazineCapacity"), TEXT("tacticalAmmunitionPerAttack"), TEXT("tacticalReloadActionPointCost"), TEXT("tacticalBurstShotCount"), TEXT("tacticalBurstActionPointCost"), TEXT("tacticalBurstAccuracyModifier"), TEXT("tacticalBlastRadius"), TEXT("tacticalScatterRadius"), TEXT("tacticalBlastFalloffPercent"), TEXT("tacticalTerrainDamagePercent"), TEXT("tacticalBlastSmoke"), TEXT("tacticalBlastFire"), TEXT("tacticalBlastSuppression"), TEXT("tacticalRadius"), TEXT("tacticalThrowArcHeight"), TEXT("tacticalSmoke"), TEXT("tacticalFire"), TEXT("tacticalSuppression"), TEXT("tacticalSmokeReduction"), TEXT("tacticalFireReduction"), TEXT("tacticalSuppressionReduction"), TEXT("tacticalMoraleRecovery"), TEXT("tacticalKineticArmor"), TEXT("tacticalThermalArmor"), TEXT("tacticalArcArmor"), TEXT("ammunitionItemId"), TEXT("magazineCapacity"), TEXT("salvoSize"), TEXT("interceptionAccuracy"), TEXT("interceptionDamage"), TEXT("fireIntervalSeconds") }, Context, Result);
		bool bValid = ReadIdentity(Object, OutRule.Identity, Context, Result);
		bValid &= ReadRequiredString(Object, TEXT("displayName"), OutRule.DisplayName, Context, Result);
		FString Category;
		bValid &= ReadRequiredString(Object, TEXT("category"), Category, Context, Result);
		OutRule.Category = FName(*Category);
		bValid &= ReadInteger(Object, TEXT("purchaseCost"), OutRule.PurchaseCost, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("sellValue"), OutRule.SellValue, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("mass"), OutRule.Mass, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("power"), OutRule.Power, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("manufactureCost"), OutRule.ManufactureCost, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("manufactureHours"), OutRule.ManufactureHours, false, Context, Result);
		bValid &= ReadManufacturingInputs(Object, OutRule.ManufactureInputs, Context, Result);
		bValid &= ReadNameArray(Object, TEXT("requires"), OutRule.RequiredResearch, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalRange"), OutRule.TacticalRange, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalAccuracyModifier"), OutRule.TacticalAccuracyModifier, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalActionPointCost"), OutRule.TacticalActionPointCost, false, Context, Result);
		bValid &= ReadTacticalDamageType(Object, TEXT("tacticalDamageType"), OutRule.TacticalDamageType, Context, Result);
		if (Object->HasField(TEXT("tacticalAmmunitionItemId")))
		{
			FString TacticalAmmunitionItemId;
			bValid &= ReadRequiredString(Object, TEXT("tacticalAmmunitionItemId"), TacticalAmmunitionItemId, Context, Result);
			OutRule.TacticalAmmunitionItemId = FName(*TacticalAmmunitionItemId);
		}
		bValid &= ReadInteger(Object, TEXT("tacticalMagazineCapacity"), OutRule.TacticalMagazineCapacity, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalAmmunitionPerAttack"), OutRule.TacticalAmmunitionPerAttack, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalReloadActionPointCost"), OutRule.TacticalReloadActionPointCost, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalBurstShotCount"), OutRule.TacticalBurstShotCount, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalBurstActionPointCost"), OutRule.TacticalBurstActionPointCost, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalBurstAccuracyModifier"), OutRule.TacticalBurstAccuracyModifier, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalBlastRadius"), OutRule.TacticalBlastRadius, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalScatterRadius"), OutRule.TacticalScatterRadius, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalBlastFalloffPercent"), OutRule.TacticalBlastFalloffPercent, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalTerrainDamagePercent"), OutRule.TacticalTerrainDamagePercent, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalBlastSmoke"), OutRule.TacticalBlastSmoke, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalBlastFire"), OutRule.TacticalBlastFire, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalBlastSuppression"), OutRule.TacticalBlastSuppression, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalRadius"), OutRule.TacticalRadius, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalThrowArcHeight"), OutRule.TacticalThrowArcHeight, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalSmoke"), OutRule.TacticalSmoke, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalFire"), OutRule.TacticalFire, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalSuppression"), OutRule.TacticalSuppression, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalSmokeReduction"), OutRule.TacticalSmokeReduction, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalFireReduction"), OutRule.TacticalFireReduction, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalSuppressionReduction"), OutRule.TacticalSuppressionReduction, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalMoraleRecovery"), OutRule.TacticalMoraleRecovery, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalKineticArmor"), OutRule.TacticalKineticArmor, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalThermalArmor"), OutRule.TacticalThermalArmor, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("tacticalArcArmor"), OutRule.TacticalArcArmor, false, Context, Result);
		if (Object->HasField(TEXT("ammunitionItemId")))
		{
			FString AmmunitionItemId;
			bValid &= ReadRequiredString(Object, TEXT("ammunitionItemId"), AmmunitionItemId, Context, Result);
			OutRule.AmmunitionItemId = FName(*AmmunitionItemId);
		}
		bValid &= ReadInteger(Object, TEXT("magazineCapacity"), OutRule.MagazineCapacity, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("salvoSize"), OutRule.SalvoSize, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("interceptionAccuracy"), OutRule.InterceptionAccuracy, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("interceptionDamage"), OutRule.InterceptionDamage, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("fireIntervalSeconds"), OutRule.FireIntervalSeconds, false, Context, Result);
		bValid &= ValidateNonNegative(OutRule.PurchaseCost, TEXT("purchaseCost"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.SellValue, TEXT("sellValue"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.Mass, TEXT("mass"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.Power, TEXT("power"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.ManufactureCost, TEXT("manufactureCost"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.ManufactureHours, TEXT("manufactureHours"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalRange, TEXT("tacticalRange"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalActionPointCost, TEXT("tacticalActionPointCost"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalMagazineCapacity, TEXT("tacticalMagazineCapacity"), Context, Result);
		bValid &= ValidatePositive(OutRule.TacticalAmmunitionPerAttack, TEXT("tacticalAmmunitionPerAttack"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalReloadActionPointCost, TEXT("tacticalReloadActionPointCost"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalBurstShotCount, TEXT("tacticalBurstShotCount"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalBurstActionPointCost, TEXT("tacticalBurstActionPointCost"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalBlastRadius, TEXT("tacticalBlastRadius"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalScatterRadius, TEXT("tacticalScatterRadius"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalBlastFalloffPercent, TEXT("tacticalBlastFalloffPercent"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalTerrainDamagePercent, TEXT("tacticalTerrainDamagePercent"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalBlastSmoke, TEXT("tacticalBlastSmoke"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalBlastFire, TEXT("tacticalBlastFire"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalBlastSuppression, TEXT("tacticalBlastSuppression"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalRadius, TEXT("tacticalRadius"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalThrowArcHeight, TEXT("tacticalThrowArcHeight"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalSmoke, TEXT("tacticalSmoke"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalFire, TEXT("tacticalFire"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalSuppression, TEXT("tacticalSuppression"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalSmokeReduction, TEXT("tacticalSmokeReduction"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalFireReduction, TEXT("tacticalFireReduction"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalSuppressionReduction, TEXT("tacticalSuppressionReduction"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalMoraleRecovery, TEXT("tacticalMoraleRecovery"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalKineticArmor, TEXT("tacticalKineticArmor"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalThermalArmor, TEXT("tacticalThermalArmor"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.TacticalArcArmor, TEXT("tacticalArcArmor"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.MagazineCapacity, TEXT("magazineCapacity"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.SalvoSize, TEXT("salvoSize"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.InterceptionAccuracy, TEXT("interceptionAccuracy"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.InterceptionDamage, TEXT("interceptionDamage"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.FireIntervalSeconds, TEXT("fireIntervalSeconds"), Context, Result);
		if (OutRule.SalvoSize > 16 || OutRule.InterceptionAccuracy > 100)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s interception salvo and accuracy exceed supported limits."), *Context));
			bValid = false;
		}
		const bool bHasAnyTacticalProfile = OutRule.TacticalRange != 0
			|| OutRule.TacticalAccuracyModifier != 0
			|| OutRule.TacticalActionPointCost != 0
			|| OutRule.TacticalBurstShotCount != 0 || OutRule.TacticalBurstActionPointCost != 0
			|| OutRule.TacticalBurstAccuracyModifier != 0 || OutRule.TacticalBlastRadius != 0
			|| OutRule.TacticalScatterRadius != 0
			|| OutRule.TacticalBlastFalloffPercent != 0 || OutRule.TacticalTerrainDamagePercent != 0
			|| OutRule.TacticalBlastSmoke != 0 || OutRule.TacticalBlastFire != 0
			|| OutRule.TacticalBlastSuppression != 0
			|| OutRule.TacticalRadius != 0 || OutRule.TacticalThrowArcHeight != 0 || OutRule.TacticalSmoke != 0
			|| OutRule.TacticalFire != 0 || OutRule.TacticalSuppression != 0
			|| OutRule.TacticalSmokeReduction != 0 || OutRule.TacticalFireReduction != 0
			|| OutRule.TacticalSuppressionReduction != 0 || OutRule.TacticalMoraleRecovery != 0;
		const bool bTacticalAmmunitionValid = OutRule.TacticalAmmunitionItemId.IsNone()
			? OutRule.TacticalMagazineCapacity == 0 && OutRule.TacticalReloadActionPointCost == 0
			: OutRule.IsTacticalWeapon() && FContentPackageResolver::IsValidPackageId(OutRule.TacticalAmmunitionItemId)
				&& OutRule.TacticalMagazineCapacity > 0 && OutRule.TacticalMagazineCapacity <= 200
				&& OutRule.TacticalAmmunitionPerAttack <= OutRule.TacticalMagazineCapacity
				&& OutRule.TacticalReloadActionPointCost > 0 && OutRule.TacticalReloadActionPointCost <= 20;
		const bool bTacticalArmorValid = OutRule.TacticalKineticArmor <= 100
			&& OutRule.TacticalThermalArmor <= 100 && OutRule.TacticalArcArmor <= 100
			&& (OutRule.Category == FName(TEXT("armor"))
				|| (OutRule.TacticalKineticArmor == 0 && OutRule.TacticalThermalArmor == 0 && OutRule.TacticalArcArmor == 0));
		const int64 BurstAmmunitionCost = static_cast<int64>(OutRule.TacticalAmmunitionPerAttack)
			* OutRule.TacticalBurstShotCount;
		const bool bTacticalBurstValid = OutRule.HasTacticalBurstMode()
			? OutRule.IsTacticalWeapon() && OutRule.TacticalBurstShotCount <= 8
				&& OutRule.TacticalBurstActionPointCost <= 20
				&& OutRule.TacticalBurstAccuracyModifier >= -50 && OutRule.TacticalBurstAccuracyModifier <= 0
				&& (OutRule.TacticalAmmunitionItemId.IsNone()
					|| BurstAmmunitionCost <= OutRule.TacticalMagazineCapacity)
				&& !OutRule.HasTacticalBlastProfile()
			: OutRule.TacticalBurstShotCount == 0 && OutRule.TacticalBurstActionPointCost == 0
				&& OutRule.TacticalBurstAccuracyModifier == 0;
		const bool bTacticalBlastValid = OutRule.HasTacticalBlastProfile()
			? OutRule.IsTacticalWeapon() && OutRule.TacticalBlastRadius <= 8
				&& OutRule.TacticalScatterRadius <= 4
				&& OutRule.TacticalBlastFalloffPercent <= 100
				&& OutRule.TacticalTerrainDamagePercent <= 300
				&& OutRule.TacticalBlastSmoke <= 100 && OutRule.TacticalBlastFire <= 100
				&& OutRule.TacticalBlastSuppression <= 100 && !OutRule.HasTacticalBurstMode()
			: OutRule.TacticalBlastRadius == 0 && OutRule.TacticalScatterRadius == 0
				&& OutRule.TacticalBlastFalloffPercent == 0
				&& OutRule.TacticalTerrainDamagePercent == 0 && OutRule.TacticalBlastSmoke == 0
				&& OutRule.TacticalBlastFire == 0 && OutRule.TacticalBlastSuppression == 0;
		const bool bTacticalDeviceValid = OutRule.IsTacticalDevice()
			? OutRule.TacticalRadius <= 8 && OutRule.TacticalThrowArcHeight <= 8 && OutRule.TacticalSmoke <= 100
				&& OutRule.TacticalFire <= 100 && OutRule.TacticalSuppression <= 100
				&& OutRule.TacticalSmokeReduction <= 100 && OutRule.TacticalFireReduction <= 100
				&& OutRule.TacticalSuppressionReduction <= 100 && OutRule.TacticalMoraleRecovery <= 100
				&& (OutRule.TacticalSmoke == 0 || OutRule.TacticalSmokeReduction == 0)
				&& (OutRule.TacticalFire == 0 || OutRule.TacticalFireReduction == 0)
				&& (OutRule.TacticalSuppression == 0 || OutRule.TacticalSuppressionReduction == 0)
				&& OutRule.Power == 0 && OutRule.TacticalAccuracyModifier == 0 && OutRule.TacticalAmmunitionItemId.IsNone()
			: OutRule.TacticalRadius == 0 && OutRule.TacticalThrowArcHeight == 0 && OutRule.TacticalSmoke == 0
				&& OutRule.TacticalFire == 0 && OutRule.TacticalSuppression == 0
				&& OutRule.TacticalSmokeReduction == 0 && OutRule.TacticalFireReduction == 0
				&& OutRule.TacticalSuppressionReduction == 0 && OutRule.TacticalMoraleRecovery == 0;
		if (OutRule.TacticalRange > 64 || OutRule.TacticalAccuracyModifier < -50
			|| OutRule.TacticalAccuracyModifier > 50 || OutRule.TacticalActionPointCost > 20
			|| OutRule.TacticalAmmunitionPerAttack > 20 || !bTacticalAmmunitionValid || !bTacticalArmorValid
			|| !bTacticalBurstValid || !bTacticalBlastValid || !bTacticalDeviceValid
			|| (OutRule.IsTacticalSignalProjector() && OutRule.Power > 100)
			|| (OutRule.Category == FName(TEXT("device")) && !OutRule.IsTacticalDevice())
			|| (bHasAnyTacticalProfile && !OutRule.IsTacticalWeapon() && !OutRule.IsTacticalDevice()
				&& !OutRule.IsTacticalSignalProjector()))
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s tactical item values are incomplete, contradictory, or exceed supported limits."), *Context));
			bValid = false;
		}
		return bValid;
	}

	bool ParseResearch(const TSharedPtr<FJsonObject>& Object, const FString& Context, FResearchRule& OutRule, FContentPackageParseResult& Result)
	{
		WarnUnknownFields(Object, { TEXT("id"), TEXT("replace"), TEXT("displayName"), TEXT("effort"),
			TEXT("prerequisites"), TEXT("requiredFacilities"), TEXT("unlocks") }, Context, Result);
		bool bValid = ReadIdentity(Object, OutRule.Identity, Context, Result);
		bValid &= ReadRequiredString(Object, TEXT("displayName"), OutRule.DisplayName, Context, Result);
		bValid &= ReadInteger(Object, TEXT("effort"), OutRule.Effort, true, Context, Result);
		bValid &= ReadNameArray(Object, TEXT("prerequisites"), OutRule.Prerequisites, Context, Result);
		bValid &= ReadNameArray(Object, TEXT("requiredFacilities"), OutRule.RequiredFacilityIds, Context, Result);
		bValid &= ReadNameArray(Object, TEXT("unlocks"), OutRule.UnlockRuleIds, Context, Result);
		bValid &= ValidatePositive(OutRule.Effort, TEXT("effort"), Context, Result);
		if (OutRule.RequiredFacilityIds.Num() > 4)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"),
				FString::Printf(TEXT("%s.requiredFacilities supports at most 4 facility ids."), *Context));
			bValid = false;
		}
		return bValid;
	}

	bool ParseArchiveEntry(
		const TSharedPtr<FJsonObject>& Object,
		const FString& Context,
		FKnowledgeArchiveEntryRule& OutRule,
		FContentPackageParseResult& Result)
	{
		WarnUnknownFields(Object,
			{ TEXT("id"), TEXT("replace"), TEXT("displayName"), TEXT("category"), TEXT("summary"),
				TEXT("body"), TEXT("sortOrder"), TEXT("requires"), TEXT("relatedEntries") },
			Context, Result);
		bool bValid = ReadIdentity(Object, OutRule.Identity, Context, Result);
		bValid &= ReadRequiredString(Object, TEXT("displayName"), OutRule.DisplayName, Context, Result);
		FString CategoryId;
		bValid &= ReadRequiredString(Object, TEXT("category"), CategoryId, Context, Result);
		OutRule.CategoryId = FName(*CategoryId);
		bValid &= ReadRequiredString(Object, TEXT("summary"), OutRule.Summary, Context, Result);
		bValid &= ReadRequiredString(Object, TEXT("body"), OutRule.Body, Context, Result);
		bValid &= ReadInteger(Object, TEXT("sortOrder"), OutRule.SortOrder, true, Context, Result);
		bValid &= ReadNameArray(Object, TEXT("requires"), OutRule.RequiredResearch, Context, Result);
		bValid &= ReadNameArray(Object, TEXT("relatedEntries"), OutRule.RelatedEntryIds, Context, Result);
		if (!FContentPackageResolver::IsValidPackageId(OutRule.CategoryId)
			|| OutRule.DisplayName.Len() > 96 || OutRule.Summary.Len() > 280 || OutRule.Body.Len() > 4000
			|| OutRule.SortOrder < 0 || OutRule.SortOrder > 100000
			|| OutRule.RequiredResearch.Num() > 8 || OutRule.RelatedEntryIds.Num() > 12)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"),
				FString::Printf(TEXT("%s archive values exceed supported identity, text, order, or link limits."), *Context));
			bValid = false;
		}
		return bValid;
	}

	bool ParseFacility(const TSharedPtr<FJsonObject>& Object, const FString& Context, FFacilityRule& OutRule, FContentPackageParseResult& Result)
	{
		WarnUnknownFields(Object, { TEXT("id"), TEXT("replace"), TEXT("displayName"), TEXT("buildCost"), TEXT("buildHours"), TEXT("monthlyMaintenance"), TEXT("gridWidth"), TEXT("gridHeight"), TEXT("craftCapacity"), TEXT("storageCapacity"), TEXT("scientistCapacity"), TEXT("engineerCapacity"), TEXT("sensorRangeKilometers"), TEXT("detectionStrength"), TEXT("maxIntegrity"), TEXT("repairCostPerIntegrity"), TEXT("repairHoursPerIntegrity"), TEXT("baseDefenseAccuracy"), TEXT("baseDefenseDamage"), TEXT("baseDefenseSupplyItemId"), TEXT("baseDefenseSupplyPerShot"), TEXT("requires") }, Context, Result);
		bool bValid = ReadIdentity(Object, OutRule.Identity, Context, Result);
		bValid &= ReadRequiredString(Object, TEXT("displayName"), OutRule.DisplayName, Context, Result);
		bValid &= ReadInteger(Object, TEXT("buildCost"), OutRule.BuildCost, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("buildHours"), OutRule.BuildHours, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("monthlyMaintenance"), OutRule.MonthlyMaintenance, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("gridWidth"), OutRule.GridWidth, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("gridHeight"), OutRule.GridHeight, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("craftCapacity"), OutRule.CraftCapacity, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("storageCapacity"), OutRule.StorageCapacity, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("scientistCapacity"), OutRule.ScientistCapacity, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("engineerCapacity"), OutRule.EngineerCapacity, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("sensorRangeKilometers"), OutRule.SensorRangeKilometers, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("detectionStrength"), OutRule.DetectionStrength, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("maxIntegrity"), OutRule.MaxIntegrity, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("repairCostPerIntegrity"), OutRule.RepairCostPerIntegrity, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("repairHoursPerIntegrity"), OutRule.RepairHoursPerIntegrity, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("baseDefenseAccuracy"), OutRule.BaseDefenseAccuracy, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("baseDefenseDamage"), OutRule.BaseDefenseDamage, false, Context, Result);
		if (Object->HasField(TEXT("baseDefenseSupplyItemId")))
		{
			FString SupplyItemId;
			bValid &= ReadRequiredString(
				Object, TEXT("baseDefenseSupplyItemId"), SupplyItemId, Context, Result);
			OutRule.BaseDefenseSupplyItemId = FName(*SupplyItemId);
			if (!FContentPackageResolver::IsValidPackageId(OutRule.BaseDefenseSupplyItemId))
			{
				AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_rule_id"),
					FString::Printf(TEXT("%s.baseDefenseSupplyItemId contains invalid id '%s'."),
						*Context, *SupplyItemId));
				bValid = false;
			}
		}
		bValid &= ReadInteger(
			Object, TEXT("baseDefenseSupplyPerShot"), OutRule.BaseDefenseSupplyPerShot, false, Context, Result);
		bValid &= ReadNameArray(Object, TEXT("requires"), OutRule.RequiredResearch, Context, Result);
		bValid &= ValidateNonNegative(OutRule.BuildCost, TEXT("buildCost"), Context, Result);
		bValid &= ValidatePositive(OutRule.BuildHours, TEXT("buildHours"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.MonthlyMaintenance, TEXT("monthlyMaintenance"), Context, Result);
		bValid &= ValidatePositive(OutRule.GridWidth, TEXT("gridWidth"), Context, Result);
		bValid &= ValidatePositive(OutRule.GridHeight, TEXT("gridHeight"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.CraftCapacity, TEXT("craftCapacity"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.StorageCapacity, TEXT("storageCapacity"), Context, Result);
		if (OutRule.StorageCapacity > 1000000)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"),
				FString::Printf(TEXT("%s.storageCapacity cannot exceed 1000000."), *Context));
			bValid = false;
		}
		bValid &= ValidateNonNegative(OutRule.ScientistCapacity, TEXT("scientistCapacity"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.EngineerCapacity, TEXT("engineerCapacity"), Context, Result);
		if (OutRule.ScientistCapacity > 1000000 || OutRule.EngineerCapacity > 1000000)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"),
				FString::Printf(TEXT("%s scientistCapacity and engineerCapacity cannot exceed 1000000."), *Context));
			bValid = false;
		}
		bValid &= ValidateNonNegative(OutRule.SensorRangeKilometers, TEXT("sensorRangeKilometers"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.DetectionStrength, TEXT("detectionStrength"), Context, Result);
		if (OutRule.DetectionStrength > 100)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s.detectionStrength cannot exceed 100."), *Context));
			bValid = false;
		}
		bValid &= ValidatePositive(OutRule.MaxIntegrity, TEXT("maxIntegrity"), Context, Result);
		if (OutRule.MaxIntegrity > 100000)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"),
				FString::Printf(TEXT("%s.maxIntegrity cannot exceed 100000."), *Context));
			bValid = false;
		}
		bValid &= ValidateNonNegative(
			OutRule.RepairCostPerIntegrity, TEXT("repairCostPerIntegrity"), Context, Result);
		bValid &= ValidatePositive(
			OutRule.RepairHoursPerIntegrity, TEXT("repairHoursPerIntegrity"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.BaseDefenseAccuracy, TEXT("baseDefenseAccuracy"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.BaseDefenseDamage, TEXT("baseDefenseDamage"), Context, Result);
		bValid &= ValidateNonNegative(
			OutRule.BaseDefenseSupplyPerShot, TEXT("baseDefenseSupplyPerShot"), Context, Result);
		if (OutRule.BaseDefenseAccuracy > 100
			|| ((OutRule.BaseDefenseAccuracy == 0) != (OutRule.BaseDefenseDamage == 0))
			|| OutRule.BaseDefenseDamage > 100000
			|| OutRule.BaseDefenseSupplyPerShot > 100000
			|| (OutRule.BaseDefenseSupplyItemId.IsNone() != (OutRule.BaseDefenseSupplyPerShot == 0))
			|| (OutRule.BaseDefenseAccuracy == 0 && !OutRule.BaseDefenseSupplyItemId.IsNone()))
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"),
				FString::Printf(TEXT("%s base-defense accuracy, damage, supply item, and per-shot supply must define one complete supported shot."), *Context));
			bValid = false;
		}
		return bValid;
	}

	bool ParsePersonnelCategory(
		const FString& Value,
		EPersonnelRoleCategory& OutCategory,
		const FString& Context,
		FContentPackageParseResult& Result)
	{
		if (Value == TEXT("field-agent"))
		{
			OutCategory = EPersonnelRoleCategory::FieldAgent;
			return true;
		}
		if (Value == TEXT("scientist"))
		{
			OutCategory = EPersonnelRoleCategory::Scientist;
			return true;
		}
		if (Value == TEXT("engineer"))
		{
			OutCategory = EPersonnelRoleCategory::Engineer;
			return true;
		}
		if (Value == TEXT("pilot"))
		{
			OutCategory = EPersonnelRoleCategory::Pilot;
			return true;
		}
		AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s.category '%s' is unknown."), *Context, *Value));
		return false;
	}

	bool ParsePersonnelRole(const TSharedPtr<FJsonObject>& Object, const FString& Context, FPersonnelRoleRule& OutRule, FContentPackageParseResult& Result)
	{
		WarnUnknownFields(Object, { TEXT("id"), TEXT("replace"), TEXT("displayName"), TEXT("category"), TEXT("recruitmentCost"), TEXT("monthlySalary"), TEXT("recruitmentHours"), TEXT("baseHealth"), TEXT("baseAccuracy"), TEXT("baseResolve"), TEXT("baseMobility"), TEXT("baseStrength"), TEXT("requires") }, Context, Result);
		bool bValid = ReadIdentity(Object, OutRule.Identity, Context, Result);
		bValid &= ReadRequiredString(Object, TEXT("displayName"), OutRule.DisplayName, Context, Result);
		FString Category;
		bValid &= ReadRequiredString(Object, TEXT("category"), Category, Context, Result);
		if (!Category.IsEmpty())
		{
			bValid &= ParsePersonnelCategory(Category, OutRule.Category, Context, Result);
		}
		bValid &= ReadInteger(Object, TEXT("recruitmentCost"), OutRule.RecruitmentCost, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("monthlySalary"), OutRule.MonthlySalary, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("recruitmentHours"), OutRule.RecruitmentHours, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("baseHealth"), OutRule.BaseHealth, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("baseAccuracy"), OutRule.BaseAccuracy, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("baseResolve"), OutRule.BaseResolve, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("baseMobility"), OutRule.BaseMobility, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("baseStrength"), OutRule.BaseStrength, true, Context, Result);
		bValid &= ReadNameArray(Object, TEXT("requires"), OutRule.RequiredResearch, Context, Result);
		bValid &= ValidateNonNegative(OutRule.RecruitmentCost, TEXT("recruitmentCost"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.MonthlySalary, TEXT("monthlySalary"), Context, Result);
		bValid &= ValidatePositive(OutRule.RecruitmentHours, TEXT("recruitmentHours"), Context, Result);
		bValid &= ValidatePositive(OutRule.BaseHealth, TEXT("baseHealth"), Context, Result);
		bValid &= ValidatePositive(OutRule.BaseAccuracy, TEXT("baseAccuracy"), Context, Result);
		bValid &= ValidatePositive(OutRule.BaseResolve, TEXT("baseResolve"), Context, Result);
		bValid &= ValidatePositive(OutRule.BaseMobility, TEXT("baseMobility"), Context, Result);
		bValid &= ValidatePositive(OutRule.BaseStrength, TEXT("baseStrength"), Context, Result);
		if (OutRule.BaseHealth > 200 || OutRule.BaseAccuracy > 100 || OutRule.BaseResolve > 100 || OutRule.BaseMobility > 100 || OutRule.BaseStrength > 100)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s base attributes exceed schema limits."), *Context));
			bValid = false;
		}
		return bValid;
	}

	bool ParsePersonnelDoctrine(
		const TSharedPtr<FJsonObject>& Object,
		const FString& Context,
		FPersonnelDoctrineRule& OutRule,
		FContentPackageParseResult& Result)
	{
		WarnUnknownFields(Object, {
			TEXT("id"), TEXT("replace"), TEXT("displayName"), TEXT("summary"), TEXT("maxSelections"),
			TEXT("maxHealthBonus"), TEXT("accuracyBonus"), TEXT("resolveBonus"),
			TEXT("mobilityBonus"), TEXT("strengthBonus") }, Context, Result);
		bool bValid = ReadIdentity(Object, OutRule.Identity, Context, Result);
		bValid &= ReadRequiredString(Object, TEXT("displayName"), OutRule.DisplayName, Context, Result);
		bValid &= ReadRequiredString(Object, TEXT("summary"), OutRule.Summary, Context, Result);
		bValid &= ReadInteger(Object, TEXT("maxSelections"), OutRule.MaxSelections, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("maxHealthBonus"), OutRule.MaxHealthBonus, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("accuracyBonus"), OutRule.AccuracyBonus, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("resolveBonus"), OutRule.ResolveBonus, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("mobilityBonus"), OutRule.MobilityBonus, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("strengthBonus"), OutRule.StrengthBonus, true, Context, Result);
		const int64 TotalBonus = static_cast<int64>(OutRule.MaxHealthBonus) + OutRule.AccuracyBonus
			+ OutRule.ResolveBonus + OutRule.MobilityBonus + OutRule.StrengthBonus;
		if (OutRule.DisplayName.TrimStartAndEnd().IsEmpty() || OutRule.Summary.TrimStartAndEnd().IsEmpty()
			|| OutRule.MaxSelections <= 0 || OutRule.MaxSelections > 10
			|| OutRule.MaxHealthBonus < 0 || OutRule.MaxHealthBonus > 50
			|| OutRule.AccuracyBonus < 0 || OutRule.AccuracyBonus > 25
			|| OutRule.ResolveBonus < 0 || OutRule.ResolveBonus > 25
			|| OutRule.MobilityBonus < 0 || OutRule.MobilityBonus > 25
			|| OutRule.StrengthBonus < 0 || OutRule.StrengthBonus > 25
			|| TotalBonus <= 0)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"),
				FString::Printf(TEXT("%s contains invalid doctrine text, selection limits, or attribute bonuses."), *Context));
			bValid = false;
		}
		return bValid;
	}

	bool ParsePersonnelCommendation(
		const TSharedPtr<FJsonObject>& Object,
		const FString& Context,
		FPersonnelCommendationRule& OutRule,
		FContentPackageParseResult& Result)
	{
		WarnUnknownFields(Object, {
			TEXT("id"), TEXT("replace"), TEXT("displayName"), TEXT("summary"),
			TEXT("requiredMissions"), TEXT("requiredKills"), TEXT("requiredRank"),
			TEXT("requiresSuccessfulMission") }, Context, Result);
		bool bValid = ReadIdentity(Object, OutRule.Identity, Context, Result);
		bValid &= ReadRequiredString(Object, TEXT("displayName"), OutRule.DisplayName, Context, Result);
		bValid &= ReadRequiredString(Object, TEXT("summary"), OutRule.Summary, Context, Result);
		bValid &= ReadInteger(Object, TEXT("requiredMissions"), OutRule.RequiredMissions, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("requiredKills"), OutRule.RequiredKills, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("requiredRank"), OutRule.RequiredRank, true, Context, Result);
		bValid &= ReadRequiredBool(
			Object, TEXT("requiresSuccessfulMission"), OutRule.bRequiresSuccessfulMission, Context, Result);
		if (OutRule.DisplayName.TrimStartAndEnd().IsEmpty() || OutRule.Summary.TrimStartAndEnd().IsEmpty()
			|| OutRule.RequiredMissions <= 0 || OutRule.RequiredMissions > 10000
			|| OutRule.RequiredKills < 0 || OutRule.RequiredKills > 10000
			|| OutRule.RequiredRank <= 0 || OutRule.RequiredRank > 100)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"),
				FString::Printf(TEXT("%s contains invalid commendation text or service thresholds."), *Context));
			bValid = false;
		}
		return bValid;
	}

	bool ParseCraft(const TSharedPtr<FJsonObject>& Object, const FString& Context, FCraftRule& OutRule, FContentPackageParseResult& Result)
	{
		WarnUnknownFields(Object, {
			TEXT("id"), TEXT("replace"), TEXT("displayName"), TEXT("purchaseCost"), TEXT("monthlyMaintenance"), TEXT("acquisitionHours"),
			TEXT("maxHull"), TEXT("fuelCapacity"), TEXT("cruiseSpeedKilometersPerHour"), TEXT("fuelBurnPerHour"),
			TEXT("agentCapacity"), TEXT("cargoCapacity"), TEXT("equipmentSlots"), TEXT("repairCostPerHull"),
			TEXT("repairHoursPerHull"), TEXT("refuelCostPerUnit"), TEXT("refuelUnitsPerHour"), TEXT("requires") }, Context, Result);
		bool bValid = ReadIdentity(Object, OutRule.Identity, Context, Result);
		bValid &= ReadRequiredString(Object, TEXT("displayName"), OutRule.DisplayName, Context, Result);
		bValid &= ReadInteger(Object, TEXT("purchaseCost"), OutRule.PurchaseCost, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("monthlyMaintenance"), OutRule.MonthlyMaintenance, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("acquisitionHours"), OutRule.AcquisitionHours, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("maxHull"), OutRule.MaxHull, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("fuelCapacity"), OutRule.FuelCapacity, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("cruiseSpeedKilometersPerHour"), OutRule.CruiseSpeedKilometersPerHour, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("fuelBurnPerHour"), OutRule.FuelBurnPerHour, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("agentCapacity"), OutRule.AgentCapacity, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("cargoCapacity"), OutRule.CargoCapacity, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("equipmentSlots"), OutRule.EquipmentSlots, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("repairCostPerHull"), OutRule.RepairCostPerHull, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("repairHoursPerHull"), OutRule.RepairHoursPerHull, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("refuelCostPerUnit"), OutRule.RefuelCostPerUnit, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("refuelUnitsPerHour"), OutRule.RefuelUnitsPerHour, true, Context, Result);
		bValid &= ReadNameArray(Object, TEXT("requires"), OutRule.RequiredResearch, Context, Result);
		bValid &= ValidateNonNegative(OutRule.PurchaseCost, TEXT("purchaseCost"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.MonthlyMaintenance, TEXT("monthlyMaintenance"), Context, Result);
		bValid &= ValidatePositive(OutRule.AcquisitionHours, TEXT("acquisitionHours"), Context, Result);
		bValid &= ValidatePositive(OutRule.MaxHull, TEXT("maxHull"), Context, Result);
		bValid &= ValidatePositive(OutRule.FuelCapacity, TEXT("fuelCapacity"), Context, Result);
		bValid &= ValidatePositive(OutRule.CruiseSpeedKilometersPerHour, TEXT("cruiseSpeedKilometersPerHour"), Context, Result);
		bValid &= ValidatePositive(OutRule.FuelBurnPerHour, TEXT("fuelBurnPerHour"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.AgentCapacity, TEXT("agentCapacity"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.CargoCapacity, TEXT("cargoCapacity"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.EquipmentSlots, TEXT("equipmentSlots"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.RepairCostPerHull, TEXT("repairCostPerHull"), Context, Result);
		bValid &= ValidatePositive(OutRule.RepairHoursPerHull, TEXT("repairHoursPerHull"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.RefuelCostPerUnit, TEXT("refuelCostPerUnit"), Context, Result);
		bValid &= ValidatePositive(OutRule.RefuelUnitsPerHour, TEXT("refuelUnitsPerHour"), Context, Result);
		if (OutRule.EquipmentSlots > 16)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s.equipmentSlots cannot exceed 16."), *Context));
			bValid = false;
		}
		return bValid;
	}

	bool ParseContact(const TSharedPtr<FJsonObject>& Object, const FString& Context, FContactRule& OutRule, FContentPackageParseResult& Result)
	{
		WarnUnknownFields(Object, { TEXT("id"), TEXT("replace"), TEXT("displayName"), TEXT("signature"), TEXT("cruiseSpeedKilometersPerHour"), TEXT("maxHull"), TEXT("threatRating"), TEXT("scoreValue"), TEXT("attackAccuracy"), TEXT("attackDamage"), TEXT("attackIntervalSeconds") }, Context, Result);
		bool bValid = ReadIdentity(Object, OutRule.Identity, Context, Result);
		bValid &= ReadRequiredString(Object, TEXT("displayName"), OutRule.DisplayName, Context, Result);
		bValid &= ReadInteger(Object, TEXT("signature"), OutRule.Signature, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("cruiseSpeedKilometersPerHour"), OutRule.CruiseSpeedKilometersPerHour, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("maxHull"), OutRule.MaxHull, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("threatRating"), OutRule.ThreatRating, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("scoreValue"), OutRule.ScoreValue, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("attackAccuracy"), OutRule.AttackAccuracy, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("attackDamage"), OutRule.AttackDamage, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("attackIntervalSeconds"), OutRule.AttackIntervalSeconds, false, Context, Result);
		bValid &= ValidatePositive(OutRule.Signature, TEXT("signature"), Context, Result);
		bValid &= ValidatePositive(OutRule.CruiseSpeedKilometersPerHour, TEXT("cruiseSpeedKilometersPerHour"), Context, Result);
		bValid &= ValidatePositive(OutRule.MaxHull, TEXT("maxHull"), Context, Result);
		bValid &= ValidatePositive(OutRule.ThreatRating, TEXT("threatRating"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.ScoreValue, TEXT("scoreValue"), Context, Result);
		bValid &= ValidatePositive(OutRule.AttackAccuracy, TEXT("attackAccuracy"), Context, Result);
		bValid &= ValidatePositive(OutRule.AttackDamage, TEXT("attackDamage"), Context, Result);
		bValid &= ValidatePositive(OutRule.AttackIntervalSeconds, TEXT("attackIntervalSeconds"), Context, Result);
		if (OutRule.AttackAccuracy > 100)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s.attackAccuracy cannot exceed 100."), *Context));
			bValid = false;
		}
		if (OutRule.Signature > 100 || OutRule.ThreatRating > 10)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s signature or threat rating exceeds schema limits."), *Context));
			bValid = false;
		}
		return bValid;
	}

	bool ParseStrategicRegion(const TSharedPtr<FJsonObject>& Object, const FString& Context, FStrategicRegionRule& OutRule, FContentPackageParseResult& Result)
	{
		WarnUnknownFields(Object, {
			TEXT("id"), TEXT("replace"), TEXT("displayName"),
			TEXT("centerLongitudeMilliDegrees"), TEXT("centerLatitudeMilliDegrees"),
			TEXT("initialSupport"), TEXT("fundingWeight"), TEXT("pressureTolerance"),
			TEXT("lowPressureSupportRecovery"), TEXT("highPressureSupportLossPerTen") }, Context, Result);
		bool bValid = ReadIdentity(Object, OutRule.Identity, Context, Result);
		bValid &= ReadRequiredString(Object, TEXT("displayName"), OutRule.DisplayName, Context, Result);
		bValid &= ReadInteger(Object, TEXT("centerLongitudeMilliDegrees"), OutRule.CenterLongitudeMilliDegrees, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("centerLatitudeMilliDegrees"), OutRule.CenterLatitudeMilliDegrees, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("initialSupport"), OutRule.InitialSupport, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("fundingWeight"), OutRule.FundingWeight, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("pressureTolerance"), OutRule.PressureTolerance, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("lowPressureSupportRecovery"), OutRule.LowPressureSupportRecovery, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("highPressureSupportLossPerTen"), OutRule.HighPressureSupportLossPerTen, true, Context, Result);
		if (OutRule.DisplayName.TrimStartAndEnd().IsEmpty()
			|| OutRule.CenterLongitudeMilliDegrees < -180000 || OutRule.CenterLongitudeMilliDegrees > 180000
			|| OutRule.CenterLatitudeMilliDegrees < -90000 || OutRule.CenterLatitudeMilliDegrees > 90000
			|| OutRule.InitialSupport < 0 || OutRule.InitialSupport > 100
			|| OutRule.FundingWeight <= 0 || OutRule.FundingWeight > 1000
			|| OutRule.PressureTolerance <= 0 || OutRule.PressureTolerance >= 100
			|| OutRule.LowPressureSupportRecovery < 0 || OutRule.LowPressureSupportRecovery > 20
			|| OutRule.HighPressureSupportLossPerTen <= 0 || OutRule.HighPressureSupportLossPerTen > 20)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"),
				FString::Printf(TEXT("%s contains invalid center, support, funding, tolerance, or monthly-review values."), *Context));
			bValid = false;
		}
		return bValid;
	}

	bool ParseAdversaryPlan(const TSharedPtr<FJsonObject>& Object, const FString& Context, FAdversaryPlanRule& OutRule, FContentPackageParseResult& Result)
	{
		WarnUnknownFields(Object, {
			TEXT("id"), TEXT("replace"), TEXT("displayName"), TEXT("openingMissionRuleId") }, Context, Result);
		bool bValid = ReadIdentity(Object, OutRule.Identity, Context, Result);
		bValid &= ReadRequiredString(Object, TEXT("displayName"), OutRule.DisplayName, Context, Result);
		FString OpeningMissionRuleId;
		bValid &= ReadRequiredString(
			Object, TEXT("openingMissionRuleId"), OpeningMissionRuleId, Context, Result);
		OutRule.OpeningMissionRuleId = FName(*OpeningMissionRuleId);
		if (!FContentPackageResolver::IsValidPackageId(OutRule.OpeningMissionRuleId))
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"),
				FString::Printf(TEXT("%s contains an invalid opening adversary mission id."), *Context));
			bValid = false;
		}
		return bValid;
	}

	bool ParseAdversaryMission(const TSharedPtr<FJsonObject>& Object, const FString& Context, FAdversaryMissionRule& OutRule, FContentPackageParseResult& Result)
	{
		WarnUnknownFields(Object, {
			TEXT("id"), TEXT("replace"), TEXT("displayName"),
			TEXT("planId"), TEXT("planStage"), TEXT("escapeBranchMissionRuleId"), TEXT("thwartBranchMissionRuleId"),
			TEXT("contactRuleId"), TEXT("targetRegionId"),
			TEXT("targetsPlayerBase"),
			TEXT("originLongitudeMilliDegrees"), TEXT("originLatitudeMilliDegrees"),
			TEXT("destinationLongitudeMilliDegrees"), TEXT("destinationLatitudeMilliDegrees"),
			TEXT("intervalHours"), TEXT("minimumEscalation"), TEXT("selectionWeight"),
			TEXT("pressureOnEscape"), TEXT("pressureReductionOnDestroyed"),
			TEXT("scorePenaltyOnEscape"), TEXT("fundingPenaltyOnEscape"),
			TEXT("supportLossOnEscape"), TEXT("supportGainOnThwarted"),
			TEXT("compactPeerSupportLossOnEscape"),
			TEXT("withdrawnCompactSupportGainOnThwarted"),
			TEXT("createsLandingSiteOnArrival"), TEXT("landingSiteLifetimeHours"), TEXT("landingSiteThreatBonus"),
			TEXT("baseFacilityDamage"), TEXT("baseFacilitiesHit") }, Context, Result);
		bool bValid = ReadIdentity(Object, OutRule.Identity, Context, Result);
		bValid &= ReadRequiredString(Object, TEXT("displayName"), OutRule.DisplayName, Context, Result);
		FString PlanId;
		FString EscapeBranchMissionRuleId;
		FString ThwartBranchMissionRuleId;
		if (Object->HasField(TEXT("planId")))
		{
			bValid &= ReadRequiredString(Object, TEXT("planId"), PlanId, Context, Result);
			OutRule.PlanId = FName(*PlanId);
		}
		bValid &= ReadInteger(Object, TEXT("planStage"), OutRule.PlanStage, false, Context, Result);
		if (Object->HasField(TEXT("escapeBranchMissionRuleId")))
		{
			bValid &= ReadRequiredString(Object, TEXT("escapeBranchMissionRuleId"),
				EscapeBranchMissionRuleId, Context, Result);
			OutRule.EscapeBranchMissionRuleId = FName(*EscapeBranchMissionRuleId);
		}
		if (Object->HasField(TEXT("thwartBranchMissionRuleId")))
		{
			bValid &= ReadRequiredString(Object, TEXT("thwartBranchMissionRuleId"),
				ThwartBranchMissionRuleId, Context, Result);
			OutRule.ThwartBranchMissionRuleId = FName(*ThwartBranchMissionRuleId);
		}
		FString ContactRuleId;
		FString TargetRegionId;
		bValid &= ReadRequiredString(Object, TEXT("contactRuleId"), ContactRuleId, Context, Result);
		bValid &= ReadRequiredString(Object, TEXT("targetRegionId"), TargetRegionId, Context, Result);
		OutRule.ContactRuleId = FName(*ContactRuleId);
		OutRule.TargetRegionId = FName(*TargetRegionId);
		bValid &= ReadOptionalBool(Object, TEXT("targetsPlayerBase"), OutRule.bTargetsPlayerBase, Context, Result);
		bValid &= ReadInteger(Object, TEXT("originLongitudeMilliDegrees"), OutRule.OriginLongitudeMilliDegrees, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("originLatitudeMilliDegrees"), OutRule.OriginLatitudeMilliDegrees, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("destinationLongitudeMilliDegrees"), OutRule.DestinationLongitudeMilliDegrees, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("destinationLatitudeMilliDegrees"), OutRule.DestinationLatitudeMilliDegrees, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("intervalHours"), OutRule.IntervalHours, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("minimumEscalation"), OutRule.MinimumEscalation, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("selectionWeight"), OutRule.SelectionWeight, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("pressureOnEscape"), OutRule.PressureOnEscape, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("pressureReductionOnDestroyed"), OutRule.PressureReductionOnDestroyed, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("scorePenaltyOnEscape"), OutRule.ScorePenaltyOnEscape, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("fundingPenaltyOnEscape"), OutRule.FundingPenaltyOnEscape, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("supportLossOnEscape"), OutRule.SupportLossOnEscape, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("supportGainOnThwarted"), OutRule.SupportGainOnThwarted, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("compactPeerSupportLossOnEscape"),
			OutRule.CompactPeerSupportLossOnEscape, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("withdrawnCompactSupportGainOnThwarted"),
			OutRule.WithdrawnCompactSupportGainOnThwarted, false, Context, Result);
		bValid &= ReadOptionalBool(Object, TEXT("createsLandingSiteOnArrival"), OutRule.bCreatesLandingSiteOnArrival, Context, Result);
		bValid &= ReadInteger(Object, TEXT("landingSiteLifetimeHours"), OutRule.LandingSiteLifetimeHours, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("landingSiteThreatBonus"), OutRule.LandingSiteThreatBonus, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("baseFacilityDamage"), OutRule.BaseFacilityDamage, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("baseFacilitiesHit"), OutRule.BaseFacilitiesHit, false, Context, Result);
		bValid &= ValidatePositive(OutRule.IntervalHours, TEXT("intervalHours"), Context, Result);
		bValid &= ValidatePositive(OutRule.MinimumEscalation, TEXT("minimumEscalation"), Context, Result);
		bValid &= ValidatePositive(OutRule.SelectionWeight, TEXT("selectionWeight"), Context, Result);
		bValid &= ValidatePositive(OutRule.PressureOnEscape, TEXT("pressureOnEscape"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.PressureReductionOnDestroyed, TEXT("pressureReductionOnDestroyed"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.ScorePenaltyOnEscape, TEXT("scorePenaltyOnEscape"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.FundingPenaltyOnEscape, TEXT("fundingPenaltyOnEscape"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.SupportLossOnEscape, TEXT("supportLossOnEscape"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.SupportGainOnThwarted, TEXT("supportGainOnThwarted"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.CompactPeerSupportLossOnEscape,
			TEXT("compactPeerSupportLossOnEscape"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.WithdrawnCompactSupportGainOnThwarted,
			TEXT("withdrawnCompactSupportGainOnThwarted"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.LandingSiteLifetimeHours, TEXT("landingSiteLifetimeHours"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.LandingSiteThreatBonus, TEXT("landingSiteThreatBonus"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.BaseFacilityDamage, TEXT("baseFacilityDamage"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.BaseFacilitiesHit, TEXT("baseFacilitiesHit"), Context, Result);
		const bool bPlanMetadataValid = OutRule.PlanId.IsNone()
			? OutRule.PlanStage == 0
				&& OutRule.EscapeBranchMissionRuleId.IsNone()
				&& OutRule.ThwartBranchMissionRuleId.IsNone()
			: FContentPackageResolver::IsValidPackageId(OutRule.PlanId)
				&& OutRule.PlanStage >= 1 && OutRule.PlanStage <= 16
				&& (OutRule.EscapeBranchMissionRuleId.IsNone()
					|| FContentPackageResolver::IsValidPackageId(OutRule.EscapeBranchMissionRuleId))
				&& (OutRule.ThwartBranchMissionRuleId.IsNone()
					|| FContentPackageResolver::IsValidPackageId(OutRule.ThwartBranchMissionRuleId));
		const bool bOriginValid = OutRule.OriginLongitudeMilliDegrees >= -180000 && OutRule.OriginLongitudeMilliDegrees <= 180000
			&& OutRule.OriginLatitudeMilliDegrees >= -90000 && OutRule.OriginLatitudeMilliDegrees <= 90000;
		const bool bDestinationValid = OutRule.DestinationLongitudeMilliDegrees >= -180000 && OutRule.DestinationLongitudeMilliDegrees <= 180000
			&& OutRule.DestinationLatitudeMilliDegrees >= -90000 && OutRule.DestinationLatitudeMilliDegrees <= 90000;
		if (!bPlanMetadataValid
			|| !FContentPackageResolver::IsValidPackageId(OutRule.ContactRuleId)
			|| !FContentPackageResolver::IsValidPackageId(OutRule.TargetRegionId)
			|| !bOriginValid || !bDestinationValid
			|| (OutRule.OriginLongitudeMilliDegrees == OutRule.DestinationLongitudeMilliDegrees
				&& OutRule.OriginLatitudeMilliDegrees == OutRule.DestinationLatitudeMilliDegrees)
			|| OutRule.MinimumEscalation > 10 || OutRule.SelectionWeight > 1000000
			|| OutRule.PressureOnEscape > 100 || OutRule.PressureReductionOnDestroyed > 100
			|| OutRule.SupportLossOnEscape > 100 || OutRule.SupportGainOnThwarted > 100
			|| OutRule.CompactPeerSupportLossOnEscape > 100
			|| OutRule.WithdrawnCompactSupportGainOnThwarted > 100
			|| (OutRule.bCreatesLandingSiteOnArrival
				? (OutRule.bTargetsPlayerBase || OutRule.LandingSiteLifetimeHours <= 0
					|| OutRule.LandingSiteLifetimeHours > 720 || OutRule.LandingSiteThreatBonus <= 0
					|| OutRule.LandingSiteThreatBonus > 9)
				: (OutRule.LandingSiteLifetimeHours != 0 || OutRule.LandingSiteThreatBonus != 0))
			|| ((OutRule.BaseFacilityDamage == 0) != (OutRule.BaseFacilitiesHit == 0))
			|| (OutRule.bTargetsPlayerBase != (OutRule.BaseFacilityDamage > 0))
			|| OutRule.BaseFacilityDamage > 100000 || OutRule.BaseFacilitiesHit > 64)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s contains invalid plan, contact, region, route, escalation, weight, or pressure metadata."), *Context));
			bValid = false;
		}
		return bValid;
	}

	bool ParseTacticalTerrain(const TSharedPtr<FJsonObject>& Object, const FString& Context, FTacticalTerrainRule& OutRule, FContentPackageParseResult& Result)
	{
		WarnUnknownFields(Object, { TEXT("id"), TEXT("replace"), TEXT("displayName"), TEXT("moveCost"), TEXT("coverPercent"), TEXT("maxIntegrity"), TEXT("blastResistancePercent"), TEXT("flammability"), TEXT("ventilationPercent"), TEXT("verticalMoveCost"), TEXT("throwObstacleHeight"), TEXT("doorActionPointCost"), TEXT("blocksMovement"), TEXT("blocksVision") }, Context, Result);
		bool bValid = ReadIdentity(Object, OutRule.Identity, Context, Result);
		bValid &= ReadRequiredString(Object, TEXT("displayName"), OutRule.DisplayName, Context, Result);
		bValid &= ReadInteger(Object, TEXT("moveCost"), OutRule.MoveCost, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("coverPercent"), OutRule.CoverPercent, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("maxIntegrity"), OutRule.MaxIntegrity, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("blastResistancePercent"), OutRule.BlastResistancePercent, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("flammability"), OutRule.Flammability, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("ventilationPercent"), OutRule.VentilationPercent, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("verticalMoveCost"), OutRule.VerticalMoveCost, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("throwObstacleHeight"), OutRule.ThrowObstacleHeight, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("doorActionPointCost"), OutRule.DoorActionPointCost, false, Context, Result);
		bValid &= ReadRequiredBool(Object, TEXT("blocksMovement"), OutRule.bBlocksMovement, Context, Result);
		bValid &= ReadRequiredBool(Object, TEXT("blocksVision"), OutRule.bBlocksVision, Context, Result);
		bValid &= ValidatePositive(OutRule.MoveCost, TEXT("moveCost"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.CoverPercent, TEXT("coverPercent"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.MaxIntegrity, TEXT("maxIntegrity"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.BlastResistancePercent, TEXT("blastResistancePercent"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.Flammability, TEXT("flammability"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.VentilationPercent, TEXT("ventilationPercent"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.VerticalMoveCost, TEXT("verticalMoveCost"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.ThrowObstacleHeight, TEXT("throwObstacleHeight"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.DoorActionPointCost, TEXT("doorActionPointCost"), Context, Result);
		if (OutRule.MoveCost > 20 || OutRule.CoverPercent > 100
			|| OutRule.BlastResistancePercent > 100 || OutRule.Flammability > 100 || OutRule.VentilationPercent > 100
			|| OutRule.VerticalMoveCost > 20
			|| OutRule.ThrowObstacleHeight > 8
			|| OutRule.DoorActionPointCost > 4
			|| (OutRule.ThrowObstacleHeight > 0 && OutRule.MaxIntegrity <= 0)
			|| (OutRule.DoorActionPointCost > 0 && (OutRule.MaxIntegrity <= 0 || !OutRule.bBlocksMovement))
			|| (OutRule.VerticalMoveCost > 0 && OutRule.bBlocksMovement)
			|| (OutRule.bBlocksMovement && OutRule.MaxIntegrity <= 0))
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s tactical terrain values exceed supported limits."), *Context));
			bValid = false;
		}
		return bValid;
	}

	bool ParseTacticalUnit(const TSharedPtr<FJsonObject>& Object, const FString& Context, FTacticalUnitRule& OutRule, FContentPackageParseResult& Result)
	{
		WarnUnknownFields(Object, { TEXT("id"), TEXT("replace"), TEXT("displayName"), TEXT("maxHealth"), TEXT("accuracy"), TEXT("resolve"), TEXT("mobility"), TEXT("strength"), TEXT("actionPoints"), TEXT("attackRange"), TEXT("attackPower"), TEXT("attackActionPointCost"), TEXT("signalPower"), TEXT("signalRange"), TEXT("signalActionPointCost"), TEXT("attackDamageType"), TEXT("kineticArmor"), TEXT("thermalArmor"), TEXT("arcArmor") }, Context, Result);
		bool bValid = ReadIdentity(Object, OutRule.Identity, Context, Result);
		bValid &= ReadRequiredString(Object, TEXT("displayName"), OutRule.DisplayName, Context, Result);
		bValid &= ReadInteger(Object, TEXT("maxHealth"), OutRule.MaxHealth, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("accuracy"), OutRule.Accuracy, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("resolve"), OutRule.Resolve, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("mobility"), OutRule.Mobility, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("strength"), OutRule.Strength, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("actionPoints"), OutRule.ActionPoints, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("attackRange"), OutRule.AttackRange, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("attackPower"), OutRule.AttackPower, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("attackActionPointCost"), OutRule.AttackActionPointCost, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("signalPower"), OutRule.SignalPower, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("signalRange"), OutRule.SignalRange, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("signalActionPointCost"), OutRule.SignalActionPointCost, false, Context, Result);
		bValid &= ReadTacticalDamageType(Object, TEXT("attackDamageType"), OutRule.AttackDamageType, Context, Result);
		bValid &= ReadInteger(Object, TEXT("kineticArmor"), OutRule.KineticArmor, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("thermalArmor"), OutRule.ThermalArmor, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("arcArmor"), OutRule.ArcArmor, false, Context, Result);
		bValid &= ValidatePositive(OutRule.MaxHealth, TEXT("maxHealth"), Context, Result);
		bValid &= ValidatePositive(OutRule.Accuracy, TEXT("accuracy"), Context, Result);
		bValid &= ValidatePositive(OutRule.Resolve, TEXT("resolve"), Context, Result);
		bValid &= ValidatePositive(OutRule.Mobility, TEXT("mobility"), Context, Result);
		bValid &= ValidatePositive(OutRule.Strength, TEXT("strength"), Context, Result);
		bValid &= ValidatePositive(OutRule.ActionPoints, TEXT("actionPoints"), Context, Result);
		bValid &= ValidatePositive(OutRule.AttackRange, TEXT("attackRange"), Context, Result);
		bValid &= ValidatePositive(OutRule.AttackPower, TEXT("attackPower"), Context, Result);
		bValid &= ValidatePositive(OutRule.AttackActionPointCost, TEXT("attackActionPointCost"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.SignalPower, TEXT("signalPower"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.SignalRange, TEXT("signalRange"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.SignalActionPointCost, TEXT("signalActionPointCost"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.KineticArmor, TEXT("kineticArmor"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.ThermalArmor, TEXT("thermalArmor"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.ArcArmor, TEXT("arcArmor"), Context, Result);
		if (OutRule.MaxHealth > 200 || OutRule.Accuracy > 100 || OutRule.Resolve > 100
			|| OutRule.Mobility > 100 || OutRule.Strength > 100 || OutRule.ActionPoints > 20
			|| OutRule.AttackRange > 64 || OutRule.AttackPower > 200 || OutRule.AttackActionPointCost > 20
			|| OutRule.SignalPower > 100 || OutRule.SignalRange > 64 || OutRule.SignalActionPointCost > 20
			|| ((OutRule.SignalPower == 0 || OutRule.SignalRange == 0 || OutRule.SignalActionPointCost == 0)
				&& (OutRule.SignalPower != 0 || OutRule.SignalRange != 0 || OutRule.SignalActionPointCost != 0))
			|| OutRule.KineticArmor > 100 || OutRule.ThermalArmor > 100 || OutRule.ArcArmor > 100)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s tactical unit attributes exceed supported limits."), *Context));
			bValid = false;
		}
		return bValid;
	}

	bool ParseTacticalMission(const TSharedPtr<FJsonObject>& Object, const FString& Context, FTacticalMissionRule& OutRule, FContentPackageParseResult& Result)
	{
		WarnUnknownFields(Object, {
			TEXT("id"), TEXT("replace"), TEXT("displayName"), TEXT("context"), TEXT("siteType"), TEXT("sourceContactRuleId"),
			TEXT("floorTerrainRuleId"), TEXT("obstacleTerrainRuleId"), TEXT("doorTerrainRuleId"), TEXT("verticalConnectorTerrainRuleId"), TEXT("adversaryUnitRuleId"), TEXT("aiPosture"), TEXT("objectiveId"),
			TEXT("objectiveType"), TEXT("objectiveRequiredInteractions"), TEXT("objectiveRewardItemId"), TEXT("objectiveRewardQuantity"),
			TEXT("missionExperienceReward"), TEXT("objectiveExperienceReward"),
			TEXT("mapWidth"), TEXT("mapHeight"), TEXT("mapLevels"), TEXT("deploymentDepth"), TEXT("obstaclePercent"),
			TEXT("baseEnemyCount"), TEXT("enemiesPerThreat"), TEXT("turnLimit"),
			TEXT("objectiveActionPointCost"), TEXT("extractionActionPointCost") }, Context, Result);
		bool bValid = ReadIdentity(Object, OutRule.Identity, Context, Result);
		bValid &= ReadRequiredString(Object, TEXT("displayName"), OutRule.DisplayName, Context, Result);
		bValid &= ReadTacticalMissionContext(Object, TEXT("context"), OutRule.Context, Context, Result);
		bValid &= ReadTacticalSiteType(Object, TEXT("siteType"), OutRule.SiteType, Context, Result);
		FString SourceContactRuleId;
		FString FloorTerrainRuleId;
		FString ObstacleTerrainRuleId;
		FString DoorTerrainRuleId;
		FString VerticalConnectorTerrainRuleId;
		FString AdversaryUnitRuleId;
		FString ObjectiveId;
		bValid &= ReadRequiredString(Object, TEXT("sourceContactRuleId"), SourceContactRuleId, Context, Result);
		bValid &= ReadRequiredString(Object, TEXT("floorTerrainRuleId"), FloorTerrainRuleId, Context, Result);
		bValid &= ReadRequiredString(Object, TEXT("obstacleTerrainRuleId"), ObstacleTerrainRuleId, Context, Result);
		if (Object->HasField(TEXT("doorTerrainRuleId")))
		{
			bValid &= ReadRequiredString(Object, TEXT("doorTerrainRuleId"), DoorTerrainRuleId, Context, Result);
			OutRule.DoorTerrainRuleId = FName(*DoorTerrainRuleId);
		}
		if (Object->HasField(TEXT("verticalConnectorTerrainRuleId")))
		{
			bValid &= ReadRequiredString(Object, TEXT("verticalConnectorTerrainRuleId"), VerticalConnectorTerrainRuleId, Context, Result);
			OutRule.VerticalConnectorTerrainRuleId = FName(*VerticalConnectorTerrainRuleId);
		}
		bValid &= ReadRequiredString(Object, TEXT("adversaryUnitRuleId"), AdversaryUnitRuleId, Context, Result);
		bValid &= ReadRequiredString(Object, TEXT("objectiveId"), ObjectiveId, Context, Result);
		OutRule.SourceContactRuleId = FName(*SourceContactRuleId);
		OutRule.FloorTerrainRuleId = FName(*FloorTerrainRuleId);
		OutRule.ObstacleTerrainRuleId = FName(*ObstacleTerrainRuleId);
		OutRule.AdversaryUnitRuleId = FName(*AdversaryUnitRuleId);
		bValid &= ReadTacticalAiPosture(Object, TEXT("aiPosture"), OutRule.AiPosture, Context, Result);
		OutRule.ObjectiveId = FName(*ObjectiveId);
		bValid &= ReadTacticalObjectiveType(Object, TEXT("objectiveType"), OutRule.ObjectiveType, Context, Result);
		bValid &= ReadInteger(Object, TEXT("objectiveRequiredInteractions"), OutRule.ObjectiveRequiredInteractions, false, Context, Result);
		if (Object->HasField(TEXT("objectiveRewardItemId")))
		{
			FString ObjectiveRewardItemId;
			bValid &= ReadRequiredString(Object, TEXT("objectiveRewardItemId"), ObjectiveRewardItemId, Context, Result);
			OutRule.ObjectiveRewardItemId = FName(*ObjectiveRewardItemId);
		}
		bValid &= ReadInteger(Object, TEXT("objectiveRewardQuantity"), OutRule.ObjectiveRewardQuantity, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("missionExperienceReward"), OutRule.MissionExperienceReward, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("objectiveExperienceReward"), OutRule.ObjectiveExperienceReward, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("mapWidth"), OutRule.MapWidth, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("mapHeight"), OutRule.MapHeight, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("mapLevels"), OutRule.MapLevels, false, Context, Result);
		bValid &= ReadInteger(Object, TEXT("deploymentDepth"), OutRule.DeploymentDepth, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("obstaclePercent"), OutRule.ObstaclePercent, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("baseEnemyCount"), OutRule.BaseEnemyCount, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("enemiesPerThreat"), OutRule.EnemiesPerThreat, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("turnLimit"), OutRule.TurnLimit, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("objectiveActionPointCost"), OutRule.ObjectiveActionPointCost, true, Context, Result);
		bValid &= ReadInteger(Object, TEXT("extractionActionPointCost"), OutRule.ExtractionActionPointCost, true, Context, Result);
		bValid &= ValidatePositive(OutRule.MapWidth, TEXT("mapWidth"), Context, Result);
		bValid &= ValidatePositive(OutRule.MapHeight, TEXT("mapHeight"), Context, Result);
		bValid &= ValidatePositive(OutRule.MapLevels, TEXT("mapLevels"), Context, Result);
		bValid &= ValidatePositive(OutRule.DeploymentDepth, TEXT("deploymentDepth"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.ObstaclePercent, TEXT("obstaclePercent"), Context, Result);
		bValid &= ValidatePositive(OutRule.BaseEnemyCount, TEXT("baseEnemyCount"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.EnemiesPerThreat, TEXT("enemiesPerThreat"), Context, Result);
		bValid &= ValidatePositive(OutRule.TurnLimit, TEXT("turnLimit"), Context, Result);
		bValid &= ValidatePositive(OutRule.ObjectiveRequiredInteractions, TEXT("objectiveRequiredInteractions"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.ObjectiveRewardQuantity, TEXT("objectiveRewardQuantity"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.MissionExperienceReward, TEXT("missionExperienceReward"), Context, Result);
		bValid &= ValidateNonNegative(OutRule.ObjectiveExperienceReward, TEXT("objectiveExperienceReward"), Context, Result);
		bValid &= ValidatePositive(OutRule.ObjectiveActionPointCost, TEXT("objectiveActionPointCost"), Context, Result);
		bValid &= ValidatePositive(OutRule.ExtractionActionPointCost, TEXT("extractionActionPointCost"), Context, Result);
		const int64 MapCells = ComputeTacticalMapCellCount(OutRule.MapWidth, OutRule.MapHeight, OutRule.MapLevels);
		const int64 MaximumEnemies = static_cast<int64>(OutRule.BaseEnemyCount) + static_cast<int64>(OutRule.EnemiesPerThreat) * 10;
		if ((OutRule.Context != ETacticalMissionContext::StrategicSite && OutRule.Context != ETacticalMissionContext::BaseDefense)
			|| (OutRule.Context == ETacticalMissionContext::BaseDefense && OutRule.MapLevels != 1)
			|| (OutRule.SiteType != ETacticalSiteType::Wreckage && OutRule.SiteType != ETacticalSiteType::Landing)
			|| (OutRule.Context == ETacticalMissionContext::BaseDefense && OutRule.SiteType != ETacticalSiteType::Wreckage)
			|| !FContentPackageResolver::IsValidPackageId(OutRule.SourceContactRuleId)
			|| !FContentPackageResolver::IsValidPackageId(OutRule.FloorTerrainRuleId)
			|| !FContentPackageResolver::IsValidPackageId(OutRule.ObstacleTerrainRuleId)
			|| (!OutRule.DoorTerrainRuleId.IsNone() && !FContentPackageResolver::IsValidPackageId(OutRule.DoorTerrainRuleId))
			|| (!OutRule.VerticalConnectorTerrainRuleId.IsNone() && !FContentPackageResolver::IsValidPackageId(OutRule.VerticalConnectorTerrainRuleId))
			|| (OutRule.MapLevels > 1 && OutRule.VerticalConnectorTerrainRuleId.IsNone())
			|| !FContentPackageResolver::IsValidPackageId(OutRule.AdversaryUnitRuleId)
			|| !FContentPackageResolver::IsValidPackageId(OutRule.ObjectiveId)
			|| (OutRule.ObjectiveType != ETacticalObjectiveType::Disrupt
				&& OutRule.ObjectiveType != ETacticalObjectiveType::Recover
				&& OutRule.ObjectiveType != ETacticalObjectiveType::Control)
			|| OutRule.ObjectiveRequiredInteractions > 20
			|| (OutRule.ObjectiveType == ETacticalObjectiveType::Recover
				? (!FContentPackageResolver::IsValidPackageId(OutRule.ObjectiveRewardItemId) || OutRule.ObjectiveRewardQuantity <= 0)
				: (!OutRule.ObjectiveRewardItemId.IsNone() || OutRule.ObjectiveRewardQuantity != 0))
			|| OutRule.ObjectiveRewardQuantity > 100
			|| OutRule.MissionExperienceReward > 10000 || OutRule.ObjectiveExperienceReward > 10000
			|| OutRule.MapWidth < 8 || OutRule.MapWidth > 64
			|| OutRule.MapHeight < 12 || OutRule.MapHeight > 96 || OutRule.MapLevels < 1 || OutRule.MapLevels > 4 || MapCells > 8192
			|| OutRule.DeploymentDepth < 2 || OutRule.DeploymentDepth > 8
			|| OutRule.MapHeight <= OutRule.DeploymentDepth * 2 + 4
			|| OutRule.ObstaclePercent > 60 || OutRule.BaseEnemyCount > 32 || OutRule.EnemiesPerThreat > 8
			|| MaximumEnemies >= static_cast<int64>(OutRule.MapWidth) * OutRule.DeploymentDepth
			|| OutRule.TurnLimit > 500 || OutRule.ObjectiveActionPointCost > 20
			|| OutRule.ExtractionActionPointCost > 20)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s tactical mission values exceed supported limits."), *Context));
			bValid = false;
		}
		return bValid;
	}

	template <typename RuleType, typename ParserType>
	void ParseRuleArray(
		const TSharedPtr<FJsonObject>& RulesObject,
		const TCHAR* Field,
		TArray<RuleType>& OutRules,
		ParserType ParseRule,
		const FString& SourceLabel,
		FContentPackageParseResult& Result)
	{
		if (!RulesObject->HasField(Field))
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!RulesObject->TryGetArrayField(Field, Values) || Values == nullptr)
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.rules.%s must be an array."), *SourceLabel, Field));
			return;
		}

		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			const FString Context = FString::Printf(TEXT("%s.rules.%s[%d]"), *SourceLabel, Field, Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(Object) || Object == nullptr || !Object->IsValid())
			{
				AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s must be an object."), *Context));
				continue;
			}

			RuleType Rule;
			if (ParseRule(*Object, Context, Rule, Result))
			{
				OutRules.Add(MoveTemp(Rule));
			}
		}
	}
}

bool FContentPackageParseResult::HasDiagnostic(const FName Code) const
{
	return Diagnostics.ContainsByPredicate(
		[Code](const FContentDiagnostic& Diagnostic)
		{
			return Diagnostic.Code == Code;
		});
}

FContentPackageParseResult FContentPackageJson::ParseString(const FString& Json, const FString& SourceLabel)
{
	using namespace ContentPackageJsonPrivate;

	FContentPackageParseResult Result;
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_json"), FString::Printf(TEXT("%s is not valid JSON: %s"), *SourceLabel, *Reader->GetErrorMessage()));
		return Result;
	}

	WarnUnknownFields(Root, { TEXT("schemaVersion"), TEXT("packageId"), TEXT("displayName"), TEXT("version"), TEXT("priority"), TEXT("dependencies"), TEXT("loadAfter"), TEXT("rules") }, SourceLabel, Result);

	ReadInteger(Root, TEXT("schemaVersion"), Result.Package.Descriptor.SchemaVersion, true, SourceLabel, Result);
	FString PackageId;
	if (ReadRequiredString(Root, TEXT("packageId"), PackageId, SourceLabel, Result))
	{
		Result.Package.Descriptor.PackageId = FName(*PackageId);
		if (!FContentPackageResolver::IsValidPackageId(Result.Package.Descriptor.PackageId))
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_package_id"), FString::Printf(TEXT("%s.packageId '%s' is invalid."), *SourceLabel, *PackageId));
		}
	}
	ReadRequiredString(Root, TEXT("displayName"), Result.Package.Descriptor.DisplayName, SourceLabel, Result);
	ReadRequiredString(Root, TEXT("version"), Result.Package.Descriptor.Version, SourceLabel, Result);
	ReadInteger(Root, TEXT("priority"), Result.Package.Descriptor.Priority, false, SourceLabel, Result);
	ReadNameArray(Root, TEXT("dependencies"), Result.Package.Descriptor.Dependencies, SourceLabel, Result);
	ReadNameArray(Root, TEXT("loadAfter"), Result.Package.Descriptor.LoadAfter, SourceLabel, Result);

	if (Result.Package.Descriptor.SchemaVersion != FContentPackageResolver::CurrentSchemaVersion)
	{
		AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("unsupported_schema_version"), FString::Printf(TEXT("%s uses schema %d; supported schema is %d."), *SourceLabel, Result.Package.Descriptor.SchemaVersion, FContentPackageResolver::CurrentSchemaVersion));
	}

	if (Root->HasField(TEXT("rules")))
	{
		const TSharedPtr<FJsonObject>* RulesObject = nullptr;
		if (!Root->TryGetObjectField(TEXT("rules"), RulesObject) || RulesObject == nullptr || !RulesObject->IsValid())
		{
			AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.rules must be an object."), *SourceLabel));
		}
		else
		{
			WarnUnknownFields(*RulesObject, { TEXT("items"), TEXT("research"), TEXT("archiveEntries"), TEXT("facilities"), TEXT("personnelRoles"), TEXT("personnelDoctrines"), TEXT("personnelCommendations"), TEXT("craft"), TEXT("regions"), TEXT("contacts"), TEXT("adversaryPlans"), TEXT("adversaryMissions"), TEXT("tacticalTerrains"), TEXT("tacticalUnits"), TEXT("tacticalMissions") }, SourceLabel + TEXT(".rules"), Result);
			ParseRuleArray(*RulesObject, TEXT("items"), Result.Package.Items, ParseItem, SourceLabel, Result);
			ParseRuleArray(*RulesObject, TEXT("research"), Result.Package.Research, ParseResearch, SourceLabel, Result);
			ParseRuleArray(*RulesObject, TEXT("archiveEntries"), Result.Package.ArchiveEntries, ParseArchiveEntry, SourceLabel, Result);
			ParseRuleArray(*RulesObject, TEXT("facilities"), Result.Package.Facilities, ParseFacility, SourceLabel, Result);
			ParseRuleArray(*RulesObject, TEXT("personnelRoles"), Result.Package.PersonnelRoles, ParsePersonnelRole, SourceLabel, Result);
			ParseRuleArray(*RulesObject, TEXT("personnelDoctrines"), Result.Package.PersonnelDoctrines, ParsePersonnelDoctrine, SourceLabel, Result);
			ParseRuleArray(*RulesObject, TEXT("personnelCommendations"), Result.Package.PersonnelCommendations, ParsePersonnelCommendation, SourceLabel, Result);
			ParseRuleArray(*RulesObject, TEXT("craft"), Result.Package.Craft, ParseCraft, SourceLabel, Result);
			ParseRuleArray(*RulesObject, TEXT("regions"), Result.Package.Regions, ParseStrategicRegion, SourceLabel, Result);
			ParseRuleArray(*RulesObject, TEXT("contacts"), Result.Package.Contacts, ParseContact, SourceLabel, Result);
			ParseRuleArray(*RulesObject, TEXT("adversaryPlans"), Result.Package.AdversaryPlans, ParseAdversaryPlan, SourceLabel, Result);
			ParseRuleArray(*RulesObject, TEXT("adversaryMissions"), Result.Package.AdversaryMissions, ParseAdversaryMission, SourceLabel, Result);
			ParseRuleArray(*RulesObject, TEXT("tacticalTerrains"), Result.Package.TacticalTerrains, ParseTacticalTerrain, SourceLabel, Result);
			ParseRuleArray(*RulesObject, TEXT("tacticalUnits"), Result.Package.TacticalUnits, ParseTacticalUnit, SourceLabel, Result);
			ParseRuleArray(*RulesObject, TEXT("tacticalMissions"), Result.Package.TacticalMissions, ParseTacticalMission, SourceLabel, Result);
		}
	}

	for (FContentDiagnostic& Diagnostic : Result.Diagnostics)
	{
		if (Diagnostic.PackageId.IsNone())
		{
			Diagnostic.PackageId = Result.Package.Descriptor.PackageId;
		}
	}

	Result.bSucceeded = !HasErrors(Result);
	return Result;
}

FContentPackageParseResult FContentPackageJson::ParseFile(const FString& FilePath)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *FilePath))
	{
		FContentPackageParseResult Result;
		ContentPackageJsonPrivate::AddDiagnostic(Result, EContentDiagnosticSeverity::Error, TEXT("file_read_failed"), FString::Printf(TEXT("Could not read content package '%s'."), *FilePath));
		return Result;
	}
	return ParseString(Json, FilePath);
}
