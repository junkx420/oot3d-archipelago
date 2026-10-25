// Offsets of the game structures the Archipelago client reads and writes,
// computed from OoT3D_Randomizer's z3D headers for the 3DS ARM target.
// Run by guest_layout.ps1; its output must match ap_guest_layout.h.
#include <stddef.h>
#include "z3D/z3D.h"

#define O(name, type, field) const unsigned name = offsetof(type, field);
O(player_getItemId, Player, getItemId)
O(player_interactRangeActor, Player, interactRangeActor)
O(player_stateFlags1, Player, stateFlags1)
O(player_stateFlags2, Player, stateFlags2)
O(player_ageProperties, Player, ageProperties)
O(actor_id, Actor, id)
O(actor_params, Actor, params)
O(actor_update, Actor, update)
O(actor_draw, Actor, draw)
O(actor_parent, Actor, parent)
O(actor_bgCheckFlags, Actor, bgCheckFlags)
O(actor_depthInWater, Actor, depthInWater)
O(actor_velocity_y, Actor, velocity.y)
O(age_idleDepthInWater, PlayerAgeProperties, idleDepthInWater)
O(gc_sceneNum, GlobalContext, sceneNum)
O(gc_sceneLoadFlag, GlobalContext, sceneLoadFlag)
O(gc_actorCtx, GlobalContext, actorCtx)
O(actorctx_titleCtx, ActorContext, titleCtx)
O(actorctx_actorList, ActorContext, actorList)
O(title_delayTimer, TitleCardContext, delayTimer)
O(title_durationTimer, TitleCardContext, durationTimer)
O(title_alpha, TitleCardContext, alpha)
O(save_equips, SaveContext, equips)
O(save_items, SaveContext, items)
O(save_ammo, SaveContext, ammo)
O(save_equipment, SaveContext, equipment)
O(save_upgrades, SaveContext, upgrades)
O(save_questItems, SaveContext, questItems)
O(save_dungeonItems, SaveContext, dungeonItems)
O(save_dungeonKeys, SaveContext, dungeonKeys)
O(save_gsTokens, SaveContext, gsTokens)
O(save_sceneFlags, SaveContext, sceneFlags)
O(save_healthCapacity, SaveContext, healthCapacity)
O(save_magicLevel, SaveContext, magicLevel)
O(save_magic, SaveContext, magic)
O(save_magicAcquired, SaveContext, magicAcquired)
O(save_doubleMagic, SaveContext, doubleMagic)
O(save_doubleDefense, SaveContext, doubleDefense)
O(save_eventChkInf, SaveContext, eventChkInf)
O(save_magicMeterSize, SaveContext, magicMeterSize)
O(save_healthAccumulator, SaveContext, healthAccumulator)
O(sceneflags_chest, SaveSceneFlags, chest)
O(sceneflags_swch, SaveSceneFlags, swch)
O(save_cutsceneIndex, SaveContext, cutsceneIndex)
O(save_gameMode, SaveContext, gameMode)
O(save_health, SaveContext, health)
O(save_sceneIndex, SaveContext, sceneIndex)
O(save_itemGetInf, SaveContext, itemGetInf)
O(save_infTable, SaveContext, infTable)
O(save_otherNewEventFlags, SaveContext, otherNewEventFlags)
O(sceneflags_collect, SaveSceneFlags, collect)
O(actorctx_flags, ActorContext, flags)
O(actorflags_chest, ActorFlags, chest)
O(actorflags_collect, ActorFlags, collect)
const unsigned size_sceneflags = sizeof(SaveSceneFlags);
const unsigned slot_hookshot = SLOT_HOOKSHOT;
const unsigned slot_ocarina = SLOT_OCARINA;
const unsigned slot_bombchu = SLOT_BOMBCHU;
const unsigned slot_bottle_1 = SLOT_BOTTLE_1;
const unsigned slot_bottle_4 = SLOT_BOTTLE_4;
const unsigned item_letter_ruto = ITEM_LETTER_RUTO;
const unsigned item_none = ITEM_NONE;
const unsigned size_actor = sizeof(Actor);
const unsigned size_player = sizeof(Player);
