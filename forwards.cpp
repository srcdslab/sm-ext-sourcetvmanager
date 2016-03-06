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
#include "hltvserverwrapper.h"

CForwardManager g_pSTVForwards;

SH_DECL_HOOK2_void(IDemoRecorder, StartRecording, SH_NOATTRIB, 0, const char *, bool)
#if SOURCE_ENGINE == SE_CSGO
SH_DECL_HOOK1_void(IDemoRecorder, StopRecording, SH_NOATTRIB, 0, CGameInfo const *)
#else
SH_DECL_HOOK0_void(IDemoRecorder, StopRecording, SH_NOATTRIB, 0)
#endif

#if SOURCE_ENGINE == SE_CSGO
SH_DECL_MANUALHOOK13(CHLTVServer_ConnectClient, 0, 0, 0, IClient *, const netadr_t &, int, int, int, const char *, const char *, const char *, int, CUtlVector<NetMsg_SplitPlayerConnect *> &, bool, CrossPlayPlatform_t, const unsigned char *, int);
SH_DECL_MANUALHOOK1_void_vafmt(CHLTVServer_RejectConnection, 0, 0, 0, const netadr_t &);
SH_DECL_HOOK1_void(IClient, Disconnect, SH_NOATTRIB, 0, const char *);
#else
SH_DECL_MANUALHOOK9(CHLTVServer_ConnectClient, 0, 0, 0, IClient *, netadr_t &, int, int, int, int, const char *, const char *, const char *, int);
SH_DECL_MANUALHOOK3_void(CHLTVServer_RejectConnection, 0, 0, 0, const netadr_t &, int, const char *);
SH_DECL_HOOK0_void_vafmt(IClient, Disconnect, SH_NOATTRIB, 0);
#endif
SH_DECL_MANUALHOOK0_void(CBaseClient_ActivatePlayer, 0, 0, 0);

SH_DECL_MANUALHOOK1(CHLTVServer_GetChallengeType, 0, 0, 0, int, const netadr_t &);

void CForwardManager::Init()
{
	int offset = -1;
	if (!g_pGameConf->GetOffset("CHLTVServer::ConnectClient", &offset) || offset == -1)
	{
		smutils->LogError(myself, "Failed to get CHLTVServer::ConnectClient offset.");
	}
	else
	{
		SH_MANUALHOOK_RECONFIGURE(CHLTVServer_ConnectClient, offset, 0, 0);
		m_bHasClientConnectOffset = true;
	}

	if (!g_pGameConf->GetOffset("CHLTVServer::RejectConnection", &offset) || offset == -1)
	{
		smutils->LogError(myself, "Failed to get CHLTVServer::RejectConnection offset.");
	}
	else
	{
		SH_MANUALHOOK_RECONFIGURE(CHLTVServer_RejectConnection, offset, 0, 0);
		m_bHasRejectConnectionOffset = true;
	}

	if (!g_pGameConf->GetOffset("CHLTVServer::GetChallengeType", &offset) || offset == -1)
	{
		smutils->LogError(myself, "Failed to get CHLTVServer::GetChallengeType offset.");
	}
	else
	{
		SH_MANUALHOOK_RECONFIGURE(CHLTVServer_GetChallengeType, offset, 0, 0);
		m_bHasGetChallengeTypeOffset = true;
	}

	if (!g_pGameConf->GetOffset("CBaseClient::ActivatePlayer", &offset) || offset == -1)
	{
		smutils->LogError(myself, "Failed to get CBaseClient::ActivatePlayer offset.");
	}
	else
	{
		SH_MANUALHOOK_RECONFIGURE(CBaseClient_ActivatePlayer, offset, 0, 0);
		m_bHasActivatePlayerOffset = true;
	}

	m_StartRecordingFwd = forwards->CreateForward("SourceTV_OnStartRecording", ET_Ignore, 2, NULL, Param_Cell, Param_String);
	m_StopRecordingFwd = forwards->CreateForward("SourceTV_OnStopRecording", ET_Ignore, 3, NULL, Param_Cell, Param_String, Param_Cell);
	m_SpectatorPreConnectFwd = forwards->CreateForward("SourceTV_OnSpectatorPreConnect", ET_LowEvent, 4, NULL, Param_String, Param_String, Param_String, Param_String);
	m_SpectatorConnectedFwd = forwards->CreateForward("SourceTV_OnSpectatorConnected", ET_Ignore, 1, NULL, Param_Cell);
	m_SpectatorDisconnectFwd = forwards->CreateForward("SourceTV_OnSpectatorDisconnect", ET_Ignore, 2, NULL, Param_Cell, Param_String);
	m_SpectatorDisconnectedFwd = forwards->CreateForward("SourceTV_OnSpectatorDisconnected", ET_Ignore, 2, NULL, Param_Cell, Param_String);
	m_SpectatorPutInServerFwd = forwards->CreateForward("SourceTV_OnSpectatorPutInServer", ET_Ignore, 1, NULL, Param_Cell);

	m_ServerStartFwd = forwards->CreateForward("SourceTV_OnServerStart", ET_Ignore, 1, NULL, Param_Cell);
	m_ServerShutdownFwd = forwards->CreateForward("SourceTV_OnServerShutdown", ET_Ignore, 1, NULL, Param_Cell);
}

void CForwardManager::Shutdown()
{
	forwards->ReleaseForward(m_StartRecordingFwd);
	forwards->ReleaseForward(m_StopRecordingFwd);
	forwards->ReleaseForward(m_SpectatorPreConnectFwd);
	forwards->ReleaseForward(m_SpectatorConnectedFwd);
	forwards->ReleaseForward(m_SpectatorDisconnectFwd);
	forwards->ReleaseForward(m_SpectatorDisconnectedFwd);
	forwards->ReleaseForward(m_SpectatorPutInServerFwd);

	forwards->ReleaseForward(m_ServerStartFwd);
	forwards->ReleaseForward(m_ServerShutdownFwd);
}

void CForwardManager::HookRecorder(IDemoRecorder *recorder)
{
	SH_ADD_HOOK(IDemoRecorder, StartRecording, recorder, SH_MEMBER(this, &CForwardManager::OnStartRecording_Post), true);
	SH_ADD_HOOK(IDemoRecorder, StopRecording, recorder, SH_MEMBER(this, &CForwardManager::OnStopRecording_Post), true);
}

void CForwardManager::UnhookRecorder(IDemoRecorder *recorder)
{
	SH_REMOVE_HOOK(IDemoRecorder, StartRecording, recorder, SH_MEMBER(this, &CForwardManager::OnStartRecording_Post), true);
	SH_REMOVE_HOOK(IDemoRecorder, StopRecording, recorder, SH_MEMBER(this, &CForwardManager::OnStopRecording_Post), true);
}

void CForwardManager::HookServer(HLTVServerWrapper *wrapper)
{
	IServer *server = wrapper->GetBaseServer();
	if (m_bHasClientConnectOffset)
		SH_ADD_MANUALHOOK(CHLTVServer_ConnectClient, server, SH_MEMBER(this, &CForwardManager::OnSpectatorConnect), false);
	
	if (m_bHasGetChallengeTypeOffset)
		SH_ADD_MANUALHOOK(CHLTVServer_GetChallengeType, server, SH_MEMBER(this, &CForwardManager::OnGetChallengeType), false);

	// Hook all already connected clients as well for late loading
	for (int i = 0; i < server->GetClientCount(); i++)
	{
		IClient *client = server->GetClient(i);
		if (client->IsConnected())
		{
			HookClient(client);
			// Ip and password unknown :(
			// Could add more gamedata to fetch it if people really lateload the extension and expect it to work :B
			wrapper->GetClient(i + 1)->Initialize("", "", client);
		}
	}
}

void CForwardManager::UnhookServer(HLTVServerWrapper *wrapper)
{
	IServer *server = wrapper->GetBaseServer();
	if (m_bHasClientConnectOffset)
		SH_REMOVE_MANUALHOOK(CHLTVServer_ConnectClient, server, SH_MEMBER(this, &CForwardManager::OnSpectatorConnect), false);

	if (m_bHasGetChallengeTypeOffset)
		SH_REMOVE_MANUALHOOK(CHLTVServer_GetChallengeType, server, SH_MEMBER(this, &CForwardManager::OnGetChallengeType), false);

	// Unhook all connected clients as well.
	for (int i = 0; i < server->GetClientCount(); i++)
	{
		IClient *client = server->GetClient(i);
		if (client->IsConnected())
			UnhookClient(client);
	}
}

void CForwardManager::HookClient(IClient *client)
{
	if (m_bHasActivatePlayerOffset)
	{
		void *pGameClient = (void *)((intptr_t)client - 4);
		SH_ADD_MANUALHOOK(CBaseClient_ActivatePlayer, pGameClient, SH_MEMBER(this, &CForwardManager::OnSpectatorPutInServer), true);
	}
	SH_ADD_HOOK(IClient, Disconnect, client, SH_MEMBER(this, &CForwardManager::OnSpectatorDisconnect), false);
}

void CForwardManager::UnhookClient(IClient *client)
{
	if (m_bHasActivatePlayerOffset)
	{
		void *pGameClient = (void *)((intptr_t)client - 4);
		SH_REMOVE_MANUALHOOK(CBaseClient_ActivatePlayer, pGameClient, SH_MEMBER(this, &CForwardManager::OnSpectatorPutInServer), true);
	}
	SH_REMOVE_HOOK(IClient, Disconnect, client, SH_MEMBER(this, &CForwardManager::OnSpectatorDisconnect), false);
}

void CForwardManager::CallOnServerStart(IHLTVServer *server)
{
	m_ServerStartFwd->PushCell(g_HLTVServers.GetInstanceNumber(server));
	m_ServerStartFwd->Execute();
}

void CForwardManager::CallOnServerShutdown(IHLTVServer *server)
{
	m_ServerShutdownFwd->PushCell(g_HLTVServers.GetInstanceNumber(server));
	m_ServerShutdownFwd->Execute();
}

#if SOURCE_ENGINE == SE_CSGO
static bool ExtractPlayerName(CUtlVector<NetMsg_SplitPlayerConnect *> &pSplitPlayerConnectVector, char *name, int maxlen)
{
	for (int i = 0; i < pSplitPlayerConnectVector.Count(); i++)
	{
		NetMsg_SplitPlayerConnect *split = pSplitPlayerConnectVector[i];
		if (!split->has_convars())
			continue;

		const CMsg_CVars cvars = split->convars();
		for (int c = 0; c < cvars.cvars_size(); c++)
		{
			const CMsg_CVars_CVar cvar = cvars.cvars(c);
			if (!cvar.has_name() || !cvar.has_value())
				continue;

			if (!strcmp(cvar.name().c_str(), "name"))
			{
				strncpy(name, cvar.value().c_str(), maxlen);
				return true;
			}
		}
	}
	return false;
}
#endif

// Mimic Connect extension https://forums.alliedmods.net/showthread.php?t=162489
// Thanks asherkin!
char passwordBuffer[255];
#if SOURCE_ENGINE == SE_CSGO
// CHLTVServer::ConnectClient(ns_address const&, int, int, int, char const*, char const*, char const*, int, CUtlVector<CNetMessagePB<16, CCLCMsg_SplitPlayerConnect, 0, true> *, CUtlMemory<CNetMessagePB<16, CCLCMsg_SplitPlayerConnect, 0, true> *, int>> &, bool, CrossPlayPlatform_t, unsigned char const*, int)
IClient *CForwardManager::OnSpectatorConnect(const netadr_t & address, int nProtocol, int iChallenge, int nAuthProtocol, const char *pchName, const char *pchPassword, const char *pCookie, int cbCookie, CUtlVector<NetMsg_SplitPlayerConnect *> &pSplitPlayerConnectVector, bool bUnknown, CrossPlayPlatform_t platform, const unsigned char *pUnknown, int iUnknown)
#else
IClient *CForwardManager::OnSpectatorConnect(netadr_t & address, int nProtocol, int iChallenge, int iClientChallenge, int nAuthProtocol, const char *pchName, const char *pchPassword, const char *pCookie, int cbCookie)
#endif
{
	if (!pCookie || (size_t)cbCookie < sizeof(uint64))
		RETURN_META_VALUE(MRES_IGNORED, nullptr);

#if SOURCE_ENGINE == SE_CSGO
	// CS:GO doesn't send the player name in pchName, but only in the client info convars.
	// Try to extract the name from the protobuf msg.
	char playerName[MAX_PLAYER_NAME_LENGTH];
	if (ExtractPlayerName(pSplitPlayerConnectVector, playerName, sizeof(playerName)))
		pchName = playerName;
#endif

	char ipString[16];
	V_snprintf(ipString, sizeof(ipString), "%u.%u.%u.%u", address.ip[0], address.ip[1], address.ip[2], address.ip[3]);
	V_strncpy(passwordBuffer, pchPassword, 255);

	// SourceTV doesn't validate steamids?!

	char rejectReason[255];

	m_SpectatorPreConnectFwd->PushString(pchName);
	m_SpectatorPreConnectFwd->PushStringEx(passwordBuffer, 255, SM_PARAM_STRING_UTF8 | SM_PARAM_STRING_COPY, SM_PARAM_COPYBACK);
	m_SpectatorPreConnectFwd->PushString(ipString);
	m_SpectatorPreConnectFwd->PushStringEx(rejectReason, 255, SM_PARAM_STRING_UTF8 | SM_PARAM_STRING_COPY, SM_PARAM_COPYBACK);

	cell_t retVal = 1;
	m_SpectatorPreConnectFwd->Execute(&retVal);

	IServer *server = META_IFACEPTR(IServer);
	if (retVal == 0)
	{
		if (m_bHasRejectConnectionOffset)
		{
#if SOURCE_ENGINE == SE_CSGO
			SH_MCALL(server, CHLTVServer_RejectConnection)(address, rejectReason);
#else
			SH_MCALL(server, CHLTVServer_RejectConnection)(address, iClientChallenge, rejectReason);
#endif
		}
		RETURN_META_VALUE(MRES_SUPERCEDE, nullptr);
	}

	// Call the original function.
#if SOURCE_ENGINE == SE_CSGO
	IClient *client = SH_MCALL(server, CHLTVServer_ConnectClient)(address, nProtocol, iChallenge, nAuthProtocol, pchName, passwordBuffer, pCookie, cbCookie, pSplitPlayerConnectVector, bUnknown, platform, pUnknown, iUnknown);
#else
	IClient *client = SH_MCALL(server, CHLTVServer_ConnectClient)(address, nProtocol, iChallenge, iClientChallenge, nAuthProtocol, pchName, passwordBuffer, pCookie, cbCookie);
#endif

	if (!client)
		RETURN_META_VALUE(MRES_SUPERCEDE, nullptr);

	HookClient(client);

	HLTVServerWrapper *wrapper = g_HLTVServers.GetWrapper(server);
	if (wrapper)
	{
		HLTVClientWrapper *clientWrapper = wrapper->GetClient(client->GetPlayerSlot() + 1);
		clientWrapper->Initialize(ipString, pchPassword, client);
	}

	m_SpectatorConnectedFwd->PushCell(client->GetPlayerSlot() + 1);
	m_SpectatorConnectedFwd->Execute();

	// Don't call the hooked function again, just return its value.
	RETURN_META_VALUE(MRES_SUPERCEDE, client);
}

// Force steam authentication
// Thanks GoD-Tony :)
int CForwardManager::OnGetChallengeType(const netadr_t &address)
{
	if (!tv_force_steamauth.GetBool())
		RETURN_META_VALUE(MRES_IGNORED, k_EAuthProtocolHashedCDKey);

	RETURN_META_VALUE(MRES_SUPERCEDE, k_EAuthProtocolSteam);
}

void CForwardManager::OnSpectatorDisconnect(const char *reason)
{
	IClient *client = META_IFACEPTR(IClient);
	if (!client)
		RETURN_META(MRES_IGNORED);

	UnhookClient(client);

	char disconnectReason[255];
	V_strncpy(disconnectReason, reason, 255);
	int clientIndex = client->GetPlayerSlot() + 1;

	m_SpectatorDisconnectFwd->PushCell(clientIndex);
	m_SpectatorDisconnectFwd->PushStringEx(disconnectReason, 255, SM_PARAM_STRING_UTF8 | SM_PARAM_STRING_COPY, SM_PARAM_COPYBACK);
	m_SpectatorDisconnectFwd->Execute();

#if SOURCE_ENGINE == SE_CSGO
	SH_CALL(client, &IClient::Disconnect)(disconnectReason);
#else
	SH_CALL(client, &IClient::Disconnect)("%s", disconnectReason);
#endif

	m_SpectatorDisconnectedFwd->PushCell(clientIndex);
	m_SpectatorDisconnectedFwd->PushString(disconnectReason);
	m_SpectatorDisconnectedFwd->Execute();

	RETURN_META(MRES_SUPERCEDE);
}

void CForwardManager::OnSpectatorPutInServer()
{
	void *pGameClient = META_IFACEPTR(void);
	if (!pGameClient)
		RETURN_META(MRES_IGNORED);

	// The IClient vtable is + 4 from the CBaseClient vtable due to multiple inheritance.
	IClient *client = (IClient *)((intptr_t)pGameClient + 4);

	m_SpectatorPutInServerFwd->PushCell(client->GetPlayerSlot() + 1);
	m_SpectatorPutInServerFwd->Execute();

	RETURN_META(MRES_IGNORED);
}

void CForwardManager::OnStartRecording_Post(const char *filename, bool bContinuously)
{
	if (m_StartRecordingFwd->GetFunctionCount() == 0)
		RETURN_META(MRES_IGNORED);

	IDemoRecorder *recorder = META_IFACEPTR(IDemoRecorder);
	HLTVServerWrapper *wrapper = g_HLTVServers.GetWrapper(recorder);
	int instance = -1;
	if (wrapper)
		instance = wrapper->GetInstanceNumber();

	m_StartRecordingFwd->PushCell(instance);
	m_StartRecordingFwd->PushString(filename);
	m_StartRecordingFwd->Execute();

	RETURN_META(MRES_IGNORED);
}

#if SOURCE_ENGINE == SE_CSGO
void CForwardManager::OnStopRecording_Post(CGameInfo const *info)
#else
void CForwardManager::OnStopRecording_Post()
#endif
{
	if (m_StopRecordingFwd->GetFunctionCount() == 0)
		RETURN_META(MRES_IGNORED);

	IDemoRecorder *recorder = META_IFACEPTR(IDemoRecorder);
	if (!recorder->IsRecording())
		RETURN_META(MRES_IGNORED);

	char *pDemoFile = (char *)recorder->GetDemoFile();
	
	HLTVServerWrapper *wrapper = g_HLTVServers.GetWrapper(recorder);
	int instance = -1;
	if (wrapper)
		instance = wrapper->GetInstanceNumber();

	m_StopRecordingFwd->PushCell(instance);
	m_StopRecordingFwd->PushString(pDemoFile);
	m_StopRecordingFwd->PushCell(recorder->GetRecordingTick());
	m_StopRecordingFwd->Execute();

	RETURN_META(MRES_IGNORED);
}