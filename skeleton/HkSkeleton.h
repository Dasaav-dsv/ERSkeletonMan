#pragma once

#include <memory>
#include <vector>
#include <string>
#include <stdexcept>
#include <unordered_map>

#include "../include/VxD.h"
#include "../include/PointerChain.h"

namespace HkModifier { class Modifier; };

// The base class for HkSkeleton and HkBone, implements modifier functionality.
class HkObj {
public:
	virtual ~HkObj() {}

	// Adds a modifier to the object which will be applied when HkSkeleton::updateAll is called.
	inline int addModifier(HkModifier::Modifier* modifier);

	// Returns a modifier by its index, which can be gotten from HkObj::addModifier.
	HkModifier::Modifier* getModifier(const int modifierID) { return this->modifiers.size() > modifierID ? this->modifiers[modifierID].get() : nullptr; }
	// Check if a modifier exists by its index.
	bool hasModifier(const int modifierID) { return this->modifiers.size() > modifierID && !!this->modifiers[modifierID]; }
	// Returns a reference to the vector that holds pointers to all of the modifiers.
	auto& getAllModifiers() { return this->modifiers; }
	// Removes a modifier by its index.
	void removeModifier(const int modifierID) { if (this->modifiers.size() > modifierID) this->modifiers[modifierID] = nullptr; }
	// Removes all modifiers.
	void clearAllModifiers() { this->modifiers.clear(); }

protected:
	std::vector<std::unique_ptr<HkModifier::Modifier>> modifiers{};
};

class HkSkeleton : public HkObj {
public:
	class HkBone : public HkObj {
	public:
		// The layout as it is in the game's memory.
		struct HkBoneData {
			V4D xzyVec;
			V4D qSpatial;
			V4D xzyScale;
		};

		// Bone id and index are separate identifiers:
		// - the id is used to determine bone hierarchy and can be duplicate
		// - the index represents the order of the bones in the skeleton and is unique
		// Both are handled by the skeleton's constructor.
		HkBone(HkSkeleton* skeleton, std::string name, const int16_t index, const int16_t id) : HkObj(), skeleton(skeleton), name(name), index(index), id(id) {}
		// Parent and child functions, setting the hierarchy is handled by the skeleton's constructor.
		// A bone can have only one parent, but multiple children.
		void setParent(HkBone* parent) { this->parent = parent; }
		HkBone* getParent() { return this->parent; }
		void addChild(HkBone* child) { this->children.push_back(child); }
		auto& getChildren() { return this->children; }
		HkSkeleton* getSkeleton() { return this->skeleton; }
		// The bone data of a given bone is the bone data struct with the bone's index.
		HkBoneData& getBoneData() { return this->skeleton->getBoneData()[this->getIndex()]; }
		const std::string& getName() const { return this->name; }
		int16_t getIndex() const { return this->index; }
		int16_t getID() const { return this->id; }
		// Modifiers can only be applied to bones, a skeleton modifier just means that a modifier is applied to every bone.
		inline bool applyModifier(HkModifier::Modifier* modifier);
		inline void applyAllModifiers();

		// Calculates the world coordinates of a bone by recursively adding up bone offsets.
		V4D getWorldPos()
		{
			V4D localPos = this->getBoneData().xzyVec;
			HkBone* parent = this->getParent();
			if (!parent || parent == this) {
				return localPos + this->getSkeleton()->getChrPos();
			}
			else {
				return localPos + parent->getWorldPos();
			}
		}

		// Calculates the orientation of a bone by recursively multiplying orientation quaternions.
		V4D getWorldQ()
		{
			V4D localQ = this->getBoneData().qSpatial;
			HkBone* parent = this->getParent();
			if (!parent || parent == this) {
				return localQ;
			}
			else {
				return parent->getWorldQ().qMul(localQ);
			}
		}

	private:
		HkSkeleton* skeleton = nullptr;
		HkBone* parent = nullptr;
		std::vector<HkBone*> children{};
		std::string name{};

		V4D worldPos{};
		V4D worldQ{};

		int16_t index{};
		int16_t id{};
	};

	// The layout of the HkaSkeleton struct as it is in the game's memory.
	struct HkaSkeleton {
		void** vft;
		uint64_t : 64;
		uint64_t : 64;
		char* boneNames;
		int16_t* boneIDs;
		int boneCount;
		uint32_t : 32;
		char** boneNameLayout;
	};

	// Maps a character's skeleton and all its bones.
	// Will throw if a character instance misses necessary data.
	HkSkeleton(void* ChrIns) : HkObj(), ChrIns(ChrIns),
		chrPos(*PointerChain::make<V4D>(ChrIns, 0x190, 0x68, 0x70)),
		chrQ(*PointerChain::make<V4D>(ChrIns, 0x190, 0x68, 0x50))
	{
		if (!ChrIns) {
			throw std::runtime_error("ChrIns is nullptr.");
		}

		uint8_t** pHkbCharacter = PointerChain::make<uint8_t*>(ChrIns, 0x190, 0x28, 0x10u, 0x30u).get();
		if (!pHkbCharacter) {
			throw std::runtime_error("hkbCharacter not found.");
		}

		HkaSkeleton* hkaSkeleton = PointerChain::make<HkaSkeleton>(pHkbCharacter, 0x90, 0x28u, 0x0u).get();
		if (!hkaSkeleton) {
			throw std::runtime_error("hkaSkeleton not found.");
		}

		int boneCount = hkaSkeleton->boneCount;
		if (boneCount <= 0) {
			throw std::runtime_error("Skeleton has invalid bone count.");
		}

		uint8_t** pBoneDataLayout = PointerChain::make<uint8_t*>(pHkbCharacter, 0x38u, 0x0u).get();
		if (!pBoneDataLayout) {
			throw std::runtime_error("Unable to find skeleton bone data.");
		}
		else {
			int boneOffset = *PointerChain::make<int>(pBoneDataLayout, 0x54);
			this->boneData = PointerChain::make<HkBone::HkBoneData>(pBoneDataLayout, boneOffset).get();
		}

		// After retrieving some important pointers, it's time to construct the bones.
		auto& bones = this->hkBones;
		bones.resize(boneCount);

		// Iterate over each bone and create a HkBone instance.
		for (int i = 0; i < boneCount; i++) {
			int16_t id = hkaSkeleton->boneIDs[i];
			const char* name = hkaSkeleton->boneNameLayout[i * 2];
			bones[i] = std::make_unique<HkBone>(this, name, i, id);
			HkBone* bone = bones[i].get();
			this->skeletonMap[name] = bone;
		}

		int16_t* pBoneHierarchy = PointerChain::make<int16_t>(ChrIns, 0x50, 0x10u, 0x68u, 0x20u, 0x28u).get();
		if (!pBoneHierarchy) {
			throw std::runtime_error("Unable to reconstruct bone hierarchy.");
		}

		// Iterate over every HkBone instance and assign the parents and children.
		for (auto& bone : bones) {
			if (!bone) continue;

			// Each bone struct is 0x30 bytes long.
			// The parent and child ids are at +0x0 and +0x2 of each struct.
			int16_t parentIndex = (pBoneHierarchy + bone->getIndex() * 0x18)[0];

			// Assign parent and children bones by index.
			if (parentIndex >= 0 && parentIndex < boneCount) {
				HkBone* parent = bones[parentIndex].get();
				if (!!parent) {
					bone->setParent(parent);
					parent->addChild(bone.get());
				}
			}
		}
	}

	~HkSkeleton() {};
	void* getChrIns() { return ChrIns; }
	HkBone::HkBoneData* getBoneData() { return this->boneData; }
	const V4D& getChrPos() { return this->chrPos; }
	const V4D& getChrQ() { return this->chrQ; }
	int getBoneCount() const { return this->hkBones.size(); }
	// Retrieve a bone by its index (not id!), as it is in the skeleton.
	HkBone* getBone(int16_t boneIndex) { return this->getBoneCount() > boneIndex ? hkBones[boneIndex].get() : nullptr; }
	// Attempt to match a name with all of the names of the bones in the skeleton, returns a pointer to the matched bone on success or nullptr on failure.
	HkBone* getBone(const std::string& name) { auto iter = this->skeletonMap.find(name); return iter != this->skeletonMap.end() ? iter->second : nullptr; }
	auto& getBones() { return this->hkBones; }

	// Updates all bones and applies all modifiers.
	void updateAll()
	{
		for (auto& bone : this->hkBones) {
			if (!bone) continue;

			// Get and apply all skeleton modifiers to every bone.
			for (auto& modifier : this->getAllModifiers()) {
				if (!modifier) continue;
				bone->applyModifier(modifier.get());
			}

			bone->applyAllModifiers();
		}
	}

private:
	void* ChrIns;
	const V4D& chrPos;
	const V4D& chrQ;
	HkBone::HkBoneData* boneData = nullptr;
	std::vector<std::unique_ptr<HkBone>> hkBones = {};
	std::unordered_map<std::string, HkBone*> skeletonMap = {};
};

#include "../modifiers/HkModifierCore.h"

inline int HkObj::addModifier(HkModifier::Modifier* modifier)
{
	this->modifiers.emplace_back(std::unique_ptr<HkModifier::Modifier>(modifier->clone()));
	return this->modifiers.size() - 1;
}

inline bool HkSkeleton::HkBone::applyModifier(HkModifier::Modifier* modifier)
{
	if (!!modifier) return modifier->apply(this);
}

inline void HkSkeleton::HkBone::applyAllModifiers()
{
	for (auto& modifier : this->getAllModifiers())
	{
		if (!modifier) continue;
		this->applyModifier(modifier.get());
	}
}
