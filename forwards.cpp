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

CForwardManager g_pSTVForwards;

SH_DECL_HOOK2_void(IDemoRecorder, StartRecording, SH_NOATTRIB, 0, const char *, bool)
#if SOURCE_ENGINE == SE_CSGO
SH_DECL_HOOK1_void(IDemoRecorder, StopRecording, SH_NOATTRIB, 0, CGameInfo const *)
#else
SH_DECL_HOOK0_void(IDemoRecorder, StopRecording, SH_NOATTRIB, 0)
#endif

void CForwardManager::Init()
{
	m_StartRecordingFwd = forwards->CreateForward("SourceTV_OnStartRecording", ET_Ignore, 3, NULL, Param_Cell, Param_String);
	m_StopRecordingFwd = forwards->CreateForward("SourceTV_OnStopRecording", ET_Ignore, 3, NULL, Param_Cell, Param_String, Param_Cell);
}

void CForwardManager::Shutdown()
{
	forwards->ReleaseForward(m_StartRecordingFwd);
	forwards->ReleaseForward(m_StopRecordingFwd);
}

void CForwardManager::HookRecorder(IDemoRecorder *recorder)
{
	SH_ADD_HOOK(IDemoRecorder, StartRecording, recorder, SH_MEMBER(this, &CForwardManager::OnStartRecording_Post), false);
	SH_ADD_HOOK(IDemoRecorder, StopRecording, recorder, SH_MEMBER(this, &CForwardManager::OnStopRecording_Post), false);
}

void CForwardManager::UnhookRecorder(IDemoRecorder *recorder)
{
	SH_REMOVE_HOOK(IDemoRecorder, StartRecording, recorder, SH_MEMBER(this, &CForwardManager::OnStartRecording_Post), false);
	SH_REMOVE_HOOK(IDemoRecorder, StopRecording, recorder, SH_MEMBER(this, &CForwardManager::OnStopRecording_Post), false);
}

void CForwardManager::OnStartRecording_Post(const char *filename, bool bContinuously)
{
	if (m_StartRecordingFwd->GetFunctionCount() == 0)
		RETURN_META(MRES_IGNORED);

	m_StartRecordingFwd->PushCell(0); // TODO: Get current hltvserver index
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
	
	m_StopRecordingFwd->PushCell(0); // TODO: Get current hltvserver index
	m_StopRecordingFwd->PushString(pDemoFile);
	m_StopRecordingFwd->PushCell(recorder->GetRecordingTick());
	m_StopRecordingFwd->Execute();

	RETURN_META(MRES_IGNORED);
}