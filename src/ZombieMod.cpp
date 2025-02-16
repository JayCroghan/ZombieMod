/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023 Source2ZE
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

#include "protobuf/generated/cstrike15_usermessages.pb.h"
#include "protobuf/generated/usermessages.pb.h"
#include "protobuf/generated/cs_gameevents.pb.h"
#include "protobuf/generated/gameevents.pb.h"
#include "protobuf/generated/te.pb.h"

#include "zombiemod.h"
#include "iserver.h"

#include "appframework/IAppSystem.h"
#include "common.h"
#include "commands.h"
#include "detours.h"
#include "patches.h"
#include "icvar.h"
#include "interface.h"
#include "tier0/dbg.h"
#include "cschemasystem.h"
#include "plat.h"
#include "entitysystem.h"
#include "engine/igameeventsystem.h"
#include "gamesystem.h"
#include "ctimer.h"
#include "playermanager.h"
#include <entity.h>
#include "adminsystem.h"
#include "commands.h"
#include "eventlistener.h"
#include "gameconfig.h"
#include "votemanager.h"
#include "zombiereborn.h"
#include "httpmanager.h"
#include "discord.h"
#include "map_votes.h"
#include "user_preferences.h"
#include "entity/cgamerules.h"
#include "entity/ccsplayercontroller.h"
#include "entitylistener.h"

#define VPROF_ENABLED
#include "tier0/vprof.h"

#include "tier0/memdbgon.h"

double g_flUniversalTime;
float g_flLastTickedTime;
bool g_bHasTicked;

void Message(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[1024] = {};
	V_vsnprintf(buf, sizeof(buf) - 1, msg, args);

	ConColorMsg(Color(255, 0, 255, 255), "[ZOMBIE] %s", buf);

	va_end(args);
}

void Panic(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[1024] = {};
	V_vsnprintf(buf, sizeof(buf) - 1, msg, args);

	if (CommandLine()->HasParm("-dedicated"))
		Warning("[ZOMBIE] %s", buf);
#ifdef _WIN32
	else
		MessageBoxA(nullptr, buf, "Warning", 0);
#endif

	va_end(args);
}

class GameSessionConfiguration_t { };

SH_DECL_HOOK3_void(IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool, bool, bool);
SH_DECL_HOOK0_void(IServerGameDLL, GameServerSteamAPIActivated, SH_NOATTRIB, 0);
SH_DECL_HOOK0_void(IServerGameDLL, GameServerSteamAPIDeactivated, SH_NOATTRIB, 0);
SH_DECL_HOOK4_void(IServerGameClients, ClientActive, SH_NOATTRIB, 0, CPlayerSlot, bool, const char *, uint64);
SH_DECL_HOOK5_void(IServerGameClients, ClientDisconnect, SH_NOATTRIB, 0, CPlayerSlot, ENetworkDisconnectionReason, const char *, uint64, const char *);
SH_DECL_HOOK4_void(IServerGameClients, ClientPutInServer, SH_NOATTRIB, 0, CPlayerSlot, char const *, int, uint64);
SH_DECL_HOOK1_void(IServerGameClients, ClientSettingsChanged, SH_NOATTRIB, 0, CPlayerSlot );
SH_DECL_HOOK6_void(IServerGameClients, OnClientConnected, SH_NOATTRIB, 0, CPlayerSlot, const char*, uint64, const char *, const char *, bool);
SH_DECL_HOOK6(IServerGameClients, ClientConnect, SH_NOATTRIB, 0, bool, CPlayerSlot, const char*, uint64, const char *, bool, CBufferString *);
SH_DECL_HOOK8_void(IGameEventSystem, PostEventAbstract, SH_NOATTRIB, 0, CSplitScreenSlot, bool, int, const uint64*,
	INetworkSerializable*, const void*, unsigned long, NetChannelBufType_t)
SH_DECL_HOOK3_void(INetworkServerService, StartupServer, SH_NOATTRIB, 0, const GameSessionConfiguration_t&, ISource2WorldSession*, const char*);
SH_DECL_HOOK6_void(ISource2GameEntities, CheckTransmit, SH_NOATTRIB, 0, CCheckTransmitInfo **, int, CBitVec<16384> &, const Entity2Networkable_t **, const uint16 *, int);
SH_DECL_HOOK2_void(IServerGameClients, ClientCommand, SH_NOATTRIB, 0, CPlayerSlot, const CCommand &);
SH_DECL_HOOK3_void(ICvar, DispatchConCommand, SH_NOATTRIB, 0, ConCommandHandle, const CCommandContext&, const CCommand&);

ZombieMod g_ZombieMod;

IGameEventSystem *g_gameEventSystem = nullptr;
IGameEventManager2 *g_gameEventManager = nullptr;
INetworkGameServer *g_pNetworkGameServer = nullptr;
CGameEntitySystem *g_pEntitySystem = nullptr;
CEntityListener *g_pEntityListener = nullptr;
CSchemaSystem *g_pSchemaSystem2 = nullptr;
CGlobalVars *gpGlobals = nullptr;
CPlayerManager *g_playerManager = nullptr;
IVEngineServer2 *g_pEngineServer2 = nullptr;
CGameConfig *g_GameConfig = nullptr;
ISteamHTTP *g_http = nullptr;
CSteamGameServerAPIContext g_steamAPI;
CCSGameRules *g_pGameRules = nullptr;

CGameEntitySystem *GameEntitySystem()
{
	static int offset = g_GameConfig->GetOffset("GameEntitySystem");
	return *reinterpret_cast<CGameEntitySystem **>((uintptr_t)(g_pGameResourceServiceServer) + offset);
}

PLUGIN_EXPOSE(ZombieMod, g_ZombieMod);
bool ZombieMod::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pEngineServer2, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pGameResourceServiceServer, IGameResourceServiceServer, GAMERESOURCESERVICESERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pSchemaSystem2, CSchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2ServerConfig, ISource2ServerConfig, SOURCE2SERVERCONFIG_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameEntities, ISource2GameEntities, SOURCE2GAMEENTITIES_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameClients, IServerGameClients, SOURCE2GAMECLIENTS_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pNetworkServerService, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_gameEventSystem, IGameEventSystem, GAMEEVENTSYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);

	// Required to get the IMetamodListener events
	g_SMAPI->AddListener(this, this);

	Message( "Starting plugin.\n" );

	SH_ADD_HOOK_MEMFUNC(IServerGameDLL, GameFrame, g_pSource2Server, this, &ZombieMod::Hook_GameFrame, true);
	SH_ADD_HOOK_MEMFUNC(IServerGameDLL, GameServerSteamAPIActivated, g_pSource2Server, this, &ZombieMod::Hook_GameServerSteamAPIActivated, false);
	SH_ADD_HOOK_MEMFUNC(IServerGameDLL, GameServerSteamAPIDeactivated, g_pSource2Server, this, &ZombieMod::Hook_GameServerSteamAPIDeactivated, false);
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, ClientActive, g_pSource2GameClients, this, &ZombieMod::Hook_ClientActive, true);
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, ClientDisconnect, g_pSource2GameClients, this, &ZombieMod::Hook_ClientDisconnect, true);
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, ClientPutInServer, g_pSource2GameClients, this, &ZombieMod::Hook_ClientPutInServer, true);
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, ClientSettingsChanged, g_pSource2GameClients, this, &ZombieMod::Hook_ClientSettingsChanged, false);
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, OnClientConnected, g_pSource2GameClients, this, &ZombieMod::Hook_OnClientConnected, false);
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, ClientConnect, g_pSource2GameClients, this, &ZombieMod::Hook_ClientConnect, false );
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, ClientCommand, g_pSource2GameClients, this, &ZombieMod::Hook_ClientCommand, false);
	SH_ADD_HOOK_MEMFUNC(IGameEventSystem, PostEventAbstract, g_gameEventSystem, this, &ZombieMod::Hook_PostEvent, false);
	SH_ADD_HOOK_MEMFUNC(INetworkServerService, StartupServer, g_pNetworkServerService, this, &ZombieMod::Hook_StartupServer, true);
	SH_ADD_HOOK_MEMFUNC(ISource2GameEntities, CheckTransmit, g_pSource2GameEntities, this, &ZombieMod::Hook_CheckTransmit, true);
	SH_ADD_HOOK_MEMFUNC(ICvar, DispatchConCommand, g_pCVar, this, &ZombieMod::Hook_DispatchConCommand, false);

	META_CONPRINTF( "All hooks started!\n" );

	CBufferStringGrowable<256> gamedirpath;
	g_pEngineServer2->GetGameDir(gamedirpath);

	std::string gamedirname = CGameConfig::GetDirectoryName(gamedirpath.Get());

	const char *gamedataPath = "addons/zombiemod/gamedata/zombiemod.games.txt";
	Message("Loading %s for game: %s\n", gamedataPath, gamedirname.c_str());

	g_GameConfig = new CGameConfig(gamedirname, gamedataPath);
	char conf_error[255] = "";
	if (!g_GameConfig->Init(g_pFullFileSystem, conf_error, sizeof(conf_error)))
	{
		snprintf(error, maxlen, "Could not read %s: %s", g_GameConfig->GetPath().c_str(), conf_error);
		Panic("%s\n", error);
		return false;
	}

	bool bRequiredInitLoaded = true;

	if (!addresses::Initialize(g_GameConfig))
		bRequiredInitLoaded = false;

	if (!InitPatches(g_GameConfig))
		bRequiredInitLoaded = false;

	if (!InitDetours(g_GameConfig))
		bRequiredInitLoaded = false;

	int offset = g_GameConfig->GetOffset("GameEventManager");
	g_gameEventManager = (IGameEventManager2 *)(CALL_VIRTUAL(uintptr_t, offset, g_pSource2Server) - 8);

	if (!g_gameEventManager)
	{
		Panic("Failed to find GameEventManager\n");
		bRequiredInitLoaded = false;
	}

	if (!InitGameSystems())
		bRequiredInitLoaded = false;

	if (!bRequiredInitLoaded)
	{
		snprintf(error, maxlen, "One or more address lookups, patches or detours failed, please refer to startup logs for more information");
		return false;
	}

	Message( "All hooks started!\n" );

	UnlockConVars();
	UnlockConCommands();
	ConVar_Register(FCVAR_RELEASE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_GAMEDLL);

	if (late)
	{
		RegisterEventListeners();
		g_pEntitySystem = GameEntitySystem();
		g_pEntitySystem->AddListenerEntity(g_pEntityListener);
		g_pNetworkGameServer = g_pNetworkServerService->GetIGameServer();
		gpGlobals = g_pNetworkGameServer->GetGlobals();
	}

	g_pAdminSystem = new CAdminSystem();
	g_playerManager = new CPlayerManager(late);
	g_pDiscordBotManager = new CDiscordBotManager();
	g_pZRPlayerClassManager = new CZRPlayerClassManager();
	g_pMapVoteSystem = new CMapVoteSystem();
	g_pUserPreferencesSystem = new CUserPreferencesSystem();
	g_pUserPreferencesStorage = new CUserPreferencesREST();
	g_pZRWeaponConfig = new ZRWeaponConfig();
	g_pEntityListener = new CEntityListener();

	RegisterWeaponCommands();

	// Steam authentication
	new CTimer(1.0f, true, []()
	{
		g_playerManager->TryAuthenticate();
		return 1.0f;
	});

	// Check hide distance
	new CTimer(0.5f, true, []()
	{
		g_playerManager->CheckHideDistances();
		return 0.5f;
	});

	// Check for the expiration of infractions like mutes or gags
	new CTimer(30.0f, true, []()
	{
		g_playerManager->CheckInfractions();
		return 30.0f;
	});

	// run our cfg
	g_pEngineServer2->ServerCommand("exec zombiemod/zombiemod");

	srand(time(0));

	return true;
}

bool ZombieMod::Unload(char *error, size_t maxlen)
{
	SH_REMOVE_HOOK_MEMFUNC(IServerGameDLL, GameFrame, g_pSource2Server, this, &ZombieMod::Hook_GameFrame, true);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, ClientActive, g_pSource2GameClients, this, &ZombieMod::Hook_ClientActive, true);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, ClientDisconnect, g_pSource2GameClients, this, &ZombieMod::Hook_ClientDisconnect, true);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, ClientPutInServer, g_pSource2GameClients, this, &ZombieMod::Hook_ClientPutInServer, true);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, ClientSettingsChanged, g_pSource2GameClients, this, &ZombieMod::Hook_ClientSettingsChanged, false);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, OnClientConnected, g_pSource2GameClients, this, &ZombieMod::Hook_OnClientConnected, false);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, ClientConnect, g_pSource2GameClients, this, &ZombieMod::Hook_ClientConnect, false);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, ClientCommand, g_pSource2GameClients, this, &ZombieMod::Hook_ClientCommand, false);
	SH_REMOVE_HOOK_MEMFUNC(IGameEventSystem, PostEventAbstract, g_gameEventSystem, this, &ZombieMod::Hook_PostEvent, false);
	SH_REMOVE_HOOK_MEMFUNC(INetworkServerService, StartupServer, g_pNetworkServerService, this, &ZombieMod::Hook_StartupServer, true);
	SH_REMOVE_HOOK_MEMFUNC(ISource2GameEntities, CheckTransmit, g_pSource2GameEntities, this, &ZombieMod::Hook_CheckTransmit, true);
	SH_REMOVE_HOOK_MEMFUNC(ICvar, DispatchConCommand, g_pCVar, this, &ZombieMod::Hook_DispatchConCommand, false);

	ConVar_Unregister();

	g_CommandList.Purge();

	FlushAllDetours();
	UndoPatches();
	RemoveTimers();
	UnregisterEventListeners();

	if (g_playerManager)
		delete g_playerManager;

	if (g_pAdminSystem)
		delete g_pAdminSystem;

	if (g_pDiscordBotManager)
		delete g_pDiscordBotManager;

	if (g_GameConfig)
		delete g_GameConfig;

	if (g_pZRPlayerClassManager)
		delete g_pZRPlayerClassManager;

	if (g_pZRWeaponConfig)
		delete g_pZRWeaponConfig;

	if (g_pUserPreferencesSystem)
		delete g_pUserPreferencesSystem;

	if (g_pUserPreferencesStorage)
		delete g_pUserPreferencesStorage;

	if (g_pEntityListener)
		delete g_pEntityListener;

	return true;
}

void ZombieMod::Hook_DispatchConCommand(ConCommandHandle cmdHandle, const CCommandContext& ctx, const CCommand& args)
{
	if (!g_pEntitySystem)
		return;

	auto iCommandPlayerSlot = ctx.GetPlayerSlot();

	bool bSay = !V_strcmp(args.Arg(0), "say");
	bool bTeamSay = !V_strcmp(args.Arg(0), "say_team");

	if (iCommandPlayerSlot != -1 && (bSay || bTeamSay))
	{
		auto pController = CCSPlayerController::FromSlot(iCommandPlayerSlot);
		bool bGagged = pController && pController->GetZEPlayer()->IsGagged();
		bool bFlooding = pController && pController->GetZEPlayer()->IsFlooding();
		bool bAdminChat = bTeamSay && *args[1] == '@';
		bool bSilent = *args[1] == '/' || bAdminChat;
		bool bCommand = *args[1] == '!' || *args[1] == '/';

		// Chat messages should generate events regardless
		if (pController)
		{
			IGameEvent *pEvent = g_gameEventManager->CreateEvent("player_chat");

			if (pEvent)
			{
				pEvent->SetBool("teamonly", bTeamSay);
				pEvent->SetInt("userid", pController->GetPlayerSlot());
				pEvent->SetString("text", args[1]);

				g_gameEventManager->FireEvent(pEvent, true);
			}
		}

		if (!bGagged && !bSilent && !bFlooding)
		{
			SH_CALL(g_pCVar, &ICvar::DispatchConCommand)(cmdHandle, ctx, args);
		}
		else if (bFlooding)
		{
			if (pController)
				ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "You are flooding the server!");
		}
		else if (bAdminChat) // Admin chat can be sent by anyone but only seen by admins, use flood protection here too
		{
			// HACK: At this point, we can safely modify the arg buffer as it won't be passed anywhere else
			// The string here is originally ("@foo bar"), trim it to be (foo bar)
			char *pszMessage = (char*)(args.ArgS() + 2);
			pszMessage[V_strlen(pszMessage) - 1] = 0;

			for (int i = 0; i < gpGlobals->maxClients; i++)
			{
				ZEPlayer *pPlayer = g_playerManager->GetPlayer(i);

				if (!pPlayer)
					continue;

				if (pPlayer->IsAdminFlagSet(ADMFLAG_GENERIC))
					ClientPrint(CCSPlayerController::FromSlot(i), HUD_PRINTTALK, " \4(ADMINS) %s:\1 %s", pController->GetPlayerName(), pszMessage);
				else if (i == iCommandPlayerSlot.Get()) // Sender is not an admin
					ClientPrint(pController, HUD_PRINTTALK, " \4(TO ADMINS) %s:\1 %s", pController->GetPlayerName(), pszMessage);
			}
		}

		// Finally, run the chat command if it is one, so anything will print after the player's message
		if (bCommand)
		{
			// Do the same trimming as with admin chat
			char *pszMessage = (char *)(args.ArgS() + 2);

			// Host_Say at some point removes the trailing " for whatever reason, so we only remove if it was never called
			if (bSilent)
				pszMessage[V_strlen(pszMessage) - 1] = 0;

			ParseChatCommand(pszMessage, pController);
		}

		RETURN_META(MRES_SUPERCEDE);
	}
}

void ZombieMod::Hook_StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession*, const char*)
{
	g_pNetworkGameServer = g_pNetworkServerService->GetIGameServer();
	g_pEntitySystem = GameEntitySystem();
	g_pEntitySystem->AddListenerEntity(g_pEntityListener);
	gpGlobals = g_pNetworkGameServer->GetGlobals();

	Message("Hook_StartupServer: %s\n", gpGlobals->mapname);

	// run our cfg
	g_pEngineServer2->ServerCommand("exec zombiemod/zombiemod");

	// Run map cfg (if present)
	char cmd[MAX_PATH];
	V_snprintf(cmd, sizeof(cmd), "exec zombiemod/maps/%s", gpGlobals->mapname);
	g_pEngineServer2->ServerCommand(cmd);

	if(g_bHasTicked)
		RemoveMapTimers();

	g_bHasTicked = false;

	RegisterEventListeners();
	g_playerManager->SetupInfiniteAmmo();

	// Disable RTV and Extend votes after map has just started
	g_RTVState = ERTVState::MAP_START;
	g_ExtendState = EExtendState::MAP_START;

	// Allow RTV and Extend votes after 2 minutes post map start
	new CTimer(120.0f, false, []()
	{
		if (g_RTVState != ERTVState::BLOCKED_BY_ADMIN)
			g_RTVState = ERTVState::RTV_ALLOWED;

		if (g_ExtendState < EExtendState::POST_EXTEND_NO_EXTENDS_LEFT)
			g_ExtendState = EExtendState::EXTEND_ALLOWED;
		return -1.0f;
	});

	if (g_bEnableZR)
		ZR_OnStartupServer();
}

void ZombieMod::Hook_GameServerSteamAPIActivated()
{
	g_steamAPI.Init();
	g_http = g_steamAPI.SteamHTTP();

	RETURN_META(MRES_IGNORED);
}

void ZombieMod::Hook_GameServerSteamAPIDeactivated()
{
	g_http = nullptr;

	RETURN_META(MRES_IGNORED);
}

void ZombieMod::Hook_PostEvent(CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64* clients,
	INetworkSerializable* pEvent, const void* pData, unsigned long nSize, NetChannelBufType_t bufType)
{
	// Message( "Hook_PostEvent(%d, %d, %d, %lli)\n", nSlot, bLocalOnly, nClientCount, clients );
	// Need to explicitly get a pointer to the right function as it's overloaded and SH_CALL can't resolve that
	static void (IGameEventSystem::*PostEventAbstract)(CSplitScreenSlot, bool, int, const uint64 *,
							INetworkSerializable *, const void *, unsigned long, NetChannelBufType_t) = &IGameEventSystem::PostEventAbstract;

	NetMessageInfo_t *info = pEvent->GetNetMessageInfo();

	if (g_bEnableStopSound && info->m_MessageId == GE_FireBulletsId)
	{
		if (g_playerManager->GetSilenceSoundMask())
		{
			// Post the silenced sound to those who use silencesound
			// Creating a new event object requires us to include the protobuf c files which I didn't feel like doing yet
			// So instead just edit the event in place and reset later
			CMsgTEFireBullets *msg = (CMsgTEFireBullets *)pData;

			int32_t weapon_id = msg->weapon_id();
			int32_t sound_type = msg->sound_type();
			int32_t item_def_index = msg->item_def_index();

			// original weapon_id will override new settings if not removed
			msg->set_weapon_id(0);
			msg->set_sound_type(10);
			msg->set_item_def_index(61); // weapon_usp_silencer

			uint64 clientMask = *(uint64 *)clients & g_playerManager->GetSilenceSoundMask();

			SH_CALL(g_gameEventSystem, PostEventAbstract)
			(nSlot, bLocalOnly, nClientCount, &clientMask, pEvent, msg, nSize, bufType);

			msg->set_weapon_id(weapon_id);
			msg->set_sound_type(sound_type);
			msg->set_item_def_index(item_def_index);
		}

		// Filter out people using stop/silence sound from the original event
		*(uint64 *)clients &= ~g_playerManager->GetStopSoundMask();
		*(uint64 *)clients &= ~g_playerManager->GetSilenceSoundMask();
	}
	else if (info->m_MessageId == TE_WorldDecalId)
	{
		*(uint64 *)clients &= ~g_playerManager->GetStopDecalsMask();
	}
}

void ZombieMod::AllPluginsLoaded()
{
	/* This is where we'd do stuff that relies on the mod or other plugins 
	 * being initialized (for example, cvars added and events registered).
	 */

	Message( "AllPluginsLoaded\n" );
}

void ZombieMod::Hook_ClientActive( CPlayerSlot slot, bool bLoadGame, const char *pszName, uint64 xuid )
{
	Message( "Hook_ClientActive(%d, %d, \"%s\", %lli)\n", slot, bLoadGame, pszName, xuid );
}

void ZombieMod::Hook_ClientCommand( CPlayerSlot slot, const CCommand &args )
{
	if ((V_stricmp(args[0], "endmatch_votenextmap") == 0) && args.ArgC() == 2) {
		g_pMapVoteSystem->RegisterPlayerVote(slot, atoi(args[1]));
	}
#ifdef _DEBUG
	Message( "Hook_ClientCommand(%d, \"%s\")\n", slot, args.GetCommandString() );
#endif
	if (g_bEnableZR && slot != -1 && !V_strncmp(args.Arg(0), "jointeam", 8))
	{
		ZR_Hook_ClientCommand_JoinTeam(slot, args);
		RETURN_META(MRES_SUPERCEDE);
	}
}

void ZombieMod::Hook_ClientSettingsChanged( CPlayerSlot slot )
{
#ifdef _DEBUG
	Message( "Hook_ClientSettingsChanged(%d)\n", slot );
#endif
}

void ZombieMod::Hook_OnClientConnected( CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, const char *pszAddress, bool bFakePlayer )
{
	Message( "Hook_OnClientConnected(%d, \"%s\", %lli, \"%s\", \"%s\", %d)\n", slot, pszName, xuid, pszNetworkID, pszAddress, bFakePlayer );

	if(bFakePlayer)
		g_playerManager->OnBotConnected(slot);
}

bool ZombieMod::Hook_ClientConnect( CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, bool unk1, CBufferString *pRejectReason )
{
	Message( "Hook_ClientConnect(%d, \"%s\", %lli, \"%s\", %d, \"%s\")\n", slot, pszName, xuid, pszNetworkID, unk1, pRejectReason->ToGrowable()->Get() );
		
	if (!g_playerManager->OnClientConnected(slot, xuid, pszNetworkID))
		RETURN_META_VALUE(MRES_SUPERCEDE, false);

	RETURN_META_VALUE(MRES_IGNORED, true);
}

void ZombieMod::Hook_ClientPutInServer( CPlayerSlot slot, char const *pszName, int type, uint64 xuid )
{
	Message( "Hook_ClientPutInServer(%d, \"%s\", %d, %d, %lli)\n", slot, pszName, type, xuid );
	g_playerManager->OnClientPutInServer(slot);

	if (g_bEnableZR)
		ZR_Hook_ClientPutInServer(slot, pszName, type, xuid);
}

void ZombieMod::Hook_ClientDisconnect( CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid, const char *pszNetworkID )
{
	Message( "Hook_ClientDisconnect(%d, %d, \"%s\", %lli, \"%s\")\n", slot, reason, pszName, xuid, pszNetworkID );

	g_playerManager->OnClientDisconnect(slot);
}

void ZombieMod::Hook_GameFrame( bool simulating, bool bFirstTick, bool bLastTick )
{
	VPROF_ENTER_SCOPE(__FUNCTION__);
	/**
	 * simulating:
	 * ***********
	 * true  | game is ticking
	 * false | game is not ticking
	 */

	if (simulating && g_bHasTicked)
	{
		g_flUniversalTime += gpGlobals->curtime - g_flLastTickedTime;
	}
	else
	{
		g_flUniversalTime += gpGlobals->interval_per_tick;
	}

	g_flLastTickedTime = gpGlobals->curtime;
	g_bHasTicked = true;

	for (int i = g_timers.Tail(); i != g_timers.InvalidIndex();)
	{
		auto timer = g_timers[i];

		int prevIndex = i;
		i = g_timers.Previous(i);

		if (timer->m_flLastExecute == -1)
			timer->m_flLastExecute = g_flUniversalTime;

		// Timer execute 
		if (timer->m_flLastExecute + timer->m_flInterval <= g_flUniversalTime)
		{
			if (!timer->Execute())
			{
				delete timer;
				g_timers.Remove(prevIndex);
			}
			else
			{
				timer->m_flLastExecute = g_flUniversalTime;
			}
		}
	}

	if (g_bEnableZR)
		CZRRegenTimer::Tick();

	VPROF_EXIT_SCOPE();
}

void ZombieMod::Hook_CheckTransmit(CCheckTransmitInfo **ppInfoList, int infoCount, CBitVec<16384> &unionTransmitEdicts,
								const Entity2Networkable_t **pNetworkables, const uint16 *pEntityIndicies, int nEntities)
{
	if (!g_pEntitySystem)
		return;

	VPROF_ENTER_SCOPE(__FUNCTION__);

	for (int i = 0; i < infoCount; i++)
	{
		auto &pInfo = ppInfoList[i];

		// the offset happens to have a player index here,
		// though this is probably part of the client class that contains the CCheckTransmitInfo
		static int offset = g_GameConfig->GetOffset("CheckTransmitPlayerSlot");
		int iPlayerSlot = (int)*((uint8 *)pInfo + offset);

		CCSPlayerController* pSelfController = CCSPlayerController::FromSlot(iPlayerSlot);

		if (!pSelfController || !pSelfController->IsConnected())
			continue;

		auto pSelfZEPlayer = g_playerManager->GetPlayer(iPlayerSlot);

		if (!pSelfZEPlayer)
			continue;

		for (int j = 0; j < gpGlobals->maxClients; j++)
		{
			CCSPlayerController* pController = CCSPlayerController::FromSlot(j);

			// Always transmit to themselves
			if (!pController || !pController->IsConnected() || j == iPlayerSlot)
				continue;

			CBarnLight *pFlashLight = g_playerManager->GetPlayer(j)->GetFlashLight();

			// Don't transmit other players' flashlights
			if (pFlashLight)
				pInfo->m_pTransmitEntity->Clear(pFlashLight->entindex());

			if (!g_bEnableHide)
				continue;

			auto pPawn = pController->GetPawn();

			if (!pPawn)
				continue;

			// Hide players marked as hidden or ANY dead player, it seems that a ragdoll of a previously hidden player can crash?
			// TODO: Revert this if/when valve fixes the issue?
			if (pSelfZEPlayer->ShouldBlockTransmit(j) || pPawn->m_lifeState != LIFE_ALIVE)
				pInfo->m_pTransmitEntity->Clear(pPawn->entindex());
		}
	}

	VPROF_EXIT_SCOPE();
}

// Potentially might not work
void ZombieMod::OnLevelInit( char const *pMapName,
									 char const *pMapEntities,
									 char const *pOldLevel,
									 char const *pLandmarkName,
									 bool loadGame,
									 bool background )
{
	Message("OnLevelInit(%s)\n", pMapName);

	g_pMapVoteSystem->OnLevelInit(pMapName);
}

// Potentially might not work
void ZombieMod::OnLevelShutdown()
{
	Message("OnLevelShutdown()\n");
}

bool ZombieMod::Pause(char *error, size_t maxlen)
{
	return true;
}

bool ZombieMod::Unpause(char *error, size_t maxlen)
{
	return true;
}

const char *ZombieMod::GetLicense()
{
	return "GPL v3 License";
}

const char *ZombieMod::GetVersion()
{
	return "4.0 a";
}

const char *ZombieMod::GetDate()
{
	return __DATE__;
}

const char *ZombieMod::GetLogTag()
{
	return "ZOMBIE";
}

const char *ZombieMod::GetAuthor()
{
	return "c0ldfyr3";
}

const char *ZombieMod::GetDescription()
{
	return "The original ZombieMod CS:S for CS2.";
}

const char *ZombieMod::GetName()
{
	return "ZombieMod CS2";
}

const char *ZombieMod::GetURL()
{
	return "https://www.c0ld.net/zombiemod/";
}
