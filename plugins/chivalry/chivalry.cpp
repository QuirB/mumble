/* Copyright (C) 2005-2010, Thorvald Natvig <thorvald@natvig.com>
   Copyright (C) 2012, Moritz Schneeweiss

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

BYTE* camptr;
BYTE* frtptr;
BYTE* topptr;

static void wcsToMultibyteStdString(wchar_t *wcs, std::string &str) {
	const int size = WideCharToMultiByte(CP_UTF8, 0, wcs, -1, NULL, 0, NULL, NULL);
	if (size == 0) return;

	str.resize(size);

	WideCharToMultiByte(CP_UTF8, 0, wcs, -1, &str[0], size, NULL, NULL);
}

static float getLength(float *a) {
	return sqrtf(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
}

static void normalize(float *a) {
	float length = getLength(a);
	if (length != 0.0f) {
		a[0] /= length;
		a[1] /= length;
		a[2] /= length;
	}
}

static int fetch(float *avatar_pos, float *avatar_front, float *avatar_top, float *camera_pos, float *camera_front, float *camera_top, std::string &context, std::wstring &/*identity*/) {
	for (int i=0;i<3;i++)
		avatar_pos[i]=avatar_front[i]=avatar_top[i]=camera_pos[i]=camera_front[i]=camera_top[i]=0.0f;

	unsigned long ip;
	bool ok;

	BYTE *ipptr0;
	BYTE *ipptr1;
	BYTE *ipptr2;
	ok = peekProc((BYTE *) pModule+0x134CFC8, ipptr0) &&
	     peekProc((BYTE *) ipptr0 + 0x651F8, ipptr1) &&
	     peekProc((BYTE *) ipptr1 + 0x78, ipptr2) &&
	     peekProc((BYTE *) ipptr2, ip);
	if (! ok)
		return false;

	if (ip == 0xFFFFFFFF) { // If not conntected to a server
		context.clear();
		return true; // This results in all vectors beeing zero which tells Mumble to ignore them.
	}

	// Dereference pointer to player position
	BYTE *posptr0;
	BYTE *posptr1;
	ok = peekProc((BYTE *) pModule + 0x1367860, posptr0) &&
	     peekProc((BYTE *) posptr0 + 0x12C, posptr1);
	if (! ok)
		return false;

	// Create containers to stuff our raw data into, so we can convert it to Mumble's coordinate system
	float cam_corrector[3];
	float pos_corrector[3];
	float front_corrector[3];
	float top_corrector[3];

	// Peekproc and assign game addresses to our containers, so we can retrieve positional data
	ok = peekProc((BYTE *) camptr, cam_corrector) &&
	     peekProc((BYTE *) frtptr, front_corrector[0]) &&
	     peekProc((BYTE *) frtptr+0x10, front_corrector[1]) &&
	     peekProc((BYTE *) frtptr+0x20, front_corrector[2]) &&
	     peekProc((BYTE *) topptr, top_corrector[0]) &&
	     peekProc((BYTE *) topptr+0x10, top_corrector[1]) &&
	     peekProc((BYTE *) topptr+0x20, top_corrector[2]) &&
	     peekProc((BYTE *) posptr1+0xA4, pos_corrector);

	if (! ok)
		return false;
	
	// Convert to left-handed coordinate system

	camera_pos[0] = cam_corrector[1];
	camera_pos[1] = cam_corrector[2];
	camera_pos[2] = cam_corrector[0];

	avatar_pos[0] = pos_corrector[1];
	avatar_pos[1] = pos_corrector[2];
	avatar_pos[2] = pos_corrector[0];

	for (int i=0;i<3;i++) {
		avatar_pos[i] /= 67.5f; // Scale to meters
		camera_pos[i] /= 67.5f;
	}

	camera_front[0] = front_corrector[1];
	camera_front[1] = front_corrector[2];
	camera_front[2] = front_corrector[0];
	
	normalize(top_corrector);
	camera_top[0] = top_corrector[1];
	camera_top[1] = top_corrector[2];
	camera_top[2] = top_corrector[0];
	
	for (int i=0;i<3;i++) {
		avatar_front[i] = camera_front[i];
		avatar_top[i] = camera_top[i];
	}

	BYTE *ipBytes = (BYTE *)&ip;

	std::ostringstream contextss;
	contextss << "{" << "\"ip\":\"" << std::dec;
	for (int i = 3; i >= 0; i--) {
		contextss << (int)(ipBytes[i]);
		if (i > 0) {
			contextss << ".";
		}
	}
	contextss << "\"" << "}";

	context = contextss.str();

	return true;
}

static int trylock(const std::multimap<std::wstring, unsigned long long int> &pids) {

	if (! initialize(pids, L"UDK.exe"))
		return false;

	BYTE* base = pModule + 0x13590C0;
	camptr = base;
	frtptr = base + 0x18;
	topptr = base + 0x14;

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
	return std::wstring(L"Chivalry: Medieval Warfare (v1.0.10246.0). Basic context support. No identity support yet.");
}

static std::wstring description(L"Chivalry: Medieval Warfare (v1.0.10246.0)");
static std::wstring shortname(L"Chivalry: Medieval Warfare");

static int trylock1() {
	return trylock(std::multimap<std::wstring, unsigned long long int>());
}

static MumblePlugin chivalryplug = {
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

static MumblePlugin2 chivalryplug2 = {
	MUMBLE_PLUGIN_MAGIC_2,
	MUMBLE_PLUGIN_VERSION,
	trylock
};

extern "C" __declspec(dllexport) MumblePlugin *getMumblePlugin() {
	return &chivalryplug;
}

extern "C" __declspec(dllexport) MumblePlugin2 *getMumblePlugin2() {
	return &chivalryplug2;
}
