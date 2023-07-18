#pragma once

#include <tuple>
#include <string>
#include <memory>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include "../matchers/ChrMatcherCore.h"
#include "../include/VFTHook.h"
#include "HkSkeleton.h"

// The Skeleton Manager (SkeletonMan) is a singleton that controls the usage and application of bone and skeleton modifiers.
class SkeletonMan {
public:
	// Singleton setup:
	static SkeletonMan& instance() 
	{
		static SkeletonMan singleton;
		return singleton;
	}

	SkeletonMan(const SkeletonMan&) = delete;
	SkeletonMan& operator = (const SkeletonMan&) = delete;

	// SkeletonMan::Target represents an abstract character that matches given conditions and has given bone and skeleton modifiers.
	class Target {
	public:
		// Add a conjunction of one or more ChrMatchers to the target as a single condition.
		template <typename... Ts> void addCondition(const Ts&... matchers) 
		{
			this->conditions.emplace_back(std::vector<std::unique_ptr<ChrMatcher::Matcher>>{});
			(this->conditions.back().emplace_back(std::make_unique<Ts>(matchers)), ...);
		}

		// Add a modifier to all bones in the skeleton.
		template <typename T> void addSkeletonModifier(const T& modifier)
		{
			this->skeletonModifiers.emplace_back(std::make_unique<T>(modifier));
		}

		// Add a modifier to all of the bone specified by "bones".
		// "bones" can contain both bone indices (not ids!) and names.
		// Example: target.addBoneModifier(HkModifier::ScaleLength(1.2f), 1, 2, "Neck", 0, 57, "L_Thigh");
		// If "bones" is empty, the modifier will be applied to the first bone in the skeleton.
		template <typename T, typename... Ts> void addBoneModifier(const T& modifier, Ts... bones) 
		{ 
			static_assert(std::conjunction_v<std::disjunction<std::is_integral<std::decay_t<Ts>>, std::is_convertible<std::decay_t<Ts>, std::string_view>>...>, "\"bones\" must contain only bone names or integral bone indices.");
			if constexpr (sizeof...(bones) > 0) {
				this->boneModifiers.emplace_back(std::make_tuple(std::make_unique<T>(modifier), extract_integers(bones...), extract_strings(bones...)));
			}
			else {
				this->boneModifiers.emplace_back(std::make_tuple(std::make_unique<T>(modifier), std::vector<short>{ 0 }, std::vector<std::string>{}));
			}
		}

	private:
		std::vector<std::vector<std::unique_ptr<ChrMatcher::Matcher>>> conditions{};
		std::vector<std::tuple<std::unique_ptr<HkModifier::Modifier>, std::vector<int16_t>, std::vector<std::string>>> boneModifiers{};
		std::vector<std::unique_ptr<HkModifier::Modifier>> skeletonModifiers{};

		// Private constructor, use the static SkeletonMan::makeTarget instead.
		template <typename... Ts> Target(const Ts&... conditions) 
		{
			this->conditions.emplace_back(std::vector<std::unique_ptr<ChrMatcher::Matcher>>{});
			(this->conditions.back().emplace_back(std::make_unique<Ts>(conditions)), ...);
		}

		// For SkeletonMan::Target::checkConditions to return true, at least one condtion must equal true.
		// For a condition/conjunction to equal true, every ChrMatcher inside must return true.
		// This means that when adding multiple conditions, they will be evaluated in a disjunction.
		bool checkConditions(void* ChrIns)
		{
			for (auto& conditionGroups : this->conditions) {
				bool groupResult = true;
				for (auto& matcher : conditionGroups) {
					groupResult &= matcher->match(ChrIns);
				}
				if (groupResult) return true;
			}
			return false;
		}

		// Helper methods for dealing with variadic parameters.
		template <typename T> static inline constexpr std::vector<int16_t> extract_integers(T t);
		template <typename T> std::vector<std::string> static inline constexpr extract_strings(T t);
		template <typename T, typename... Ts> static inline constexpr std::vector<int16_t> extract_integers(T t, Ts... ts);
		template <typename T, typename... Ts> std::vector<std::string> static inline constexpr extract_strings(T t, Ts... ts);

		friend class SkeletonMan;
	};

	// Makes a SkeletonMan::Target and adds it to the SkeletonMan target list.
	// Returns a reference to the object.
	template <typename... Ts> static Target& makeTarget(Ts&&... conditions) 
	{ 
		SkeletonMan::targets.emplace_back(std::unique_ptr<Target>(new Target(std::forward<Ts>(conditions)...)));
		return *SkeletonMan::targets.back().get(); 
	}

	// Initializes the hooks by scanning for RTTI data. Can be provided a pointer to a custom scanner instance.
	// Only call this after you are done editing the SkeletonMan targets.
	bool initialize(RTTIScanner* scanner = nullptr)
	{
		if (this->scanner && isScannerOwner) delete this->scanner;

		if (scanner) {
			this->isScannerOwner = false;
			this->scanner = scanner;
		}
		else {
			this->scanner = new RTTIScanner();
		}

		if (!this->scanner->scan()) return false;

		// We hook:
		// - the final character instance initialization function
		// - the character unload function
		// - the character instance destructor
		this->playerHooks.ctorHook = std::make_unique<VFTHookTemplate<ExitHook>>("CS::PlayerIns", 10, SkeletonMan::ctorHookFn);
		this->playerHooks.unloadHook = std::make_unique<VFTHook>("CS::PlayerIns", 11, SkeletonMan::dtorHookFn);
		this->playerHooks.dtorHook = std::make_unique<VFTHook>("CS::PlayerIns", 1, SkeletonMan::dtorHookFn);

		this->enemyHooks.ctorHook = std::make_unique<VFTHookTemplate<ExitHook>>("CS::EnemyIns", 10, SkeletonMan::ctorHookFn);
		this->enemyHooks.unloadHook = std::make_unique<VFTHook>("CS::EnemyIns", 11, SkeletonMan::dtorHookFn);
		this->enemyHooks.dtorHook = std::make_unique<VFTHook>("CS::EnemyIns", 1, SkeletonMan::dtorHookFn);

		// This is a semi-unrelated havok function we hook for its good execution timing.
		// It is used to update and apply all of the bone modifiers.
		this->hkHook = std::make_unique<VFTHook>("CS::NoUpdateInterface", 10, SkeletonMan::hkHookFn);

		return true;
	}

private:
	SkeletonMan() : scanner(new RTTIScanner()), isScannerOwner(true) {}
	virtual ~SkeletonMan() {}

	RTTIScanner* scanner;
	bool isScannerOwner;

	struct Hooks {
		std::unique_ptr<VFTHookTemplate<ExitHook>> ctorHook{};
		std::unique_ptr<VFTHook> unloadHook{};
		std::unique_ptr<VFTHook> dtorHook{};
	} playerHooks, enemyHooks;
	std::unique_ptr<VFTHook> hkHook{};

	static inline std::vector<std::unique_ptr<Target>> targets{};
	static inline std::unordered_map<void*, std::unique_ptr<HkSkeleton>> skeletons{};

	// Attempts to create a new HkSkeleton instance with a given ChrIns.
	// Used inside the constructor hook.
	static HkSkeleton* makeSkeleton(void* ChrIns)
	{
		HkSkeleton* skeleton;
		try {
			skeleton = new HkSkeleton(ChrIns);
		}
		catch (const std::runtime_error&) {
			return nullptr;
		}
		return skeleton;
	}

	// Checks the newly created character instance for matching conditions.
	// Creates a HkSkeleton and adds all of the modifiers from the matched SkeletonMan::Target.
	// Adds it to the vector of skeletons managed by SkeletonMan.
	static void ctorHookFn(void* ChrIns)
	{
		HkSkeleton* skeleton = nullptr;
		for (auto& target : SkeletonMan::targets) {
			if (target->checkConditions(ChrIns)) {
				if (!skeleton) {
					skeleton = SkeletonMan::makeSkeleton(ChrIns);
					if (!skeleton) return;
				}
				for (auto& modifier : target->skeletonModifiers) {
					skeleton->addModifier(modifier.get());
				}
				for (auto& [modifier, indices, names] : target->boneModifiers) {
					for (int16_t index : indices) {
						auto bone = skeleton->getBone(index);
						if (!!bone) bone->addModifier(modifier.get());
					}
					for (auto& name : names) {
						auto bone = skeleton->getBone(name);
						if (!!bone) bone->addModifier(modifier.get());
					}
				}
			}
		}
		if (!!skeleton) SkeletonMan::skeletons.emplace(ChrIns, std::unique_ptr<HkSkeleton>(skeleton));
	}

	// Removes the managed skeleton once its character instance has been unloaded or destroyed.
	static void dtorHookFn(void* ChrIns)
	{
		SkeletonMan::skeletons.erase(ChrIns);
	}

	// Iterates over and updates all skeletons.
	static void hkHookFn()
	{
		for (auto& iter : SkeletonMan::skeletons) {
			iter.second->updateAll();
		}
	}
};

// Helper methods for dealing with variadic parameters.
template <typename T> static inline constexpr std::vector<int16_t> SkeletonMan::Target::extract_integers(T t)
{
	if constexpr (std::is_integral_v<T>) {
		return { static_cast<int16_t>(t) };
	}
	else {
		return {};
	}
}

template <typename T> static inline constexpr std::vector<std::string> SkeletonMan::Target::extract_strings(T t)
{
	if constexpr (std::is_convertible_v<T, std::string_view>) {
		return { t };
	}
	else {
		return {};
	}
}

template <typename T, typename... Ts> static inline constexpr std::vector<int16_t> SkeletonMan::Target::extract_integers(T t, Ts... ts)
{
	auto integers = extract_integers(ts...);
	if constexpr (std::is_integral_v<T>) {
		integers.push_back(static_cast<int16_t>(t));
	}
	return integers;
}

template <typename T, typename... Ts> static inline constexpr std::vector<std::string> SkeletonMan::Target::extract_strings(T t, Ts... ts)
{
	auto strings = extract_strings(ts...);
	if constexpr (std::is_convertible_v<T, std::string_view>) {
		strings.push_back(t);
	}
	return strings;
}
