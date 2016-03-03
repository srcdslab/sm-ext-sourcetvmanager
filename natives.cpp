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
#include "natives.h"

#define TICK_INTERVAL			(gpGlobals->interval_per_tick)
#define TIME_TO_TICKS( dt )		( (int)( 0.5f + (float)(dt) / TICK_INTERVAL ) )

extern const sp_nativeinfo_t sourcetv_natives[];

SH_DECL_MANUALHOOK0_void_vafmt(CBaseServer_BroadcastPrintf, 0, 0, 0);

bool g_bHasClientPrintfOffset = false;

void SetupNativeCalls()
{
	int offset = -1;
	if (!g_pGameConf->GetOffset("CBaseServer::BroadcastPrintf", &offset) || offset == -1)
	{
		smutils->LogError(myself, "Failed to get CBaseServer::BroadcastPrintf offset.");
	}
	else
	{
		SH_MANUALHOOK_RECONFIGURE(CBaseServer_BroadcastPrintf, offset, 0, 0);
		g_bHasClientPrintfOffset = true;
	}
}

// native SourceTV_GetServerInstanceCount();
static cell_t Native_GetServerInstanceCount(IPluginContext *pContext, const cell_t *params)
{
#if SOURCE_ENGINE == SE_CSGO
	return hltvdirector->GetHLTVServerCount();
#else
	return hltvdirector->GetHLTVServer() ? 1 : 0;
#endif
}

// native SourceTV_SelectServerInstance();
static cell_t Native_SelectServerInstance(IPluginContext *pContext, const cell_t *params)
{
#if SOURCE_ENGINE == SE_CSGO
	if (params[1] < 0 || params[1] >= hltvdirector->GetHLTVServerCount())
	{
		pContext->ReportError("Invalid HLTV server instance number (%d).", params[1]);
		return 0;
	}
	g_STVManager.SelectSourceTVServer(hltvdirector->GetHLTVServer(params[1]));
#else
	if (params[1] != 0 || !hltvdirector->GetHLTVServer())
	{
		pContext->ReportError("Invalid HLTV server instance number (%d).", params[1]);
		return 0;
	}
	// There only is one hltv server.
	g_STVManager.SelectSourceTVServer(hltvdirector->GetHLTVServer());
#endif

	return 0;
}

// native SourceTV_GetSelectedServerInstance();
static cell_t Native_GetSelectedServerInstance(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return -1;

#if SOURCE_ENGINE == SE_CSGO
	for (int i = 0; i < hltvdirector->GetHLTVServerCount(); i++)
	{
		if (hltvserver == hltvdirector->GetHLTVServer(i))
			return i;
	}

	// We should have found it in the above loop :S
	hltvserver = nullptr;
	return -1;
#else
	// There only is one hltv server.
	return 0;
#endif
}

// native SourceTV_GetBotIndex();
static cell_t Native_GetBotIndex(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	return hltvserver->GetHLTVSlot() + 1;
}

// native bool:SourceTV_GetLocalStats(&proxies, &slots, &specs);
static cell_t Native_GetLocalStats(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	int proxies, slots, specs;
	hltvserver->GetLocalStats(proxies, slots, specs);

	cell_t *plProxies, *plSlots, *plSpecs;
	pContext->LocalToPhysAddr(params[1], &plProxies);
	pContext->LocalToPhysAddr(params[2], &plSlots);
	pContext->LocalToPhysAddr(params[3], &plSpecs);

	*plProxies = proxies;
	*plSlots = slots;
	*plSpecs = specs;
	return 1;
}

// native bool:SourceTV_GetGlobalStats(&proxies, &slots, &specs);
static cell_t Native_GetGlobalStats(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	int proxies, slots, specs;
	hltvserver->GetGlobalStats(proxies, slots, specs);

	cell_t *plProxies, *plSlots, *plSpecs;
	pContext->LocalToPhysAddr(params[1], &plProxies);
	pContext->LocalToPhysAddr(params[2], &plSlots);
	pContext->LocalToPhysAddr(params[3], &plSpecs);

	*plProxies = proxies;
	*plSlots = slots;
	*plSpecs = specs;
	return 1;
}

// native SourceTV_GetBroadcastTick();
static cell_t Native_GetBroadcastTick(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return -1;

	return hltvdirector->GetDirectorTick();
}

// native Float:SourceTV_GetDelay();
static cell_t Native_GetDelay(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return -1;

	return sp_ftoc(hltvdirector->GetDelay());
}

static bool BroadcastEventLocal(IHLTVServer *server, IGameEvent *event, bool bReliable)
{
	static ICallWrapper *pBroadcastEventLocal = nullptr;

	if (!pBroadcastEventLocal)
	{
		void *addr = nullptr;
		if (!g_pGameConf->GetMemSig("CHLTVServer::BroadcastEventLocal", &addr) || !addr)
		{
			smutils->LogError(myself, "Failed to get CHLTVServer::BroadcastEventLocal signature.");
			return false;
		}

		PassInfo pass[2];
		pass[0].flags = PASSFLAG_BYVAL;
		pass[0].type = PassType_Basic;
		pass[0].size = sizeof(IGameEvent *);
		pass[1].flags = PASSFLAG_BYVAL;
		pass[1].type = PassType_Basic;
		pass[1].size = sizeof(bool);

		pBroadcastEventLocal = bintools->CreateCall(addr, CallConv_ThisCall, NULL, pass, 2);
	}

	if (pBroadcastEventLocal)
	{
		unsigned char vstk[sizeof(void *) + sizeof(IGameEvent *) + sizeof(bool)];
		unsigned char *vptr = vstk;

		*(void **)vptr = (void *)server;
		vptr += sizeof(void *);
		*(IGameEvent **)vptr = event;
		vptr += sizeof(char *);
		*(bool *)vptr = bReliable;

		pBroadcastEventLocal->Execute(vstk, NULL);
		return true;
	}
	return false;
}

// native bool:SourceTV_BroadcastScreenMessage(bool:bLocalOnly, const String:format[], any:...);
static cell_t Native_BroadcastScreenMessage(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	char buffer[1024];
	size_t len;
	{
		DetectExceptions eh(pContext);
		len = smutils->FormatString(buffer, sizeof(buffer), pContext, params, 2);
		if (eh.HasException())
			return 0;
	}

	IGameEvent *msg = gameevents->CreateEvent("hltv_message", true);
	if (!msg)
		return 0;

	msg->SetString("text", buffer);

	int ret = 1;
	bool bLocalOnly = params[1] != 0;
	if (bLocalOnly)
		hltvserver->BroadcastEvent(msg);
	else
		ret = BroadcastEventLocal(hltvserver, msg, false);

	gameevents->FreeEvent(msg);

	return ret;
}

// native bool:SourceTV_BroadcastConsoleMessage(const String:format[], any:...);
static cell_t Native_BroadcastConsoleMessage(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	if (!g_bHasClientPrintfOffset)
		return 0;

	char buffer[1024];
	size_t len;
	{
		DetectExceptions eh(pContext);
		len = smutils->FormatString(buffer, sizeof(buffer), pContext, params, 1);
		if (eh.HasException())
			return 0;
	}

	SH_MCALL(hltvserver->GetBaseServer(), CBaseServer_BroadcastPrintf)("%s\n", buffer);

	return 1;
}

// native bool:SourceTV_BroadcastChatMessage(bool:bLocalOnly, const String:format[], any:...);
static cell_t Native_BroadcastChatMessage(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	char buffer[1024];
	size_t len;
	{
		DetectExceptions eh(pContext);
		len = smutils->FormatString(buffer, sizeof(buffer), pContext, params, 2);
		if (eh.HasException())
			return 0;
	}

	IGameEvent *msg = gameevents->CreateEvent("hltv_chat", true);
	if (!msg)
		return 0;

	msg->SetString("text", buffer);

	int ret = 1;
	bool bLocalOnly = params[1] != 0;
	if (bLocalOnly)
		hltvserver->BroadcastEvent(msg);
	else
		ret = BroadcastEventLocal(hltvserver, msg, false);

	gameevents->FreeEvent(msg);

	return ret;
}

// native SourceTV_GetViewEntity();
static cell_t Native_GetViewEntity(IPluginContext *pContext, const cell_t *params)
{
	return hltvdirector->GetPVSEntity();
}

// native SourceTV_GetViewOrigin();
static cell_t Native_GetViewOrigin(IPluginContext *pContext, const cell_t *params)
{
	Vector pvs = hltvdirector->GetPVSOrigin();

	cell_t *addr;
	pContext->LocalToPhysAddr(params[1], &addr);
	addr[0] = sp_ftoc(pvs.x);
	addr[1] = sp_ftoc(pvs.y);
	addr[2] = sp_ftoc(pvs.z);
	return 0;
}

// native bool:SourceTV_ForceFixedCameraShot(Float:pos[], Float:angle[], iTarget, Float:fov, Float:fDuration);
static cell_t Native_ForceFixedCameraShot(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	// Validate entities.
	if (params[3] != 0 && !gamehelpers->ReferenceToEntity(params[3]))
	{
		pContext->ReportError("Invalid target (%d - %d)", gamehelpers->ReferenceToIndex(params[3]), params[3]);
		return 0;
	}

	cell_t *pos;
	pContext->LocalToPhysAddr(params[1], &pos);

	cell_t *angle;
	pContext->LocalToPhysAddr(params[2], &angle);

	// Update director state like it would do itself.
	g_HLTVDirectorWrapper.SetPVSEntity(0);
	Vector vPos(sp_ctof(pos[0]), sp_ctof(pos[1]), sp_ctof(pos[2]));
	g_HLTVDirectorWrapper.SetPVSOrigin(vPos);

	IGameEvent *shot = gameevents->CreateEvent("hltv_fixed", true);
	if (!shot)
		return 0;
	
	shot->SetInt("posx", vPos.x);
	shot->SetInt("posy", vPos.y);
	shot->SetInt("posz", vPos.z);
	shot->SetInt("theta", sp_ctof(angle[0]));
	shot->SetInt("phi", sp_ctof(angle[1]));
	shot->SetInt("target", params[3] ? gamehelpers->ReferenceToIndex(params[3]) : 0);
	shot->SetFloat("fov", sp_ctof(params[4]));

	hltvserver->BroadcastEvent(shot);
	gameevents->FreeEvent(shot);

	// Prevent auto director from changing shots until we allow it to again.
	g_HLTVDirectorWrapper.SetNextThinkTick(hltvdirector->GetDirectorTick() + TIME_TO_TICKS(sp_ctof(params[5])));

	return 1;
}

// native bool:SourceTV_ForceChaseCameraShot(iTarget1, iTarget2, distance, phi, theta, bool:bInEye, Float:fDuration);
static cell_t Native_ForceChaseCameraShot(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	// Validate entities.
	if (!gamehelpers->ReferenceToEntity(params[1]))
	{
		pContext->ReportError("Invalid target1 (%d - %d)", gamehelpers->ReferenceToIndex(params[1]), params[1]);
		return 0;
	}

	if (params[2] != 0 && !gamehelpers->ReferenceToEntity(params[2]))
	{
		pContext->ReportError("Invalid target2 (%d - %d)", gamehelpers->ReferenceToIndex(params[2]), params[2]);
		return 0;
	}

	IGameEvent *shot = gameevents->CreateEvent("hltv_chase", true);
	if (!shot)
		return 0;

	shot->SetInt("target1", gamehelpers->ReferenceToIndex(params[1]));
	shot->SetInt("target2", params[2] ? gamehelpers->ReferenceToIndex(params[2]) : 0);
	shot->SetInt("distance", params[3]);
	shot->SetInt("phi", params[4]); // hi/low
	shot->SetInt("theta", params[5]); // left/right
	shot->SetInt("ineye", params[6] ? 1 : 0);

	// Update director state
	g_HLTVDirectorWrapper.SetPVSEntity(gamehelpers->ReferenceToIndex(params[1]));

	hltvserver->BroadcastEvent(shot);
	gameevents->FreeEvent(shot);

	// Prevent auto director from changing shots until we allow it to again.
	g_HLTVDirectorWrapper.SetNextThinkTick(hltvdirector->GetDirectorTick() + TIME_TO_TICKS(sp_ctof(params[7])));

	return 1;
}

// native bool:SourceTV_IsRecording();
static cell_t Native_IsRecording(IPluginContext *pContext, const cell_t *params)
{
	if (demorecorder == nullptr)
		return 0;

	return demorecorder->IsRecording();
}

// Checks in COM_IsValidPath in the engine
static bool IsValidPath(const char *path)
{
	return strlen(path) > 0 && !strstr(path, "\\\\") && !strstr(path, ":") && !strstr(path, "..") && !strstr(path, "\n") && !strstr(path, "\r");
}

// native bool:SourceTV_StartRecording(const String:sFilename[]);
static cell_t Native_StartRecording(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr || demorecorder == nullptr)
		return 0;

	// SourceTV is not active.
	if (!hltvserver->GetBaseServer()->IsActive())
		return 0;

	// Only SourceTV Master can record demos instantly
	if (!hltvserver->IsMasterProxy())
		return 0;

	// already recording
	if (demorecorder->IsRecording())
		return 0;

	char *pFile;
	pContext->LocalToString(params[1], &pFile);
	
	// Invalid path.
	if (!IsValidPath(pFile))
		return 0;

	// Make sure there is a '.dem' suffix
	char pPath[PLATFORM_MAX_PATH];
	size_t len = strlen(pFile);
	const char *ext = libsys->GetFileExtension(pFile);
	if (!ext || stricmp(ext, "dem") != 0)
		ext = ".dem";
	else
		ext = "";
	smutils->Format(pPath, sizeof(pPath), "%s%s", pFile, ext);

#if SOURCE_ENGINE == SE_CSGO
	if (hltvdirector->GetHLTVServerCount() > 1)
	{
		for (int i = 0; i < hltvdirector->GetHLTVServerCount(); i++)
		{
			IHLTVServer *otherserver = hltvdirector->GetHLTVServer(i);
			if (!otherserver->IsRecording())
				continue;
			
			// Cannot record. another SourceTV is currently recording into that file.
			if (!stricmp(pPath, otherserver->GetRecordingDemoFilename()))
				return 0;
		}
	}
#endif

	demorecorder->StartRecording(pPath, false);

	return 1;
}

// native bool:SourceTV_StopRecording();
static cell_t Native_StopRecording(IPluginContext *pContext, const cell_t *params)
{
	if (demorecorder == nullptr)
		return 0;

	if (!demorecorder->IsRecording())
		return 0;

#if SOURCE_ENGINE == SE_CSGO
	hltvserver->StopRecording(NULL);
	// TODO: Stop recording on all other active hltvservers (tv_stoprecord in csgo does this)
#else
	demorecorder->StopRecording();
#endif

	return 1;
}

// native bool:SourceTV_GetDemoFileName(String:sFilename[], maxlen);
static cell_t Native_GetDemoFileName(IPluginContext *pContext, const cell_t *params)
{
	if (demorecorder == nullptr)
		return 0;

	if (!demorecorder->IsRecording())
		return 0;

	char *pDemoFile = (char *)demorecorder->GetDemoFile();
	if (!pDemoFile)
		return 0;

	pContext->StringToLocalUTF8(params[1], params[2], pDemoFile, NULL);

	return 1;
}

// native SourceTV_GetRecordingTick();
static cell_t Native_GetRecordingTick(IPluginContext *pContext, const cell_t *params)
{
	if (demorecorder == nullptr)
		return -1;

	if (!demorecorder->IsRecording())
		return -1;

	return demorecorder->GetRecordingTick();
}

// native bool:SourceTV_PrintToDemoConsole(const String:format[], any:...);
static cell_t Native_PrintToDemoConsole(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	if (!iserver)
		return 0;
	IClient *pClient = iserver->GetClient(hltvserver->GetHLTVSlot());
	if (!pClient)
		return 0;

	char buffer[1024];
	size_t len;
	{
		DetectExceptions eh(pContext);
		len = smutils->FormatString(buffer, sizeof(buffer), pContext, params, 1);
		if (eh.HasException())
			return 0;
	}

	pClient->ClientPrintf("%s\n", buffer);

	return 1;
}



// native SourceTV_GetSpectatorCount();
static cell_t Native_GetSpectatorCount(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return -1;

	return hltvserver->GetBaseServer()->GetNumClients();
}

// native SourceTV_GetMaxClients();
static cell_t Native_GetMaxClients(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return -1;

	return hltvserver->GetBaseServer()->GetMaxClients();
}

// native SourceTV_GetClientCount();
static cell_t Native_GetClientCount(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return -1;

	return hltvserver->GetBaseServer()->GetClientCount();
}

// native bool:SourceTV_IsClientConnected(client);
static cell_t Native_IsClientConnected(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	cell_t client = params[1];
	if (client < 1 || client > hltvserver->GetBaseServer()->GetClientCount())
	{
		pContext->ReportError("Invalid spectator client index %d.", client);
		return 0;
	}

	HLTVClientWrapper *pClient = g_HLTVClientManager.GetClient(client);
	return pClient->IsConnected();
}

// native SourceTV_GetSpectatorName(client, String:name[], maxlen);
static cell_t Native_GetSpectatorName(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	cell_t client = params[1];
	if (client < 1 || client > hltvserver->GetBaseServer()->GetClientCount())
	{
		pContext->ReportError("Invalid spectator client index %d.", client);
		return 0;
	}

	HLTVClientWrapper *pClient = g_HLTVClientManager.GetClient(client);
	if (!pClient->IsConnected())
	{
		pContext->ReportError("Client %d is not connected.", client);
		return 0;
	}
	
	pContext->StringToLocalUTF8(params[2], static_cast<size_t>(params[3]), pClient->Name(), NULL);
	return 0;
}

// native SourceTV_GetSpectatorIP(client, String:ip[], maxlen);
static cell_t Native_GetSpectatorIP(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	cell_t client = params[1];
	if (client < 1 || client > hltvserver->GetBaseServer()->GetClientCount())
	{
		pContext->ReportError("Invalid spectator client index %d.", client);
		return 0;
	}

	HLTVClientWrapper *pClient = g_HLTVClientManager.GetClient(client);
	if (!pClient->IsConnected())
	{
		pContext->ReportError("Client %d is not connected.", client);
		return 0;
	}

	pContext->StringToLocalUTF8(params[2], static_cast<size_t>(params[3]), pClient->Ip(), NULL);
	return 0;
}

// native SourceTV_GetSpectatorPassword(client, String:password[], maxlen);
static cell_t Native_GetSpectatorPassword(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	cell_t client = params[1];
	if (client < 1 || client > hltvserver->GetBaseServer()->GetClientCount())
	{
		pContext->ReportError("Invalid spectator client index %d.", client);
		return 0;
	}

	HLTVClientWrapper *pClient = g_HLTVClientManager.GetClient(client);
	if (!pClient->IsConnected())
	{
		pContext->ReportError("Client %d is not connected.", client);
		return 0;
	}

	pContext->StringToLocalUTF8(params[2], static_cast<size_t>(params[3]), pClient->Password(), NULL);
	return 0;
}

// native SourceTV_KickClient(client, const String:sReason[]);
static cell_t Native_KickClient(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	cell_t client = params[1];
	if (client < 1 || client > hltvserver->GetBaseServer()->GetClientCount())
	{
		pContext->ReportError("Invalid spectator client index %d.", client);
		return 0;
	}

	HLTVClientWrapper *pClient = g_HLTVClientManager.GetClient(client);
	if (!pClient->IsConnected())
	{
		pContext->ReportError("Client %d is not connected.", client);
		return 0;
	}

	char *pReason;
	pContext->LocalToString(params[2], &pReason);

	pClient->Kick(pReason);
	return 0;
}

const sp_nativeinfo_t sourcetv_natives[] =
{
	{ "SourceTV_GetServerInstanceCount", Native_GetServerInstanceCount },
	{ "SourceTV_SelectServerInstance", Native_SelectServerInstance },
	{ "SourceTV_GetSelectedServerInstance", Native_GetSelectedServerInstance },
	{ "SourceTV_GetBotIndex", Native_GetBotIndex },
	{ "SourceTV_GetLocalStats", Native_GetLocalStats },
	{ "SourceTV_GetGlobalStats", Native_GetGlobalStats },
	{ "SourceTV_GetBroadcastTick", Native_GetBroadcastTick },
	{ "SourceTV_GetDelay", Native_GetDelay },
	{ "SourceTV_BroadcastScreenMessage", Native_BroadcastScreenMessage },
	{ "SourceTV_BroadcastConsoleMessage", Native_BroadcastConsoleMessage },
	{ "SourceTV_BroadcastChatMessage", Native_BroadcastChatMessage },
	{ "SourceTV_GetViewEntity", Native_GetViewEntity },
	{ "SourceTV_GetViewOrigin", Native_GetViewOrigin },
	{ "SourceTV_ForceFixedCameraShot", Native_ForceFixedCameraShot },
	{ "SourceTV_ForceChaseCameraShot", Native_ForceChaseCameraShot },
	{ "SourceTV_StartRecording", Native_StartRecording },
	{ "SourceTV_StopRecording", Native_StopRecording },
	{ "SourceTV_IsRecording", Native_IsRecording },
	{ "SourceTV_GetDemoFileName", Native_GetDemoFileName },
	{ "SourceTV_GetRecordingTick", Native_GetRecordingTick },
	{ "SourceTV_PrintToDemoConsole", Native_PrintToDemoConsole },
	{ "SourceTV_GetSpectatorCount", Native_GetSpectatorCount },
	{ "SourceTV_GetMaxClients", Native_GetMaxClients },
	{ "SourceTV_GetClientCount", Native_GetClientCount },
	{ "SourceTV_IsClientConnected", Native_IsClientConnected },
	{ "SourceTV_GetSpectatorName", Native_GetSpectatorName },
	{ "SourceTV_GetSpectatorIP", Native_GetSpectatorIP },
	{ "SourceTV_GetSpectatorPassword", Native_GetSpectatorPassword },
	{ "SourceTV_KickClient", Native_KickClient },
	{ NULL, NULL },
};
