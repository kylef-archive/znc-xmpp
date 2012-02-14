/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/Modules.h>

#include "Client.h"
#include "Listener.h"

class CXMPPModule : public CModule {
public:
	MODCONSTRUCTOR(CXMPPModule) {};

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		CXMPPListener *pClient = new CXMPPListener(this);
		pClient->Listen(5222, false);

		return true;
	}

	virtual EModRet OnDeleteUser(CUser& User) {
		// Delete clients
		vector<CXMPPClient*>::iterator it;
		for (it = m_vClients.begin(); it != m_vClients.end();) {
			CXMPPClient *pClient = *it;

			if (pClient->GetUser() == &User) {
				CZNC::Get().GetManager().DelSockByAddr(pClient);
				it = m_vClients.erase(it);
			} else {
				++it;
			}
		}

		return CONTINUE;
	}

	void ClientConnected(CXMPPClient &Client) {
		m_vClients.push_back(&Client);
	}

	void ClientDisconnected(CXMPPClient &Client) {
		vector<CXMPPClient*>::iterator it;
		for (it = m_vClients.begin(); it != m_vClients.end(); ++it) {
			CXMPPClient *pClient = *it;

			if (pClient == &Client) {
				m_vClients.erase(it);
				break;
			}
		}
	}

protected:
	vector<CXMPPClient*> m_vClients;
};

GLOBALMODULEDEFS(CXMPPModule, "XMPP support for ZNC");

