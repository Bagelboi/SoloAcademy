#include "g_local.h"
#include "w_local.h"
#include "g_functions.h"
#include "b_local.h"

static void reelOut(gentity_t* self) {
	self->s.pos.trType = TR_LINEAR;
	self->clipmask = CONTENTS_NONE;
	self->owner->client->ps.grappleHookState = SOLO_GRAPPLE_REELOUT;
	self->setTime = level.time; // <-- Add this line
	NPC_SetAnim(self->owner, SETANIM_TORSO, BOTH_FORCEPULL, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
}

static void freeHook(gentity_t* self) {
	self->owner->client->ps.grappleHookState = SOLO_GRAPPLE_NONE;
	self->owner->client->ps.grappleHookEntNum = -1;
	self->owner = NULL;
	G_FreeEntity(self);
}

void grapple_hook_think(gentity_t* self)
{
	int nextthink = 100;

	if (self->owner && self->owner->client) {
		nextthink = 20;
		switch (self->owner->client->ps.grappleHookState) {
		case SOLO_GRAPPLE_REELIN: {
			trace_t tr;
			vec3_t start, end, velocity;

			VectorSubtract(self->owner->currentOrigin, self->currentOrigin, end);
			if (VectorLength(end) > GRAPPLE_HOOK_LENGTH_MAX) {
				reelOut(self);
				break;
			}

			VectorCopy(self->currentOrigin, start);
			VectorCopy(self->s.pos.trDelta, velocity);

			VectorNormalize(velocity);


			// Calculate end point
			VectorMA(start, 80, velocity, end);

			gi.trace(&tr, start, NULL, NULL, end, self->owner->s.number, MASK_NPCSOLID, (EG2_Collision)0, 0);
			if (!tr.startsolid && tr.fraction < 1.0f) {
				if (tr.entityNum == ENTITYNUM_WORLD) {

						if (tr.surfaceFlags & SURF_SKY) {
							// Reel out if we hit the sky
							reelOut(self);
							break;
						}

					VectorCopy(tr.endpos, self->s.pos.trBase);
					self->s.pos.trTime = level.time;
					VectorScale(self->s.pos.trDelta, 0, self->s.pos.trDelta);
					self->s.pos.trType = TR_STATIONARY;
					VectorCopy(tr.endpos, self->currentOrigin);

					// Set grapple orientation to the surface normal
					vectoangles(tr.plane.normal, self->currentAngles);
					self->currentAngles[YAW] -= 90;
					self->owner->client->ps.grappleHookState = SOLO_GRAPPLE_STUCK;
					self->setTime = level.time;
					NPC_SetAnim(self->owner, SETANIM_TORSO, BOTH_FORCEPULL, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_RESTART);
					break;
				}
				else {
					gentity_t* someobject = &g_entities[tr.entityNum];
					if (someobject->NPC != NULL && self->owner->client->ps.forcePower > 15) {
						self->owner->client->ps.forcePower -= 15;
						vec3_t dirToNpc;
						VectorSubtract(someobject->currentOrigin, self->currentOrigin, dirToNpc);
						VectorNormalize(dirToNpc);
						G_Knockdown(someobject, self->owner, dirToNpc, 200, qtrue);
						self->owner->client->ps.forcePower = maximum(1, self->owner->client->ps.forcePower - 15);
						reelOut(self);
						break;
					}
				}
			}
			VectorMA(self->s.pos.trDelta, GRAPPLE_RETRACT_SPEED, velocity, self->s.pos.trDelta);
			break;
		}
		case SOLO_GRAPPLE_STUCK: {
			if ((level.time - self->setTime) % 300 == 0) {
				self->owner->client->ps.forcePower -= 1;
				if (self->owner->client->ps.forcePower < 0)
				{
					self->owner->client->ps.forcePower = 0;
				}
			}
			break;
		}
		case SOLO_GRAPPLE_REELOUT: {
			/*vec3_t dirToPlayer;

			VectorSubtract(self->currentOrigin, self->owner->currentOrigin, dirToPlayer);
			if (GRAPPLE_HOOK_LENGTH_MIN * 0.5 < VectorLength(dirToPlayer)) {
				VectorNormalize(dirToPlayer);
				VectorMA(self->s.pos.trDelta, GRAPPLE_RETRACT_SPEED * -1.2, dirToPlayer, self->s.pos.trDelta);
			}
			else*/
				freeHook(self);
			break;
		}

		default: {
			freeHook(self);
			break;
		}
		}

		if (self->owner && self->owner->client->ps.grappleHookState != SOLO_GRAPPLE_NONE)
			AddEventToPlayerstate(EV_SOLO_LINE, 0, &self->owner->client->ps);
	}
	self->nextthink = level.time + nextthink; // Update every 100 ms
}
