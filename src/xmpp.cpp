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
#include "Stanza.h"

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

CXMPPClient* CXMPPModule::Client(const CString& sJID, bool bAcceptNegative) const {
	CString sUser = sJID.Token(0, false, "@");
	CString sDomain = sJID.Token(1, false, "@");
	CString sResource = sDomain.Token(1, false, "/");
	sDomain = sDomain.Token(0, false, "/");

	if (!sDomain.Equals(GetServerName())) {
		return NULL;
	}

	CXMPPClient *pCurrent = NULL;

	vector<CXMPPClient*>::const_iterator it;
	for (it = m_vClients.begin(); it != m_vClients.end(); ++it) {
		CXMPPClient *pClient = *it;

		if (pClient->GetUser() && pClient->GetUser()->GetUserName().Equals(sUser)) {
			if (!sResource.empty() && sResource.Equals(pClient->GetResource())) {
				return pClient;
			}

			if (!pCurrent || (pClient->GetPriority() > pCurrent->GetPriority())) {
				pCurrent = pClient;
			}
		}
	}

	if (!bAcceptNegative && pCurrent && (pCurrent->GetPriority() < 0)) {
		return NULL;
	}

	return pCurrent;
}

bool CXMPPModule::IsTLSAvailible() const {
#ifdef HAVE_LIBSS
	CString sPemFile = CZNC::Get().GetPemLocation();
	if (!sPemFile.empty() && access(sPemFile.c_str(), R_OK) == 0) {
		return true;
	}
#endif

	return false;
}

void CXMPPModule::SendStanza(CXMPPStanza &Stanza) {
	CXMPPClient *pClient = Client(Stanza.GetAttribute("to"));
	if (pClient) {
		pClient->Write(Stanza);
		return;
	}

	pClient = Client(Stanza.GetAttribute("from"));
	if (pClient) {
		CXMPPStanza errorStanza(Stanza.GetName());
		errorStanza.SetAttribute("to", Stanza.GetAttribute("from"));
		errorStanza.SetAttribute("from", Stanza.GetAttribute("to"));
		errorStanza.SetAttribute("type", "error");
		CXMPPStanza& error = errorStanza.NewChild("error");
		error.SetAttribute("type", "cancel");
		CXMPPStanza& unavailable = error.NewChild("service-unavailable", "urn:ietf:params:xml:ns:xmpp-stanzas");
		pClient->Write(errorStanza);
	}
}

GLOBALMODULEDEFS(CXMPPModule, "XMPP support for ZNC");

