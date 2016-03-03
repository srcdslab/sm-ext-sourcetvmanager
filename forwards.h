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

#ifndef _INCLUDE_SOURCEMOD_EXTENSION_FORWARDS_H_
#define _INCLUDE_SOURCEMOD_EXTENSION_FORWARDS_H_

#include "extension.h"
#include "netadr.h"

#if SOURCE_ENGINE == SE_CSGO
#include "netmessages.pb.h"

template <int Type, class NetMessage, int Group, bool reliable>
class CNetMessagePB : public INetMessage, public NetMessage {
public:
	~CNetMessagePB() {}

};

typedef CNetMessagePB<16, CCLCMsg_SplitPlayerConnect, 0, true>	NetMsg_SplitPlayerConnect;

#endif

typedef enum EAuthProtocol
{
	k_EAuthProtocolWONCertificate = 1,
	k_EAuthProtocolHashedCDKey = 2,
	k_EAuthProtocolSteam = 3
} EAuthProtocol;

class CGameInfo;

class CForwardManager
{
public:
	void Init();
	void Shutdown();

	void HookRecorder(IDemoRecorder *recorder);
	void UnhookRecorder(IDemoRecorder *recorder);

	void HookServer(IServer *server);
	void UnhookServer(IServer *server);

private:
	void HookClient(IClient *client);
	void UnhookClient(IClient *client);

private:
	void OnStartRecording_Post(const char *filename, bool bContinuously);
#if SOURCE_ENGINE == SE_CSGO
	void OnStopRecording_Post(CGameInfo const *info);
	IClient *OnSpectatorConnect(const netadr_t & address, int nProtocol, int iChallenge, int nAuthProtocol, const char *pchName, const char *pchPassword, const char *pCookie, int cbCookie, CUtlVector<NetMsg_SplitPlayerConnect *> &pSplitPlayerConnectVector, bool bUnknown, CrossPlayPlatform_t platform, const unsigned char *pUnknown, int iUnknown);
#else
	void OnStopRecording_Post();
	IClient *OnSpectatorConnect(netadr_t &address, int nProtocol, int iChallenge, int iClientChallenge, int nAuthProtocol, const char *pchName, const char *pchPassword, const char *pCookie, int cbCookie);
#endif
	void OnSpectatorDisconnect(const char *reason);
	void OnSpectatorPutInServer();

	int OnGetChallengeType(const netadr_t &address);

private:
	IForward *m_StartRecordingFwd;
	IForward *m_StopRecordingFwd;
	IForward *m_SpectatorPreConnectFwd;
	IForward *m_SpectatorConnectedFwd;
	IForward *m_SpectatorDisconnectFwd;
	IForward *m_SpectatorDisconnectedFwd;
	IForward *m_SpectatorPutInServerFwd;

	bool m_bHasClientConnectOffset = false;
	bool m_bHasRejectConnectionOffset = false;
	bool m_bHasGetChallengeTypeOffset = false;
	bool m_bHasActivatePlayerOffset = false;
};

extern CForwardManager g_pSTVForwards;

#endif // _INCLUDE_SOURCEMOD_EXTENSION_FORWARDS_H_