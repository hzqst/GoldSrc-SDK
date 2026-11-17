#pragma once

#include <cstddef>
#include <cstring>

// Instantiate these adapters with each game's real player type and array size.
template<class Player, size_t Count>
void UpdateViewportPlayerScore( Player ( &players )[Count], int player, int frags,
	int deaths, int playerClass, int team )
{
	if ( player <= 0 || static_cast<size_t>( player ) >= Count )
		return;
	Player& info = players[player];
	info.frags = static_cast<decltype( info.frags )>( frags );
	info.deaths = static_cast<decltype( info.deaths )>( deaths );
	info.playerclass = static_cast<decltype( info.playerclass )>( playerClass );
	info.teamnumber = static_cast<decltype( info.teamnumber )>( team < 0 ? 0 : team );
}

template<class Player, size_t Count>
void UpdateViewportPlayerTeam( Player ( &players )[Count], int player, const char* team )
{
	if ( player <= 0 || static_cast<size_t>( player ) >= Count || !team )
		return;
	Player& info = players[player];
	std::strncpy( info.teamname, team, sizeof( info.teamname ) - 1 );
	info.teamname[sizeof( info.teamname ) - 1] = '\0';
}
