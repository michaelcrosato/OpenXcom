// Copyright 2026 UEGT contributors. MIT License.

#include "Strategic/PersonnelServiceHistory.h"

FPersonnelServiceHistoryView FPersonnelServiceHistory::Project(const int32 Missions)
{
	const int32 SafeMissions = FMath::Max(0, Missions);
	FPersonnelServiceHistoryView View;
	if (SafeMissions < 5)
	{
		View.Band = EPersonnelServiceBand::FirstWatch;
		View.NextBand = EPersonnelServiceBand::FieldProven;
		View.NextBandMissions = 5;
	}
	else if (SafeMissions < 10)
	{
		View.Band = EPersonnelServiceBand::FieldProven;
		View.NextBand = EPersonnelServiceBand::LongWatch;
		View.NextBandMissions = 10;
	}
	else if (SafeMissions < 20)
	{
		View.Band = EPersonnelServiceBand::LongWatch;
		View.NextBand = EPersonnelServiceBand::LegacyAnchor;
		View.NextBandMissions = 20;
	}
	else if (SafeMissions < 40)
	{
		View.Band = EPersonnelServiceBand::LegacyAnchor;
		View.NextBand = EPersonnelServiceBand::EnduringBeacon;
		View.NextBandMissions = 40;
	}
	else
	{
		View.Band = EPersonnelServiceBand::EnduringBeacon;
		View.NextBand = EPersonnelServiceBand::EnduringBeacon;
		View.NextBandMissions = 40;
		View.MissionsUntilNextBand = 0;
		View.bMaximumBand = true;
		return View;
	}
	View.MissionsUntilNextBand = View.NextBandMissions - SafeMissions;
	return View;
}
