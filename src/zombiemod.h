/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2025 Source2ZE
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include "adminsystem.h"
#include "ctimer.h"
#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"
#include "eventlistener.h"
#include "gamesystem.h"
#include "vendor/nlohmann/json_fwd.hpp"
#include "zombiereborn.h"

using ordered_json = nlohmann::ordered_json;

extern CConVar<bool> g_cvarZMEnable;
extern CConVar<bool> g_cvarZMInfiniteAmmo;
extern CConVar<int> g_cvarZMInfiniteAmmoTotal;
extern CConVar<bool> g_cvarZMUserPresToFile;

#define ZM_PREFIX " \4[ZombieMod]\1 "

enum class EZMRoundState
{
	ROUND_START,
	POST_INFECTION,
	ROUND_END,
};

extern EZMRoundState g_ZMRoundState;

enum EZMSpawnType
{
	ZM_IN_PLACE,
	ZM_RESPAWN,
};

void ZM_OnLevelInit();
void ZM_OnRoundPrestart(IGameEvent* pEvent);
void ZM_OnRoundStart(IGameEvent* pEvent);
void ZM_OnPlayerSpawn(CCSPlayerController* pController);
void ZM_OnPlayerTakeDamage(CCSPlayerPawn* pVictimPawn, const CTakeDamageInfo* pInfo, const float damage);
void ZM_OnPlayerDeath(IGameEvent* pEvent);
void ZM_OnRoundFreezeEnd(IGameEvent* pEvent);
void ZM_OnRoundTimeWarning(IGameEvent* pEvent);
bool ZM_Hook_OnTakeDamage_Alive(CTakeDamageInfo* pInfo, CCSPlayerPawn* pVictimPawn, bool bNotAlive);
AcquireResult ZM_Detour_CCSPlayer_ItemServices_CanAcquire(CCSPlayer_ItemServices* pItemServices, CEconItemView* pEconItem);
bool ZM_Detour_CEntityIdentity_AcceptInput(CEntityIdentity* pThis, CUtlSymbolLarge* pInputName, CEntityInstance* pActivator, CEntityInstance* pCaller, variant_t* value);
void ZM_Hook_ClientPutInServer(CPlayerSlot slot, char const* pszName, int type, uint64 xuid);
void ZM_Hook_ClientCommand_JoinTeam(CPlayerSlot slot, const CCommand& args);
void ZM_Precache(IEntityResourceManifest* pResourceManifest);
bool ZM_CheckTeamWinConditions(int iTeamNum);
void ZM_CCSPlayer_WeaponServices_EquipWeapon(CCSPlayer_WeaponServices* pWeaponServices, CBasePlayerWeapon* pWeapon);
void ZM_PostEventAbstract_SosStartSoundEvent(const uint64* pClients, CNetMessagePB<CMsgSosStartSoundEvent>* pMsg);
void ZM_DecoyExploded(IGameEvent* pEvent);
void ZM_CheckFrozenPlayers();
void ZM_FreezePlayer(ZEPlayer* pPlayer, CCSPlayerController* pController, bool freeze);

void ConVarZMEnableChange(CConVar<bool>* cvar, CSplitScreenSlot nSlot, const bool* pNewValue, const bool* pOldValue);

extern void zmsounds_callback(const CCommand& args, CCSPlayerController* player);
extern void zmtele_callback(const CCommand& args, CCSPlayerController* player);
extern void zmclass_callback(const CCommand& args, CCSPlayerController* player);
extern void zminfect_callback(const CCommand& args, CCSPlayerController* player);
extern void zmrevive_callback(const CCommand& args, CCSPlayerController* player);
extern void zmmotherzombies_callback(const CCommand& args, CCSPlayerController* player);
extern void zmmz_callback(const CCommand& args, CCSPlayerController* player);