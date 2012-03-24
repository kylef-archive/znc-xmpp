/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Socket.h"
#include "xmpp.h"

/* libxml2 handlers */

static void _start_element(void *userdata, const xmlChar *name, const xmlChar **attrs) {
	CXMPPSocket *pSocket = (CXMPPSocket*)userdata;

	if (pSocket->GetDepth() == 0) {
		if (strcmp((char*)name, "stream:stream") != 0) {
			DEBUG("XMPPClient: Socket did not open valid stream. [" << name << "]");
			pSocket->Close(Csock::CLT_AFTERWRITE);
			pSocket->IncrementDepth(); /* Incrmement because otherwise end_element will be confused */
			return;
		}

		CXMPPStanza Stanza;
		Stanza.SetName("stream:stream");
		if (attrs) {
			Stanza.SetAttributes(attrs);
		}

		pSocket->StreamStart(Stanza);
	} else if (!pSocket->GetStanza()) {
		/* New top level stanza */
		CXMPPStanza *pStanza = new CXMPPStanza((char *)name);

		pSocket->SetStanza(pStanza);

		if (attrs) {
			pStanza->SetAttributes(attrs);
		}
	} else {
		/* New child stanza */
		CXMPPStanza& child = pSocket->GetStanza()->NewChild((char *)name);
		pSocket->SetStanza(&child);

		if (attrs) {
			child.SetAttributes(attrs);
		}
	}

	DEBUG("libxml (" << pSocket->GetDepth() << ") start_element: [" << name << "]");

	pSocket->IncrementDepth();
}

static void _end_element(void *userdata, const xmlChar *name) {
	CXMPPSocket *pSocket = (CXMPPSocket*)userdata;

	pSocket->DeincrementDepth();

	if (pSocket->GetDepth() == 0) {
		pSocket->StreamEnd();
		DEBUG("XMPPClient: RECV </stream:stream>");
		return;
	} else if (pSocket->GetStanza()->GetParent()) {
		/* We are finishing a child stanza, so set current to parent */
		pSocket->SetStanza(pSocket->GetStanza()->GetParent());
	} else {
		CXMPPStanza *pStanza = pSocket->GetStanza();
		pSocket->SetStanza(NULL);

		pSocket->ReceiveStanza(*pStanza);
		delete pStanza;
	}

	DEBUG("libxml (" << pSocket->GetDepth() << ") end_element: [" << name << "]");
}

static void _characters(void *userdata, const xmlChar *chr, int len) {
	CXMPPSocket *pSocket = (CXMPPSocket*)userdata;

	if (pSocket->GetStanza()) {
		CXMPPStanza &Stanza = pSocket->GetStanza()->NewChild();
		Stanza.SetText(CString((char*)chr, len));
	}
}

CXMPPSocket::CXMPPSocket(CModule *pModule) : CSocket(pModule) {
	m_uiDepth = 0;
	m_pStanza = NULL;

	DisableReadLine();

	m_xmlContext = NULL;
	m_bResetParser = true;

	memset(&m_xmlHandlers, 0, sizeof(xmlSAXHandler));
	m_xmlHandlers.startElement = _start_element;
	m_xmlHandlers.endElement = _end_element;
	m_xmlHandlers.characters = _characters;
}

CXMPPSocket::~CXMPPSocket() {
	xmlFreeParserCtxt(m_xmlContext);

	/* We might have a leftover stanza */
	if (m_pStanza) {
		while (m_pStanza->GetParent()) {
			m_pStanza = m_pStanza->GetParent();
		}

		delete m_pStanza;
	}
}

CString CXMPPSocket::GetServerName() const {
	return GetModule()->GetServerName();
}

void CXMPPSocket::ReadData(const char *data, size_t len) {
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

bool CXMPPSocket::Write(const CXMPPStanza &Stanza) {
	return Write(Stanza.ToString());
}

bool CXMPPSocket::Write(const CString &sString) {
	return CSocket::Write(sString);
}

void CXMPPSocket::StreamStart(CXMPPStanza &Stanza) {
	return;
}

void CXMPPSocket::StreamEnd() {
	Close(Csock::CLT_AFTERWRITE);
}

void CXMPPSocket::ReceiveStanza(CXMPPStanza &Stanza) {
	DEBUG("CXMPPSocket unsupported stanza [" << Stanza.GetName() << "]");
}

