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

		bool apply(Bone* bone) { return onApply(bone, bone->getBoneData()); }

		// These two functions must be defined in a derived modifier class for it to be valid.
		virtual Modifier* clone() = 0;
		// The return value signfies whether the modifier should be only applied once when applied as a skeleton modifier.
		virtual bool onApply(Bone*, BoneData&) = 0;
	};

	namespace Impl {
		// TODO: map out the unk offsets.
		struct SpEffectNode {
			void* pSpEffectParam;
			int id;
			int unk00[9];
			SpEffectNode* next;
			SpEffectNode* previous;
			float effectEndurance;
			float motionInterval;
			float effectLife;
			float unk01[3];
			int unk02;
		};

		// Iterate over all SpEffect entries in the linked list to match ours.
		inline bool checkSpEffectID(void* ChrIns, int spEffectID)
		{
			SpEffectNode* current = PointerChain::make<SpEffectNode*>(ChrIns, 0x178, 0x8u).dereference(nullptr);
			while (!!current) {
				if (current->id == spEffectID) return true;
				current = current->next;
			}
			return false;
		}
	}
}

#include "BaseModifiers.h"
#include "CustomModifiers.h"