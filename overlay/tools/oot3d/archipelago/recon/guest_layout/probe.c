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
const unsigned size_actor = sizeof(Actor);
const unsigned size_player = sizeof(Player);
