/* Copyright (C) 2005-2010, Thorvald Natvig <thorvald@natvig.com>
   Copyright (C) 2012, Moritz Schneeweiss <quirb@hotmail.com>

   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   - Neither the name of the Mumble Developers nor the names of its
     contributors may be used to endorse or promote products derived from this
     software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "../mumble_plugin_win32.h"

static int fetch(float *avatar_pos, float *avatar_front, float *avatar_top, float *camera_pos, float *camera_front, float *camera_top, std::string &context, std::wstring &/*identity*/) {
	for (int i=0;i<3;i++)
		avatar_pos[i]=avatar_front[i]=avatar_top[i]=camera_pos[i]=camera_front[i]=camera_top[i]=0.0f;

	unsigned int state;
	bool ok;

	/*BYTE *stateptr;
	ok = peekProc((BYTE *) pModule+0x2A44680, state);

	if (! ok)
		return false;

	if (state == 0) {
		context.clear();
		return true; // This results in all vectors beeing zero which tells Mumble to ignore them.
	}*/

	// Create containers to stuff our raw data into, so we can convert it to Mumble's coordinate system
	float pos_corrector[3];
	float front_corrector[3];
	float top_corrector[3];

	// Peekproc and assign game addresses to our containers, so we can retrieve positional data
	BYTE *baseptr0;
	BYTE *baseptr1;
	BYTE *baseptr2;
	BYTE *baseptr;

	ok = peekProc((BYTE *) pModule + 0x001391BC, baseptr0) &&
	     peekProc((BYTE *) baseptr0 + 0x28, baseptr1) &&
	     peekProc((BYTE *) baseptr1 + 0x4D8, baseptr2);
	
	if (! ok)
		return false;

	baseptr = baseptr2 + 0x5264;

	ok = peekProc((BYTE *) baseptr, pos_corrector) &&
	     peekProc((BYTE *) baseptr + 0x24, top_corrector) &&
	     peekProc((BYTE *) baseptr + 0x3C, front_corrector);

	if (! ok)
		return false;

	// Convert to left-handed coordinate system
	avatar_pos[0] = -pos_corrector[0];
	avatar_pos[1] = pos_corrector[1];
	avatar_pos[2] = pos_corrector[2];

	for (int i=0; i<3; i++)
		avatar_pos[i] *= 0.75f;

	avatar_front[0] = -front_corrector[0];
	avatar_front[1] = front_corrector[1];
	avatar_front[2] = front_corrector[2];
	
	avatar_top[0] = -top_corrector[0];
	avatar_top[1] = top_corrector[1];
	avatar_top[2] = top_corrector[2];
	
	for (int i=0;i<3;i++) {
		camera_pos[i] = avatar_pos[i];
		camera_front[i] = avatar_front[i];
		camera_top[i] = avatar_top[i];
	}

	// Read ip address
	BYTE *ipbase = peekProc<BYTE *> ((BYTE *) getModuleAddr(L"Spark_Core.dll") + 0x0058B1A8);
	BYTE *ipptr1 = peekProc<BYTE *> ((BYTE *) ipbase + 0xA48);
	BYTE *ipptr2 = peekProc<BYTE *> ((BYTE *) ipptr1);
	BYTE *ipptr3 = peekProc<BYTE *> ((BYTE *) ipptr2 + 0x80);
	BYTE *ipptr4 = peekProc<BYTE *> ((BYTE *) ipptr3 + 0x2D94);
	BYTE *ipptr5 = peekProc<BYTE *> ((BYTE *) ipptr4 + 0x14);
	BYTE *ipptr6 = peekProc<BYTE *> ((BYTE *) ipptr5);
	BYTE *ipptr7 = ipptr6 + 0x40;

	BYTE ip [4];
	peekProc(ipptr7, ip);

	/*std::ostringstream debugss;
	debugss << std::hex << (int)ipbase;
	debugss << " -> " << (int)ipptr1;
	debugss << " -> " << (int)ipptr2;
	debugss << " -> " << (int)ipptr3;
	debugss << " -> " << (int)ipptr4;
	debugss << " -> " << (int)ipptr5;
	debugss << " -> " << (int)ipptr6;
	debugss << " -> " << (int)ipptr7;
	OutputDebugStringA(debugss.str().c_str());*/

	std::ostringstream contextss;
	contextss << "{" << "\"ip\":\"" << std::dec;
	for (int i = 3; i >= 0; i--) {
		contextss << (int)(ip[i]);
		if (i > 0) {
			contextss << ".";
		}
	}
	contextss << "\"" << "}";

	context = contextss.str();

	return true;
}

static int trylock(const std::multimap<std::wstring, unsigned long long int> &pids) {

	if (! initialize(pids, L"NS2.exe", L"fmodex.dll"))
		return false;

	// Check if we can get meaningful data from it
	float apos[3], afront[3], atop[3];
	float cpos[3], cfront[3], ctop[3];
	std::wstring sidentity;
	std::string scontext;

	if (fetch(apos, afront, atop, cpos, cfront, ctop, scontext, sidentity)) {
		return true;
	} else {
		generic_unlock();
		return false;
	}
}

static const std::wstring longdesc() {
	return std::wstring(L"Supports Natural Selection 2 (Build 249). No identity or context support yet.");
}

static std::wstring description(L"Natural Selection 2 (Build 249)");
static std::wstring shortname(L"Natural Selection 2");

static int trylock1() {
	return trylock(std::multimap<std::wstring, unsigned long long int>());
}

static MumblePlugin ns2plug = {
	MUMBLE_PLUGIN_MAGIC,
	description,
	shortname,
	NULL,
	NULL,
	trylock1,
	generic_unlock,
	longdesc,
	fetch
};

static MumblePlugin2 ns2plug2 = {
	MUMBLE_PLUGIN_MAGIC_2,
	MUMBLE_PLUGIN_VERSION,
	trylock
};

extern "C" __declspec(dllexport) MumblePlugin *getMumblePlugin() {
	return &ns2plug;
}

extern "C" __declspec(dllexport) MumblePlugin2 *getMumblePlugin2() {
	return &ns2plug2;
}
