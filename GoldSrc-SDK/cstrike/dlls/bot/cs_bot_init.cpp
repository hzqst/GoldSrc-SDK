#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "cs_bot_manager.h"

CBotManager* TheBots = NULL;

cvar_t cv_bot_traceview = { "bot_traceview", "0", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_stop = { "bot_stop", "0", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_show_nav = { "bot_show_nav", "0", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_show_danger = { "bot_show_danger", "0", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_nav_edit = { "bot_nav_edit", "0", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_nav_zdraw = { "bot_nav_zdraw", "4", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_walk = { "bot_walk", "0", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_difficulty = { "bot_difficulty", "0", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_debug = { "bot_debug", "0", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_quicksave = { "bot_quicksave", "0", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_quota = { "bot_quota", "0", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_quota_match = { "bot_quota_match", "0", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_prefix = { "bot_prefix", "", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_allow_rogues = { "bot_allow_rogues", "1", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_allow_pistols = { "bot_allow_pistols", "1", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_allow_shotguns = { "bot_allow_shotguns", "1", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_allow_sub_machine_guns = { "bot_allow_sub_machine_guns", "1", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_allow_rifles = { "bot_allow_rifles", "1", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_allow_machine_guns = { "bot_allow_machine_guns", "1", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_allow_grenades = { "bot_allow_grenades", "1", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_allow_snipers = { "bot_allow_snipers", "1", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_allow_shield = { "bot_allow_shield", "1", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_join_team = { "bot_join_team", "any", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_join_after_player = { "bot_join_after_player", "1", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_auto_vacate = { "bot_auto_vacate", "1", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_zombie = { "bot_zombie", "0", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_defer_to_human = { "bot_defer_to_human", "0", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_chatter = { "bot_chatter", "normal", FCVAR_SERVER, 0.0f, NULL };
cvar_t cv_bot_profile_db = { "bot_profile_db", "BotProfile.db", FCVAR_SERVER, 0.0f, NULL };

void InstallBotControl()
{
	delete TheBots;
	TheBots = new CCSBotManager;
}

void Bot_ServerCommand()
{
	if ( TheBots != NULL )
		TheBots->ServerCommand( CMD_ARGV( 0 ) );
}

void Bot_RegisterCvars()
{
	if ( !UTIL_IsGame( "czero" ) )
		return;

	CVAR_REGISTER( &cv_bot_traceview );
	CVAR_REGISTER( &cv_bot_stop );
	CVAR_REGISTER( &cv_bot_show_nav );
	CVAR_REGISTER( &cv_bot_show_danger );
	CVAR_REGISTER( &cv_bot_nav_edit );
	CVAR_REGISTER( &cv_bot_nav_zdraw );
	CVAR_REGISTER( &cv_bot_walk );
	CVAR_REGISTER( &cv_bot_difficulty );
	CVAR_REGISTER( &cv_bot_debug );
	CVAR_REGISTER( &cv_bot_quicksave );
	CVAR_REGISTER( &cv_bot_quota );
	CVAR_REGISTER( &cv_bot_quota_match );
	CVAR_REGISTER( &cv_bot_prefix );
	CVAR_REGISTER( &cv_bot_allow_rogues );
	CVAR_REGISTER( &cv_bot_allow_pistols );
	CVAR_REGISTER( &cv_bot_allow_shotguns );
	CVAR_REGISTER( &cv_bot_allow_sub_machine_guns );
	CVAR_REGISTER( &cv_bot_allow_rifles );
	CVAR_REGISTER( &cv_bot_allow_machine_guns );
	CVAR_REGISTER( &cv_bot_allow_grenades );
	CVAR_REGISTER( &cv_bot_allow_snipers );
	CVAR_REGISTER( &cv_bot_allow_shield );
	CVAR_REGISTER( &cv_bot_join_team );
	CVAR_REGISTER( &cv_bot_join_after_player );
	CVAR_REGISTER( &cv_bot_auto_vacate );
	CVAR_REGISTER( &cv_bot_zombie );
	CVAR_REGISTER( &cv_bot_defer_to_human );
	CVAR_REGISTER( &cv_bot_chatter );
	CVAR_REGISTER( &cv_bot_profile_db );
}
