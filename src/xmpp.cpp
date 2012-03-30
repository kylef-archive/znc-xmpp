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

CXMPPClient* CXMPPModule::Client(const CXMPPJID& jid, bool bAcceptNegative) const {
	if (!jid.IsLocal(*this)) {
		return NULL;
	}

	CXMPPClient *pCurrent = NULL;

	vector<CXMPPClient*>::const_iterator it;
	for (it = m_vClients.begin(); it != m_vClients.end(); ++it) {
		CXMPPClient *pClient = *it;

		if (pClient->GetUser() && pClient->GetUser()->GetUserName().Equals(jid.GetUser())) {
			if (!jid.GetResource().empty() && jid.GetResource().Equals(pClient->GetResource())) {
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
	CXMPPJID to(Stanza.GetAttribute("to"));

	if (to.IsLocal(*this)) {
		CXMPPClient *pClient = Client(to);
		if (pClient) {
			pClient->Write(Stanza);
			return;
		}
	}

	CXMPPJID from(Stanza.GetAttribute("from"));
	if (from.IsLocal(*this)) {
		CXMPPClient *pClient = Client(from);
		if (pClient) {
			CXMPPStanza errorStanza(Stanza.GetName());
			errorStanza.SetAttribute("to", Stanza.GetAttribute("from"));
			errorStanza.SetAttribute("from", Stanza.GetAttribute("to"));
			errorStanza.SetAttribute("id", Stanza.GetAttribute("id"));
			errorStanza.SetAttribute("type", "error");

			CXMPPStanza& error = errorStanza.NewChild("error");
			error.SetAttribute("type", "cancel");

			if (to.IsLocal(*this)) {
				CXMPPStanza& unavailable = error.NewChild("service-unavailable",
											"urn:ietf:params:xml:ns:xmpp-stanzas");
			} else {
				CXMPPStanza &notFound = error.NewChild("remote-server-not-found",
											"urn:ietf:params:xml:ns:xmpp-stanzas");
			}

			pClient->Write(errorStanza);
		}
	}
}

GLOBALMODULEDEFS(CXMPPModule, "XMPP support for ZNC");

