/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "xmpp.h"
#include "Client.h"
#include "Listener.h"

bool CXMPPModule::OnLoad(const CString& sArgs, CString& sMessage) {
	m_sServerName = sArgs.Token(1);
	if (m_sServerName.empty()) {
		m_sServerName = "localhost";
	}

	CXMPPListener *pClient = new CXMPPListener(this);
	pClient->Listen(5222, false);

	return true;
}

CModule::EModRet CXMPPModule::OnDeleteUser(CUser& User) {
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

void CXMPPModule::ClientConnected(CXMPPClient &Client) {
	m_vClients.push_back(&Client);
}

void CXMPPModule::ClientDisconnected(CXMPPClient &Client) {
	vector<CXMPPClient*>::iterator it;
	for (it = m_vClients.begin(); it != m_vClients.end(); ++it) {
		CXMPPClient *pClient = *it;

		if (pClient == &Client) {
			m_vClients.erase(it);
			break;
		}
	}
}

CXMPPClient* CXMPPModule::Client(CUser& User, CString sResource) const {
	vector<CXMPPClient*>::const_iterator it;
	for (it = m_vClients.begin(); it != m_vClients.end(); ++it) {
		CXMPPClient *pClient = *it;

		if (pClient->GetUser() == &User && sResource.Equals(pClient->GetResource())) {
			return pClient;
		}
	}

	return NULL;
}

GLOBALMODULEDEFS(CXMPPModule, "XMPP support for ZNC");

