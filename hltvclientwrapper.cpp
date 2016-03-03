#include "hltvclientwrapper.h"

HLTVClientWrapper::HLTVClientWrapper()
{
	m_Client = nullptr;
}

void HLTVClientWrapper::Initialize(const char *ip, const char *password, IClient *client)
{
	m_Ip = ip;
	m_Password = password;
	m_Client = client;
}

const char *HLTVClientWrapper::Name()
{
	return m_Client->GetClientName();
}

const char *HLTVClientWrapper::Ip()
{
	return m_Ip.chars();
}

const char *HLTVClientWrapper::Password()
{
	return m_Password.chars();
}

bool HLTVClientWrapper::IsConnected()
{
	return m_Client && m_Client->IsConnected();
}

IClient *HLTVClientWrapper::BaseClient()
{
	return m_Client;
}

void HLTVClientWrapper::Kick(const char *reason)
{
	// Go this route due to different IClient::Disconnect signatures in games..
	m_Client->GetServer()->DisconnectClient(m_Client, reason);
}

HLTVClientWrapper *HLTVClientManager::GetClient(int index)
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

// FIXME: ClientManager instance for each hltvserver instance in csgo!
HLTVClientManager g_HLTVClientManager;