/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Stanza.h"

CXMPPStanza::CXMPPStanza(CString sName, CString sNamespace) {
	m_pParent = NULL;
	m_eType = XMPP_STANZA_UNKNOWN;

	if (!sName.empty()) {
		SetName(sName);
	}

	if (!sNamespace.empty()) {
		SetAttribute("xmlns", sNamespace);
	}
}

CXMPPStanza::~CXMPPStanza() {
	/* Kill our children */
	for (vector<CXMPPStanza*>::iterator it = m_vChildren.begin(); it != m_vChildren.end();) {
		CXMPPStanza *pChild = *it;
		it = m_vChildren.erase(it);
		delete pChild;
	}
}

CString CXMPPStanza::ToString() const {
	if (IsTag()) {
		CString sOutput = "<" + GetName();

		for (MCString::const_iterator it = m_msAttributes.begin(); it != m_msAttributes.end(); ++it) {
			sOutput += " " + it->first + "='" + it->second + "'";
		}

		if (m_vChildren.empty()) {
			return sOutput + " />";
		}

		sOutput += ">";

		vector<CXMPPStanza*>::const_iterator it;
		for (it = m_vChildren.begin(); it != m_vChildren.end(); ++it) {
			CXMPPStanza *pChild = *it;
			sOutput += pChild->ToString();
		}

		sOutput += "</" + GetName() + ">";

		return sOutput;
	} else if (IsText()) {
		return GetText();
	}

	return "";
}

bool CXMPPStanza::SetName(CString sName) {
	if (IsTag()) {
		return false;
	}

	m_eType = XMPP_STANZA_TAG;
	m_sData = sName;
	return true;
}

CString CXMPPStanza::GetName() const {
	if (IsTag()) {
		return m_sData;
	}

	return "";
}

bool CXMPPStanza::SetText(CString sText) {
	if (!IsTag()) {
		m_sData = sText;
		return true;
	}

	return false;
}

CString CXMPPStanza::GetText() const {
	if (IsTag()) {
		return "";
	}

	return m_sData;
}

void CXMPPStanza::SetAttributes(const xmlChar **attrs) {
	if (!IsTag()) {
		return;
	}

	for (unsigned int i = 0; attrs[i]; i += 2) {
		SetAttribute((char*) attrs[i], (char*) attrs[i+1]);
	}
}

CString CXMPPStanza::GetAttribute(CString sName) const {
	if (!IsTag()) {
		return "";
	}

	MCString::const_iterator it = m_msAttributes.find(sName);

	if (it == m_msAttributes.end()) {
		return "";
	}

	return it->second;
}

void CXMPPStanza::SetAttribute(CString sName, CString sValue) {
	if (!IsTag()) {
		return;
	}

	m_msAttributes[sName] = sValue;
}

void CXMPPStanza::AddChild(CXMPPStanza &child) {
	m_vChildren.push_back(&child);
}


CXMPPStanza& CXMPPStanza::NewChild(CString sName, CString sNamespace) {
	CXMPPStanza *pChild = new CXMPPStanza(sName, sNamespace);
	pChild->SetParent(this);
	AddChild(*pChild);
	return *pChild;
}

CXMPPStanza* CXMPPStanza::GetChildByName(CString sName) const {
	vector<CXMPPStanza*>::const_iterator it;
	for (it = m_vChildren.begin(); it != m_vChildren.end(); ++it) {
		CXMPPStanza *pChild = *it;

		if (pChild->GetName().Equals(sName)) {
			return pChild;
		}
	}

	return NULL;
}

