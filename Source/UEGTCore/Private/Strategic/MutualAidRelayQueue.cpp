// Copyright 2026 UEGT contributors. MIT License.

#include "Strategic/MutualAidRelayQueue.h"

namespace MutualAidRelayQueuePrivate
{
	bool GuidLess(const FGuid& Left, const FGuid& Right)
	{
		if (Left.A != Right.A)
		{
			return Left.A < Right.A;
		}
		if (Left.B != Right.B)
		{
			return Left.B < Right.B;
		}
		if (Left.C != Right.C)
		{
			return Left.C < Right.C;
		}
		return Left.D < Right.D;
	}

	int64 SaturatingAdd(const int64 Left, const int64 Right)
	{
		if (Left < 0 || Right < 0)
		{
			return 0;
		}
		return Left > MAX_int64 - Right ? MAX_int64 : Left + Right;
	}

	struct FRelayJob
	{
		FGuid ConvoyId;
		int64 DispatchSequence = 0;
		int64 JourneySeconds = 0;
		bool bProspective = false;
	};

	void SortJobs(TArray<FRelayJob>& Jobs)
	{
		Jobs.Sort(
			[](const FRelayJob& Left, const FRelayJob& Right)
			{
				if (Left.DispatchSequence != Right.DispatchSequence)
				{
					return Left.DispatchSequence < Right.DispatchSequence;
				}
				return GuidLess(Left.ConvoyId, Right.ConvoyId);
			});
	}

	int32 CalculateQueuePressurePercent(const int32 TotalConvoyCount, const int32 ActiveConvoyCount)
	{
		if (TotalConvoyCount <= 0)
		{
			return 0;
		}
		const int32 WaitingConvoyCount = FMath::Max(
			0, TotalConvoyCount - FMath::Max(0, ActiveConvoyCount));
		if (WaitingConvoyCount <= 0)
		{
			return 0;
		}
		const int64 Numerator = static_cast<int64>(WaitingConvoyCount) * 100;
		return FMath::Clamp<int32>(
			static_cast<int32>((Numerator + TotalConvoyCount - 1) / TotalConvoyCount), 0, 100);
	}

	void BuildBaseViews(
		const FGuid& SourceBaseId,
		const int32 FacilityRelayChannelCount,
		const int32 SignalWatchScientistCount,
		const int32 RelayChannelCount,
		FMutualAidRelayQueueBaseView& OutBaseView,
		TArray<FRelayJob>& Jobs,
		TArray<FMutualAidRelayQueueView>& OutViews)
	{
		SortJobs(Jobs);
		const int32 ActiveCount = FMath::Min(RelayChannelCount, Jobs.Num());
		OutBaseView = {};
		OutBaseView.bValid = true;
		OutBaseView.BaseId = SourceBaseId;
		OutBaseView.RelayChannelCount = RelayChannelCount;
		OutBaseView.FacilityRelayChannelCount = FacilityRelayChannelCount;
		OutBaseView.SignalWatchScientistCount = FMath::Max(0, SignalWatchScientistCount);
		OutBaseView.SignalWatchBonusChannelCount = FMath::Max(
			0, RelayChannelCount - FacilityRelayChannelCount);
		OutBaseView.ActiveConvoyCount = ActiveCount;
		OutBaseView.TotalConvoyCount = Jobs.Num();
		OutBaseView.WaitingConvoyCount = FMath::Max(0, Jobs.Num() - ActiveCount);
		OutBaseView.QueuePressurePercent = CalculateQueuePressurePercent(
			OutBaseView.TotalConvoyCount, OutBaseView.ActiveConvoyCount);
		OutBaseView.bRelayAvailable = RelayChannelCount > 0;
		TArray<int64> ChannelAvailableSeconds;
		ChannelAvailableSeconds.Init(0, ActiveCount);

		for (int32 JobIndex = 0; JobIndex < Jobs.Num(); ++JobIndex)
		{
			FMutualAidRelayQueueView& View = OutViews.AddDefaulted_GetRef();
			View.bValid = true;
			View.PolicyId = FMutualAidRelayQueue::PolicyId();
			View.SourceBaseId = SourceBaseId;
			View.ConvoyId = Jobs[JobIndex].ConvoyId;
			View.DispatchSequence = Jobs[JobIndex].DispatchSequence;
			View.RelayChannelCount = RelayChannelCount;
			View.FacilityRelayChannelCount = FacilityRelayChannelCount;
			View.SignalWatchScientistCount = FMath::Max(0, SignalWatchScientistCount);
			View.SignalWatchBonusChannelCount = FMath::Max(
				0, RelayChannelCount - FacilityRelayChannelCount);
			View.ActiveConvoyCount = ActiveCount;
			View.TotalConvoyCount = Jobs.Num();
			View.QueuePosition = JobIndex + 1;
			View.WaitingConvoyCount = OutBaseView.WaitingConvoyCount;
			View.QueuePressurePercent = OutBaseView.QueuePressurePercent;
			View.bRelayAvailable = RelayChannelCount > 0;
			if (!View.bRelayAvailable)
			{
				View.WaitingPosition = JobIndex + 1;
				continue;
			}

			int32 ChannelIndex = 0;
			for (int32 Candidate = 1; Candidate < ChannelAvailableSeconds.Num(); ++Candidate)
			{
				if (ChannelAvailableSeconds[Candidate] < ChannelAvailableSeconds[ChannelIndex])
				{
					ChannelIndex = Candidate;
				}
			}
			View.EstimatedWaitSeconds = ChannelAvailableSeconds[ChannelIndex];
			View.EstimatedArrivalSeconds = SaturatingAdd(
				View.EstimatedWaitSeconds, Jobs[JobIndex].JourneySeconds);
			ChannelAvailableSeconds[ChannelIndex] = View.EstimatedArrivalSeconds;
			View.bInTransit = View.EstimatedWaitSeconds == 0;
			View.WaitingPosition = View.bInTransit ? 0 : JobIndex - ActiveCount + 1;
			View.RelayChannelNumber = ChannelIndex + 1;
			OutBaseView.QueueTailArrivalSeconds = FMath::Max(
				OutBaseView.QueueTailArrivalSeconds, View.EstimatedArrivalSeconds);
		}
	}

	TArray<FRelayJob> GatherJobs(const FCampaignState& Campaign, const FGuid& SourceBaseId)
	{
		TArray<FRelayJob> Jobs;
		for (const FMutualAidConvoyState& Convoy : Campaign.MutualAidConvoys)
		{
			if (Convoy.SourceBaseId == SourceBaseId && Convoy.ConvoyId.IsValid()
				&& Convoy.DispatchSequence > 0 && Convoy.RemainingTransitSeconds > 0)
			{
				Jobs.Add({ Convoy.ConvoyId, Convoy.DispatchSequence,
					FMutualAidRelayQueue::ProjectedJourneySeconds(Convoy), false });
			}
		}
		return Jobs;
	}
}

const FMutualAidRelayQueueView* FMutualAidRelayQueueSnapshot::FindConvoy(
	const FGuid& ConvoyId) const
{
	return Convoys.FindByPredicate(
		[&ConvoyId](const FMutualAidRelayQueueView& View)
		{
			return View.ConvoyId == ConvoyId;
		});
}

const FMutualAidRelayQueueBaseView* FMutualAidRelayQueueSnapshot::FindBase(
	const FGuid& BaseId) const
{
	return Bases.FindByPredicate(
		[&BaseId](const FMutualAidRelayQueueBaseView& View)
		{
			return View.BaseId == BaseId;
		});
}

FName FMutualAidRelayQueue::PolicyId()
{
	return TEXT("logistics.mutual-aid-relay-weave");
}

FName FMutualAidRelayQueue::SignalWatchPolicyId()
{
	return TEXT("logistics.signal-watch");
}

int32 FMutualAidRelayQueue::EvaluateFacilityRelayChannelCount(
	const FStrategicBaseState& Base,
	const FResolvedRuleSet& Rules)
{
	int32 RelayChannels = 0;
	const auto AddStrength = [&RelayChannels](const int32 Strength)
	{
		if (Strength <= 0 || RelayChannels == MAX_int32)
		{
			return;
		}
		const int32 AddedChannels = 1 + (Strength - 1) / 50;
		RelayChannels = AddedChannels > MAX_int32 - RelayChannels
			? MAX_int32
			: RelayChannels + AddedChannels;
	};

	if (!Base.Facilities.IsEmpty())
	{
		for (const FBaseFacilityState& Facility : Base.Facilities)
		{
			const FFacilityRule* Rule = Rules.Facilities.Find(Facility.FacilityId);
			if (Rule != nullptr && Rule->MaxIntegrity > 0
				&& Facility.Damage >= 0 && Facility.Damage <= Rule->MaxIntegrity)
			{
				AddStrength(Rule->ScaleEffectByIntegrity(
					Rule->DetectionStrength, Facility.Damage));
			}
		}
		return RelayChannels;
	}

	for (const FName FacilityId : Base.BuiltFacilities)
	{
		if (const FFacilityRule* Rule = Rules.Facilities.Find(FacilityId))
		{
			AddStrength(Rule->DetectionStrength);
		}
	}
	return RelayChannels;
}

int32 FMutualAidRelayQueue::EvaluateRelayChannelCount(
	const FStrategicBaseState& Base,
	const FResolvedRuleSet& Rules)
{
	const int32 FacilityChannels = EvaluateFacilityRelayChannelCount(Base, Rules);
	const int32 EffectiveWatchScientists = FMath::Min(
		FMath::Max(0, Base.SignalWatchScientists), FacilityChannels);
	return EffectiveWatchScientists > MAX_int32 - FacilityChannels
		? MAX_int32
		: FacilityChannels + EffectiveWatchScientists;
}

FMutualAidRelayQueueSnapshot FMutualAidRelayQueue::Evaluate(
	const FCampaignState& Campaign,
	const FResolvedRuleSet& Rules)
{
	using namespace MutualAidRelayQueuePrivate;

	FMutualAidRelayQueueSnapshot Snapshot;
	Snapshot.PolicyId = PolicyId();
	TArray<const FStrategicBaseState*> StableBases;
	for (const FStrategicBaseState& Base : Campaign.Bases)
	{
		if (Base.BaseId.IsValid())
		{
			StableBases.Add(&Base);
		}
	}
	StableBases.Sort(
		[](const FStrategicBaseState& Left, const FStrategicBaseState& Right)
		{
			return GuidLess(Left.BaseId, Right.BaseId);
		});

	for (const FStrategicBaseState* Base : StableBases)
	{
		TArray<FRelayJob> Jobs = GatherJobs(Campaign, Base->BaseId);
		const int32 FacilityChannels = EvaluateFacilityRelayChannelCount(*Base, Rules);
		FMutualAidRelayQueueBaseView& BaseView = Snapshot.Bases.AddDefaulted_GetRef();
		BuildBaseViews(Base->BaseId, FacilityChannels, Base->SignalWatchScientists,
			EvaluateRelayChannelCount(*Base, Rules), BaseView, Jobs, Snapshot.Convoys);
	}
	return Snapshot;
}

FMutualAidRelayQueueView FMutualAidRelayQueue::ProjectNext(
	const FCampaignState& Campaign,
	const FResolvedRuleSet& Rules,
	const FGuid& SourceBaseId,
	const int64 JourneySeconds)
{
	using namespace MutualAidRelayQueuePrivate;

	FMutualAidRelayQueueView Projection;
	const FStrategicBaseState* Source = Campaign.Bases.FindByPredicate(
		[&SourceBaseId](const FStrategicBaseState& Base)
		{
			return Base.BaseId == SourceBaseId;
		});
	if (Source == nullptr || JourneySeconds <= 0 || Campaign.CommandSequence == MAX_int64)
	{
		return Projection;
	}

	TArray<FRelayJob> Jobs = GatherJobs(Campaign, SourceBaseId);
	constexpr FGuid ProspectiveId(MAX_uint32, MAX_uint32, MAX_uint32, MAX_uint32);
	Jobs.Add({ ProspectiveId, Campaign.CommandSequence + 1, JourneySeconds, true });
	TArray<FMutualAidRelayQueueView> Views;
	const int32 FacilityChannels = EvaluateFacilityRelayChannelCount(*Source, Rules);
	FMutualAidRelayQueueBaseView ProjectionBaseView;
	BuildBaseViews(SourceBaseId, FacilityChannels, Source->SignalWatchScientists,
		EvaluateRelayChannelCount(*Source, Rules), ProjectionBaseView, Jobs, Views);
	if (const FMutualAidRelayQueueView* Found = Views.FindByPredicate(
		[](const FMutualAidRelayQueueView& View)
		{
			return View.ConvoyId == ProspectiveId;
		}))
	{
		Projection = *Found;
	}
	return Projection;
}

int64 FMutualAidRelayQueue::ProjectedJourneySeconds(const FMutualAidConvoyState& Convoy)
{
	using namespace MutualAidRelayQueuePrivate;
	const int64 PendingDelay = !Convoy.bInterdictionResolved && !Convoy.bSignalEscort
		? Convoy.ForecastInterdictionDelaySeconds
		: 0;
	int64 JourneySeconds = SaturatingAdd(
		FMath::Max<int64>(0, Convoy.RemainingTransitSeconds),
		FMath::Max<int64>(0, PendingDelay));
	if (Convoy.RelayWaypointBaseId.IsValid())
	{
		JourneySeconds = SaturatingAdd(
			JourneySeconds, FMath::Max<int64>(0, Convoy.OnwardTotalTransitSeconds));
		const int64 OnwardPendingDelay =
			!Convoy.bOnwardInterdictionResolved && !Convoy.bSignalEscort
				? Convoy.OnwardForecastInterdictionDelaySeconds
				: 0;
		JourneySeconds = SaturatingAdd(
			JourneySeconds, FMath::Max<int64>(0, OnwardPendingDelay));
	}
	return JourneySeconds;
}
