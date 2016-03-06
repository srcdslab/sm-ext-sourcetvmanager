#include "hltvserverwrapper.h"
#include "forwards.h"

void *old_host_client = nullptr;
bool g_HostClientOverridden = false;

SH_DECL_HOOK1(IClient, ExecuteStringCommand, SH_NOATTRIB, 0, bool, const char *);
SH_DECL_MANUALHOOK0_void(CHLTVServer_Shutdown, 0, 0, 0);

#if SOURCE_ENGINE != SE_CSGO

// Stuff to print to demo console
SH_DECL_HOOK0_void_vafmt(IClient, ClientPrintf, SH_NOATTRIB, 0);
// This should be large enough.
#define FAKE_VTBL_LENGTH 70
static void *FakeNetChanVtbl[FAKE_VTBL_LENGTH];
static void *FakeNetChan = &FakeNetChanVtbl;
SH_DECL_MANUALHOOK3(NetChan_SendNetMsg, 0, 0, 0, bool, INetMessage &, bool, bool);
#endif // SOURCE_ENGINE != SE_CSGO

HLTVServerWrapper::HLTVServerWrapper(IHLTVServer *hltvserver)
{
	m_HLTVServer = hltvserver;
	m_DemoRecorder = g_HLTVServers.GetDemoRecorderPtr(hltvserver);
	m_Connected = true;

	Hook();

	// Inform the plugins
	g_pSTVForwards.CallOnServerStart(hltvserver);
}

void HLTVServerWrapper::Shutdown(bool bInformPlugins)
{
	if (!m_Connected)
		return;

	if (bInformPlugins)
		g_pSTVForwards.CallOnServerShutdown(m_HLTVServer);

	Unhook();

	m_HLTVServer = nullptr;
	m_DemoRecorder = nullptr;
	m_Connected = false;
}

IServer *HLTVServerWrapper::GetBaseServer()
{
	return m_HLTVServer->GetBaseServer();
}

IHLTVServer *HLTVServerWrapper::GetHLTVServer()
{
	return m_HLTVServer;
}

IDemoRecorder *HLTVServerWrapper::GetDemoRecorder()
{
	return m_DemoRecorder;
}

int HLTVServerWrapper::GetInstanceNumber()
{
	return g_HLTVServers.GetInstanceNumber(m_HLTVServer);
}

HLTVClientWrapper *HLTVServerWrapper::GetClient(int index)
{
	// Grow the vector with null pointers
	// There might have been clients with lower indexes before we were loaded.
	if (m_Clients.length() < (size_t)index)
	{
		int start = m_Clients.length();
		m_Clients.resize(index);
		for (int i = start; i < index; i++)
		{
			m_Clients[i] = nullptr;
		}
	}

	if (!m_Clients[index - 1])
	{
		m_Clients[index - 1] = new HLTVClientWrapper();
	}

	return m_Clients[index - 1];
}

void HLTVServerWrapper::Hook()
{
	if (!m_Connected)
		return;

	g_pSTVForwards.HookServer(this);
	g_pSTVForwards.HookRecorder(m_DemoRecorder);

	if (g_HLTVServers.HasShutdownOffset())
		SH_ADD_MANUALHOOK(CHLTVServer_Shutdown, m_HLTVServer->GetBaseServer(), SH_MEMBER(this, &HLTVServerWrapper::OnHLTVServerShutdown), false);

	if (iserver)
	{
		IClient *pClient = iserver->GetClient(m_HLTVServer->GetHLTVSlot());
		if (pClient)
		{
			SH_ADD_HOOK(IClient, ExecuteStringCommand, pClient, SH_MEMBER(this, &HLTVServerWrapper::OnHLTVBotExecuteStringCommand), false);
			SH_ADD_HOOK(IClient, ExecuteStringCommand, pClient, SH_MEMBER(this, &HLTVServerWrapper::OnHLTVBotExecuteStringCommand_Post), true);
#if SOURCE_ENGINE != SE_CSGO
			SH_ADD_HOOK(IClient, ClientPrintf, pClient, SH_MEMBER(this, &HLTVServerWrapper::OnHLTVBotClientPrintf_Post), false);
#endif
		}
	}
}

void HLTVServerWrapper::Unhook()
{
	if (!m_Connected)
		return;

	g_pSTVForwards.UnhookServer(this);
	g_pSTVForwards.UnhookRecorder(m_DemoRecorder);

	if (g_HLTVServers.HasShutdownOffset())
		SH_REMOVE_MANUALHOOK(CHLTVServer_Shutdown, m_HLTVServer->GetBaseServer(), SH_MEMBER(this, &HLTVServerWrapper::OnHLTVServerShutdown), false);

	if (iserver)
	{
		IClient *pClient = iserver->GetClient(m_HLTVServer->GetHLTVSlot());
		if (pClient)
		{
			SH_REMOVE_HOOK(IClient, ExecuteStringCommand, pClient, SH_MEMBER(this, &HLTVServerWrapper::OnHLTVBotExecuteStringCommand), false);
			SH_REMOVE_HOOK(IClient, ExecuteStringCommand, pClient, SH_MEMBER(this, &HLTVServerWrapper::OnHLTVBotExecuteStringCommand_Post), true);
#if SOURCE_ENGINE != SE_CSGO
			SH_REMOVE_HOOK(IClient, ClientPrintf, pClient, SH_MEMBER(this, &HLTVServerWrapper::OnHLTVBotClientPrintf_Post), false);
#endif
		}
	}
}

// CHLTVServer::Shutdown deregisters the hltvserver from the hltvdirector, 
// so RemoveHLTVServer/SetHLTVServer(NULL) is called too on the master proxy.
void HLTVServerWrapper::OnHLTVServerShutdown()
{
	if (!m_Connected)
		RETURN_META(MRES_IGNORED);

	Shutdown(true);

	RETURN_META(MRES_IGNORED);
}

// When bots issue a command that would print stuff to their console, 
// the server might crash, because ExecuteStringCommand doesn't set the 
// global host_client pointer to the client on whom the command is run.
// Host_Client_Printf blatantly tries to call host_client->ClientPrintf
// while the pointer might point to some other player or garbage.
// This leads to e.g. the output of the "status" command not being 
// recorded in the SourceTV demo.
// The approach here is to set host_client correctly for the SourceTV
// bot and reset it to the old value after command execution.
bool HLTVServerWrapper::OnHLTVBotExecuteStringCommand(const char *s)
{
	if (!host_client)
		RETURN_META_VALUE(MRES_IGNORED, 0);

	IClient *pClient = META_IFACEPTR(IClient);
	if (!pClient)
		RETURN_META_VALUE(MRES_IGNORED, 0);

	// The IClient vtable is +4 from the CBaseClient vtable due to multiple inheritance.
	void *pGameClient = (void *)((intptr_t)pClient - 4);

	old_host_client = *(void **)host_client;
	*(void **)host_client = pGameClient;
	g_HostClientOverridden = true;

	RETURN_META_VALUE(MRES_IGNORED, 0);
}

bool HLTVServerWrapper::OnHLTVBotExecuteStringCommand_Post(const char *s)
{
	if (!host_client || !g_HostClientOverridden)
		RETURN_META_VALUE(MRES_IGNORED, 0);

	*(void **)host_client = old_host_client;
	g_HostClientOverridden = false;
	RETURN_META_VALUE(MRES_IGNORED, 0);
}

#if SOURCE_ENGINE != SE_CSGO
void HLTVServerWrapper::OnHLTVBotClientPrintf_Post(const char* buf)
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
#endif

/**
 * Manage the wrappers!
 */
void HLTVServerWrapperManager::InitHooks()
{
	int offset;
	if (g_pGameConf->GetOffset("CHLTVServer::Shutdown", &offset))
	{
		SH_MANUALHOOK_RECONFIGURE(CHLTVServer_Shutdown, offset, 0, 0);
		m_bHasShutdownOffset = true;
	}
	else
	{
		smutils->LogError(myself, "Failed to find CHLTVServer::Shutdown offset.");
	}

#if SOURCE_ENGINE != SE_CSGO
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
			SH_ADD_MANUALHOOK(NetChan_SendNetMsg, &FakeNetChan, SH_MEMBER(this, &HLTVServerWrapperManager::OnHLTVBotNetChanSendNetMsg), false);
			m_bSendNetMsgHooked = true;
		}
	}
	else
	{
		smutils->LogError(myself, "Failed to find CNetChan::SendNetMsg offset. Can't print to demo console.");
	}
#endif
}

void HLTVServerWrapperManager::ShutdownHooks()
{
#if SOURCE_ENGINE != SE_CSGO
	if (m_bSendNetMsgHooked)
	{
		SH_REMOVE_MANUALHOOK(NetChan_SendNetMsg, &FakeNetChan, SH_MEMBER(this, &HLTVServerWrapperManager::OnHLTVBotNetChanSendNetMsg), false);
		m_bSendNetMsgHooked = false;
	}
#endif
}

void HLTVServerWrapperManager::AddServer(IHLTVServer *hltvserver)
{
	HLTVServerWrapper *wrapper = new HLTVServerWrapper(hltvserver);
	m_HLTVServers.append(wrapper);
}

void HLTVServerWrapperManager::RemoveServer(IHLTVServer *hltvserver, bool bInformPlugins)
{
	for (unsigned int i = 0; i < m_HLTVServers.length(); i++)
	{
		HLTVServerWrapper *wrapper = m_HLTVServers[i];
		if (wrapper->GetHLTVServer() != hltvserver)
			continue;

		wrapper->Shutdown(bInformPlugins);
		m_HLTVServers.remove(i);
		break;
	}
}

HLTVServerWrapper *HLTVServerWrapperManager::GetWrapper(IHLTVServer *hltvserver)
{
	for (unsigned int i = 0; i < m_HLTVServers.length(); i++)
	{
		HLTVServerWrapper *wrapper = m_HLTVServers[i];
		if (wrapper->GetHLTVServer() == hltvserver)
			return wrapper;
	}
	return nullptr;
}

HLTVServerWrapper *HLTVServerWrapperManager::GetWrapper(IServer *server)
{
	for (unsigned int i = 0; i < m_HLTVServers.length(); i++)
	{
		HLTVServerWrapper *wrapper = m_HLTVServers[i];
		if (wrapper->GetBaseServer() == server)
			return wrapper;
	}
	return nullptr;
}

HLTVServerWrapper *HLTVServerWrapperManager::GetWrapper(IDemoRecorder *demorecorder)
{
	for (unsigned int i = 0; i < m_HLTVServers.length(); i++)
	{
		HLTVServerWrapper *wrapper = m_HLTVServers[i];
		if (wrapper->GetDemoRecorder() == demorecorder)
			return wrapper;
	}
	return nullptr;
}

int HLTVServerWrapperManager::GetInstanceNumber(IHLTVServer *hltvserver)
{
#if SOURCE_ENGINE == SE_CSGO
	for (int i = 0; i < hltvdirector->GetHLTVServerCount(); i++)
	{
		if (hltvserver == hltvdirector->GetHLTVServer(i))
			return i;
	}

	// We should have found it in the above loop :S
	smutils->LogError(myself, "Failed to find IHLTVServer instance in director.");
	return -1;
#else
	return 0;
#endif
}

IDemoRecorder *HLTVServerWrapperManager::GetDemoRecorderPtr(IHLTVServer *hltv)
{
	static int offset = -1;
	if (offset == -1)
	{
		void *addr;
		if (!g_pGameConf->GetAddress("CHLTVServer::m_DemoRecorder", &addr))
		{
			smutils->LogError(myself, "Failed to get CHLTVServer::m_DemoRecorder offset.");
			return nullptr;
		}

		*(int **)&offset = (int *)addr;
	}

	if (hltv)
	{
#if SOURCE_ENGINE == SE_CSGO
		return (IDemoRecorder *)((intptr_t)hltv + offset);
#else
		IServer *baseServer = hltv->GetBaseServer();
		return (IDemoRecorder *)((intptr_t)baseServer + offset);
#endif
	}
	else
	{
		return nullptr;
	}
}

bool HLTVServerWrapperManager::HasShutdownOffset()
{
	return m_bHasShutdownOffset;
}

#if SOURCE_ENGINE != SE_CSGO
bool HLTVServerWrapperManager::OnHLTVBotNetChanSendNetMsg(INetMessage &msg, bool bForceReliable, bool bVoice)
{
	// No need to worry about the right selected hltvserver, because there can only be one.
	IClient *pClient = iserver->GetClient(hltvserver->GetHLTVServer()->GetHLTVSlot());
	if (!pClient)
		RETURN_META_VALUE(MRES_SUPERCEDE, false);

	// Let the message flow through the intended path like CGameClient::SendNetMsg wants to.
	bool bRetSent = pClient->SendNetMsg(msg, bForceReliable);

	// It's important to supercede, because there is no original function to call.
	// (the "vtable" was empty before hooking it)
	// See FakeNetChan variable at the top.
	RETURN_META_VALUE(MRES_SUPERCEDE, bRetSent);
}
#endif

HLTVServerWrapperManager g_HLTVServers;