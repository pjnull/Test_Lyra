#include "GameFeature_World_ActionBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeature_World_ActionBase)


void UGameFeature_World_ActionBase::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		//월드를 순회하면서 ExperienceManager에서 GameFeatureAction을 활성화하면서 Context에 World를 넣어줌
		//이를 통해 적용할 대상인지 판단
		if (Context.ShouldApplyToWorldContext(WorldContext))
		{
			//WorldActionBase의 Interface인 AddToWorld호출
			AddToWorld(WorldContext, Context);
		}
	}
}
