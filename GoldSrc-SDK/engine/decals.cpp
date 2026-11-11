#if defined( SW_RENDERER )

#include "decals.h"
#include "draw.h"
#include "wad.h"
#include "common.h"
#include "filesystem.h"
#include "mem.h"
#include "model.h"
#include "bspfile.h"
#include "conprint.h"
#include "sys.h"
#include "zone.h"
#include "client.h"
#include "server.h"

#include <cstdio>
#include <cstring>

typedef struct lumplist_s
{
	lumpinfo_t*			lump;
	qboolean			breplaced;
	struct lumplist_s*	next;
} lumplist_t;

qboolean gfCustomBuild;
char szCustName[10];

static cachewad_t* decal_wad;
cachewad_t* menu_wad;
qboolean m_bDrawInitialized;
char decal_names[MAX_BASE_DECALS][DECAL_NAME_BYTES];

void Draw_AllocateCacheSpace( cachewad_t* wad, int cacheMax, int )
{
	wad->cache = (cacheentry_t*)Mem_Malloc( cacheMax * sizeof( cacheentry_t ) );
	Q_memset( wad->cache, 0, cacheMax * sizeof( cacheentry_t ) );
}

void Draw_CacheWadHandler( cachewad_t* wad, PFNCACHE fn, int extraDataSize )
{
	wad->pfnCacheBuild = fn;
	wad->cacheExtra = extraDataSize;
}

int Draw_CacheIndex( cachewad_t* wad, char* path )
{
	int i;
	cacheentry_t* pic;

	for ( i = 0, pic = wad->cache; i < wad->cacheCount; i++, pic++ )
	{
		if ( !Q_strcmp( path, pic->name ) )
			break;
	}

	if ( i == wad->cacheCount )
	{
		if ( wad->cacheCount == wad->cacheMax )
			Sys_Error( "Cache wad (%s) out of %d entries", wad->name, wad->cacheMax );

		wad->cacheCount++;
		Q_strncpy( pic->name, path, sizeof( pic->name ) - 1 );
		pic->name[sizeof( pic->name ) - 1] = 0;
	}

	return i;
}

int Draw_CacheFindIndex( cachewad_t* wad, char* path )
{
	int i;
	cacheentry_t* pic;

	for ( i = 0, pic = wad->cache; i < wad->cacheCount; i++, pic++ )
	{
		if ( !Q_strcmp( path, pic->name ) )
			return i;
	}

	return -1;
}

void Draw_CacheWadInitFromFile( FileHandle_t hFile, int len, char* name, int cacheMax, cachewad_t* wad )
{
	int i;
	wadinfo_t header;
	lumpinfo_t* lump_p;

	FS_Read( &header, sizeof( header ), 1, hFile );
	if ( header.identification[0] != 'W' || header.identification[1] != 'A' || header.identification[2] != 'D'
		|| header.identification[3] != '3' )
		Sys_Error( "Wad file %s doesn't have WAD3 id\n", name );

	wad->lumps = (lumpinfo_t*)Mem_Malloc( len - header.infotableofs );
	FS_Seek( hFile, header.infotableofs, FILESYSTEM_SEEK_HEAD );
	FS_Read( wad->lumps, len - header.infotableofs, 1, hFile );

	for ( i = 0, lump_p = wad->lumps; i < header.numlumps; i++, lump_p++ )
		W_CleanupName( lump_p->name, lump_p->name );

	wad->lumpCount = header.numlumps;
	wad->cacheCount = 0;
	wad->cacheMax = cacheMax;
	wad->name = Mem_Strdup( name );
	Draw_AllocateCacheSpace( wad, cacheMax, 0 );
	wad->pfnCacheBuild = NULL;
	wad->cacheExtra = 0;
}

void Draw_CacheWadInit( char* name, int cacheMax, cachewad_t* wad )
{
	int len;
	FileHandle_t hFile;

	hFile = FS_Open( name, "rb" );
	if ( !hFile )
		Sys_Error( "Draw_LoadWad: Couldn't open %s\n", name );

	len = FS_Size( hFile );
	Draw_CacheWadInitFromFile( hFile, len, name, cacheMax, wad );
	FS_Close( hFile );
}

void Draw_FreeWad( cachewad_t* pWad )
{
	int i;
	cacheentry_t* pic;

	if ( !pWad )
		return;

	if ( pWad->lumps )
		Mem_Free( pWad->lumps );
	pWad->lumps = NULL;

	Mem_Free( pWad->name );

	if ( pWad->numpaths )
	{
		for ( i = 0; i < pWad->numpaths; i++ )
		{
			Mem_Free( pWad->basedirs[i] );
			pWad->basedirs[i] = NULL;
		}
		Mem_Free( pWad->basedirs );
		pWad->basedirs = NULL;
	}

	if ( pWad->lumppathindices )
	{
		Mem_Free( pWad->lumppathindices );
		pWad->lumppathindices = NULL;
	}

	if ( pWad->cache )
	{
		for ( i = 0, pic = pWad->cache; i < pWad->cacheCount; i++, pic++ )
		{
			if ( Cache_Check( &pic->cache ) )
				Cache_Free( &pic->cache );
		}
		Mem_Free( pWad->cache );
	}
}

qboolean Draw_CustomCacheWadInit( int cacheMax, cachewad_t* wad, void* raw, int nFileSize )
{
	wadinfo_t header;
	lumpinfo_t* lump_p;
	int tableEnd, lumpEnd;

	memcpy( &header, raw, sizeof( header ) );

	if ( header.identification[0] != 'W' || header.identification[1] != 'A' || header.identification[2] != 'D'
		|| header.identification[3] != '3' )
	{
		Con_Printf( "Custom file doesn't have WAD3 id\n" );
		return FALSE;
	}

	if ( header.numlumps != 1 )
	{
		Con_Printf( "Custom file has wrong number of lumps %i\n", header.numlumps );
		return FALSE;
	}

	if ( header.infotableofs <= 0 )
	{
		Con_Printf( "Custom file has bogus infotableofs %i\n", header.infotableofs );
		return FALSE;
	}

	tableEnd = (int)( (unsigned int)header.infotableofs + sizeof( lumpinfo_t ) );
	if ( tableEnd != nFileSize )
	{
		Con_Printf( "Custom file has bogus infotableofs ( %i > %i )\n", tableEnd, nFileSize );
		return FALSE;
	}

	wad->lumps = (lumpinfo_t*)Mem_Malloc( sizeof( *wad->lumps ) );
	Q_memcpy( wad->lumps, (byte*)raw + header.infotableofs, sizeof( *wad->lumps ) );

	lump_p = wad->lumps;
	W_CleanupName( lump_p->name, lump_p->name );

	if ( lump_p->size != lump_p->disksize )
	{
		Con_Printf( "Custom file has mismatched lump size ( %i vs. %i )\n", lump_p->size, lump_p->disksize );
		return FALSE;
	}

	if ( lump_p->disksize <= 0 )
	{
		Con_Printf( "Custom file has bogus lump size %i\n", lump_p->disksize );
		return FALSE;
	}

	if ( (unsigned int)lump_p->filepos < sizeof( wadinfo_t ) )
	{
		Con_Printf( "Custom file has bogus lump offset %i\n", lump_p->filepos );
		return FALSE;
	}

	lumpEnd = (int)( (unsigned int)lump_p->filepos + (unsigned int)lump_p->disksize );
	if ( lumpEnd > header.infotableofs )
	{
		Con_Printf( "Custom file has bogus lump %i\n", 0 );
		return FALSE;
	}

	wad->lumpCount = 1;
	wad->cacheCount = 0;
	wad->cacheMax = cacheMax;
	wad->name = Mem_Strdup( "tempdecal.wad" );
	Draw_AllocateCacheSpace( wad, cacheMax, 0 );
	wad->pfnCacheBuild = NULL;
	wad->cacheExtra = 0;
	return TRUE;
}

int Draw_CacheByIndex( cachewad_t* wad, int nIndex, int playernum )
{
	int i;
	cacheentry_t* pic;
	char szTestName[32];

	snprintf( szTestName, sizeof( szTestName ), "%03i%02i", playernum, nIndex );

	for ( i = 0, pic = wad->cache; i < wad->cacheCount; i++, pic++ )
	{
		if ( !Q_strcmp( pic->name, szTestName ) )
			break;
	}

	if ( i == wad->cacheCount )
	{
		if ( wad->cacheCount == wad->cacheMax )
			Sys_Error( "Cache wad (%s) out of %d entries", wad->name, wad->cacheMax );

		wad->cacheCount++;
		snprintf( pic->name, sizeof( pic->name ), "%s", szTestName );
	}

	return i;
}

qboolean CustomDecal_Init( cachewad_t* wad, void* raw, int nFileSize, int playernum )
{
	int i;
	qboolean bret;

	bret = Draw_CustomCacheWadInit( 16, wad, raw, nFileSize );
	if ( bret )
	{
		Draw_CacheWadHandler( wad, Draw_MiptexTexture, MIP_EXTRASIZE );
		for ( i = 0; i < wad->lumpCount; i++ )
			Draw_CacheByIndex( wad, i, playernum );
	}

	return bret;
}

qboolean CustomDecal_Validate( void* raw, int nFileSize )
{
	qboolean bret;
	cachewad_t* wad;

	wad = (cachewad_t*)Mem_ZeroMalloc( sizeof( *wad ) );
	if ( !wad )
		return FALSE;

	bret = CustomDecal_Init( wad, raw, nFileSize, -2 );
	if ( bret )
		bret = Draw_CustomCacheGet( wad, raw, nFileSize, 0 ) != NULL;

	Draw_FreeWad( wad );
	Mem_Free( wad );
	return bret;
}

qboolean Draw_ValidateCustomLogo( cachewad_t* wad, byte* data, lumpinfo_t* lump )
{
	int i;
	texture_t tex;
	miptex_t tmp;
	unsigned int pix, size, cachedSize, end;
	unsigned short palsize;

	if ( wad->cacheExtra != MIP_EXTRASIZE )
	{
		Con_Printf( "Draw_ValidateCustomLogo: Bad cached wad %s\n", wad->name );
		return FALSE;
	}

	memcpy( &tmp, data + MIP_EXTRASIZE, sizeof( tmp ) );
	memcpy( &tex, data, sizeof( tex ) );

	Q_memcpy( tex.name, tmp.name, sizeof( tex.name ) );
	tex.width = LittleLong( tmp.width );
	tex.height = LittleLong( tmp.height );
	tex.anim_total = 0;
	tex.anim_min = 0;
	tex.anim_max = 0;
	tex.anim_next = NULL;
	tex.alternate_anims = NULL;

	for ( i = 0; i < MIPLEVELS; i++ )
		tex.offsets[i] = (unsigned int)LittleLong( tmp.offsets[i] ) + wad->cacheExtra;

	if ( !tex.width || !tex.height || tex.width > 256 || tex.height > 256 )
	{
		Con_Printf( "Draw_ValidateCustomLogo: Bad cached wad tex size %ux%u on %s\n", tex.width, tex.height, wad->name );
		return FALSE;
	}

	pix = tex.width * tex.height;
	size = pix + ( pix >> 2 ) + ( pix >> 4 ) + ( pix >> 6 );

	cachedSize = (unsigned int)lump->size + wad->cacheExtra;
	if ( size + sizeof( miptex_t ) >= cachedSize )
	{
		Con_Printf( "Draw_ValidateCustomLogo: Bad cached wad size %i/%i on %s\n", (int)( size + sizeof( miptex_t ) ),
			(int)cachedSize, wad->name );
	}

	memcpy( &palsize, data + MIP_EXTRASIZE + size + sizeof( miptex_t ), sizeof( palsize ) );

	for ( i = 0; i < MIPLEVELS - 1; i++ )
	{
		if ( tmp.offsets[i] + pix != tmp.offsets[i + 1] )
		{
			Con_Printf( "Draw_ValidateCustomLogo: Bad cached wad %s\n", wad->name );
			return FALSE;
		}
		pix >>= 2;
	}

	if ( palsize > 256 )
	{
		Con_Printf( "Draw_ValidateCustomLogo: Bad cached wad palette size %i on %s\n", palsize, wad->name );
		return FALSE;
	}

	end = (unsigned int)LittleLong( tmp.offsets[0] );
	end += size + 3 * palsize + sizeof( palsize );
	if ( end > (unsigned int)lump->disksize )
	{
		Con_Printf( "Draw_ValidateCustomLogo: Bad cached wad %s\n", wad->name );
		return FALSE;
	}

	return TRUE;
}

qboolean Draw_CacheLoadFromCustom( char* clean, cachewad_t* wad, void* raw, int, cacheentry_t* pic )
{
	int idx;
	byte* buf;
	lumpinfo_t* pLump;

	idx = 0;
	if ( Q_strlen( clean ) >= 5 )
	{
		idx = Q_atoi( clean + 3 );
		if ( idx < 0 || idx >= wad->lumpCount )
			return FALSE;
	}

	pLump = &wad->lumps[idx];
	buf = (byte*)Cache_Alloc( &pic->cache, pLump->size + wad->cacheExtra + 1, clean );
	if ( !buf )
		Sys_Error( "Draw_CacheGet: not enough space for %s in %s", clean, wad->name );

	buf[pLump->size + wad->cacheExtra] = 0;
	Q_memcpy( buf + wad->cacheExtra, (byte*)raw + pLump->filepos, pLump->size );

	if ( !Draw_ValidateCustomLogo( wad, buf, pLump ) )
		return FALSE;

	gfCustomBuild = TRUE;
	Q_strcpy( szCustName, "T" );
	Q_memcpy( szCustName + 1, clean, 5 );
	szCustName[6] = 0;

	if ( wad->pfnCacheBuild )
		wad->pfnCacheBuild( wad, buf );

	gfCustomBuild = FALSE;
	return TRUE;
}

void* Draw_CustomCacheGet( cachewad_t* wad, void* raw, int rawsize, int index )
{
	cacheentry_t* pic;
	void* dat;
	char name[WAD_LUMP_NAME_BYTES];
	char clean[WAD_LUMP_NAME_BYTES];

	if ( index >= wad->cacheCount )
		Sys_Error( "Cache wad indexed before load %s: %d", wad->name, index );

	pic = &wad->cache[index];
	dat = Cache_Check( &pic->cache );
	if ( !dat )
	{
		if ( !COM_FileBase_s( pic->name, name, sizeof( name ) ) )
			Sys_Error( "Draw_CacheGet: Invalid path length: %s\n", pic->name );

		W_CleanupName( name, clean );

		if ( !Draw_CacheLoadFromCustom( clean, wad, raw, rawsize, pic ) )
			return NULL;

		dat = pic->cache.data;
		if ( !dat )
			Sys_Error( "Draw_CacheGet: failed to load %s", pic->name );
	}

	return dat;
}

qboolean Draw_CacheReload( cachewad_t* wad, int index, lumpinfo_t* pLump, cacheentry_t* pic, char* clean, char* path )
{
	int size;
	byte* buf;
	FileHandle_t hFile;

	if ( wad->numpaths > 0 )
		hFile = FS_OpenPathID( wad->name, "rb", wad->basedirs[wad->lumppathindices[index]] );
	else
		hFile = FS_Open( wad->name, "rb" );

	if ( !hFile )
		return FALSE;

	FS_Size( hFile );

	size = pLump->size + wad->cacheExtra + 1;
	buf = (byte*)Cache_Alloc( &pic->cache, size, clean );

	if ( !buf )
		Sys_Error( "Draw_CacheGet: not enough space for %s in %s", path, wad->name );

	buf[pLump->size + wad->cacheExtra] = 0;

	FS_Seek( hFile, pLump->filepos, FILESYSTEM_SEEK_HEAD );
	FS_Read( buf + wad->cacheExtra, pLump->size, 1, hFile );
	FS_Close( hFile );

	if ( wad->pfnCacheBuild )
		wad->pfnCacheBuild( wad, buf );

	return TRUE;
}

void* Draw_CacheGet( cachewad_t* wad, int index )
{
	int i;
	void* dat;
	cacheentry_t* pic;
	lumpinfo_t* pLump;
	char name[WAD_LUMP_NAME_BYTES];
	char clean[WAD_LUMP_NAME_BYTES];

	if ( index >= wad->cacheCount )
		Sys_Error( "Cache wad indexed before load %s: %d", wad->name, index );

	pic = &wad->cache[index];
	dat = Cache_Check( &pic->cache );
	if ( !dat )
	{
		if ( !COM_FileBase_s( pic->name, name, sizeof( name ) ) )
			Sys_Error( "Draw_CacheGet: Invalid path length: %s\n", pic->name );

		W_CleanupName( name, clean );

		for ( i = 0, pLump = wad->lumps; i < wad->lumpCount; i++, pLump++ )
		{
			if ( !Q_strcmp( clean, pLump->name ) )
				break;
		}

		if ( i >= wad->lumpCount )
			return NULL;

		if ( !Draw_CacheReload( wad, i, pLump, pic, clean, pic->name ) )
			return NULL;

		dat = pic->cache.data;
		if ( !dat )
			Sys_Error( "Draw_CacheGet: failed to load %s", pic->name );
	}

	return dat;
}

void Draw_Shutdown()
{
	if ( !m_bDrawInitialized )
		return;

	m_bDrawInitialized = FALSE;

	Draw_FreeWad( menu_wad );
	if ( menu_wad )
		Mem_Free( menu_wad );
	menu_wad = NULL;
}

void Draw_DecalShutdown()
{
	Draw_FreeWad( decal_wad );
	if ( decal_wad )
		Mem_Free( decal_wad );
	decal_wad = NULL;
}

void Draw_DecalSetName( int decal, char* name )
{
	if ( decal < 0 || decal >= MAX_BASE_DECALS )
		return;

	Q_strncpy( decal_names[decal], name, sizeof( decal_names[0] ) - 1 );
	decal_names[decal][sizeof( decal_names[0] ) - 1] = 0;
}

int Draw_DecalIndex( int id )
{
	char tmp[32];
	char* pName;

	pName = decal_names[id];
	if ( !pName[0] )
		Sys_Error( "Used decal #%d without a name\n", id );

	if ( !Host_IsServerActive() && !violence_hblood.value && !Q_strncmp( pName, "{blood", 6 ) )
	{
		snprintf( tmp, sizeof( tmp ), "{yblood%s", pName + 6 );
		pName = tmp;
	}

	return Draw_CacheIndex( decal_wad, pName );
}

int Draw_DecalCount()
{
	if ( !decal_wad )
		return 0;

	return decal_wad->lumpCount;
}

int Draw_DecalSize( int number )
{
	if ( !decal_wad || number >= decal_wad->lumpCount )
		return 0;

	return decal_wad->lumps[number].size;
}

const char* Draw_DecalName( int number )
{
	if ( !decal_wad || number >= decal_wad->lumpCount )
		return NULL;

	return decal_wad->lumps[number].name;
}

// called from cl_parse.cpp
// find the server side decal id given it's name.
// used for save/restore
int Draw_DecalIndexFromName( char* name )
{
	int i;
	char tmpName[DECAL_NAME_BYTES];

	Q_strncpy( tmpName, name, sizeof( tmpName ) - 1 );
	tmpName[sizeof( tmpName ) - 1] = 0;

	if ( tmpName[0] == '}' )
		tmpName[0] = '{';

	for ( i = 0; i < MAX_BASE_DECALS; i++ )
	{
		if ( decal_names[i][0] && !Q_strcmp( tmpName, decal_names[i] ) )
			return i;
	}

	return 0;
}

texture_t* Draw_DecalTexture( int index )
{
	int playernum;
	void* dat;
	customization_t* pCust;

	if ( index >= 0 )
		return (texture_t*)Draw_CacheGet( decal_wad, index );

	playernum = ~index;
	pCust = cl.players[playernum].customdata.pNext;
	if ( !pCust || !pCust->bInUse || !pCust->pInfo || !pCust->pBuffer )
	{
		Sys_Error( "Failed to load custom decal for player #%i:%s using default decal 0.\n", playernum,
			cl.players[playernum].name );
	}

	dat = Draw_CustomCacheGet(
		(cachewad_t*)pCust->pInfo, pCust->pBuffer, pCust->resource.nDownloadSize, pCust->nUserData1 );
	if ( !dat )
		dat = Draw_CacheGet( decal_wad, 0 );

	return (texture_t*)dat;
}

void Decal_ReplaceOrAppendLump( lumplist_t** ppList, lumpinfo_t* lump, qboolean bsecondlump )
{
	lumplist_t* p;

	for ( p = *ppList; p; p = p->next )
	{
		if ( !Q_stricmp( lump->name, p->lump->name ) )
			break;
	}

	if ( p )
	{
		Mem_Free( p->lump );
		p->lump = (lumpinfo_t*)Mem_Malloc( sizeof( *p->lump ) );
		Q_memcpy( p->lump, lump, sizeof( *p->lump ) );
		p->breplaced = bsecondlump;
		return;
	}

	p = (lumplist_t*)Mem_Malloc( sizeof( *p ) );
	Q_memset( p, 0, sizeof( *p ) );
	p->lump = (lumpinfo_t*)Mem_Malloc( sizeof( *p->lump ) );
	Q_memcpy( p->lump, lump, sizeof( *p->lump ) );
	p->breplaced = bsecondlump;
	p->next = *ppList;
	*ppList = p;
}

int Decal_CountLumps( lumplist_t* plist )
{
	int c = 0;

	for ( ; plist; plist = plist->next )
		c++;

	return c;
}

int Decal_SizeLumps( lumplist_t* plist )
{
	return Decal_CountLumps( plist ) * (int)sizeof( lumpinfo_t );
}

void Decal_MergeInDecals( cachewad_t* pwad, const char* pathID )
{
	int i;
	cachewad_t* merged;
	lumplist_t* plist;
	lumplist_t* pnext;
	lumpinfo_t* lump;

	if ( !pwad )
		Sys_Error( "Decal_MergeInDecals called with NULL wad\n" );

	if ( !decal_wad )
	{
		decal_wad = pwad;
		pwad->numpaths = 1;
		decal_wad->basedirs = (char**)Mem_Malloc( sizeof( *decal_wad->basedirs ) );
		decal_wad->basedirs[0] = Mem_Strdup( pathID );
		decal_wad->lumppathindices = (int*)Mem_Malloc( sizeof( int ) * decal_wad->cacheMax );
		Q_memset( decal_wad->lumppathindices, 0, sizeof( int ) * decal_wad->cacheMax );
		return;
	}

	plist = NULL;
	merged = (cachewad_t*)Mem_Malloc( sizeof( *merged ) );
	Q_memset( merged, 0, sizeof( *merged ) );

	for ( i = 0; i < decal_wad->lumpCount; i++ )
		Decal_ReplaceOrAppendLump( &plist, &decal_wad->lumps[i], FALSE );
	for ( i = 0; i < pwad->lumpCount; i++ )
		Decal_ReplaceOrAppendLump( &plist, &pwad->lumps[i], TRUE );

	merged->lumpCount = Decal_CountLumps( plist );
	merged->cacheCount = 0;
	merged->cacheMax = decal_wad->cacheMax;
	merged->name = Mem_Strdup( decal_wad->name );
	Draw_AllocateCacheSpace( merged, merged->cacheMax, 0 );
	merged->pfnCacheBuild = decal_wad->pfnCacheBuild;
	merged->cacheExtra = decal_wad->cacheExtra;
	merged->lumppathindices = (int*)Mem_Malloc( sizeof( int ) * merged->cacheMax );
	Q_memset( merged->lumppathindices, 0, sizeof( int ) * merged->cacheMax );

	merged->numpaths = 2;
	merged->basedirs = (char**)Mem_Malloc( sizeof( char* ) * 2 );
	merged->basedirs[0] = Mem_Strdup( decal_wad->basedirs[0] );
	merged->basedirs[1] = Mem_Strdup( pathID );
	merged->lumps = (lumpinfo_t*)Mem_Malloc( Decal_SizeLumps( plist ) );

	for ( i = 0, lump = merged->lumps; plist; i++, lump++ )
	{
		pnext = plist->next;

		Q_memcpy( lump, plist->lump, sizeof( *lump ) );
		Mem_Free( plist->lump );
		plist->lump = NULL;
		merged->lumppathindices[i] = plist->breplaced ? 1 : 0;
		Mem_Free( plist );

		plist = pnext;
	}

	Draw_FreeWad( decal_wad );
	Mem_Free( decal_wad );
	decal_wad = merged;

	Draw_FreeWad( pwad );
	Mem_Free( pwad );
}

void Decal_Init()
{
	int i;
	int len;
	FileHandle_t hfile;
	cachewad_t* pwad;
	const char* pszPathID[2] = { "DEFAULTGAME", "GAME" };

	Draw_DecalShutdown();

	for ( i = 0; i < 2; i++ )
	{
		hfile = FS_OpenPathID( "decals.wad", "rb", pszPathID[i] );
		if ( !hfile )
		{
			if ( i == 0 )
				Sys_Error( "Couldn't find '%s' in \"%s\" search path\n", "decals.wad", pszPathID[i] );
			continue;
		}

		len = FS_Size( hfile );
		pwad = (cachewad_t*)Mem_Malloc( sizeof( *pwad ) );
		Q_memset( pwad, 0, sizeof( *pwad ) );
		Draw_CacheWadInitFromFile( hfile, len, "decals.wad", MAX_BASE_DECALS, pwad );
		Draw_CacheWadHandler( pwad, Draw_MiptexTexture, MIP_EXTRASIZE );
		Decal_MergeInDecals( pwad, pszPathID[i] );
		FS_Close( hfile );
	}

	sv_decalnamecount = Draw_DecalCount();
	if ( sv_decalnamecount > MAX_BASE_DECALS )
		Sys_Error( "Too many decals: %d / %d\n", sv_decalnamecount, MAX_BASE_DECALS );

	for ( i = 0; i < sv_decalnamecount; i++ )
	{
		Q_memset( &sv_decalnames[i], 0, sizeof( sv_decalnames[i] ) );
		Q_strncpy( sv_decalnames[i].name, Draw_DecalName( i ), sizeof( sv_decalnames[i].name ) - 1 );
		sv_decalnames[i].name[sizeof( sv_decalnames[i].name ) - 1] = 0;
	}
}

#else

#include "draw.h"
#include "wad.h"
#include "common.h"
#include "filesystem.h"
#include "mem.h"
#include "model.h"
#include "gl_model.h"
#include "bspfile.h"
#include "conprint.h"
#include "sys.h"
#include "zone.h"
#include "client.h"
#include "decals.h"
#include "server.h"

#include <cmath>
#include <cstdio>
#include <cstring>

typedef struct lumplist_s
{
	lumpinfo_t* lump;
	qboolean breplaced;
	struct lumplist_s* next;
} lumplist_t;

qboolean gfCustomBuild;
char szCustName[10];

static cachewad_t* decal_wad;
cachewad_t* menu_wad;
qboolean m_bDrawInitialized;
char decal_names[MAX_BASE_DECALS][DECAL_NAME_BYTES];

void Draw_AllocateCacheSpace( cachewad_t* wad, int cacheMax, int tempWad )
{
	wad->cache = (cacheentry_t*)Mem_Malloc( cacheMax * sizeof( cacheentry_t ) );
	Q_memset( wad->cache, 0, cacheMax * sizeof( cacheentry_t ) );
	wad->tempWad = tempWad;
}

void Draw_CacheWadHandler( cachewad_t* wad, PFNCACHE fn, int extraDataSize )
{
	wad->pfnCacheBuild = fn;
	wad->cacheExtra = extraDataSize;
}

int Draw_CacheIndex( cachewad_t* wad, char* path )
{
	int i;
	cacheentry_t* pic;

	for ( i = 0, pic = wad->cache; i < wad->cacheCount; i++, pic++ )
	{
		if ( !Q_strcmp( path, pic->name ) )
			break;
	}

	if ( i == wad->cacheCount )
	{
		if ( wad->cacheCount == wad->cacheMax )
			Sys_Error( "Cache wad (%s) out of %d entries", wad->name, wad->cacheMax );

		wad->cacheCount++;
		Q_strncpy( pic->name, path, sizeof( pic->name ) - 1 );
		pic->name[sizeof( pic->name ) - 1] = 0;
	}

	return i;
}

int Draw_CacheFindIndex( cachewad_t* wad, char* path )
{
	int i;
	cacheentry_t* pic;

	for ( i = 0, pic = wad->cache; i < wad->cacheCount; i++, pic++ )
	{
		if ( !Q_strcmp( path, pic->name ) )
			return i;
	}

	return -1;
}

void Draw_CacheWadInitFromFile( FileHandle_t hFile, int len, char* name, int cacheMax, cachewad_t* wad )
{
	int i;
	wadinfo_t header;
	lumpinfo_t* lump_p;

	FS_Read( &header, sizeof( header ), 1, hFile );
	if ( header.identification[0] != 'W' || header.identification[1] != 'A' || header.identification[2] != 'D'
		|| header.identification[3] != '3' )
		Sys_Error( "Wad file %s doesn't have WAD3 id\n", name );

	wad->lumps = (lumpinfo_t*)Mem_Malloc( len - header.infotableofs );
	FS_Seek( hFile, header.infotableofs, FILESYSTEM_SEEK_HEAD );
	FS_Read( wad->lumps, len - header.infotableofs, 1, hFile );

	for ( i = 0, lump_p = wad->lumps; i < header.numlumps; i++, lump_p++ )
		W_CleanupName( lump_p->name, lump_p->name );

	wad->lumpCount = header.numlumps;
	wad->cacheCount = 0;
	wad->cacheMax = cacheMax;
	wad->name = Mem_Strdup( name );
	Draw_AllocateCacheSpace( wad, cacheMax, 0 );
	wad->pfnCacheBuild = NULL;
	wad->cacheExtra = 0;
}

void Draw_CacheWadInit( char* name, int cacheMax, cachewad_t* wad )
{
	int len;
	FileHandle_t hFile;

	hFile = FS_Open( name, "rb" );
	if ( !hFile )
		Sys_Error( "Draw_LoadWad: Couldn't open %s\n", name );

	len = FS_Size( hFile );
	Draw_CacheWadInitFromFile( hFile, len, name, cacheMax, wad );
	FS_Close( hFile );
}

void Draw_FreeWad( cachewad_t* pWad )
{
	int i;
	cacheentry_t* pic;

	if ( !pWad )
		return;

	if ( pWad->lumps )
		Mem_Free( pWad->lumps );
	pWad->lumps = NULL;

	Mem_Free( pWad->name );

	if ( pWad->numpaths )
	{
		for ( i = 0; i < pWad->numpaths; i++ )
		{
			Mem_Free( pWad->basedirs[i] );
			pWad->basedirs[i] = NULL;
		}
		Mem_Free( pWad->basedirs );
		pWad->basedirs = NULL;
	}

	if ( pWad->lumppathindices )
	{
		Mem_Free( pWad->lumppathindices );
		pWad->lumppathindices = NULL;
	}

	if ( pWad->cache )
	{
		if ( !pWad->tempWad )
		{
			for ( i = 0, pic = pWad->cache; i < pWad->cacheCount; i++, pic++ )
			{
				if ( Cache_Check( &pic->cache ) )
					Cache_Free( &pic->cache );
			}
		}
		Mem_Free( pWad->cache );
	}
}

qboolean Draw_CustomCacheWadInit( int cacheMax, cachewad_t* wad, void* raw, int nFileSize )
{
	wadinfo_t header;
	lumpinfo_t* lump_p;
	int tableEnd, lumpEnd;

	memcpy( &header, raw, sizeof( header ) );

	if ( header.identification[0] != 'W' || header.identification[1] != 'A' || header.identification[2] != 'D'
		|| header.identification[3] != '3' )
	{
		Con_Printf( "Custom file doesn't have WAD3 id\n" );
		return FALSE;
	}

	if ( header.numlumps != 1 )
	{
		Con_Printf( "Custom file has wrong number of lumps %i\n", header.numlumps );
		return FALSE;
	}

	if ( header.infotableofs <= 0 )
	{
		Con_Printf( "Custom file has bogus infotableofs %i\n", header.infotableofs );
		return FALSE;
	}

	tableEnd = (int)( (unsigned int)header.infotableofs + sizeof( lumpinfo_t ) );
	if ( tableEnd != nFileSize )
	{
		Con_Printf( "Custom file has bogus infotableofs ( %i > %i )\n", tableEnd, nFileSize );
		return FALSE;
	}

	wad->lumps = (lumpinfo_t*)Mem_Malloc( sizeof( lumpinfo_t ) );
	Q_memcpy( wad->lumps, (byte*)raw + header.infotableofs, sizeof( lumpinfo_t ) );

	lump_p = wad->lumps;
	W_CleanupName( lump_p->name, lump_p->name );

	if ( lump_p->size != lump_p->disksize )
	{
		Con_Printf( "Custom file has mismatched lump size ( %i vs. %i )\n", lump_p->size, lump_p->disksize );
		return FALSE;
	}

	if ( lump_p->disksize <= 0 )
	{
		Con_Printf( "Custom file has bogus lump size %i\n", lump_p->disksize );
		return FALSE;
	}

	if ( (unsigned int)lump_p->filepos < sizeof( wadinfo_t ) )
	{
		Con_Printf( "Custom file has bogus lump offset %i\n", lump_p->filepos );
		return FALSE;
	}

	lumpEnd = (int)( (unsigned int)lump_p->filepos + (unsigned int)lump_p->disksize );
	if ( lumpEnd > header.infotableofs )
	{
		Con_Printf( "Custom file has bogus lump %i\n", 0 );
		return FALSE;
	}

	wad->lumpCount = 1;
	wad->cacheCount = 0;
	wad->cacheMax = cacheMax;
	wad->name = Mem_Strdup( "tempdecal.wad" );
	Draw_AllocateCacheSpace( wad, cacheMax, 0 );
	wad->pfnCacheBuild = NULL;
	wad->cacheExtra = 0;
	return TRUE;
}

int Draw_CacheByIndex( cachewad_t* wad, int nIndex, int playernum )
{
	int i;
	cacheentry_t* pic;
	char szTestName[32];

	snprintf( szTestName, sizeof( szTestName ), "%03i%02i", playernum, nIndex );

	for ( i = 0, pic = wad->cache; i < wad->cacheCount; i++, pic++ )
	{
		if ( !Q_strcmp( pic->name, szTestName ) )
			break;
	}

	if ( i == wad->cacheCount )
	{
		if ( wad->cacheCount == wad->cacheMax )
			Sys_Error( "Cache wad (%s) out of %d entries", wad->name, wad->cacheMax );

		wad->cacheCount++;
		snprintf( pic->name, sizeof( pic->name ), "%s", szTestName );
	}

	return i;
}

qboolean CustomDecal_Init( cachewad_t* wad, void* raw, int nFileSize, int playernum )
{
	int i;
	qboolean bret;

	bret = Draw_CustomCacheWadInit( 16, wad, raw, nFileSize );
	if ( bret )
	{
		Draw_CacheWadHandler( wad, Draw_MiptexTexture, MIP_EXTRASIZE );
		for ( i = 0; i < wad->lumpCount; i++ )
			Draw_CacheByIndex( wad, i, playernum );
	}

	return bret;
}

qboolean CustomDecal_Validate( void* raw, int nFileSize )
{
	qboolean bret;
	cachewad_t* wad;

	wad = (cachewad_t*)Mem_ZeroMalloc( sizeof( cachewad_t ) );
	if ( !wad )
		return FALSE;

	bret = CustomDecal_Init( wad, raw, nFileSize, -2 );
	if ( bret )
		bret = Draw_CustomCacheGet( wad, raw, nFileSize, 0 ) != NULL;

	Draw_FreeWad( wad );
	Mem_Free( wad );
	return bret;
}

qboolean Draw_ValidateCustomLogo( cachewad_t* wad, byte* data, lumpinfo_t* lump )
{
	int i;
	texture_t tex;
	miptex_t tmp;
	unsigned int pix, size, cachedSize, end;
	unsigned short palsize;

	if ( wad->cacheExtra != MIP_EXTRASIZE )
	{
		Con_Printf( "Draw_ValidateCustomLogo: Bad cached wad %s\n", wad->name );
		return FALSE;
	}

	memcpy( &tmp, data + MIP_EXTRASIZE, sizeof( tmp ) );
	memcpy( &tex, data, sizeof( tex ) );

	Q_memcpy( tex.name, tmp.name, sizeof( tex.name ) );
	tex.width = LittleLong( tmp.width );
	tex.height = LittleLong( tmp.height );
	tex.anim_total = 0;
	tex.anim_min = 0;
	tex.anim_max = 0;
	tex.anim_next = NULL;
	tex.alternate_anims = NULL;

	for ( i = 0; i < MIPLEVELS; i++ )
		tex.offsets[i] = (unsigned int)LittleLong( tmp.offsets[i] ) + wad->cacheExtra;

	if ( !tex.width || !tex.height || tex.width > 256 || tex.height > 256 )
	{
		Con_Printf( "Draw_ValidateCustomLogo: Bad cached wad tex size %ux%u on %s\n", tex.width, tex.height, wad->name );
		return FALSE;
	}

	pix = tex.width * tex.height;
	size = pix + ( pix >> 2 ) + ( pix >> 4 ) + ( pix >> 6 );

	cachedSize = (unsigned int)lump->size + wad->cacheExtra;
	if ( size + sizeof( miptex_t ) >= cachedSize )
	{
		Con_Printf( "Draw_ValidateCustomLogo: Bad cached wad size %i/%i on %s\n", (int)( size + sizeof( miptex_t ) ),
			(int)cachedSize, wad->name );
	}

	memcpy( &palsize, data + MIP_EXTRASIZE + size + sizeof( miptex_t ), sizeof( palsize ) );

	for ( i = 0; i < MIPLEVELS - 1; i++ )
	{
		if ( tmp.offsets[i] + pix != tmp.offsets[i + 1] )
		{
			Con_Printf( "Draw_ValidateCustomLogo: Bad cached wad %s\n", wad->name );
			return FALSE;
		}
		pix >>= 2;
	}

	if ( palsize > 256 )
	{
		Con_Printf( "Draw_ValidateCustomLogo: Bad cached wad palette size %i on %s\n", palsize, wad->name );
		return FALSE;
	}

	end = (unsigned int)LittleLong( tmp.offsets[0] );
	end += size + 3 * palsize + sizeof( palsize );
	if ( end > (unsigned int)lump->disksize )
	{
		Con_Printf( "Draw_ValidateCustomLogo: Bad cached wad %s\n", wad->name );
		return FALSE;
	}

	return TRUE;
}

qboolean Draw_CacheLoadFromCustom( char* clean, cachewad_t* wad, void* raw, int, cacheentry_t* pic )
{
	int idx;
	byte* buf;
	lumpinfo_t* pLump;

	idx = 0;
	if ( Q_strlen( clean ) >= 5 )
	{
		idx = Q_atoi( clean + 3 );
		if ( idx < 0 || idx >= wad->lumpCount )
			return FALSE;
	}

	pLump = &wad->lumps[idx];
	buf = (byte*)Cache_Alloc( &pic->cache, pLump->size + wad->cacheExtra + 1, clean );
	if ( !buf )
		Sys_Error( "Draw_CacheGet: not enough space for %s in %s", clean, wad->name );

	buf[pLump->size + wad->cacheExtra] = 0;
	Q_memcpy( buf + wad->cacheExtra, (byte*)raw + pLump->filepos, pLump->size );

	if ( !Draw_ValidateCustomLogo( wad, buf, pLump ) )
		return FALSE;

	gfCustomBuild = TRUE;
	Q_strcpy( szCustName, "T" );
	Q_memcpy( szCustName + 1, clean, 5 );
	szCustName[6] = 0;

	if ( wad->pfnCacheBuild )
		wad->pfnCacheBuild( wad, buf );

	gfCustomBuild = FALSE;
	return TRUE;
}

void* Draw_CustomCacheGet( cachewad_t* wad, void* raw, int rawsize, int index )
{
	cacheentry_t* pic;
	void* dat;
	char name[WAD_LUMP_NAME_BYTES];
	char clean[WAD_LUMP_NAME_BYTES];

	if ( index >= wad->cacheCount )
		Sys_Error( "Cache wad indexed before load %s: %d", wad->name, index );

	pic = &wad->cache[index];
	dat = Cache_Check( &pic->cache );
	if ( !dat )
	{
		if ( !COM_FileBase_s( pic->name, name, sizeof( name ) ) )
			Sys_Error( "Draw_CacheGet: Invalid path length: %s\n", pic->name );

		W_CleanupName( name, clean );

		if ( !Draw_CacheLoadFromCustom( clean, wad, raw, rawsize, pic ) )
			return NULL;

		dat = pic->cache.data;
		if ( !dat )
			Sys_Error( "Draw_CacheGet: failed to load %s", pic->name );
	}

	return dat;
}

qboolean Draw_CacheReload( cachewad_t* wad, int index, lumpinfo_t* pLump, cacheentry_t* pic, char* clean, char* path )
{
	int size;
	byte* buf;
	FileHandle_t hFile;

	if ( wad->numpaths > 0 )
		hFile = FS_OpenPathID( wad->name, "rb", wad->basedirs[wad->lumppathindices[index]] );
	else
		hFile = FS_Open( wad->name, "rb" );

	if ( !hFile )
		return FALSE;

	FS_Size( hFile );

	size = pLump->size + wad->cacheExtra + 1;
	if ( wad->tempWad )
	{
		buf = (byte*)Hunk_TempAlloc( size );
		pic->cache.data = buf;
	}
	else
	{
		buf = (byte*)Cache_Alloc( &pic->cache, size, clean );
	}

	if ( !buf )
		Sys_Error( "Draw_CacheGet: not enough space for %s in %s", path, wad->name );

	buf[pLump->size + wad->cacheExtra] = 0;

	FS_Seek( hFile, pLump->filepos, FILESYSTEM_SEEK_HEAD );
	FS_Read( buf + wad->cacheExtra, pLump->size, 1, hFile );
	FS_Close( hFile );

	if ( wad->pfnCacheBuild )
		wad->pfnCacheBuild( wad, buf );

	return TRUE;
}

void* Draw_CacheGet( cachewad_t* wad, int index )
{
	int i;
	void* dat;
	cacheentry_t* pic;
	lumpinfo_t* pLump;
	char name[WAD_LUMP_NAME_BYTES];
	char clean[WAD_LUMP_NAME_BYTES];

	if ( index >= wad->cacheCount )
		Sys_Error( "Cache wad indexed before load %s: %d", wad->name, index );

	pic = &wad->cache[index];
	dat = NULL;
	if ( wad->tempWad || !( dat = Cache_Check( &pic->cache ) ) )
	{
		if ( !COM_FileBase_s( pic->name, name, sizeof( name ) ) )
			Sys_Error( "Draw_CacheGet: Invalid path length: %s\n", pic->name );

		W_CleanupName( name, clean );

		for ( i = 0, pLump = wad->lumps; i < wad->lumpCount; i++, pLump++ )
		{
			if ( !Q_strcmp( clean, pLump->name ) )
				break;
		}

		if ( i >= wad->lumpCount )
			return NULL;

		if ( !Draw_CacheReload( wad, i, pLump, pic, clean, pic->name ) )
			return NULL;

		dat = pic->cache.data;
		if ( !dat )
			Sys_Error( "Draw_CacheGet: failed to load %s", pic->name );
	}

	return dat;
}

void Draw_Shutdown()
{
	if ( !m_bDrawInitialized )
		return;

	m_bDrawInitialized = FALSE;

	Draw_FreeWad( menu_wad );
	if ( menu_wad )
		Mem_Free( menu_wad );
	menu_wad = NULL;
}

void Draw_DecalShutdown()
{
	Draw_FreeWad( decal_wad );
	if ( decal_wad )
		Mem_Free( decal_wad );
	decal_wad = NULL;
}

void Draw_DecalSetName( int decal, char* name )
{
	if ( decal < 0 || decal >= MAX_BASE_DECALS )
		return;

	Q_strncpy( decal_names[decal], name, sizeof( decal_names[0] ) - 1 );
	decal_names[decal][sizeof( decal_names[0] ) - 1] = 0;
}

int Draw_DecalIndex( int id )
{
	char tmp[32];
	char* pName;

	pName = decal_names[id];
	if ( !pName[0] )
		Sys_Error( "Used decal #%d without a name\n", id );

	if ( !Host_IsServerActive() && !violence_hblood.value && !Q_strncmp( pName, "{blood", 6 ) )
	{
		snprintf( tmp, sizeof( tmp ), "{yblood%s", pName + 6 );
		pName = tmp;
	}

	return Draw_CacheIndex( decal_wad, pName );
}

int Draw_DecalCount()
{
	if ( !decal_wad )
		return 0;

	return decal_wad->lumpCount;
}

int Draw_DecalSize( int number )
{
	if ( !decal_wad || number >= decal_wad->lumpCount )
		return 0;

	return decal_wad->lumps[number].size;
}

const char* Draw_DecalName( int number )
{
	if ( !decal_wad || number >= decal_wad->lumpCount )
		return NULL;

	return decal_wad->lumps[number].name;
}

// called from cl_parse.cpp
// find the server side decal id given it's name.
// used for save/restore
int Draw_DecalIndexFromName( char* name )
{
	int i;
	char tmpName[DECAL_NAME_BYTES];

	Q_strncpy( tmpName, name, sizeof( tmpName ) - 1 );
	tmpName[sizeof( tmpName ) - 1] = 0;

	if ( tmpName[0] == '}' )
		tmpName[0] = '{';

	for ( i = 0; i < MAX_BASE_DECALS; i++ )
	{
		if ( decal_names[i][0] && !Q_strcmp( tmpName, decal_names[i] ) )
			return i;
	}

	return 0;
}

texture_t* Draw_DecalTexture( int index )
{
	int playernum;
	void* dat;
	customization_t* pCust;

	if ( index >= 0 )
		return (texture_t*)Draw_CacheGet( decal_wad, index );

	playernum = ~index;
	pCust = cl.players[playernum].customdata.pNext;
	if ( !pCust || !pCust->bInUse || !pCust->pInfo || !pCust->pBuffer )
	{
		Sys_Error( "Failed to load custom decal for player #%i:%s using default decal 0.\n", playernum,
			cl.players[playernum].name );
	}

	dat = Draw_CustomCacheGet(
		(cachewad_t*)pCust->pInfo, pCust->pBuffer, pCust->resource.nDownloadSize, pCust->nUserData1 );
	if ( !dat )
		dat = Draw_CacheGet( decal_wad, 0 );

	return (texture_t*)dat;
}

void Decal_ReplaceOrAppendLump( lumplist_t** ppList, lumpinfo_t* lump, qboolean bsecondlump )
{
	lumplist_t* p;

	for ( p = *ppList; p; p = p->next )
	{
		if ( !Q_strcasecmp( lump->name, p->lump->name ) )
			break;
	}

	if ( p )
	{
		Mem_Free( p->lump );
		p->lump = (lumpinfo_t*)Mem_Malloc( sizeof( lumpinfo_t ) );
		Q_memcpy( p->lump, lump, sizeof( lumpinfo_t ) );
		p->breplaced = bsecondlump;
		return;
	}

	p = (lumplist_t*)Mem_Malloc( sizeof( lumplist_t ) );
	Q_memset( p, 0, sizeof( lumplist_t ) );
	p->lump = (lumpinfo_t*)Mem_Malloc( sizeof( lumpinfo_t ) );
	Q_memcpy( p->lump, lump, sizeof( lumpinfo_t ) );
	p->breplaced = bsecondlump;
	p->next = *ppList;
	*ppList = p;
}

int Decal_CountLumps( lumplist_t* plist )
{
	int c = 0;

	for ( ; plist; plist = plist->next )
		c++;

	return c;
}

int Decal_SizeLumps( lumplist_t* plist )
{
	return Decal_CountLumps( plist ) * (int)sizeof( lumpinfo_t );
}

void Decal_MergeInDecals( cachewad_t* pwad, const char* pathID )
{
	int i;
	cachewad_t* merged;
	lumplist_t* plist;
	lumplist_t* pnext;
	lumpinfo_t* lump;

	if ( !pwad )
		Sys_Error( "Decal_MergeInDecals called with NULL wad\n" );

	if ( !decal_wad )
	{
		decal_wad = pwad;
		pwad->numpaths = 1;
		decal_wad->basedirs = (char**)Mem_Malloc( sizeof( char* ) );
		decal_wad->basedirs[0] = Mem_Strdup( pathID );
		decal_wad->lumppathindices = (int*)Mem_Malloc( sizeof( int ) * decal_wad->cacheMax );
		Q_memset( decal_wad->lumppathindices, 0, sizeof( int ) * decal_wad->cacheMax );
		return;
	}

	plist = NULL;
	merged = (cachewad_t*)Mem_Malloc( sizeof( cachewad_t ) );
	Q_memset( merged, 0, sizeof( cachewad_t ) );

	for ( i = 0; i < decal_wad->lumpCount; i++ )
		Decal_ReplaceOrAppendLump( &plist, &decal_wad->lumps[i], FALSE );
	for ( i = 0; i < pwad->lumpCount; i++ )
		Decal_ReplaceOrAppendLump( &plist, &pwad->lumps[i], TRUE );

	merged->lumpCount = Decal_CountLumps( plist );
	merged->cacheCount = 0;
	merged->cacheMax = decal_wad->cacheMax;
	merged->name = Mem_Strdup( decal_wad->name );
	Draw_AllocateCacheSpace( merged, merged->cacheMax, 0 );
	merged->pfnCacheBuild = decal_wad->pfnCacheBuild;
	merged->cacheExtra = decal_wad->cacheExtra;
	merged->lumppathindices = (int*)Mem_Malloc( sizeof( int ) * merged->cacheMax );
	Q_memset( merged->lumppathindices, 0, sizeof( int ) * merged->cacheMax );

	merged->numpaths = 2;
	merged->basedirs = (char**)Mem_Malloc( sizeof( char* ) * 2 );
	merged->basedirs[0] = Mem_Strdup( decal_wad->basedirs[0] );
	merged->basedirs[1] = Mem_Strdup( pathID );
	merged->lumps = (lumpinfo_t*)Mem_Malloc( Decal_SizeLumps( plist ) );

	for ( i = 0, lump = merged->lumps; plist; i++, lump++ )
	{
		pnext = plist->next;

		Q_memcpy( lump, plist->lump, sizeof( lumpinfo_t ) );
		Mem_Free( plist->lump );
		plist->lump = NULL;
		merged->lumppathindices[i] = plist->breplaced ? 1 : 0;
		Mem_Free( plist );

		plist = pnext;
	}

	Draw_FreeWad( decal_wad );
	Mem_Free( decal_wad );
	decal_wad = merged;

	Draw_FreeWad( pwad );
	Mem_Free( pwad );
}

void Decal_Init()
{
	int i;
	int len;
	FileHandle_t hfile;
	cachewad_t* pwad;
	const char* pszPathID[2] = { "DEFAULTGAME", "GAME" };

	Draw_DecalShutdown();

	for ( i = 0; i < 2; i++ )
	{
		hfile = FS_OpenPathID( "decals.wad", "rb", pszPathID[i] );
		if ( !hfile )
		{
			if ( i == 0 )
				Sys_Error( "Couldn't find '%s' in \"%s\" search path\n", "decals.wad", pszPathID[i] );
			continue;
		}

		len = FS_Size( hfile );
		pwad = (cachewad_t*)Mem_Malloc( sizeof( cachewad_t ) );
		Q_memset( pwad, 0, sizeof( cachewad_t ) );
		Draw_CacheWadInitFromFile( hfile, len, "decals.wad", MAX_BASE_DECALS, pwad );
		Draw_CacheWadHandler( pwad, Draw_MiptexTexture, MIP_EXTRASIZE );
		Decal_MergeInDecals( pwad, pszPathID[i] );
		FS_Close( hfile );
	}

	sv_decalnamecount = Draw_DecalCount();
	if ( sv_decalnamecount > MAX_BASE_DECALS )
		Sys_Error( "Too many decals: %d / %d\n", sv_decalnamecount, MAX_BASE_DECALS );

	for ( i = 0; i < sv_decalnamecount; i++ )
	{
		Q_memset( &sv_decalnames[i], 0, sizeof( decalname_t ) );
		Q_strncpy( sv_decalnames[i].name, Draw_DecalName( i ), sizeof( sv_decalnames[i].name ) - 1 );
		sv_decalnames[i].name[sizeof( sv_decalnames[i].name ) - 1] = 0;
	}
}

int R_DecalUnProject( decal_t* decal, vec_t* position )
{
	int entityIndex;
	float s, t, scale, len;
	float inverseScale;
	mtexinfo_t* tex;
	texture_t* ptexture;
	edict_t* ent;
	model_t* model;
	vec3_t forward, right, up, temp;

	if ( !decal || !decal->psurface )
		return -1;

	tex = decal->psurface->texinfo;
	s = decal->dx * tex->texture->width - decal->psurface->texturemins[0];
	t = decal->dy * tex->texture->height - decal->psurface->texturemins[1];
	scale = (float)( Length( tex->vecs[0] ) * 0.5 );

	ptexture = Draw_DecalTexture( decal->texture );
	s = ptexture->width * scale + s + decal->psurface->texturemins[0] - tex->vecs[0][3];
	t = ptexture->height * scale + t + decal->psurface->texturemins[1] - tex->vecs[1][3];

	inverseScale = 0;
	len = fabsf( Length( tex->vecs[0] ) );
	if ( len != 0 )
	{
		len = (float)( 1.0 / len );
		inverseScale = len * len;
	}

	VectorScale( tex->vecs[0], inverseScale * s, position );
	VectorMA( position, inverseScale * t, tex->vecs[1], position );
	VectorMA( position, decal->psurface->plane->dist, decal->psurface->plane->normal, position );

	entityIndex = decal->entityIndex;
	if ( !entityIndex )
		return 0;

	ent = &sv.edicts[entityIndex];
	if ( !ent->v.modelindex )
		return 0;

	model = sv.models[ent->v.modelindex];
	if ( !model || model->type != mod_brush )
		return 0;

	if ( ent->v.angles[0] || ent->v.angles[1] || ent->v.angles[2] )
	{
		AngleVectorsTranspose( ent->v.angles, forward, right, up );
		VectorCopy( position, temp );
		position[0] = DotProduct( temp, forward );
		position[1] = DotProduct( temp, right );
		position[2] = DotProduct( temp, up );
	}

	if ( model != sv.worldmodel )
	{
		VectorAdd( position, model->hulls[0].clip_mins, position );
		VectorAdd( position, ent->v.origin, position );
	}

	return entityIndex;
}

#endif
