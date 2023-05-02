#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "../skeleton/SkeletonMan.h"

void onAttach()
{
    // Create a new target for modification, specifying a condition that matches all characters.
    auto& exampleTarget0 = SkeletonMan::makeTarget(ChrMatcher::All()); // Note that this is a reference auto&
    // Add a modifier that scales the length of Spine1 and Spine2 bones for characters that match the target's conditions
    // In this case, it applies to all characters, since the ChrMatcher matches any and all character.
    exampleTarget0.addBoneModifier(HkModifier::ScaleLength(0.95f), "Spine1", "Spine2");

    // Create a target with multiple conditions in a condition group/conjunction.
    // When multiple conditions are added together, they are evaluated together, and must all be true in order to match the character.
    // This condition group means: match model c3080 AND in map m35_00_00_00
    auto& exampleTarget1 = SkeletonMan::makeTarget(ChrMatcher::Model(L"c3080"), ChrMatcher::Map(L"m35_00_00_00"));
    // On the other hand, conditions added separately are evaluated as a separate condition group.
    exampleTarget1.addCondition(ChrMatcher::EntityID(18000850));
    // Another condition group.
    // Altogether, these 3 condition groups are evaluated like this:
    // (match model c3080 AND in map m35_00_00_00) OR (match EntityID 18000850) OR (match EntityGroupID 1044345106 AND EntityGroupID 1044355810)
    exampleTarget1.addCondition(ChrMatcher::EntityGroupID(1044345106), ChrMatcher::EntityGroupID(1044355810));
    // A skeleton modifier is applied to all of the bones in a skeleton.
    // This scales all of the bones to half their length, scaling the character down.
    exampleTarget1.addSkeletonModifier(HkModifier::ScaleLength(0.5f));
    // The "size" of a bone is different from its length, as it represents its thickness.
    exampleTarget1.addSkeletonModifier(HkModifier::ScaleSize(0.5f));
    exampleTarget1.addBoneModifier(HkModifier::ScaleLength(2.0f), "Head");
    exampleTarget1.addBoneModifier(HkModifier::ScaleSize(2.0f), "Head");
    // It's good practice to disable cloth physics when scaling characters.
    // Cloth physics will not work as expected at small or large scaling values.
    exampleTarget1.addSkeletonModifier(HkModifier::DisableClothPhysics());

    // ThinkParamID example:
    auto& exampleTarget2 = SkeletonMan::makeTarget(ChrMatcher::ThinkParamID(48100900)); // Target Erdtree Avatars with this ThinkParam
    exampleTarget2.addCondition(ChrMatcher::ThinkParamID(523210000)); // ...or Kenneth Haight
    // When using SkeletonMan::Target::addBoneModifier without specifying any bones, the modifier is applied to the first bone in the skeleton.
    // This is a custom modifier example. You can add your own in CustomModifiers.h.
    exampleTarget2.addBoneModifier(HkModifier::CapriSun(V4D(0.0840444f, -0.0490552f, 0.0612987f, 0.9933643f)));

    // NPCParamID example:
    auto& exampleTarget3 = SkeletonMan::makeTarget(ChrMatcher::NPCParamID(30200014));
    exampleTarget3.addSkeletonModifier(HkModifier::ScaleLength(1.5f));
    exampleTarget3.addSkeletonModifier(HkModifier::ScaleSize(1.5f));

    // Map name example, this applies to every entity on the map:
    auto& exampleTarget4 = SkeletonMan::makeTarget(ChrMatcher::Map(L"m15_00_00_00"));
    exampleTarget4.addBoneModifier(HkModifier::Floss(), "L_UpperArm", "R_UpperArm");

    // Player example. Makes the player character beefier.
    // To target all c0000 player instances use ChrMatcher::Player(true)
    // Note: quaternions are used to represent rotation. 
    // I recommend using https://www.andre-gaschler.com/rotationconverter/ (don't forget to choose radians/degrees)
    auto& playerTarget = SkeletonMan::makeTarget(ChrMatcher::Player());
    playerTarget.addBoneModifier(HkModifier::ScaleLength(1.35f), "L_Clavicle", "R_Clavicle", "L_Shoulder", "R_Shoulder");
    playerTarget.addBoneModifier(HkModifier::Rotate(V4D(0.0f, 0.0610485f, 0.0f, 0.9981348f)), "Neck");
    playerTarget.addBoneModifier(HkModifier::Rotate(V4D(0.0f, 0.0f, -0.1305262f, 0.9914449f)), "L_Clavicle");
    playerTarget.addBoneModifier(HkModifier::Rotate(V4D(0.0f, 0.0f, 0.1305262f, 0.9914449f)), "R_Clavicle");
    playerTarget.addBoneModifier(HkModifier::Rotate(V4D(-0.1178125f, 0.0f, 0.1623879f, 0.9796685f)), "L_UpperArm");
    playerTarget.addBoneModifier(HkModifier::Rotate(V4D(0.1178125f, 0.0f, -0.1623879f, 0.9796685f)), "R_UpperArm");
    playerTarget.addBoneModifier(HkModifier::ScaleLength(1.1f), "Neck", "Head");
    playerTarget.addSkeletonModifier(HkModifier::ScaleLength(1.1f));
    playerTarget.addBoneModifier(HkModifier::ScaleSize(0.9f), "Head");
    playerTarget.addSkeletonModifier(HkModifier::ScaleSize(1.2f));

    // Conditional SpEffect modifiers - these will only be applied if SpEffect 3245 is present.
    // (SpEffect 3245 is applied on equipping the Lantern)
    playerTarget.addBoneModifier(HkModifier::SpEffect::Offset(V4D(0.0f, 0.25f, 0.0f), 3245), "RootPos");
    playerTarget.addBoneModifier(HkModifier::SpEffect::Rotate(V4D(0.0f, 0.0f, 1.0f, 0.0f), 3245), "RootPos");

    // Torrent example, targets all Torrent instances (in mods like Seamless Co-op)
    auto& torrentTarget = SkeletonMan::makeTarget(ChrMatcher::Torrent(true));
    torrentTarget.addSkeletonModifier(HkModifier::ScaleLength(0.2f));
    torrentTarget.addSkeletonModifier(HkModifier::ScaleSize(0.2f));
    torrentTarget.addSkeletonModifier(HkModifier::DisableClothPhysics());
    // Adding a PLAYER modifier that will disable cloth physics when riding Torrent.
    playerTarget.addSkeletonModifier(HkModifier::Mounted::DisableClothPhysics());

    SkeletonMan::instance().initialize();
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        onAttach();
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

