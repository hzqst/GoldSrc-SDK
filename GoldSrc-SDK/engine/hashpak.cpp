#include "hashpak.h"
#include "common.h"
#include "conprint.h"
#include "crc.h"
#include "mem.h"
#include "sys.h"
#include "cmd.h"
#include "cvar.h"

#include <cstdio>
#include <cstring>

#define HASHPAK_VERSION 1

#define MAX_HASHPAK_FILE_ENTRIES 32768
#define MAX_HASHPAK_LUMP_SIZE ( 128 * 1024 )

typedef struct hash_pack_entry_s
{
	resource_t resource;
	int nOffset;
	int nFileLength;
} hash_pack_entry_t;

typedef struct hash_pack_directory_s
{
	int nEntries;
	hash_pack_entry_t* p_rgEntries;
} hash_pack_directory_t;

typedef struct hash_pack_header_s
{
	char szFileStamp[4];
	int version;
	int nDirectoryOffset;
} hash_pack_header_t;

typedef struct hash_pack_queue_s
{
	char* pakname;
	resource_t resource;
	int datasize;
	void* data;
	hash_pack_queue_s* next;
} hash_pack_queue_t;

hash_pack_queue_t* gp_hpak_queue;
hash_pack_directory_t hash_pack_dir;
hash_pack_header_t hash_pack_header;

extern cvar_t hpk_maxsize;

static const char* HPAK_ResourceTypeName( resourcetype_t type )
{
	switch ( type )
	{
	case t_sound:
		return "sound";
	case t_skin:
		return "skin";
	case t_model:
		return "model";
	case t_decal:
		return "decal";
	case t_generic:
		return "generic";
	case t_eventscript:
		return "event";
	default:
		return "?";
	}
}

static void HPAK_HashLump( resource_t* pResource, void* pData, FileHandle_t fpSource, byte* digest )
{
	MD5Context_t ctx;
	byte* pbuf;
	int curpos;

	Q_memset( &ctx, 0, sizeof( ctx ) );
	MD5Init( &ctx );

	if ( pData )
	{
		MD5Update( &ctx, (byte*)pData, pResource->nDownloadSize );
	}
	else
	{
		curpos = FS_Tell( fpSource );
		pbuf = (byte*)Mem_Malloc( pResource->nDownloadSize + 1 );
		Q_memset( pbuf, 0, pResource->nDownloadSize );
		FS_Read( pbuf, pResource->nDownloadSize, 1, fpSource );
		FS_Seek( fpSource, curpos, FILESYSTEM_SEEK_HEAD );
		MD5Update( &ctx, pbuf, pResource->nDownloadSize );
		Mem_Free( pbuf );
	}

	MD5Final( digest, &ctx );
}

static bool HPAK_ReadDirectoryHeader(
	FileHandle_t fp, const char* name, const char* versionError, hash_pack_directory_t* directory )
{
	hash_pack_header_t header;

	FS_Read( &header, sizeof( header ), 1, fp );

	if ( Q_strncmp( header.szFileStamp, "HPAK", sizeof( header.szFileStamp ) ) )
	{
		Con_Printf( "%s is not an HPAK file\n", name );
		return false;
	}

	if ( header.version != HASHPAK_VERSION )
	{
		Con_Printf( "%s", versionError );
		return false;
	}

	// Now read in the directory structure.
	FS_Seek( fp, header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &directory->nEntries, sizeof( directory->nEntries ), 1, fp );

	if ( directory->nEntries < 1 || directory->nEntries > MAX_HASHPAK_FILE_ENTRIES )
	{
		Con_Printf( "ERROR: HPAK had bogus # of directory entries:  %i\n", directory->nEntries );
		return false;
	}

	return true;
}

qboolean HPAK_FindResource( hash_pack_directory_t* pDir, byte* hash, resource_t* pResourceEntry )
{
	int i;

	for ( i = 0; i < pDir->nEntries; i++ )
	{
		if ( !Q_memcmp( hash, pDir->p_rgEntries[i].resource.rgucMD5_hash, MD5_DIGEST_BYTES ) )
		{
			if ( pResourceEntry )
				memcpy( pResourceEntry, &pDir->p_rgEntries[i].resource, sizeof( resource_t ) );
			return TRUE;
		}
	}

	return FALSE;
}

void HPAK_AddToQueue( const char* pakname, resource_t* pResource, void* pData, FileHandle_t fpSource )
{
	hash_pack_queue_t* n;

	n = (hash_pack_queue_t*)Mem_Malloc( sizeof( hash_pack_queue_t ) );
	if ( !n )
		Sys_Error( "Unable to allocate %i bytes for hpak queue!", (int)sizeof( hash_pack_queue_t ) );

	Q_memset( n, 0, sizeof( hash_pack_queue_t ) );
	n->pakname = Mem_Strdup( pakname );
	memcpy( &n->resource, pResource, sizeof( resource_t ) );
	n->datasize = pResource->nDownloadSize;
	n->data = Mem_Malloc( n->datasize );
	if ( !n->data )
		Sys_Error( "Unable to allocate %i bytes for hpak queue!", n->datasize );

	if ( pData )
	{
		Q_memcpy( n->data, pData, n->datasize );
	}
	else
	{
		if ( !fpSource )
			Sys_Error( "Add to Queue called without data or file pointer!" );
		FS_Read( n->data, n->datasize, 1, fpSource );
	}

	n->next = gp_hpak_queue;
	gp_hpak_queue = n;
}

void HPAK_CreatePak( const char* pakname, resource_t* pResource, void* pData, FileHandle_t fpSource )
{
	char name[MAX_PATH];
	byte md5[MD5_DIGEST_BYTES];
	int curpos;
	FileHandle_t fp;
	hash_pack_entry_t* pCurrentEntry;
	int i;

	if ( ( pData && fpSource ) || ( !pData && !fpSource ) )
	{
		Con_Printf( "HPAK_CreatePak, must specify one of pData or fpSource\n" );
		return;
	}

	snprintf( name, 256, "%s", pakname );
	COM_DefaultExtension( name, ".hpk" );

	Con_Printf( "Creating HPAK %s.\n", name );
	fp = FS_Open( name, "wb" );
	if ( !fp )
	{
		Con_Printf( "ERROR: couldn't open new .hpk, check access rights to %s.\n", name );
		return;
	}

	HPAK_HashLump( pResource, pData, fpSource, md5 );
	if ( Q_memcmp( pResource->rgucMD5_hash, md5, sizeof( md5 ) ) )
	{
		Con_Printf( "HPAK_CreatePak called with bogus lump, md5 mismatch\n" );
		Con_Printf( "Purported:  %s\n", MD5_Print( pResource->rgucMD5_hash ) );
		Con_Printf( "Actual   :  %s\n", MD5_Print( md5 ) );
		Con_Printf( "Ignoring lump addition\n" );
		return;
	}

	Q_memset( &hash_pack_header, 0, sizeof( hash_pack_header ) );
	Q_strncpy( hash_pack_header.szFileStamp, "HPAK", sizeof( hash_pack_header.szFileStamp ) );
	hash_pack_header.version = HASHPAK_VERSION;
	hash_pack_header.nDirectoryOffset = 0; // Temporary.  We rewrite the structure at the end.

	FS_Write( &hash_pack_header, sizeof( hash_pack_header ), 1, fp );

	Q_memset( &hash_pack_dir, 0, sizeof( hash_pack_dir ) );
	hash_pack_dir.nEntries = 1;
	hash_pack_dir.p_rgEntries = (hash_pack_entry_t*)Mem_Malloc( sizeof( hash_pack_entry_t ) );
	Q_memset( hash_pack_dir.p_rgEntries, 0, sizeof( hash_pack_entry_t ) * hash_pack_dir.nEntries );

	pCurrentEntry = hash_pack_dir.p_rgEntries;
	memcpy( &pCurrentEntry->resource, pResource, sizeof( resource_t ) );
	pCurrentEntry->nOffset = FS_Tell( fp );
	pCurrentEntry->nFileLength = pResource->nDownloadSize;

	if ( pData )
		FS_Write( pData, pResource->nDownloadSize, 1, fp );
	else
		COM_CopyFileChunk( fp, fpSource, pResource->nDownloadSize );

	curpos = FS_Tell( fp );

	FS_Write( &hash_pack_dir.nEntries, sizeof( hash_pack_dir.nEntries ), 1, fp );
	for ( i = 0; i < hash_pack_dir.nEntries; i++ )
		FS_Write( &hash_pack_dir.p_rgEntries[i], sizeof( hash_pack_entry_t ), 1, fp );

	if ( hash_pack_dir.p_rgEntries )
		Mem_Free( hash_pack_dir.p_rgEntries );
	hash_pack_dir.p_rgEntries = NULL;
	hash_pack_dir.nEntries = 0;

	hash_pack_header.nDirectoryOffset = curpos;

	FS_Seek( fp, 0, FILESYSTEM_SEEK_HEAD );
	FS_Write( &hash_pack_header, sizeof( hash_pack_header ), 1, fp );
	FS_Close( fp );
}

void HPAK_AddLump( qboolean bUseQueue, const char* pakname, resource_t* pResource, void* pData, FileHandle_t fpSource )
{
	int i, j;
	FileHandle_t iRead, iWrite;
	char name[MAX_PATH];
	char szTempName[MAX_PATH];
	char szOriginalName[256];
	byte md5[MD5_DIGEST_BYTES];
	byte* hash;
	hash_pack_directory_t olddirectory, newdirectory;
	hash_pack_entry_t* pNewEntry;

	if ( !pakname )
	{
		Con_Printf( "HPAK_AddLump called with invalid arguments:  no .pak filename\n" );
		return;
	}

	if ( !pResource )
	{
		Con_Printf( "HPAK_AddLump called with invalid arguments:  no lump to add\n" );
		return;
	}

	if ( !pData && !fpSource )
	{
		Con_Printf( "HPAK_AddLump called with invalid arguments:  no file handle\n" );
		return;
	}

	if ( pResource->nDownloadSize < 1024 || pResource->nDownloadSize > MAX_HASHPAK_LUMP_SIZE )
	{
		Con_Printf( "HPAK_AddLump called with bogus lump, size:  %i\n", pResource->nDownloadSize );
		return;
	}

	HPAK_HashLump( pResource, pData, fpSource, md5 );
	hash = pResource->rgucMD5_hash;
	if ( Q_memcmp( hash, md5, sizeof( md5 ) ) )
	{
		Con_Printf( "HPAK_AddLump called with bogus lump, md5 mismatch\n" );
		Con_Printf( "Purported:  %s\n", MD5_Print( hash ) );
		Con_Printf( "Actual   :  %s\n", MD5_Print( md5 ) );
		Con_Printf( "Ignoring lump addition\n" );
		return;
	}

	if ( bUseQueue )
	{
		HPAK_AddToQueue( pakname, pResource, pData, fpSource );
		return;
	}

	snprintf( name, 256, "%s", pakname );
	COM_DefaultExtension( name, ".hpk" );
	COM_FixSlashes( name );

	Q_strncpy( szOriginalName, name, sizeof( szOriginalName ) - 1 );
	szOriginalName[sizeof( szOriginalName ) - 1] = 0;

	iRead = FS_Open( name, "rb" );
	if ( !iRead )
	{
		HPAK_CreatePak( pakname, pResource, pData, fpSource );
		return;
	}

	COM_StripExtension( name, szTempName );
	COM_DefaultExtension( szTempName, ".hp2" );

	iWrite = FS_Open( szTempName, "w+b" );
	if ( !iWrite )
	{
		FS_Close( iRead );
		Con_Printf( "ERROR: couldn't open %s.\n", name );
		return;
	}

	FS_Read( &hash_pack_header, sizeof( hash_pack_header ), 1, iRead );

	if ( hash_pack_header.version != HASHPAK_VERSION )
	{
		FS_Close( iRead );
		FS_Close( iWrite );
		FS_Unlink( szTempName );
		Con_Printf( "Invalid .hpk version in HPAK_AddLump\n" );
		return;
	}

	FS_Seek( iRead, 0, FILESYSTEM_SEEK_HEAD );
	COM_CopyFileChunk( iWrite, iRead, FS_Size( iRead ) );

	// Now read in the directory structure.
	FS_Seek( iRead, hash_pack_header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &olddirectory.nEntries, sizeof( olddirectory.nEntries ), 1, iRead );

	if ( olddirectory.nEntries < 0 || olddirectory.nEntries > MAX_HASHPAK_FILE_ENTRIES )
	{
		Con_Printf( "ERROR: .hpk had bogus # of directory entries:  %i\n", olddirectory.nEntries );
		FS_Close( iRead );
		FS_Close( iWrite );
		FS_Unlink( szTempName );
		return;
	}

	olddirectory.p_rgEntries =
		(hash_pack_entry_t*)Mem_Malloc( sizeof( hash_pack_entry_t ) * ( olddirectory.nEntries + 1 ) );
	FS_Read( olddirectory.p_rgEntries, sizeof( hash_pack_entry_t ) * olddirectory.nEntries, 1, iRead );

	FS_Close( iRead );

	if ( HPAK_FindResource( &olddirectory, hash, NULL ) )
	{
		FS_Close( iWrite );
		FS_Unlink( szTempName );
		Mem_Free( olddirectory.p_rgEntries );
		return;
	}

	newdirectory.nEntries = olddirectory.nEntries + 1;
	newdirectory.p_rgEntries = (hash_pack_entry_t*)Mem_Malloc( sizeof( hash_pack_entry_t ) * newdirectory.nEntries );

	Q_memset( newdirectory.p_rgEntries, 0, sizeof( hash_pack_entry_t ) * newdirectory.nEntries );
	Q_memcpy( newdirectory.p_rgEntries, olddirectory.p_rgEntries, sizeof( hash_pack_entry_t ) * olddirectory.nEntries );

	for ( i = 0; i < olddirectory.nEntries; i++ )
	{
		if ( Q_memcmp( hash, olddirectory.p_rgEntries[i].resource.rgucMD5_hash, MD5_DIGEST_BYTES ) < 0 )
			break;
	}

	for ( j = i; j < olddirectory.nEntries; j++ )
		memcpy( &newdirectory.p_rgEntries[j + 1], &olddirectory.p_rgEntries[j], sizeof( hash_pack_entry_t ) );

	pNewEntry = &newdirectory.p_rgEntries[i];
	Q_memset( pNewEntry, 0, sizeof( hash_pack_entry_t ) );

	FS_Seek( iWrite, hash_pack_header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );

	memcpy( &pNewEntry->resource, pResource, sizeof( resource_t ) );
	pNewEntry->nOffset = FS_Tell( iWrite );
	pNewEntry->nFileLength = pResource->nDownloadSize;

	if ( pData )
		FS_Write( pData, pResource->nDownloadSize, 1, iWrite );
	else
		COM_CopyFileChunk( iWrite, fpSource, pResource->nDownloadSize );

	hash_pack_header.nDirectoryOffset = FS_Tell( iWrite );

	FS_Write( &newdirectory.nEntries, sizeof( newdirectory.nEntries ), 1, iWrite );
	for ( i = 0; i < newdirectory.nEntries; i++ )
		FS_Write( &newdirectory.p_rgEntries[i], sizeof( hash_pack_entry_t ), 1, iWrite );

	if ( newdirectory.p_rgEntries )
		Mem_Free( newdirectory.p_rgEntries );
	if ( olddirectory.p_rgEntries )
		Mem_Free( olddirectory.p_rgEntries );

	FS_Seek( iWrite, 0, FILESYSTEM_SEEK_HEAD );
	FS_Write( &hash_pack_header, sizeof( hash_pack_header ), 1, iWrite );
	FS_Close( iWrite );

	FS_Unlink( szOriginalName );
	FS_Rename( szTempName, szOriginalName );
}

void HPAK_FlushHostQueue()
{
	hash_pack_queue_t* p;

	while ( gp_hpak_queue )
	{
		p = gp_hpak_queue;
		gp_hpak_queue = p->next;

		HPAK_AddLump( FALSE, p->pakname, &p->resource, p->data, NULL );

		Mem_Free( p->pakname );
		Mem_Free( p->data );
		Mem_Free( p );
	}
}

qboolean HPAK_GetDataPointer( const char* pakname, resource_t* pResource, byte** pbuffer, int* bufsize )
{
	qboolean retval = FALSE;
	FileHandle_t fp;
	hash_pack_directory_t directory;
	hash_pack_entry_t* entry;
	hash_pack_queue_t* p;
	char name[MAX_PATH];
	byte* pbuf;
	int i;

	if ( pbuffer )
		*pbuffer = NULL;
	if ( bufsize )
		*bufsize = 0;

	for ( p = gp_hpak_queue; p; p = p->next )
	{
		if ( Q_strcasecmp( p->pakname, pakname )
			 || Q_memcmp( p->resource.rgucMD5_hash, pResource->rgucMD5_hash, MD5_DIGEST_BYTES ) )
			continue;

		if ( pbuffer )
		{
			pbuf = (byte*)Mem_Malloc( p->datasize );
			if ( !pbuf )
				Sys_Error( "Error allocating %i bytes for hpak!", p->datasize );
			Q_memcpy( pbuf, p->data, p->datasize );
			*pbuffer = pbuf;
		}

		if ( bufsize )
			*bufsize = p->datasize;

		return TRUE;
	}

	snprintf( name, 256, "%s", pakname );
	COM_DefaultExtension( name, ".hpk" );

	fp = FS_Open( name, "rb" );
	if ( !fp )
		return FALSE;

	if ( !HPAK_ReadDirectoryHeader( fp, name, "HPAK_List:  version mismatch\n", &directory ) )
	{
		FS_Close( fp );
		return FALSE;
	}

	directory.p_rgEntries = (hash_pack_entry_t*)Mem_ZeroMalloc( sizeof( hash_pack_entry_t ) * directory.nEntries );
	FS_Read( directory.p_rgEntries, sizeof( hash_pack_entry_t ) * directory.nEntries, 1, fp );

	for ( i = 0; i < directory.nEntries; i++ )
	{
		entry = &directory.p_rgEntries[i];

		if ( Q_memcmp( entry->resource.rgucMD5_hash, pResource->rgucMD5_hash, MD5_DIGEST_BYTES ) )
			continue;

		FS_Seek( fp, entry->nOffset, FILESYSTEM_SEEK_HEAD );
		retval = TRUE;

		if ( pbuffer && entry->nFileLength > 0 )
		{
			if ( bufsize )
				*bufsize = entry->nFileLength;

			pbuf = (byte*)Mem_Malloc( entry->nFileLength );
			if ( pbuf )
			{
				FS_Read( pbuf, entry->nFileLength, 1, fp );
				*pbuffer = pbuf;
			}
			else
			{
				Con_Printf( "Couln't allocate %i bytes for HPAK entry\n", entry->nFileLength );
				if ( bufsize )
					*bufsize = 0;
				retval = FALSE;
			}
		}
		break;
	}

	Mem_Free( directory.p_rgEntries );
	FS_Close( fp );
	return retval;
}

qboolean HPAK_ResourceForHash( const char* pakname, byte* hash, resource_t* pResourceEntry )
{
	qboolean bFound;
	FileHandle_t fp;
	hash_pack_directory_t directory;
	hash_pack_queue_t* p;
	char name[MAX_PATH];

	for ( p = gp_hpak_queue; p; p = p->next )
	{
		if ( !Q_strcasecmp( p->pakname, pakname ) && !Q_memcmp( p->resource.rgucMD5_hash, hash, MD5_DIGEST_BYTES ) )
		{
			if ( pResourceEntry )
				memcpy( pResourceEntry, &p->resource, sizeof( resource_t ) );
			return TRUE;
		}
	}

	snprintf( name, 256, "%s", pakname );
	COM_DefaultExtension( name, ".hpk" );

	fp = FS_Open( name, "rb" );
	if ( !fp )
	{
		Con_Printf( "ERROR: couldn't open %s.\n", name );
		return FALSE;
	}

	if ( !HPAK_ReadDirectoryHeader( fp, name, "HPAK_List:  version mismatch\n", &directory ) )
	{
		FS_Close( fp );
		return FALSE;
	}

	directory.p_rgEntries = (hash_pack_entry_t*)Mem_Malloc( sizeof( hash_pack_entry_t ) * directory.nEntries );
	FS_Read( directory.p_rgEntries, sizeof( hash_pack_entry_t ) * directory.nEntries, 1, fp );

	bFound = HPAK_FindResource( &directory, hash, pResourceEntry );

	FS_Close( fp );
	Mem_Free( directory.p_rgEntries );
	return bFound;
}

void HPAK_CheckSize( const char* pakname )
{
	char name[MAX_PATH];
	float maxsize, actualsize;
	FileHandle_t fp;

	if ( !hpk_maxsize.value || !pakname )
		return;

	if ( hpk_maxsize.value < 0.0f )
	{
		Con_Printf( "hpk_maxsize < 0, setting to 0\n" );
#if defined( _WIN32 ) && !defined( SWDS_DLL )
		Cvar_DirectSet( &hpk_maxsize, "0" );
#else
		Cvar_Set( hpk_maxsize.name, "0" );
#endif
		return;
	}

	snprintf( name, 256, "%s", pakname );
	COM_DefaultExtension( name, ".hpk" );
	COM_FixSlashes( name );

	maxsize = hpk_maxsize.value * 1000000.0f;
	actualsize = 0.0f;

	fp = FS_Open( name, "rb" );
	if ( fp )
	{
		actualsize = (float)FS_Size( fp );
		FS_Close( fp );
	}

	if ( actualsize >= maxsize )
	{
		Con_Printf( "Server: Size of %s > %f MB, deleting.\n", name, hpk_maxsize.value );
		Log_Printf( "Server: Size of %s > %f MB, deleting.\n", name, hpk_maxsize.value );
		FS_RemoveFile( name, NULL );
	}
}

void HPAK_Init()
{
	Cmd_AddCommand( "hpklist", HPAK_List_f );
	Cmd_AddCommand( "hpkremove", HPAK_Remove_f );
	Cmd_AddCommand( "hpkval", HPAK_Validate_f );
	Cmd_AddCommand( "hpkextract", HPAK_Extract_f );

	gp_hpak_queue = NULL;
}

qboolean HPAK_ResourceForIndex( const char* pakname, int nIndex, resource_t* pResource )
{
	hash_pack_directory_t directory;
	char name[MAX_PATH];
	FileHandle_t fp;

	if ( cmd_source != src_command )
		return FALSE;

	snprintf( name, 256, "%s", pakname );
	COM_DefaultExtension( name, ".hpk" );

	fp = FS_Open( name, "rb" );
	if ( !fp )
	{
		Con_Printf( "ERROR: couldn't open %s.\n", name );
		return FALSE;
	}

	if ( !HPAK_ReadDirectoryHeader( fp, name, "HPAK_List:  version mismatch\n", &directory ) )
	{
		FS_Close( fp );
		return FALSE;
	}

	directory.p_rgEntries = (hash_pack_entry_t*)Mem_Malloc( sizeof( hash_pack_entry_t ) * directory.nEntries );

	if ( nIndex < 1 || nIndex > directory.nEntries )
	{
		Con_Printf( "ERROR: HPAK bogus directory entry request:  %i\n", nIndex );
		FS_Close( fp );
		return FALSE;
	}

	FS_Read( directory.p_rgEntries, sizeof( hash_pack_entry_t ) * directory.nEntries, 1, fp );

	Q_memcpy( pResource, &directory.p_rgEntries[nIndex - 1].resource, sizeof( resource_t ) );

	FS_Close( fp );
	Mem_Free( directory.p_rgEntries );
	return TRUE;
}

void HPAK_RemoveLump( const char* pakname, resource_t* pResource )
{
	char name[MAX_PATH];
	char szTempName[MAX_PATH];
	char szOriginalName[256];
	FileHandle_t fp, tmp;
	hash_pack_directory_t olddirectory, newdirectory;
	hash_pack_entry_t *oldentry, *newentry;
	int n, i;

	if ( !pakname || !pakname[0] || !pResource )
	{
		Con_Printf( "HPAK_RemoveLump:  Invalid arguments\n" );
		return;
	}

	HPAK_FlushHostQueue();

	snprintf( name, 256, "%s", pakname );
	COM_DefaultExtension( name, ".hpk" );

	Q_strncpy( szOriginalName, name, sizeof( szOriginalName ) - 1 );
	szOriginalName[sizeof( szOriginalName ) - 1] = '\0';

	fp = FS_Open( szOriginalName, "rb" );
	if ( !fp )
	{
		Con_Printf( "Error:  couldn't open HPAK file %s for removal.\n", name );
		return;
	}

	COM_StripExtension( name, szTempName );
	COM_DefaultExtension( szTempName, ".hp2" );

	tmp = FS_Open( szTempName, "w+b" );
	if ( !tmp )
	{
		FS_Close( fp );
		Con_Printf( "ERROR: couldn't create %s.\n", szTempName );
		return;
	}

	FS_Seek( fp, 0, FILESYSTEM_SEEK_HEAD );
	FS_Seek( tmp, 0, FILESYSTEM_SEEK_HEAD );

	FS_Read( &hash_pack_header, sizeof( hash_pack_header ), 1, fp );

	FS_Write( &hash_pack_header, sizeof( hash_pack_header ), 1, tmp );

	if ( Q_strncmp( hash_pack_header.szFileStamp, "HPAK", sizeof( hash_pack_header.szFileStamp ) ) )
	{
		Con_Printf( "%s is not an HPAK file\n", name );
		FS_Close( tmp );
		FS_Close( fp );
		FS_Unlink( szTempName );
		return;
	}

	if ( hash_pack_header.version != HASHPAK_VERSION )
	{
		Con_Printf( "ERROR: HPAK version outdated\n" );
		FS_Close( tmp );
		FS_Close( fp );
		FS_Unlink( szTempName );
		return;
	}

	// Now read in the directory structure.
	FS_Seek( fp, hash_pack_header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &olddirectory.nEntries, sizeof( olddirectory.nEntries ), 1, fp );

	if ( olddirectory.nEntries < 1 || olddirectory.nEntries > MAX_HASHPAK_FILE_ENTRIES )
	{
		Con_Printf( "ERROR: HPAK had bogus # of directory entries:  %i\n", olddirectory.nEntries );
		FS_Close( tmp );
		FS_Close( fp );
		FS_Unlink( szTempName );
		return;
	}

	if ( olddirectory.nEntries == 1 )
	{
		Con_Printf( "Removing final lump from HPAK, deleting HPAK:\n  %s\n", szOriginalName );
		FS_Close( tmp );
		FS_Close( fp );
		FS_Unlink( szTempName );
		FS_Unlink( szOriginalName );
		return;
	}

	olddirectory.p_rgEntries = (hash_pack_entry_t*)Mem_Malloc( sizeof( hash_pack_entry_t ) * olddirectory.nEntries );
	FS_Read( olddirectory.p_rgEntries, sizeof( hash_pack_entry_t ) * olddirectory.nEntries, 1, fp );

	newdirectory.nEntries = olddirectory.nEntries - 1;
	newdirectory.p_rgEntries = (hash_pack_entry_t*)Mem_Malloc( sizeof( hash_pack_entry_t ) * newdirectory.nEntries );

	if ( !HPAK_FindResource( &olddirectory, pResource->rgucMD5_hash, NULL ) )
	{
		Con_Printf( "ERROR: HPAK doesn't contain specified lump:  %s\n", pResource->szFileName );
		FS_Close( tmp );
		FS_Close( fp );
		FS_Unlink( szTempName );
		return;
	}

	Con_Printf( "Removing %s from HPAK %s.\n", pResource->szFileName, name );

	for ( i = 0, n = 0; i < olddirectory.nEntries; i++ )
	{
		oldentry = &olddirectory.p_rgEntries[i];

		if ( !Q_memcmp( oldentry->resource.rgucMD5_hash, pResource->rgucMD5_hash, MD5_DIGEST_BYTES ) )
			continue;

		newentry = &newdirectory.p_rgEntries[n++];
		Q_memcpy( newentry, oldentry, sizeof( hash_pack_entry_t ) );
		newentry->nOffset = FS_Tell( tmp );

		FS_Seek( fp, oldentry->nOffset, FILESYSTEM_SEEK_HEAD );
		COM_CopyFileChunk( tmp, fp, newentry->nFileLength );
	}

	hash_pack_header.nDirectoryOffset = FS_Tell( tmp );

	FS_Write( &newdirectory.nEntries, sizeof( newdirectory.nEntries ), 1, tmp );
	for ( i = 0; i < newdirectory.nEntries; i++ )
		FS_Write( &newdirectory.p_rgEntries[i], sizeof( hash_pack_entry_t ), 1, tmp );

	FS_Seek( tmp, 0, FILESYSTEM_SEEK_HEAD );
	FS_Write( &hash_pack_header, sizeof( hash_pack_header ), 1, tmp );

	FS_Close( tmp );
	FS_Close( fp );

	FS_Unlink( szOriginalName );
	FS_Rename( szTempName, szOriginalName );

	Mem_Free( newdirectory.p_rgEntries );
	Mem_Free( olddirectory.p_rgEntries );
}

void HPAK_Remove_f()
{
	int nIndex;
	resource_t resource;

	if ( cmd_source != src_command )
		return;

	HPAK_FlushHostQueue();

	if ( Cmd_Argc() != 3 )
	{
		Con_Printf( "Usage:  hpkremove <hpk> <index>\n" );
		return;
	}

	nIndex = Q_atoi( Cmd_Argv( 2 ) );
	if ( !HPAK_ResourceForIndex( Cmd_Argv( 1 ), nIndex, &resource ) )
	{
		Con_Printf( "Could not locate resource %i in %s\n", nIndex, Cmd_Argv( 1 ) );
		return;
	}

	HPAK_RemoveLump( Cmd_Argv( 1 ), &resource );
}

void HPAK_List_f()
{
	int nCurrent;
	hash_pack_directory_t directory;
	hash_pack_entry_t* entry;
	char name[MAX_PATH];
	char szFileName[MAX_PATH];
	FileHandle_t fp;

	if ( cmd_source != src_command )
		return;

	HPAK_FlushHostQueue();

	snprintf( name, 256, "%s", Cmd_Argv( 1 ) );
	COM_DefaultExtension( name, ".hpk" );

	Con_Printf( "Contents for %s.\n", name );

	fp = FS_Open( name, "rb" );
	if ( !fp )
	{
		Con_Printf( "ERROR: couldn't open %s.\n", name );
		return;
	}

	if ( !HPAK_ReadDirectoryHeader( fp, name, "HPAK_List:  version mismatch\n", &directory ) )
	{
		FS_Close( fp );
		return;
	}

	Con_Printf( "# of Entries:  %i\n", directory.nEntries );
	Con_Printf( "# Type Size FileName : MD5 Hash\n" );

	directory.p_rgEntries = (hash_pack_entry_t*)Mem_Malloc( sizeof( hash_pack_entry_t ) * directory.nEntries );
	FS_Read( directory.p_rgEntries, sizeof( hash_pack_entry_t ) * directory.nEntries, 1, fp );

	for ( nCurrent = 0; nCurrent < directory.nEntries; nCurrent++ )
	{
		entry = &directory.p_rgEntries[nCurrent];
		COM_FileBase( entry->resource.szFileName, szFileName );

		Con_Printf( "%i: %10s %.2fK %s\n  :  %s\n", nCurrent + 1, HPAK_ResourceTypeName( entry->resource.type ),
			entry->resource.nDownloadSize / 1024.0f, szFileName, MD5_Print( entry->resource.rgucMD5_hash ) );
	}

	FS_Close( fp );
	Mem_Free( directory.p_rgEntries );
}

void HPAK_Validate_f()
{
	int nCurrent;
	int nDataSize;
	hash_pack_directory_t directory;
	hash_pack_entry_t* entry;
	char name[MAX_PATH];
	char szFileName[MAX_PATH];
	byte* pData;
	byte md5[MD5_DIGEST_BYTES];
	MD5Context_t ctx;
	FileHandle_t fp;

	if ( cmd_source != src_command )
		return;

	HPAK_FlushHostQueue();

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "Usage:  hpkval hpkname\n" );
		return;
	}

	snprintf( name, 256, "%s", Cmd_Argv( 1 ) );
	COM_DefaultExtension( name, ".hpk" );

	Con_Printf( "Validating %s.\n", name );

	fp = FS_Open( name, "rb" );
	if ( !fp )
	{
		Con_Printf( "ERROR: couldn't open %s.\n", name );
		return;
	}

	if ( !HPAK_ReadDirectoryHeader( fp, name, "hpkval:  version mismatch\n", &directory ) )
	{
		FS_Close( fp );
		return;
	}

	Con_Printf( "# of Entries:  %i\n", directory.nEntries );
	Con_Printf( "# Type Size FileName : MD5 Hash\n" );

	directory.p_rgEntries = (hash_pack_entry_t*)Mem_Malloc( sizeof( hash_pack_entry_t ) * directory.nEntries );
	FS_Read( directory.p_rgEntries, sizeof( hash_pack_entry_t ) * directory.nEntries, 1, fp );

	for ( nCurrent = 0; nCurrent < directory.nEntries; nCurrent++ )
	{
		entry = &directory.p_rgEntries[nCurrent];
		COM_FileBase( entry->resource.szFileName, szFileName );

		Con_Printf( "%i: %10s %.2fK %s:  ", nCurrent + 1, HPAK_ResourceTypeName( entry->resource.type ),
			entry->resource.nDownloadSize / 1024.0f, szFileName );

		nDataSize = entry->nFileLength;
		if ( nDataSize < 1 || nDataSize >= MAX_HASHPAK_LUMP_SIZE )
		{
			Con_Printf( "Unable to MD5 hash data, size invalid:  %i\n", nDataSize );
			continue;
		}

		pData = (byte*)Mem_Malloc( nDataSize + 1 );
		Q_memset( pData, 0, nDataSize );
		FS_Seek( fp, entry->nOffset, FILESYSTEM_SEEK_HEAD );
		FS_Read( pData, nDataSize, 1, fp );

		Q_memset( &ctx, 0, sizeof( ctx ) );
		MD5Init( &ctx );
		MD5Update( &ctx, pData, nDataSize );
		MD5Final( md5, &ctx );

		if ( Q_memcmp( entry->resource.rgucMD5_hash, md5, sizeof( md5 ) ) )
		{
			Con_Printf( " MISMATCHED\n" );
			Con_Printf( "--------------------\n" );
			Con_Printf( " File  :  %s\n", MD5_Print( entry->resource.rgucMD5_hash ) );
			Con_Printf( " Actual:  %s\n", MD5_Print( md5 ) );
			Con_Printf( "--------------------\n" );
		}
		else
		{
			Con_Printf( " OK\n" );
		}

		Mem_Free( pData );
	}

	FS_Close( fp );
	Mem_Free( directory.p_rgEntries );
}

void HPAK_Extract_f()
{
	int nCurrent;
	int nIndex = -1;
	int nDataSize;
	hash_pack_directory_t directory;
	hash_pack_entry_t* entry;
	char name[MAX_PATH];
	char szFileName[MAX_PATH];
	char szFileOut[MAX_PATH];
	byte* pData;
	FileHandle_t fp, fpOut;

	if ( cmd_source != src_command )
		return;

	HPAK_FlushHostQueue();

	if ( Cmd_Argc() != 3 )
	{
		Con_Printf( "Usage:  hpkextract hpkname [all | single index]\n" );
		return;
	}

	if ( Q_strcasecmp( Cmd_Argv( 2 ), "all" ) )
		nIndex = Q_atoi( Cmd_Argv( 2 ) );

	snprintf( name, 256, "%s", Cmd_Argv( 1 ) );
	COM_DefaultExtension( name, ".hpk" );

	if ( nIndex == -1 )
		Con_Printf( "Extracting all lumps from %s.\n", name );
	else
		Con_Printf( "Extracting lump %i from %s\n", nIndex, name );

	fp = FS_Open( name, "rb" );
	if ( !fp )
	{
		Con_Printf( "ERROR: couldn't open %s.\n", name );
		return;
	}

	if ( !HPAK_ReadDirectoryHeader( fp, name, "hpkextract:  version mismatch\n", &directory ) )
	{
		FS_Close( fp );
		return;
	}

	Con_Printf( "# of Entries:  %i\n", directory.nEntries );
	Con_Printf( "# Type Size FileName : MD5 Hash\n" );

	directory.p_rgEntries = (hash_pack_entry_t*)Mem_Malloc( sizeof( hash_pack_entry_t ) * directory.nEntries );
	FS_Read( directory.p_rgEntries, sizeof( hash_pack_entry_t ) * directory.nEntries, 1, fp );

	for ( nCurrent = 0; nCurrent < directory.nEntries; nCurrent++ )
	{
		if ( nIndex != -1 && nIndex != nCurrent )
			continue;

		entry = &directory.p_rgEntries[nCurrent];
		COM_FileBase( entry->resource.szFileName, szFileName );

		Con_Printf( "Extracting %i: %10s %.2fK %s\n", nCurrent + 1, HPAK_ResourceTypeName( entry->resource.type ),
			entry->resource.nDownloadSize / 1024.0f, szFileName );

		nDataSize = entry->nFileLength;
		if ( nDataSize < 1 || nDataSize >= MAX_HASHPAK_LUMP_SIZE )
		{
			Con_Printf( "Unable to extract data, size invalid:  %s\n", nDataSize );
			continue;
		}

		pData = (byte*)Mem_Malloc( nDataSize + 1 );
		Q_memset( pData, 0, nDataSize );
		FS_Seek( fp, entry->nOffset, FILESYSTEM_SEEK_HEAD );
		FS_Read( pData, nDataSize, 1, fp );

		snprintf( szFileOut, sizeof( szFileOut ), "hpklmps\\lmp%04i.wad", nCurrent );
		COM_FixSlashes( szFileOut );
		COM_CreatePath( szFileOut );

		fpOut = FS_Open( szFileOut, "wb" );
		if ( fpOut )
		{
			FS_Write( pData, nDataSize, 1, fpOut );
			FS_Close( fpOut );
		}
		else
		{
			Con_Printf( "Error creating lump file %s\n", szFileOut );
		}

		Mem_Free( pData );
	}

	FS_Close( fp );
	Mem_Free( directory.p_rgEntries );
}

void HPAK_CheckIntegrity( const char* pakname )
{
	char name[MAX_PATH];

	snprintf( name, 256, "%s", pakname );
	COM_DefaultExtension( name, ".hpk" );
	COM_FixSlashes( name );

	HPAK_ValidatePak( name );
}

char* HPAK_GetItem( int item )
{
	static char name[MAX_PATH];
	int i;
	hash_pack_directory_t directory;
	hash_pack_entry_t* entry;
	FileHandle_t fp;

	HPAK_FlushHostQueue();

	Q_strcpy( name, "custom" );
	COM_DefaultExtension( name, ".hpk" );

	fp = FS_Open( name, "rb" );
	if ( !fp )
		return (char*)com_empty_string;

	if ( !HPAK_ReadDirectoryHeader( fp, name, "HPAK_List:  version mismatch\n", &directory ) )
	{
		FS_Close( fp );
		return (char*)com_empty_string;
	}

	directory.p_rgEntries = (hash_pack_entry_t*)Mem_Malloc( sizeof( hash_pack_entry_t ) * directory.nEntries );
	FS_Read( directory.p_rgEntries, sizeof( hash_pack_entry_t ) * directory.nEntries, 1, fp );

	i = directory.nEntries - 1;
	if ( i > item )
		i = item;

	entry = &directory.p_rgEntries[i];
	snprintf( name, sizeof( name ), "!MD5%s", MD5_Print( entry->resource.rgucMD5_hash ) );

	FS_Close( fp );
	Mem_Free( directory.p_rgEntries );
	return name;
}

void HPAK_ValidatePak( const char* fullpakname )
{
	int i;
	int nDataSize;
	hash_pack_header_t header;
	hash_pack_directory_t directory;
	hash_pack_entry_t* entry;
	byte* pData;
	byte md5[MD5_DIGEST_BYTES];
	MD5Context_t ctx;
	FileHandle_t fp;

	HPAK_FlushHostQueue();

	fp = FS_Open( fullpakname, "rb" );
	if ( !fp )
		return;

	FS_Read( &header, sizeof( header ), 1, fp );

	if ( header.version != HASHPAK_VERSION || Q_strncmp( header.szFileStamp, "HPAK", sizeof( header.szFileStamp ) ) )
	{
		Con_Printf( "%s is not a PAK file, deleting\n", fullpakname );
		FS_Close( fp );
		FS_RemoveFile( fullpakname, NULL );
		return;
	}

	FS_Seek( fp, header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &directory.nEntries, sizeof( directory.nEntries ), 1, fp );

	if ( directory.nEntries < 1 || directory.nEntries > MAX_HASHPAK_FILE_ENTRIES )
	{
		Con_Printf( "ERROR: HPAK %s had bogus # of directory entries:  %i, deleting\n", fullpakname, directory.nEntries );
		FS_Close( fp );
		FS_RemoveFile( fullpakname, NULL );
		return;
	}

	directory.p_rgEntries = (hash_pack_entry_t*)Mem_Malloc( sizeof( hash_pack_entry_t ) * directory.nEntries );
	FS_Read( directory.p_rgEntries, sizeof( hash_pack_entry_t ) * directory.nEntries, 1, fp );

	for ( i = 0; i < directory.nEntries; i++ )
	{
		entry = &directory.p_rgEntries[i];

		nDataSize = entry->nFileLength;
		if ( nDataSize < 1 || nDataSize >= MAX_HASHPAK_LUMP_SIZE )
		{
			Mem_Free( directory.p_rgEntries );
			Con_Printf( "Mismatched data in HPAK file %s, deleting\n", fullpakname );
			Con_Printf( "Unable to MD5 hash data lump %i, size invalid:  %i\n", i + 1, nDataSize );
			FS_Close( fp );
			FS_RemoveFile( fullpakname, NULL );
			return;
		}

		pData = (byte*)Mem_Malloc( nDataSize + 1 );
		Q_memset( pData, 0, nDataSize );
		FS_Seek( fp, entry->nOffset, FILESYSTEM_SEEK_HEAD );
		FS_Read( pData, nDataSize, 1, fp );

		Q_memset( &ctx, 0, sizeof( ctx ) );
		MD5Init( &ctx );
		MD5Update( &ctx, pData, nDataSize );
		MD5Final( md5, &ctx );

		if ( Q_memcmp( entry->resource.rgucMD5_hash, md5, sizeof( md5 ) ) )
		{
			Con_Printf( "Mismatched data in HPAK file %s, deleting\n", fullpakname );
			Mem_Free( pData );
			Mem_Free( directory.p_rgEntries );
			FS_Close( fp );
			FS_RemoveFile( fullpakname, NULL );
			return;
		}

		Mem_Free( pData );
	}

	FS_Close( fp );
	Mem_Free( directory.p_rgEntries );
}
