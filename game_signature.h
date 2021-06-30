#ifndef _INCLUDE_GAME_SIGNATURE_
#define _INCLUDE_GAME_SIGNATURE_

#ifdef WIN32
#if SOURCE_ENGINE == SE_LEFT4DEAD
#include "l4d1_signature_win32.h"
#else
#include "l4d2_signature_win32.h"
#endif
#else
#if SOURCE_ENGINE == SE_LEFT4DEAD
#include "l4d1_signature_linux.h"
#else
#include "l4d2_signature_linux.h"
#endif
#endif

#endif //_INCLUDE_GAME_SIGNATURE_
