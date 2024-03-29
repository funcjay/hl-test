/***
*
*	Copyright (c) 2020, Jay Foremska.
*
*	Use and modification of this code is allowed as long
*	as credit is provided! Enjoy!
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"

#define	KNIFE_DELAY	0.25

#define KNIFE_BODYHIT_VOLUME	512
#define KNIFE_WALLHIT_VOLUME	128


enum knife_e
{
	KNIFE_IDLE1 = 0,
	KNIFE_DRAW,
	KNIFE_HOLSTER,
	KNIFE_ATTACK1HIT,
	KNIFE_ATTACK1MISS,
	KNIFE_ATTACK2MISS,
	KNIFE_ATTACK2HIT,
	KNIFE_ATTACK3MISS,
	KNIFE_ATTACK3HIT,
	KNIFE_IDLE2,
	KNIFE_IDLE3
};


class CKnife : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 1; }
	void EXPORT SwingAgain( void );
	void EXPORT Smack( void );
	int GetItemInfo(ItemInfo *p);

	void FindHullIntersection( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity );
	void PrimaryAttack( void );
	int Swing( int fFirst );
	BOOL Deploy( void );
	void Holster( void );
	int m_iSwing;
	TraceResult m_trHit;
};

LINK_ENTITY_TO_CLASS( weapon_knife, CKnife );

void CKnife::Spawn( )
{
	Precache();
	m_iId = WEAPON_KNIFE;
	SET_MODEL(ENT(pev), "models/w_knife.mdl");
	m_iClip = WEAPON_NOCLIP;

	FallInit();// get ready to fall down.
}

void CKnife::Precache( )
{
	PRECACHE_MODEL( "models/w_knife.mdl" );
	PRECACHE_MODEL( "models/v_knife.mdl" );
	PRECACHE_MODEL( "models/p_crowbar.mdl" );

	PRECACHE_SOUND( "weapons/knife1.wav" );
	PRECACHE_SOUND( "weapons/knife2.wav" );
	PRECACHE_SOUND( "weapons/knife3.wav" );
	PRECACHE_SOUND( "weapons/knife_hit_wall1.wav" );
	PRECACHE_SOUND( "weapons/knife_hit_wall2.wav" );
	PRECACHE_SOUND( "weapons/knife_hit_flesh1.wav" );
	PRECACHE_SOUND( "weapons/knife_hit_flesh2.wav" );
}

int CKnife::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = WEAPON_NOCLIP;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = WEAPON_NOCLIP;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 2;
	p->iId = WEAPON_KNIFE;
	p->iWeight = KNIFE_WEIGHT;
	return 1;
}

BOOL CKnife::Deploy( )
{
	return DefaultDeploy( "models/v_knife.mdl", "models/p_crowbar.mdl", KNIFE_DRAW, "crowbar" );
}

void CKnife::Holster( )
{
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
	SendWeaponAnim( KNIFE_HOLSTER );
}

void CKnife::FindHullIntersection( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity )
{
	int			i, j, k;
	float		distance;
	float		*minmaxs[2] = {mins, maxs};
	TraceResult tmpTrace;
	Vector		vecHullEnd = tr.vecEndPos;
	Vector		vecEnd;

	distance = 1e6f;

	vecHullEnd = vecSrc + ((vecHullEnd - vecSrc)*2);
	UTIL_TraceLine( vecSrc, vecHullEnd, dont_ignore_monsters, pEntity, &tmpTrace );
	if ( tmpTrace.flFraction < 1.0 )
	{
		tr = tmpTrace;
		return;
	}

	for ( i = 0; i < 2; i++ )
	{
		for ( j = 0; j < 2; j++ )
		{
			for ( k = 0; k < 2; k++ )
			{
				vecEnd.x = vecHullEnd.x + minmaxs[i][0];
				vecEnd.y = vecHullEnd.y + minmaxs[j][1];
				vecEnd.z = vecHullEnd.z + minmaxs[k][2];

				UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, pEntity, &tmpTrace );
				if ( tmpTrace.flFraction < 1.0 )
				{
					float thisDistance = (tmpTrace.vecEndPos - vecSrc).Length();
					if ( thisDistance < distance )
					{
						tr = tmpTrace;
						distance = thisDistance;
					}
				}
			}
		}
	}
}

void CKnife::PrimaryAttack( )
{
	if (! Swing( 1 ))
	{
		SetThink(&CKnife::SwingAgain );
		pev->nextthink = UTIL_WeaponTimeBase() + 0.1;
	}
}

void CKnife::Smack( )
{
	DecalGunshot( &m_trHit, BULLET_PLAYER_CROWBAR );
}

void CKnife::SwingAgain( )
{
	Swing( 0 );
}

int CKnife::Swing( int fFirst )
{
	int fDidHit = FALSE;

	TraceResult tr;

	UTIL_MakeVectors (m_pPlayer->pev->v_angle);
	Vector vecSrc	= m_pPlayer->GetGunPosition( );
	Vector vecEnd	= vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( m_pPlayer->pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	if ( tr.flFraction >= 1.0 )
	{
		if (fFirst)
		{
			// miss
			switch( (m_iSwing++) % 3 )
			{
			case 0:
				SendWeaponAnim( KNIFE_ATTACK1MISS ); break;
			case 1:
				SendWeaponAnim( KNIFE_ATTACK2MISS ); break;
			case 2:
				SendWeaponAnim( KNIFE_ATTACK3MISS ); break;
			}
			m_flNextPrimaryAttack = gpGlobals->time + KNIFE_DELAY;
			// play wiff or swish sound
			switch( RANDOM_LONG( 0, 2 ) )
			{
			case 0:
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/knife1.wav", VOL_NORM, ATTN_NORM, 0, PITCH_NORM);
				break;
			case 1:
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/knife2.wav", VOL_NORM, ATTN_NORM, 0, PITCH_NORM);
				break;
			case 2:
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/knife3.wav", VOL_NORM, ATTN_NORM, 0, PITCH_NORM);
				break;
			}

			// player "shoot" animation
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		}
	}
	else
	{
		// hit
		fDidHit = TRUE;

		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

		switch( (m_iSwing++) % 3 )
		{
		case 0:
			SendWeaponAnim( KNIFE_ATTACK1HIT ); break;
		case 1:
			SendWeaponAnim( KNIFE_ATTACK2HIT ); break;
		case 2:
			SendWeaponAnim( KNIFE_ATTACK3HIT ); break;
		}

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		ClearMultiDamage( );
		pEntity->TraceAttack(m_pPlayer->pev, gSkillData.plrDmgKnife, gpGlobals->v_forward, &tr, DMG_SLASH );
		ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );

		m_flNextPrimaryAttack = gpGlobals->time + KNIFE_DELAY;

		// play thwack, smack, or dong sound
		float flVol = 1.0;
		int fHitWorld = TRUE;

		if (pEntity)
		{
			if (pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE)
			{
				switch( RANDOM_LONG( 0, 1 ) )
				{
				case 0:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/knife_hit_flesh1.wav", VOL_NORM, ATTN_NORM);
					break;
				case 1:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/knife_hit_flesh2.wav", VOL_NORM, ATTN_NORM);
					break;
				}

				m_pPlayer->m_iWeaponVolume = KNIFE_BODYHIT_VOLUME;

				if (!pEntity->IsAlive() )
					return TRUE;
				else
					flVol = 0.1;

				fHitWorld = FALSE;
			}
		}

		// play texture hit sound
		// UNDONE: Calculate the correct point of intersection when we hit with the hull instead of the line

		if (fHitWorld)
		{
			float fvolbar = TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + (vecEnd-vecSrc)*2, BULLET_PLAYER_CROWBAR);

			if ( g_pGameRules->IsMultiplayer() )
			{
				// override the volume here, cause we don't play texture sounds in multiplayer, 
				// and fvolbar is going to be 0 from the above call.

				fvolbar = 1;
			}

			// also play knife strike
			switch( RANDOM_LONG( 0, 1 ) )
			{
			case 0:
				EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/knife_hit_wall1.wav", VOL_NORM, ATTN_NORM);
				break;
			case 1:
				EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/knife_hit_wall2.wav", VOL_NORM, ATTN_NORM);
				break;
			}
		}

		// delay the decal a bit
		m_trHit = tr;
		SetThink( &CKnife::Smack );
		pev->nextthink = gpGlobals->time + 0.2;

		m_pPlayer->m_iWeaponVolume = flVol * KNIFE_WALLHIT_VOLUME;
	}
	return fDidHit;
}
