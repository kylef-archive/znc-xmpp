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

/* libxml2 handlers */

static void _start_element(void *userdata, const xmlChar *name, const xmlChar **attrs) {
	CXMPPClient *pClient = (CXMPPClient*)userdata;

	if (pClient->GetDepth() == 0) {
		if (strcmp((char*)name, "stream:stream") != 0) {
			DEBUG("XMPPClient: Client did not open valid stream. [" << name << "]");
			pClient->Close(Csock::CLT_AFTERWRITE);
			pClient->IncrementDepth(); /* Incrmement because otherwise end_element will be confused */
			return;
		}

		CXMPPStanza Stanza;
		Stanza.SetName("stream:stream");
		if (attrs) {
			Stanza.SetAttributes(attrs);
		}

		pClient->StreamStart(Stanza);
	} else if (!pClient->GetStanza()) {
		/* New top level stanza */
		CXMPPStanza *pStanza = new CXMPPStanza((char *)name);

		pClient->SetStanza(pStanza);

		if (attrs) {
			pStanza->SetAttributes(attrs);
		}
	} else {
		/* New child stanza */
		CXMPPStanza& child = pClient->GetStanza()->NewChild((char *)name);
		pClient->SetStanza(&child);

		if (attrs) {
			child.SetAttributes(attrs);
		}
	}

	DEBUG("libxml (" << pClient->GetDepth() << ") start_element: [" << name << "]");

	pClient->IncrementDepth();
}

static void _end_element(void *userdata, const xmlChar *name) {
	CXMPPClient *pClient = (CXMPPClient*)userdata;

	pClient->DeincrementDepth();

	if (pClient->GetDepth() == 0) {
		pClient->StreamEnd();
		DEBUG("XMPPClient: RECV </stream:stream>");
		return;
	} else if (pClient->GetStanza()->GetParent()) {
		/* We are finishing a child stanza, so set current to parent */
		pClient->SetStanza(pClient->GetStanza()->GetParent());
	} else {
		CXMPPStanza *pStanza = pClient->GetStanza();
		pClient->SetStanza(NULL);

		pClient->ReceiveStanza(*pStanza);
		delete pStanza;
	}

	DEBUG("libxml (" << pClient->GetDepth() << ") end_element: [" << name << "]");
}

static void _characters(void *userdata, const xmlChar *chr, int len) {
	CXMPPClient *pClient = (CXMPPClient*)userdata;

	if (pClient->GetStanza()) {
		CXMPPStanza &Stanza = pClient->GetStanza()->NewChild();
		Stanza.SetText(CString((char*)chr, len));
	}
}

CXMPPClient::CXMPPClient(CModule *pModule) : CSocket(pModule) {
	m_uiDepth = 0;
	m_pStanza = NULL;

	m_pUser = NULL;
	m_sResource = "";

	DisableReadLine();

	m_xmlContext = NULL;
	m_bResetParser = true;

	memset(&m_xmlHandlers, 0, sizeof(xmlSAXHandler));
	m_xmlHandlers.startElement = _start_element;
	m_xmlHandlers.endElement = _end_element;
	m_xmlHandlers.characters = _characters;

	if (m_pModule) {
		((CXMPPModule*)m_pModule)->ClientConnected(*this);
	}
}

CXMPPClient::~CXMPPClient() {
	xmlFreeParserCtxt(m_xmlContext);

	/* We might have a leftover stanza */
	if (m_pStanza) {
		while (m_pStanza->GetParent()) {
			m_pStanza = m_pStanza->GetParent();
		}

		delete m_pStanza;
	}

	if (m_pModule) {
		((CXMPPModule*)m_pModule)->ClientDisconnected(*this);
	}
}

CString CXMPPClient::GetServerName() const {
	return ((CXMPPModule*)m_pModule)->GetServerName();
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

void CXMPPClient::ReadData(const char *data, size_t len) {
	if (m_bResetParser) {
		m_uiDepth = 0;

		if (m_xmlContext) {
			xmlFreeParserCtxt(m_xmlContext);
		}

		m_xmlContext = xmlCreatePushParserCtxt(&m_xmlHandlers, this, NULL, 0, NULL);

		m_bResetParser = false;
	}

	xmlParseChunk(m_xmlContext, data, len, 0);
}

bool CXMPPClient::Write(CXMPPStanza &Stanza, const CXMPPStanza *pStanza) {
	Stanza.SetAttribute("to", GetJID());

	if (pStanza && pStanza->HasAttribute("id")) {
		Stanza.SetAttribute("id", pStanza->GetAttribute("id"));
	}
	return Write(Stanza.ToString());
}

bool CXMPPClient::Write(const CXMPPStanza &Stanza) {
	return Write(Stanza.ToString());
}

bool CXMPPClient::Write(const CString &sString) {
	return CSocket::Write(sString);
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
		features.NewChild("starttls", "urn:ietf:params:xml:ns:xmpp-tls");
	}
#endif

	if (m_pUser) {
		features.NewChild("bind", "urn:ietf:params:xml:ns:xmpp-bind");
	} else if (GetSSL() || true) { /* Remove true to force TLS before auth */
		CXMPPStanza& mechanisms = features.NewChild("mechanisms", "urn:ietf:params:xml:ns:xmpp-sasl");

		CXMPPStanza& plain = mechanisms.NewChild("mechanism");
		plain.NewChild().SetText("PLAIN");
	}

	Write(features);
}

void CXMPPClient::StreamEnd() {
	Close(Csock::CLT_AFTERWRITE);
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

	if (Stanza.GetName().Equals("iq")) {
		CXMPPStanza iq("iq");

		if (Stanza.GetAttribute("type").Equals("set")) {
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
		CXMPPStanza message("message");
		message.SetAttribute("type", "error");

		if (Stanza.HasAttribute("to")) {
			message.SetAttribute("from", Stanza.GetAttribute("to"));
		}

		CXMPPStanza& error = message.NewChild("error");
		error.SetAttribute("type", "cancel");
		CXMPPStanza& unavailable = error.NewChild("service-unavailable", "urn:ietf:params:xml:ns:xmpp-stanzas");

		Write(message, &Stanza);
		return;
	}

	DEBUG("XMPPClient unsupported stanza [" << Stanza.GetName() << "]");
}

