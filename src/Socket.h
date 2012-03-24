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

class CXMPPModule;

class CXMPPSocket : public CSocket {
public:
	CXMPPSocket(CModule *pModule);
	virtual ~CXMPPSocket();

	CXMPPModule *GetModule() const { return (CXMPPModule*)m_pModule; }
	CString GetServerName() const;

	virtual void ReadData(const char *data, size_t len);

	bool Write(const CXMPPStanza& Stanza);
	bool Write(const CString &sString);

	unsigned int GetDepth() const { return m_uiDepth; }
	void IncrementDepth() { m_uiDepth++; }
	void DeincrementDepth() { m_uiDepth--; }

	CXMPPStanza* GetStanza() const { return m_pStanza; }
	void SetStanza(CXMPPStanza *pStanza) { m_pStanza = pStanza; }

	virtual void StreamStart(CXMPPStanza &Stanza);
	virtual void StreamEnd();

	virtual void ReceiveStanza(CXMPPStanza &Stanza);

protected:
	xmlParserCtxtPtr m_xmlContext;
	xmlSAXHandler    m_xmlHandlers;

	unsigned int     m_uiDepth;
	CXMPPStanza     *m_pStanza;

	bool             m_bResetParser;
};

