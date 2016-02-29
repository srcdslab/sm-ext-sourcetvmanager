#if defined _stvmngr_included
 #endinput
#endif
#define _stvmngr_included

native SourceTV_GetHLTVServerCount();
native SourceTV_SelectHLTVServer(instance);
native SourceTV_GetSelectedHLTVServer();
native SourceTV_GetBotIndex();
native bool:SourceTV_GetLocalStats(&proxies, &slots, &specs);
native SourceTV_GetBroadcastTick();
native Float:SourceTV_GetDelay();
native bool:SourceTV_BroadcastHintMessage(const String:format[], any:...);
native bool:SourceTV_BroadcastConsoleMessage(const String:format[], any:...);
// get current view entity (PVS), 0 if coords are used
native SourceTV_GetViewEntity();
// get current PVS origin
native SourceTV_GetViewCoordinates(Float:view[3]);

native bool:SourceTV_StartRecording(const String:sFilename[]);
native bool:SourceTV_StopRecording();
native bool:SourceTV_IsRecording();
native bool:SourceTV_GetDemoFileName(String:sFilename[], maxlen);
native SourceTV_GetRecordingTick();
native bool:SourceTV_PrintToDemoConsole(const String:format[], any:...);

native SourceTV_GetSpectatorCount();
native SourceTV_GetMaxClients();
native SourceTV_GetClientCount();
native bool:SourceTV_IsClientConnected(client);
native SourceTV_GetSpectatorName(client, String:name[], maxlen);
native SourceTV_KickClient(client, const String:sReason[]);

forward SourceTV_OnStartRecording(hltvinstance, const String:filename[], bool:bContinuously);
forward SourceTV_OnStopRecording(hltvinstance, const String:filename[], recordingtick);

/**
 * Do not edit below this line!
 */
public Extension:__ext_stvmngr = 
{
	name = "SourceTV Manager",
	file = "sourcetvmanager.ext",
#if defined AUTOLOAD_EXTENSIONS
	autoload = 1,
#else
	autoload = 0,
#endif
#if defined REQUIRE_EXTENSIONS
	required = 1,
#else
	required = 0,
#endif
};

#if !defined REQUIRE_EXTENSIONS
public __ext_stvmngr_SetNTVOptional()
{
	MarkNativeAsOptional("SourceTV_GetHLTVServerCount");
	MarkNativeAsOptional("SourceTV_SelectHLTVServer");
	MarkNativeAsOptional("SourceTV_GetSelectedHLTVServer");
	MarkNativeAsOptional("SourceTV_GetBotIndex");
	MarkNativeAsOptional("SourceTV_GetLocalStats");
	MarkNativeAsOptional("SourceTV_GetBroadcastTick");
	MarkNativeAsOptional("SourceTV_GetDelay");
	MarkNativeAsOptional("SourceTV_BroadcastHintMessage");
	MarkNativeAsOptional("SourceTV_BroadcastConsoleMessage");
	MarkNativeAsOptional("SourceTV_GetViewEntity");
	MarkNativeAsOptional("SourceTV_GetViewCoordinates");
	MarkNativeAsOptional("SourceTV_StartRecording");
	MarkNativeAsOptional("SourceTV_StopRecording");
	MarkNativeAsOptional("SourceTV_IsRecording");
	MarkNativeAsOptional("SourceTV_GetDemoFileName");
	MarkNativeAsOptional("SourceTV_GetRecordingTick");
	MarkNativeAsOptional("SourceTV_PrintToDemoConsole");
	MarkNativeAsOptional("SourceTV_GetSpectatorCount");
	MarkNativeAsOptional("SourceTV_GetMaxClients");
	MarkNativeAsOptional("SourceTV_GetClientCount");
	MarkNativeAsOptional("SourceTV_IsClientConnected");
	MarkNativeAsOptional("SourceTV_GetSpectatorName");
	MarkNativeAsOptional("SourceTV_KickClient");
}
#endif
