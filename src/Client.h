/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <znc/User.h>
#include <znc/znc.h>

#include "Stanza.h"

class CXMPPClient : public CSocket {
public:
	CXMPPClient(CModule *pModule);
	virtual ~CXMPPClient();

	CUser* GetUser() const { return m_pUser; }

	virtual void ReadData(const char *data, size_t len);
	virtual void ReadLine(const CString& sData);

	bool Write(const CXMPPStanza &Stanza);
	bool Write(const CString &sString);

	unsigned int GetDepth() const { return m_uiDepth; }
	void IncrementDepth() { m_uiDepth++; }
	void DeincrementDepth() { m_uiDepth--; }

	CXMPPStanza* GetStanza() const { return m_pStanza; }
	void SetStanza(CXMPPStanza *pStanza) { m_pStanza = pStanza; }

	void StreamStart(CXMPPStanza &Stanza);
	void StreamEnd();

	void ReceiveStanza(CXMPPStanza &Stanza);

protected:
	xmlParserCtxtPtr m_xmlContext;
	xmlSAXHandler    m_xmlHandlers;

	unsigned int     m_uiDepth;
	CXMPPStanza     *m_pStanza;

	CUser *m_pUser;
};

