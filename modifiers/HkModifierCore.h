#pragma once

#include "../include/PointerChain.h"

// All modifiers must be a part of this namespace
namespace HkModifier {
	// This is the base modifier class. All modifiers must derive from it.
	class Modifier {
	protected:
		Modifier() {}
		using Bone = HkSkeleton::HkBone;
		using BoneData = Bone::HkBoneData;

	public:
		virtual ~Modifier() {};

		void apply(Bone* bone) { onApply(bone, bone->getBoneData()); }

		// These two functions must be defined in a derived modifier class for it to be valid.
		virtual Modifier* clone() = 0;
		virtual void onApply(Bone*, BoneData&) = 0;
	};
}

#include "BaseModifiers.h"
#include "CustomModifiers.h"