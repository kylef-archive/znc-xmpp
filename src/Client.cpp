/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Client.h"

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
		Stanza.SetAttributes(attrs);

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

		child.SetAttributes(attrs);
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

	DisableReadLine();

	memset(&m_xmlHandlers, 0, sizeof(xmlSAXHandler));
	m_xmlHandlers.startElement = _start_element;
	m_xmlHandlers.endElement = _end_element;
	m_xmlHandlers.characters = _characters;

	m_xmlContext = xmlCreatePushParserCtxt(&m_xmlHandlers, this, NULL, 0, NULL);
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
}

void CXMPPClient::ReadData(const char *data, size_t len) {
	xmlParseChunk(m_xmlContext, data, len, 0);
}

void CXMPPClient::ReadLine(const CString& sData) {
	xmlParseChunk(m_xmlContext, sData.c_str(), sData.size(), 0);
}

bool CXMPPClient::Write(const CXMPPStanza &Stanza) {
	return Write(Stanza.ToString());
}

bool CXMPPClient::Write(const CString &sString) {
	return CSocket::Write(sString);
}

void CXMPPClient::StreamStart(CXMPPStanza &Stanza) {
	Write("<?xml version='1.0' ?>");
	CString sJid = "bla";
	Write("<stream:stream from='xmpp.znc.in' to='" + sJid + "' version='1.0' xml:lang='en' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams'>");

	CXMPPStanza features("stream:features");

	if (m_pUser) {
		//features.NewChild("bind", "urn:ietf:params:xml:ns:xmpp-bind");
	} else {
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
			CString sSASL = Stanza.GetText().Base64Decode_n();

			//CString sUsername = sSASL.Token(1, false, "\0", true);
			//CString sPassword = sSASL.Token(2, false, "\0", true);

			CString sUsername = "kylef";
			CString sPassword = "pass";

			CUser *pUser = CZNC::Get().FindUser(sUsername);

			if (pUser && pUser->CheckPass(sPassword)) {
				Write(CXMPPStanza("success", "urn:ietf:params:xml:ns:xmpp-sasl"));

				m_pUser = pUser;
				DEBUG("XMPPClient SASL::PLAIN for [" << sUsername << "] success.");

				/* Restart the stream */
				m_uiDepth = 0;

				return;
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
	}

	DEBUG("XMPPClient unsupported stanza [" << Stanza.GetName() << "]");
}

