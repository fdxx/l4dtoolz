#pragma once

#include <tier1/interface.h>
#include <engine/iserverplugin.h>

class l4dtoolz: public IServerPluginCallbacks
{
public:
	bool Load( CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory );
	void Unload();
	void Pause() {}
	void UnPause() {}
	const char *GetPluginDescription() { return "l4dtoolz v0.5.1 https://github.com/fdxx/l4dtoolz"; }     
	void LevelInit( char const *pMapName ) {}
	void ServerActivate( edict_t *pEdictList, int edictCount, int clientMax ) {}
	void GameFrame( bool simulating ) {}
	void LevelShutdown() {}
	void ClientActive( edict_t *pEntity ) {}
	void ClientDisconnect( edict_t *pEntity ) {}
	void ClientPutInServer( edict_t *pEntity, char const *playername ) {}
	void SetCommandClient( int index ) {}
	void ClientSettingsChanged( edict_t *pEdict ) {}
	PLUGIN_RESULT ClientConnect( bool *bAllowConnect, edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen ) { return PLUGIN_CONTINUE; }
	PLUGIN_RESULT ClientCommand( edict_t *pEntity, const CCommand &args ) { return PLUGIN_CONTINUE; }
	PLUGIN_RESULT NetworkIDValidated( const char *pszUserName, const char *pszNetworkID ) { return PLUGIN_CONTINUE; }
	void OnQueryCvarValueFinished( QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue ) {}
};
