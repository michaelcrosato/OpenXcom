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

	void BuildBaseViews(
		const FGuid& SourceBaseId,
		const int32 FacilityRelayChannelCount,
		const int32 SignalWatchScientistCount,
		const int32 RelayChannelCount,
		TArray<FRelayJob>& Jobs,
		TArray<FMutualAidRelayQueueView>& OutViews)
	{
		SortJobs(Jobs);
		const int32 ActiveCount = FMath::Min(RelayChannelCount, Jobs.Num());
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
		if (!Jobs.IsEmpty())
		{
			const int32 FacilityChannels = EvaluateFacilityRelayChannelCount(*Base, Rules);
			BuildBaseViews(Base->BaseId, FacilityChannels, Base->SignalWatchScientists,
				EvaluateRelayChannelCount(*Base, Rules), Jobs, Snapshot.Convoys);
		}
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
	BuildBaseViews(SourceBaseId, FacilityChannels, Source->SignalWatchScientists,
		EvaluateRelayChannelCount(*Source, Rules), Jobs, Views);
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
