/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Listener.h"
#include "Client.h"

Csock* CXMPPListener::GetSockObj(const CString& sHost, unsigned short uPort) {
	return new CXMPPClient(GetModule());
}

