/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _JID_H
#define _JID_H

#include <znc/ZNCString.h>

class CXMPPModule;

class CXMPPJID {
public:
	CXMPPJID(CString sJID);
	CString ToString() const;

	const CString& GetUser() const { return m_sUser; }
	const CString& GetDomain() const { return m_sDomain; }
	const CString& GetResource() const { return m_sResource; }

	bool IsLocal(const CXMPPModule &Module) const;

protected:
	CString m_sUser;
	CString m_sDomain;
	CString m_sResource;
};

#endif

