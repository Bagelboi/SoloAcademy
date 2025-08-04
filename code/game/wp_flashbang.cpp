/*
===========================================================================
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

#include "g_local.h"
#include "b_local.h"
#include "g_functions.h"
#include "wp_saber.h"
#include "w_local.h"
#include "../cgame/cg_camera.h"
extern qboolean PM_InKnockDown(playerState_t* ps);
extern qboolean PM_HasAnimation(gentity_t* ent, int animation);
//---------------------
//	Flashbang
//---------------------
static void MYFUCKINEARS(gentity_t* ent)
{
	gentity_t* radiusEnts[128];
	int numEnts;
	const float radius = weaponData[WP_FLASHBANG].splashRadius;
	const float halfRadSquared = ((radius / 2) * (radius / 2));
	const float radiusSquared = (radius * radius);
	float distSq;
	int i;
	vec3_t boltOrg;

	VectorCopy(ent->currentOrigin, boltOrg);

	numEnts = G_RadiusList(boltOrg, radius, ent, qtrue, radiusEnts);

	for (i = 0; i < numEnts; i++)
	{
		gentity_t* targ = radiusEnts[i];
		if (!targ || !targ->inuse || !targ->client)
			continue;

		if (targ->client->NPC_class == CLASS_HOWLER || targ->client->NPC_class == CLASS_BOBAFETT)
			continue;

		distSq = DistanceSquared(targ->currentOrigin, boltOrg);
		if (distSq <= radiusSquared)
		{
			if (distSq < halfRadSquared && Q_irand(0, g_spskill->integer))
			{
				G_Damage(targ, ent, ent->owner, vec3_origin, boltOrg, weaponData[WP_FLASHBANG].splashDamage, DAMAGE_NO_KNOCKBACK, MOD_IMPACT);
			}

			if (targ->health > 0 &&
				targ->client->NPC_class != CLASS_RANCOR &&
				targ->client->NPC_class != CLASS_ATST &&
				!PM_InKnockDown(&targ->client->ps))
			{
				if (PM_HasAnimation(targ, BOTH_SONICPAIN_START))
				{
					if (targ->client->ps.torsoAnim != BOTH_SONICPAIN_START &&
						targ->client->ps.torsoAnim != BOTH_SONICPAIN_HOLD)
					{
						NPC_SetAnim(targ, SETANIM_LEGS, BOTH_SONICPAIN_START, SETANIM_FLAG_NORMAL);
						NPC_SetAnim(targ, SETANIM_TORSO, BOTH_SONICPAIN_START, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
						targ->client->ps.torsoAnimTimer += 100;
						targ->client->ps.weaponTime = targ->client->ps.torsoAnimTimer;
					}
					else if (targ->client->ps.torsoAnimTimer <= 100)
					{
						NPC_SetAnim(targ, SETANIM_LEGS, BOTH_SONICPAIN_HOLD, SETANIM_FLAG_NORMAL);
						NPC_SetAnim(targ, SETANIM_TORSO, BOTH_SONICPAIN_HOLD, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD);
						targ->client->ps.torsoAnimTimer += 100;
						targ->client->ps.weaponTime = targ->client->ps.torsoAnimTimer;
					}
				}
			}
		}
	}

	// Optional: camera shake effect for player
	if (player && player->inuse)
	{
		float playerDist = Distance(player->currentOrigin, boltOrg);
		if (playerDist < 256.0f)
		{
			CGCam_Shake(1.0f * playerDist / 128.0f, 200);
		}
	}
}


//---------------------------------------------------------
void WP_flashbang_explode_think(gentity_t* ent)
//---------------------------------------------------------
{
	if ((ent->s.eFlags & EF_HELD_BY_SAND_CREATURE))
	{
		ent->takedamage = qfalse; // don't allow double deaths!

		G_Damage(ent->activator, ent, ent->owner, vec3_origin, ent->currentOrigin, weaponData[WP_FLASHBANG].altDamage, 0, MOD_EXPLOSIVE);
		G_PlayEffect("thermal/shockwave", ent->currentOrigin);
		G_PlayEffect("howler/sonic", ent->currentOrigin);

		G_FreeEntity(ent);
	}
	else if (!ent->count)
	{
		G_Sound(ent, G_SoundIndex("sound/weapons/thermal/warning.wav"));
		ent->count = -1;
		ent->nextthink = level.time + 800;
		ent->svFlags |= SVF_BROADCAST;//so everyone hears/sees the explosion?
	}
	else
	{
		if (ent->count < 0) {
			vec3_t pos;
			VectorSet(pos, ent->currentOrigin[0], ent->currentOrigin[1], ent->currentOrigin[2] + 8);

			ent->takedamage = qfalse; // don't allow double deaths!

			ent->count = 52; //deafen for 5 seconds
			ent->nextthink = level.time + 50;
			G_PlayEffect("thermal/shockwave", ent->currentOrigin);
			G_PlayEffect("howler/sonic", ent->currentOrigin);
		}

		else if (ent->count > 2)
		{
			MYFUCKINEARS(ent);
			ent->nextthink = level.time + 100;
			ent->count -= 1;
		}
		else {
			G_FreeEntity(ent);
		}
	}

}

//-------------------------------------------------------------------------------------------------------------
void flashbang_die(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, int mod, int dFlags, int hitLoc)
//-------------------------------------------------------------------------------------------------------------
{
	WP_flashbang_explode_think(self);
}


//---------------------------------------------------------
void WP_flashbang_think(gentity_t* ent)
//---------------------------------------------------------
{
	int			count;
	qboolean	blow = qfalse;

	// Thermal detonators for the player do occasional radius checks and blow up if there are entities in the blast radius
	//	This is done so that the main fire is actually useful as an attack.  We explode anyway after delay expires.

	if ((ent->s.eFlags & EF_HELD_BY_SAND_CREATURE))
	{//blow once creature is underground (done with anim)
		//FIXME: chance of being spit out?  Especially if lots of delay left...
		ent->e_TouchFunc = touchF_NULL;//don't impact on anything
		if (!ent->activator
			|| !ent->activator->client
			|| !ent->activator->client->ps.legsAnimTimer)
		{//either something happened to the sand creature or it's done with it's attack anim
			//blow!
			ent->e_ThinkFunc = thinkF_WP_flashbang_explode_think;
			ent->nextthink = level.time + Q_irand(50, 2000);
		}
		else
		{//keep checking
			ent->nextthink = level.time + TD_THINK_TIME;
		}
		return;
	}
	else if (ent->delay > level.time)
	{
		//	Finally, we force it to bounce at least once before doing the special checks, otherwise it's just too easy for the player?
		if (ent->has_bounced)
		{
			count = G_RadiusList(ent->currentOrigin, TD_TEST_RAD, ent, qtrue, ent_list);

			for (int i = 0; i < count; i++)
			{
				if (ent_list[i]->s.number == 0)
				{
					// avoid deliberately blowing up next to the player, no matter how close any enemy is..
					//	...if the delay time expires though, there is no saving the player...muwhaaa haa ha
					blow = qfalse;
					break;
				}
				else if (ent_list[i]->client
					&& ent_list[i]->client->NPC_class != CLASS_SAND_CREATURE//ignore sand creatures
					&& ent_list[i]->health > 0)
				{
					//FIXME! sometimes the ent_list order changes, so we should make sure that the player isn't anywhere in this list
					blow = qtrue;
				}
			}
		}
	}
	else
	{
		// our death time has arrived, even if nothing is near us
		blow = qtrue;
	}

	if (blow)
	{
		ent->e_ThinkFunc = thinkF_WP_flashbang_explode_think;
		ent->nextthink = level.time + 50;
	}
	else
	{
		// we probably don't need to do this thinking logic very often...maybe this is fast enough?
		ent->nextthink = level.time + TD_THINK_TIME;
	}
}

//---------------------------------------------------------
gentity_t* WP_fire_flashbang(gentity_t* ent, qboolean alt_fire)
//---------------------------------------------------------
{
	gentity_t* bolt;
	vec3_t		dir, start;
	float		damageScale = 1.0f;

	VectorCopy(forwardVec, dir);
	VectorCopy(muzzle, start);

	bolt = G_Spawn();

	bolt->classname = "flashbang";

	if (ent->s.number != 0)
	{
		// If not the player, cut the damage a bit so we don't get pounded on so much
		damageScale = TD_NPC_DAMAGE_CUT;
	}

	if (!alt_fire && ent->s.number == 0)
	{
		// Main fires for the players do a little bit of extra thinking
		bolt->e_ThinkFunc = thinkF_WP_flashbang_think;
		bolt->nextthink = level.time + TD_THINK_TIME;
		bolt->delay = level.time + TD_TIME; // How long 'til she blows
	}
	else
	{
		bolt->e_ThinkFunc = thinkF_WP_flashbang_explode_think;
		bolt->nextthink = level.time + TD_TIME; // How long 'til she blows
	}

	bolt->mass = 10;

	// How 'bout we give this thing a size...
	VectorSet(bolt->mins, -4.0f, -4.0f, -4.0f);
	VectorSet(bolt->maxs, 4.0f, 4.0f, 4.0f);
	bolt->clipmask = MASK_SHOT;
	bolt->clipmask &= ~CONTENTS_CORPSE;
	bolt->contents = CONTENTS_SHOTCLIP;
	bolt->takedamage = qtrue;
	bolt->health = 15;
	bolt->e_DieFunc = dieF_thermal_die;

	WP_TraceSetStart(ent, start, bolt->mins, bolt->maxs);//make sure our start point isn't on the other side of a wall

	float chargeAmount = 1.0f; // default of full charge

	if (ent->client)
	{
		chargeAmount = level.time - ent->client->ps.weaponChargeTime;
	}

	// get charge amount
	chargeAmount = chargeAmount / (float)TD_VELOCITY;

	if (chargeAmount > 1.0f)
	{
		chargeAmount = 1.0f;
	}
	else if (chargeAmount < TD_MIN_CHARGE)
	{
		chargeAmount = TD_MIN_CHARGE;
	}

	float	thrownSpeed = TD_VELOCITY;
	const qboolean thisIsAShooter = (qboolean)!Q_stricmp("misc_weapon_shooter", ent->classname);

	if (thisIsAShooter)
	{
		if (ent->delay != 0)
		{
			thrownSpeed = ent->delay;
		}
	}

	// normal ones bounce, alt ones explode on impact
	bolt->s.pos.trType = TR_GRAVITY;
	bolt->owner = ent;
	VectorScale(dir, thrownSpeed * chargeAmount, bolt->s.pos.trDelta);

	if (ent->health > 0)
	{
		bolt->s.pos.trDelta[2] += 120;

		if ((ent->NPC || (ent->s.number && thisIsAShooter))
			&& ent->enemy)
		{//NPC or misc_weapon_shooter
			//FIXME: we're assuming he's actually facing this direction...
			vec3_t	target;

			VectorCopy(ent->enemy->currentOrigin, target);
			if (target[2] <= start[2])
			{
				vec3_t	vec;
				VectorSubtract(target, start, vec);
				VectorNormalize(vec);
				VectorMA(target, Q_flrand(0, -32), vec, target);//throw a little short
			}

			target[0] += Q_flrand(-5, 5) + (Q_flrand(-1.0f, 1.0f) * (6 - ent->NPC->currentAim) * 2);
			target[1] += Q_flrand(-5, 5) + (Q_flrand(-1.0f, 1.0f) * (6 - ent->NPC->currentAim) * 2);
			target[2] += Q_flrand(-5, 5) + (Q_flrand(-1.0f, 1.0f) * (6 - ent->NPC->currentAim) * 2);

			WP_LobFire(ent, start, target, bolt->mins, bolt->maxs, bolt->clipmask, bolt->s.pos.trDelta, qtrue, ent->s.number, ent->enemy->s.number);
		}
		else if (thisIsAShooter && ent->target && !VectorCompare(ent->pos1, vec3_origin))
		{//misc_weapon_shooter firing at a position
			WP_LobFire(ent, start, ent->pos1, bolt->mins, bolt->maxs, bolt->clipmask, bolt->s.pos.trDelta, qtrue, ent->s.number, ent->enemy->s.number);
		}
	}

	if (alt_fire)
	{
		bolt->alt_fire = qtrue;
	}
	else
	{
		bolt->s.eFlags |= EF_BOUNCE_HALF;
	}

	bolt->s.loopSound = G_SoundIndex("sound/weapons/thermal/thermloop.wav");
	bolt->damage = weaponData[WP_FLASHBANG].damage * damageScale;
	bolt->dflags = 0;
	bolt->splashDamage = weaponData[WP_FLASHBANG].splashDamage * damageScale;
	bolt->splashRadius = weaponData[WP_FLASHBANG].splashRadius;

	bolt->s.eType = ET_MISSILE;
	bolt->svFlags = SVF_USE_CURRENT_ORIGIN;
	bolt->s.weapon = WP_FLASHBANG;

	if (alt_fire)
	{
		bolt->methodOfDeath = MOD_IMPACT;
		bolt->splashMethodOfDeath = MOD_IMPACT;//? SPLASH;
	}
	else
	{
		bolt->methodOfDeath = MOD_IMPACT;
		bolt->splashMethodOfDeath = MOD_IMPACT;//? SPLASH;
	}

	bolt->s.pos.trTime = level.time;		// move a bit on the very first frame
	VectorCopy(start, bolt->s.pos.trBase);

	SnapVector(bolt->s.pos.trDelta);			// save net bandwidth
	VectorCopy(start, bolt->currentOrigin);

	VectorCopy(start, bolt->pos2);

	return bolt;
}

//---------------------------------------------------------
gentity_t* WP_drop_flashbang(gentity_t* ent)
//---------------------------------------------------------
{
	AngleVectors(ent->client->ps.viewangles, forwardVec, vrightVec, up);
	CalcEntitySpot(ent, SPOT_WEAPON, muzzle);
	return (WP_fire_flashbang(ent, qfalse));
}
