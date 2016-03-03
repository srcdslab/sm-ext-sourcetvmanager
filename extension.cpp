/**
 * vim: set ts=4 :
 * =============================================================================
 * SourceMod SourceTV Manager Extension
 * Copyright (C) 2004-2016 AlliedModders LLC.  All rights reserved.
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
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 *
 * Version: $Id$
 */

#include "extension.h"
#include "forwards.h"

IHLTVDirector *hltvdirector = nullptr;
IHLTVServer *hltvserver = nullptr;
IDemoRecorder *demorecorder = nullptr;
void *host_client = nullptr;
void *old_host_client = nullptr;
bool g_HostClientOverridden = false;

IGameEventManager2 *gameevents = nullptr;
CGlobalVars *gpGlobals = nullptr;
ICvar *icvar = nullptr;

IBinTools *bintools = nullptr;
ISDKTools *sdktools = nullptr;
IServer *iserver = nullptr;
IGameConfig *g_pGameConf = nullptr;

#if SOURCE_ENGINE != SE_CSGO
bool g_SendNetMsgHooked = false;
#endif

#if SOURCE_ENGINE == SE_CSGO
SH_DECL_HOOK1_void(IHLTVDirector, AddHLTVServer, SH_NOATTRIB, 0, IHLTVServer *);
SH_DECL_HOOK1_void(IHLTVDirector, RemoveHLTVServer, SH_NOATTRIB, 0, IHLTVServer *);
#else
SH_DECL_HOOK1_void(IHLTVDirector, SetHLTVServer, SH_NOATTRIB, 0, IHLTVServer *);

// Stuff to print to demo console
SH_DECL_HOOK0_void_vafmt(IClient, ClientPrintf, SH_NOATTRIB, 0);
// This should be large enough.
#define FAKE_VTBL_LENGTH 70
static void *FakeNetChanVtbl[FAKE_VTBL_LENGTH];
static void *FakeNetChan = &FakeNetChanVtbl;

SH_DECL_MANUALHOOK3(NetChan_SendNetMsg, 0, 0, 0, bool, INetMessage &, bool, bool);
#endif

SH_DECL_HOOK1(IClient, ExecuteStringCommand, SH_NOATTRIB, 0, bool, const char *);

/**
 * @file extension.cpp
 * @brief Implement extension code here.
 */

SourceTVManager g_STVManager;		/**< Global singleton for extension's main interface */

SMEXT_LINK(&g_STVManager);

extern const sp_nativeinfo_t sourcetv_natives[];

ConVar tv_force_steamauth("tv_force_steamauth", "1", FCVAR_NONE, "Validate SourceTV clients with Steam.");

bool SourceTVManager::SDK_OnLoad(char *error, size_t maxlength, bool late)
{
	sharesys->AddDependency(myself, "bintools.ext", true, true);
	sharesys->AddDependency(myself, "sdktools.ext", true, true);

	char conf_error[255];
	if (!gameconfs->LoadGameConfigFile("sourcetvmanager.games", &g_pGameConf, conf_error, sizeof(conf_error)))
	{
		if (error)
		{
			snprintf(error, maxlength, "Could not read sourcetvmanager.games: %s", conf_error);
		}
		return false;
	}

	// Get the host_client pointer
	// This is used to fix a null pointer crash when executing fake commands on bots.
	if (!g_pGameConf->GetAddress("host_client", &host_client) || !host_client)
	{
		smutils->LogError(myself, "Failed to find host_client pointer. Server might crash when executing commands on SourceTV bot.");
	}

#if SOURCE_ENGINE != SE_CSGO
	int offset;
	if (g_pGameConf->GetOffset("CNetChan::SendNetMsg", &offset))
	{
		if (offset >= FAKE_VTBL_LENGTH)
		{
			smutils->LogError(myself, "CNetChan::SendNetMsg offset too big. Need to raise define and recompile. Contact the author.");
		}
		else
		{
			// This is a hack. Bots don't have a net channel, but ClientPrintf tries to call m_NetChannel->SendNetMsg directly.
			// CGameClient::SendNetMsg would have redirected it to the hltvserver correctly, but isn't used there..
			// We craft a fake object with a large enough "vtable" and hook it using sourcehook.
			// Before a call to ClientPrintf, this fake object is set as CBaseClient::m_NetChannel, so ClientPrintf creates 
			// the SVC_Print INetMessage and calls our "hooked" m_NetChannel->SendNetMsg function.
			// In that function we just call CGameClient::SendNetMsg with the given INetMessage to flow it through the same
			// path as other net messages.
			SH_MANUALHOOK_RECONFIGURE(NetChan_SendNetMsg, offset, 0, 0);
			SH_ADD_MANUALHOOK(NetChan_SendNetMsg, &FakeNetChan, SH_MEMBER(this, &SourceTVManager::OnHLTVBotNetChanSendNetMsg), false);
			g_SendNetMsgHooked = true;
		}
	}
	else
	{
		smutils->LogError(myself, "Failed to find CNetChan::SendNetMsg offset. Can't print to demo console.");
	}
#endif

	sharesys->AddNatives(myself, sourcetv_natives);
	sharesys->RegisterLibrary(myself, "sourcetvmanager");

	return true;
}

void SourceTVManager::SDK_OnAllLoaded()
{
#if SOURCE_ENGINE == SE_CSGO
	SH_ADD_HOOK(IHLTVDirector, AddHLTVServer, hltvdirector, SH_MEMBER(this, &SourceTVManager::OnAddHLTVServer_Post), true);
	SH_ADD_HOOK(IHLTVDirector, RemoveHLTVServer, hltvdirector, SH_MEMBER(this, &SourceTVManager::OnRemoveHLTVServer_Post), true);
#else
	SH_ADD_HOOK(IHLTVDirector, SetHLTVServer, hltvdirector, SH_MEMBER(this, &SourceTVManager::OnSetHLTVServer_Post), true);
#endif

	SM_GET_LATE_IFACE(BINTOOLS, bintools);
	SM_GET_LATE_IFACE(SDKTOOLS, sdktools);

	g_pSTVForwards.Init();

	iserver = sdktools->GetIServer();
	if (!iserver)
		smutils->LogError(myself, "Failed to get IServer interface from SDKTools. Some functions won't work.");

#if SOURCE_ENGINE == SE_CSGO
	if (hltvdirector->GetHLTVServerCount() > 0)
		SelectSourceTVServer(hltvdirector->GetHLTVServer(0));

	// Hook all the exisiting servers.
	for (int i = 0; i < hltvdirector->GetHLTVServerCount(); i++)
	{
		HookSourceTVServer(hltvdirector->GetHLTVServer(i));
	}
#else
	if (hltvdirector->GetHLTVServer())
	{
		SelectSourceTVServer(hltvdirector->GetHLTVServer());
		HookSourceTVServer(hltvdirector->GetHLTVServer());
	}
#endif
}

bool SourceTVManager::SDK_OnMetamodLoad(ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	GET_V_IFACE_CURRENT(GetServerFactory, hltvdirector, IHLTVDirector, INTERFACEVERSION_HLTVDIRECTOR);
	GET_V_IFACE_CURRENT(GetEngineFactory, gameevents, IGameEventManager2, INTERFACEVERSION_GAMEEVENTSMANAGER2);
	GET_V_IFACE_CURRENT(GetEngineFactory, icvar, ICvar, CVAR_INTERFACE_VERSION);

	gpGlobals = ismm->GetCGlobals();

	g_pCVar = icvar;
	ConVar_Register(0, this);

	return true;
}

bool SourceTVManager::RegisterConCommandBase(ConCommandBase *pCommandBase)
{
	/* Always call META_REGCVAR instead of going through the engine. */
	return META_REGCVAR(pCommandBase);
}

void SourceTVManager::SDK_OnUnload()
{
#if SOURCE_ENGINE == SE_CSGO
	SH_REMOVE_HOOK(IHLTVDirector, AddHLTVServer, hltvdirector, SH_MEMBER(this, &SourceTVManager::OnAddHLTVServer_Post), true);
	SH_REMOVE_HOOK(IHLTVDirector, RemoveHLTVServer, hltvdirector, SH_MEMBER(this, &SourceTVManager::OnRemoveHLTVServer_Post), true);
#else
	SH_REMOVE_HOOK(IHLTVDirector, SetHLTVServer, hltvdirector, SH_MEMBER(this, &SourceTVManager::OnSetHLTVServer_Post), true);

	if (g_SendNetMsgHooked)
	{
		SH_REMOVE_MANUALHOOK(NetChan_SendNetMsg, &FakeNetChan, SH_MEMBER(this, &SourceTVManager::OnHLTVBotNetChanSendNetMsg), false);
		g_SendNetMsgHooked = false;
	}

#endif

	gameconfs->CloseGameConfigFile(g_pGameConf);

#if SOURCE_ENGINE == SE_CSGO
	// Unhook all the existing servers.
	for (int i = 0; i < hltvdirector->GetHLTVServerCount(); i++)
	{
		UnhookSourceTVServer(hltvdirector->GetHLTVServer(i));
	}
#else
	// Unhook the server
	if (hltvdirector->GetHLTVServer())
		UnhookSourceTVServer(hltvdirector->GetHLTVServer());
#endif
	g_pSTVForwards.Shutdown();
}

bool SourceTVManager::QueryRunning(char *error, size_t maxlength)
{
	SM_CHECK_IFACE(BINTOOLS, bintools);
	SM_CHECK_IFACE(SDKTOOLS, sdktools);

	return true;
}

void SourceTVManager::HookSourceTVServer(IHLTVServer *hltv)
{
	if (hltv != nullptr)
	{
		g_pSTVForwards.HookServer(hltv->GetBaseServer());
		g_pSTVForwards.HookRecorder(GetDemoRecorderPtr(hltv));

		if (iserver)
		{
			IClient *pClient = iserver->GetClient(hltv->GetHLTVSlot());
			if (pClient)
			{
				SH_ADD_HOOK(IClient, ExecuteStringCommand, pClient, SH_MEMBER(this, &SourceTVManager::OnHLTVBotExecuteStringCommand), false);
				SH_ADD_HOOK(IClient, ExecuteStringCommand, pClient, SH_MEMBER(this, &SourceTVManager::OnHLTVBotExecuteStringCommand_Post), true);
#if SOURCE_ENGINE != SE_CSGO
				SH_ADD_HOOK(IClient, ClientPrintf, pClient, SH_MEMBER(this, &SourceTVManager::OnHLTVBotClientPrintf_Post), false);
#endif
			}
		}
	}
}

void SourceTVManager::UnhookSourceTVServer(IHLTVServer *hltv)
{
	if (hltv != nullptr)
	{
		g_pSTVForwards.UnhookServer(hltv->GetBaseServer());
		g_pSTVForwards.UnhookRecorder(GetDemoRecorderPtr(hltv));

		if (iserver)
		{
			IClient *pClient = iserver->GetClient(hltv->GetHLTVSlot());
			if (pClient)
			{
				SH_REMOVE_HOOK(IClient, ExecuteStringCommand, pClient, SH_MEMBER(this, &SourceTVManager::OnHLTVBotExecuteStringCommand), false);
				SH_REMOVE_HOOK(IClient, ExecuteStringCommand, pClient, SH_MEMBER(this, &SourceTVManager::OnHLTVBotExecuteStringCommand_Post), true);
#if SOURCE_ENGINE != SE_CSGO
				SH_REMOVE_HOOK(IClient, ClientPrintf, pClient, SH_MEMBER(this, &SourceTVManager::OnHLTVBotClientPrintf_Post), false);
#endif
			}
		}
	}
}

void SourceTVManager::SelectSourceTVServer(IHLTVServer *hltv)
{
	// Select the new server.
	hltvserver = hltv;
	demorecorder = GetDemoRecorderPtr(hltvserver);
}

IDemoRecorder *SourceTVManager::GetDemoRecorderPtr(IHLTVServer *hltv)
{
	static int offset = -1;
	if (offset == -1 && !g_pGameConf->GetOffset("CHLTVServer::m_DemoRecorder", &offset))
	{
		smutils->LogError(myself, "Failed to get CHLTVServer::m_DemoRecorder offset.");
		return nullptr;
	}

	if (hltv)
		return (IDemoRecorder *)((intptr_t)hltv + offset);
	else
		return nullptr;
}

#if SOURCE_ENGINE == SE_CSGO
void SourceTVManager::OnAddHLTVServer_Post(IHLTVServer *hltv)
{
	HookSourceTVServer(hltv);

	// We already selected some SourceTV server. Keep it.
	if (hltvserver != nullptr)
		RETURN_META(MRES_IGNORED);
	
	// This is the first SourceTV server to be added. Hook it.
	SelectSourceTVServer(hltv);
	RETURN_META(MRES_IGNORED);
}

void SourceTVManager::OnRemoveHLTVServer_Post(IHLTVServer *hltv)
{
	UnhookSourceTVServer(hltv);

	// We got this SourceTV server selected. Now it's gone :(
	if (hltvserver == hltv)
	{
		// Is there another one available? Try to keep us operable.
		if (hltvdirector->GetHLTVServerCount() > 0)
		{
			SelectSourceTVServer(hltvdirector->GetHLTVServer(0));
			HookSourceTVServer(hltvserver);
		}
		// No sourcetv active.
		else
		{
			SelectSourceTVServer(nullptr);
		}
	}
	RETURN_META(MRES_IGNORED);
}
#else
void SourceTVManager::OnHLTVBotClientPrintf_Post(const char* buf)
{
	// Craft our own "NetChan" pointer
	static int offset = -1;
	if (!g_pGameConf->GetOffset("CBaseClient::m_NetChannel", &offset) || offset == -1)
	{
		smutils->LogError(myself, "Failed to find CBaseClient::m_NetChannel offset. Can't print to demo console.");
		RETURN_META(MRES_IGNORED);
	}

	IClient *pClient = META_IFACEPTR(IClient);

	void *pNetChannel = (void *)((char *)pClient + offset);
	// Set our fake netchannel
	*(void **)pNetChannel = &FakeNetChan;
	// Call ClientPrintf again, this time with a "Netchannel" set on the bot.
	// This will call our own OnHLTVBotNetChanSendNetMsg function
	SH_CALL(pClient, &IClient::ClientPrintf)("%s", buf);
	// Set the fake netchannel back to 0.
	*(void **)pNetChannel = nullptr;

	RETURN_META(MRES_IGNORED);
}

bool SourceTVManager::OnHLTVBotNetChanSendNetMsg(INetMessage &msg, bool bForceReliable, bool bVoice)
{
	IClient *pClient = iserver->GetClient(hltvserver->GetHLTVSlot());
	if (!pClient)
		RETURN_META_VALUE(MRES_SUPERCEDE, false);

	// Let the message flow through the intended path like CGameClient::SendNetMsg wants to.
	bool bRetSent = pClient->SendNetMsg(msg, bForceReliable);

	// It's important to supercede, because there is no original function to call.
	// (the "vtable" was empty before hooking it)
	// See FakeNetChan variable at the top.
	RETURN_META_VALUE(MRES_SUPERCEDE, bRetSent);
}

void SourceTVManager::OnSetHLTVServer_Post(IHLTVServer *hltv)
{
	// Server shut down?
	if (!hltv)
	{
		// We didn't catch the server being set..
		if (!hltvserver)
			RETURN_META(MRES_IGNORED);

		UnhookSourceTVServer(hltvserver);
	}
	else
	{
		HookSourceTVServer(hltv);
	}
	SelectSourceTVServer(hltv);
	RETURN_META(MRES_IGNORED);
}
#endif

bool SourceTVManager::OnHLTVBotExecuteStringCommand(const char *s)
{
	if (!hltvserver || !iserver || !host_client)
		RETURN_META_VALUE(MRES_IGNORED, 0);

	IClient *pClient = iserver->GetClient(hltvserver->GetHLTVSlot());
	if (!pClient)
		RETURN_META_VALUE(MRES_IGNORED, 0);

	// The IClient vtable is +4 from the CBaseClient vtable due to multiple inheritance.
	void *pGameClient = (void *)((intptr_t)pClient - 4);

	old_host_client = *(void **)host_client;
	*(void **)host_client = pGameClient;
	g_HostClientOverridden = true;

	RETURN_META_VALUE(MRES_IGNORED, 0);
}

bool SourceTVManager::OnHLTVBotExecuteStringCommand_Post(const char *s)
{
	if (!host_client || !g_HostClientOverridden)
		RETURN_META_VALUE(MRES_IGNORED, 0);

	*(void **)host_client = old_host_client;
	g_HostClientOverridden = false;
	RETURN_META_VALUE(MRES_IGNORED, 0);
}

