/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Client.h"
#include "xmpp.h"

#define SUPPORT_RFC_3921

CXMPPClient::CXMPPClient(CModule *pModule) : CXMPPSocket(pModule) {
	m_pUser = NULL;

	GetModule()->ClientConnected(*this);
}

CXMPPClient::~CXMPPClient() {
	GetModule()->ClientDisconnected(*this);
}

CString CXMPPClient::GetJID() const {
	CString sResult;

	if (m_pUser) {
		sResult = m_pUser->GetUserName() + "@";
	}

	sResult += GetServerName();

	if (!m_sResource.empty()) {
		sResult += "/" + m_sResource;
	}

	return sResult;
}

bool CXMPPClient::Write(CString sData) {
	return CXMPPSocket::Write(sData);
}

bool CXMPPClient::Write(const CXMPPStanza& Stanza) {
	return CXMPPSocket::Write(Stanza);
}

bool CXMPPClient::Write(CXMPPStanza &Stanza, const CXMPPStanza *pStanza) {
	Stanza.SetAttribute("to", GetJID());

	if (pStanza && pStanza->HasAttribute("id")) {
		Stanza.SetAttribute("id", pStanza->GetAttribute("id"));
	}
	return Write(Stanza.ToString());
}

void CXMPPClient::StreamStart(CXMPPStanza &Stanza) {
	Write("<?xml version='1.0' ?>");
	Write("<stream:stream from='" + GetServerName() + "' version='1.0' xml:lang='en' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams'>");

	if (!Stanza.GetAttribute("to").Equals(GetServerName())) {
		CXMPPStanza error("stream:error");
		CXMPPStanza& host = error.NewChild("host-unknown", "urn:ietf:params:xml:ns:xmpp-streams");
		CXMPPStanza& text = host.NewChild("text", "urn:ietf:params:xml:ns:xmpp-streams");
		text.NewChild().SetText("This server does not serve " + Stanza.GetAttribute("to"));
		Write(error);

		Write("</stream:stream>");
		Close(Csock::CLT_AFTERWRITE);
		return;
	}

	CXMPPStanza features("stream:features");

#ifdef HAVE_LIBSSL
	if (!GetSSL() && ((CXMPPModule*)m_pModule)->IsTLSAvailible()) {
		CXMPPStanza &starttls = features.NewChild("starttls", "urn:ietf:params:xml:ns:xmpp-tls");
		starttls.NewChild("required");
	}
#endif

	if (m_pUser) {
		features.NewChild("bind", "urn:ietf:params:xml:ns:xmpp-bind");
	} else if (!((CXMPPModule*)m_pModule)->IsTLSAvailible() || GetSSL()) {
		CXMPPStanza& mechanisms = features.NewChild("mechanisms", "urn:ietf:params:xml:ns:xmpp-sasl");

		CXMPPStanza& plain = mechanisms.NewChild("mechanism");
		plain.NewChild().SetText("PLAIN");
	}

	Write(features);
}

void CXMPPClient::ReceiveStanza(CXMPPStanza &Stanza) {
	if (Stanza.GetName().Equals("auth")) {
		if (Stanza.GetAttribute("mechanism").Equals("plain")) {
			CString sSASL;
			CXMPPStanza *pStanza = Stanza.GetTextChild();

			if (pStanza)
				sSASL = pStanza->GetText().Base64Decode_n();

			const char *sasl = sSASL.c_str();
			unsigned int y = 0;
			for (unsigned int x = 0; x < sSASL.size(); x++) {
				if (sasl[x] == 0) {
					y++;
				}
			}

			CString sUsername = "unknown";

			if (y > 1) {
				const char *username = &sasl[strlen(sasl) + 1];
				const char *password = &username[strlen(username) + 1];
				sUsername = username;

				CUser *pUser = CZNC::Get().FindUser(sUsername);

				if (pUser && pUser->CheckPass(password)) {
					Write(CXMPPStanza("success", "urn:ietf:params:xml:ns:xmpp-sasl"));

					m_pUser = pUser;
					DEBUG("XMPPClient SASL::PLAIN for [" << sUsername << "] success.");

					/* Restart the stream */
					m_bResetParser = true;

					return;
				}
			}

			DEBUG("XMPPClient SASL::PLAIN for [" << sUsername << "] failed.");

			CXMPPStanza failure("failure", "urn:ietf:params:xml:ns:xmpp-sasl");
			failure.NewChild("not-authorized");
			Write(failure);
		} else {
			CXMPPStanza failure("failure", "urn:ietf:params:xml:ns:xmpp-sasl");
			failure.NewChild("invalid-mechanism");
			Write(failure);
		}
	} else if (Stanza.GetName().Equals("starttls")) {
#ifdef HAVE_LIBSSL
		if (!GetSSL() && ((CXMPPModule*)m_pModule)->IsTLSAvailible()) {
			Write(CXMPPStanza("proceed", "urn:ietf:params:xml:ns:xmpp-tls"));

			/* Restart the stream */
			m_bResetParser = true;

			SetPemLocation(CZNC::Get().GetPemLocation());
			StartTLS();

			return;
		}
#endif

		Write(CXMPPStanza("failure", "urn:ietf:params:xml:ns:xmpp-tls"));
		Write("</stream:stream>");
		Close(Csock::CLT_AFTERWRITE);
	}

	if (!m_pUser) {
		return; /* the following stanzas require auth */
	}

	Stanza.SetAttribute("from", GetJID());

	if (Stanza.GetName().Equals("iq")) {
		CXMPPStanza iq("iq");

		if (Stanza.GetAttribute("type").Equals("get")) {
			if (Stanza.GetChildByName("ping")) {
				iq.SetAttribute("type", "result");
			}
		} else if (Stanza.GetAttribute("type").Equals("set")) {
			CXMPPStanza *bindStanza = Stanza.GetChildByName("bind");

			if (bindStanza) {
				bool bResource = false;
				CString sResource;

				CXMPPStanza *pResourceStanza = bindStanza->GetChildByName("resource");

				if (pResourceStanza) {
					CXMPPStanza *pStanza = pResourceStanza->GetTextChild();
					if (pStanza) {
						bResource = true;
						sResource = pStanza->GetText();
					}
				}

				if (!bResource) {
					// Generate a resource
					sResource = CString::RandomString(32).SHA256();
				}

				if (sResource.empty()) {
					/* Invalid resource*/
					iq.SetAttribute("type", "error");
					CXMPPStanza& error = iq.NewChild("error");
					error.SetAttribute("type", "modify");
					error.NewChild("bad-request", "urn:ietf:params:xml:ns:xmpp-stanzas");
					Write(iq, &Stanza);
					return;
				}

				if (((CXMPPModule*)m_pModule)->Client(*m_pUser, sResource)) {
					/* We already have a client with this resource */
					iq.SetAttribute("type", "error");
					CXMPPStanza& error = iq.NewChild("error");
					error.SetAttribute("type", "modify");
					error.NewChild("conflict", "urn:ietf:params:xml:ns:xmpp-stanzas");
					Write(iq, &Stanza);
					return;
				}

				/* The resource is all good, lets use it */
				m_sResource = sResource;

				iq.SetAttribute("type", "result");
				CXMPPStanza& bindStanza = iq.NewChild("bind", "urn:ietf:params:xml:ns:xmpp-bind");
				CXMPPStanza& jidStanza = bindStanza.NewChild("jid");
				jidStanza.NewChild().SetText(GetJID());
#ifdef SUPPORT_RFC_3921
			} else if (Stanza.GetChildByName("session")) {
				iq.SetAttribute("type", "result");
			}
#endif
		}

		if (!iq.HasAttribute("type")) {
			iq.SetAttribute("type", "error");
			iq.NewChild("bad-request");

			DEBUG("XMPPClient unsupported iq type [" + Stanza.GetAttribute("type") + "]");
		}

		Write(iq, &Stanza);
		return;
	} else if (Stanza.GetName().Equals("message")) {
		GetModule()->SendStanza(Stanza);
		return;
	} else if (Stanza.GetName().Equals("presence")) {
		CXMPPStanza presence("presence");

		if (!Stanza.HasAttribute("type")) {
			CXMPPStanza *pPriority = Stanza.GetChildByName("priority");

			if (pPriority) {
				CXMPPStanza *pPriorityText = pPriority->GetTextChild();
				if (pPriorityText) {
					int priority = pPriorityText->GetText().ToInt();

					if ((priority >= -128) && (priority <= 127)) {
						m_uiPriority = priority;
					}
				}

				CXMPPStanza& priority = presence.NewChild("priority");
				priority.NewChild().SetText(CString(GetPriority()));
			}
		} else if (Stanza.GetAttribute("type").Equals("unavailable")) {
			presence.NewChild("unavailable");
		} else if (Stanza.GetAttribute("type").Equals("available")) {
			presence.NewChild("available");
		}

		Write(presence, &Stanza);
		return;
	}

	DEBUG("XMPPClient unsupported stanza [" << Stanza.GetName() << "]");
}

