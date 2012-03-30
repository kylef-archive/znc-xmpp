/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _STANZA_H
#define _STANZA_H

#include <libxml/parser.h>

#include <znc/ZNCString.h>

class CXMPPStanza {
public:
	typedef enum {
	    XMPP_STANZA_UNKNOWN,
    	XMPP_STANZA_TEXT,
	    XMPP_STANZA_TAG
	} EStanzaType;

	CXMPPStanza(CString sName = "", CString sNamespace = "");
	~CXMPPStanza();

	CString ToString() const;

	bool IsText() const { return m_eType == XMPP_STANZA_TEXT; }
	bool IsTag()  const { return m_eType == XMPP_STANZA_TAG; }

	bool SetName(CString sName);
	CString GetName() const;

	bool SetText(CString sText);
	CString GetText() const;

	CString GetData() const { return m_sData; }
	void SetData(CString sData) { m_sData = sData; }

	CXMPPStanza* GetParent() const { return m_pParent; }
	void SetParent(CXMPPStanza *pParent) { m_pParent = pParent; }
	CXMPPStanza& NewChild(CString sName = "", CString sNamespace = "");

	/* Get the first child of stanza with name. */
	CXMPPStanza* GetChildByName(CString sName) const;
	CXMPPStanza* GetTextChild() const;

	CString GetAttribute(CString sName) const;
	bool HasAttribute(CString sName) const;
	void SetAttribute(CString sName, CString sValue);

	void SetAttributes(const xmlChar **attrs);

protected:
	void AddChild(CXMPPStanza &child);

	EStanzaType m_eType;

	CString m_sData;
	CXMPPStanza *m_pParent;
	MCString m_msAttributes;

	vector<CXMPPStanza*> m_vChildren;
};

#endif

