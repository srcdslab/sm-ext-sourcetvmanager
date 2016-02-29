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

extern const sp_nativeinfo_t sourcetv_natives[];

// native SourceTV_GetHLTVServerCount();
static cell_t Native_GetHLTVServerCount(IPluginContext *pContext, const cell_t *params)
{
	return hltvdirector->GetHLTVServerCount();
}

// native SourceTV_SelectHLTVServer();
static cell_t Native_SelectHLTVServer(IPluginContext *pContext, const cell_t *params)
{
	if (params[1] < 0 || params[1] >= hltvdirector->GetHLTVServerCount())
	{
		pContext->ReportError("Invalid HLTV server instance number (%d).", params[1]);
		return 0;
	}
	g_STVManager.SelectSourceTVServer(hltvdirector->GetHLTVServer(params[1]));

	return 0;
}

// native SourceTV_GetSelectedHLTVServer();
static cell_t Native_GetSelectedHLTVServer(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return -1;

	for (int i = 0; i < hltvdirector->GetHLTVServerCount(); i++)
	{
		if (hltvserver == hltvdirector->GetHLTVServer(i))
			return i;
	}

	// We should have found it in the above loop :S
	hltvserver = nullptr;
	return -1;
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

// native bool:SourceTV_BroadcastHintMessage(const String:format[], any:...);
static cell_t Native_BroadcastHintMessage(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	char buffer[1024];
	size_t len;
	{
		DetectExceptions eh(pContext);
		len = smutils->FormatString(buffer, sizeof(buffer), pContext, params, 1);
		if (eh.HasException())
			return 0;
	}

	IGameEvent *msg = gameevents->CreateEvent("hltv_message", true);
	if (!msg)
		return 0;

	msg->SetString("text", buffer);
	hltvserver->BroadcastEvent(msg);
	gameevents->FreeEvent(msg);

	return 1;
}

// native bool:SourceTV_BroadcastConsoleMessage(const String:format[], any:...);
static cell_t Native_BroadcastConsoleMessage(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	static ICallWrapper *pBroadcastPrintf = nullptr;

	if (!pBroadcastPrintf)
	{
		int offset = -1;
		if (!g_pGameConf->GetOffset("BroadcastPrintf", &offset) || offset == -1)
		{
			pContext->ReportError("Failed to get CBaseServer::BroadcastPrintf offset.");
			return 0;
		}

		PassInfo pass[3];
		pass[0].flags = PASSFLAG_BYVAL;
		pass[0].type = PassType_Basic;
		pass[0].size = sizeof(void *);
		pass[1].flags = PASSFLAG_BYVAL;
		pass[1].type = PassType_Basic;
		pass[1].size = sizeof(char *);
		pass[2].flags = PASSFLAG_BYVAL;
		pass[2].type = PassType_Basic;
		pass[2].size = sizeof(char *);

		void *iserver = (void *)hltvserver->GetBaseServer();
		void **vtable = *(void ***)iserver;
		void *func = vtable[offset];

		pBroadcastPrintf = bintools->CreateCall(func, CallConv_Cdecl, NULL, pass, 3);
	}

	char buffer[1024];
	size_t len;
	{
		DetectExceptions eh(pContext);
		len = smutils->FormatString(buffer, sizeof(buffer) - 2, pContext, params, 1);
		if (eh.HasException())
			return 0;
	}

	buffer[len++] = '\n';
	buffer[len] = '\0';

	if (pBroadcastPrintf)
	{
		unsigned char vstk[sizeof(void *) + sizeof(char *) * 2];
		unsigned char *vptr = vstk;

		*(void **)vptr = (void *)hltvserver->GetBaseServer();
		vptr += sizeof(void *);
		*(char **)vptr = "%s";
		vptr += sizeof(char *);
		*(char **)vptr = buffer;

		pBroadcastPrintf->Execute(vstk, NULL);
	}

	return 1;
}

// native SourceTV_GetViewEntity();
static cell_t Native_GetViewEntity(IPluginContext *pContext, const cell_t *params)
{
	return hltvdirector->GetPVSEntity();
}

// native SourceTV_GetViewCoordinates();
static cell_t Native_GetViewCoordinates(IPluginContext *pContext, const cell_t *params)
{
	Vector pvs = hltvdirector->GetPVSOrigin();

	cell_t *addr;
	pContext->LocalToPhysAddr(params[1], &addr);
	addr[0] = sp_ftoc(pvs.x);
	addr[1] = sp_ftoc(pvs.y);
	addr[2] = sp_ftoc(pvs.z);
	return 0;
}

// native bool:SourceTV_IsRecording();
static cell_t Native_IsRecording(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	return hltvserver->IsRecording();
}

// Checks in COM_IsValidPath in the engine
static bool IsValidPath(const char *path)
{
	return strlen(path) > 0 && !strstr(path, "\\\\") && !strstr(path, ":") && !strstr(path, "..") && !strstr(path, "\n") && !strstr(path, "\r");
}

// native bool:SourceTV_StartRecording(const String:sFilename[]);
static cell_t Native_StartRecording(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	// SourceTV is not active.
	if (!hltvserver->GetBaseServer()->IsActive())
		return 0;

	// Only SourceTV Master can record demos instantly
	if (!hltvserver->IsMasterProxy())
		return 0;

	// already recording
	if (hltvserver->IsRecording())
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

	demorecorder->StartRecording(pPath, false);

	return 1;
}

// native bool:SourceTV_StopRecording();
static cell_t Native_StopRecording(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	if (!hltvserver->IsRecording())
		return 0;

	hltvserver->StopRecording(NULL);

	// TODO: Stop recording on all other active hltvservers (tv_stoprecord in csgo does this)

	return 1;
}

// native bool:SourceTV_GetDemoFileName(String:sFilename[], maxlen);
static cell_t Native_GetDemoFileName(IPluginContext *pContext, const cell_t *params)
{
	if (hltvserver == nullptr)
		return 0;

	if (!hltvserver->IsRecording())
		return 0;

	char *pDemoFile = hltvserver->GetRecordingDemoFilename();
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

	pClient->ClientPrintf("%s", buffer);

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

	IClient *pClient = hltvserver->GetBaseServer()->GetClient(client - 1);
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

	IClient *pClient = hltvserver->GetBaseServer()->GetClient(client - 1);
	if (!pClient || !pClient->IsConnected())
	{
		pContext->ReportError("Client %d is not connected.", client);
		return 0;
	}
	
	pContext->StringToLocalUTF8(params[2], static_cast<size_t>(params[3]), pClient->GetClientName(), NULL);
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

	IClient *pClient = hltvserver->GetBaseServer()->GetClient(client - 1);
	if (!pClient || !pClient->IsConnected())
	{
		pContext->ReportError("Client %d is not connected.", client);
		return 0;
	}

	char *pReason;
	pContext->LocalToString(params[2], &pReason);

	hltvserver->GetBaseServer()->DisconnectClient(pClient, pReason);
	return 0;
}

const sp_nativeinfo_t sourcetv_natives[] =
{
	{ "SourceTV_GetHLTVServerCount", Native_GetHLTVServerCount },
	{ "SourceTV_SelectHLTVServer", Native_SelectHLTVServer },
	{ "SourceTV_GetSelectedHLTVServer", Native_GetSelectedHLTVServer },
	{ "SourceTV_GetBotIndex", Native_GetBotIndex },
	{ "SourceTV_GetLocalStats", Native_GetLocalStats },
	{ "SourceTV_GetBroadcastTick", Native_GetBroadcastTick },
	{ "SourceTV_GetDelay", Native_GetDelay },
	{ "SourceTV_BroadcastHintMessage", Native_BroadcastHintMessage },
	{ "SourceTV_BroadcastConsoleMessage", Native_BroadcastConsoleMessage },
	{ "SourceTV_GetViewEntity", Native_GetViewEntity },
	{ "SourceTV_GetViewCoordinates", Native_GetViewCoordinates },
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
	{ "SourceTV_KickClient", Native_KickClient },
	{ NULL, NULL },
};
