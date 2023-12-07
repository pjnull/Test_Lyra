
#include "LyraClone_CosmeticAnimationTypes.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraClone_CosmeticAnimationTypes)

USkeletalMesh* FLyraClone_AnimBodyStyleSelectionSet::SelectBestBodyStyle(const FGameplayTagContainer& CosmeticTags) const
{
	for (const FLyraClone_AnimBodyStyleSelectionEntry& Rule : MeshRules)
	{
		if (Rule.Mesh && CosmeticTags.HasAll(Rule.RequiredTags))
		{
			return Rule.Mesh;
		}
	}
	return DefaultMesh;
}
