//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#ifndef IREGISTRY_H
#define IREGISTRY_H

//-----------------------------------------------------------------------------
// Purpose: Interface to registry
//-----------------------------------------------------------------------------
class IRegistry
{
public:
	// Init/shutdown
	virtual void Init() = 0;
	virtual void Shutdown() = 0;

	// Read/write integers
	virtual int ReadInt( const char* key, int defaultValue ) = 0;
	virtual void WriteInt( const char* key, int value ) = 0;
	// Read/write strings
	virtual const char* ReadString( const char* key, const char* defaultValue ) = 0;
	virtual void WriteString( const char* key, const char* value ) = 0;
};

extern IRegistry*	registry;

#endif
