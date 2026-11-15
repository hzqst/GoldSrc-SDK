#include "server.h"
#include "draw.h"
#include "hashpak.h"
#include "mem.h"
#include "zone.h"

void R_DecalRemoveAll( int textureIndex );

void COM_ClearCustomizationList( customization_t* pHead, qboolean bCleanDecals )
{
	int i;
	customization_t *pCurrent, *pNext;
	cachewad_t* pWad;
	cache_user_t* pCache;

	pCurrent = pHead->pNext;
	while ( pCurrent )
	{
		pNext = pCurrent->pNext;

		if ( pCurrent->bInUse )
		{
			if ( pCurrent->pBuffer )
				Mem_Free( pCurrent->pBuffer );

			if ( pCurrent->pInfo )
			{
				if ( pCurrent->resource.type == t_decal )
				{
					if ( bCleanDecals && cls.state == ca_active )
						R_DecalRemoveAll( ~pCurrent->resource.playernum );

					pWad = (cachewad_t*)pCurrent->pInfo;
					Mem_Free( pWad->lumps );

					for ( i = 0; i < pWad->cacheCount; i++ )
					{
						pCache = &pWad->cache[i].cache;
						if ( Cache_Check( pCache ) )
							Cache_Free( pCache );
					}

					Mem_Free( pWad->name );
					Mem_Free( pWad->cache );
				}

				Mem_Free( pCurrent->pInfo );
			}
		}

		Mem_Free( pCurrent );
		pCurrent = pNext;
	}

	pHead->pNext = NULL;
}

qboolean COM_CreateCustomization( customization_t* pListHead, resource_t* pResource, int playernumber, int flags,
	customization_t** pCustomization, int* nLumps )
{
	customization_t* pCust;
	cachewad_t* pWad;
	qboolean bError;

	if ( pCustomization )
		*pCustomization = NULL;

	pCust = (customization_t*)Mem_ZeroMalloc( sizeof( customization_t ) );
	Q_memcpy( &pCust->resource, pResource, sizeof( *pResource ) );

	if ( pResource->nDownloadSize <= 0 )
		goto CustomizationError;

	pCust->bInUse = TRUE;

	if ( flags & FCUST_FROMHPAK )
	{
		if ( !HPAK_GetDataPointer( "custom.hpk", pResource, (byte**)&pCust->pBuffer, NULL ) )
			goto CustomizationError;
	}
	else
	{
		pCust->pBuffer = COM_LoadFile( pResource->szFileName, 5, NULL );
	}

	if ( ( pCust->resource.ucFlags & RES_CUSTOM ) && pCust->resource.type == t_decal )
	{
		pCust->resource.playernum = (byte)playernumber;

		if ( !CustomDecal_Validate( pCust->pBuffer, pResource->nDownloadSize ) )
			goto CustomizationError;

		if ( !( flags & FCUST_IGNOREINIT ) )
		{
			pWad = (cachewad_t*)Mem_ZeroMalloc( sizeof( cachewad_t ) );
			pCust->pInfo = pWad;

			if ( pResource->nDownloadSize < 1024 || pResource->nDownloadSize > 20480 )
				goto CustomizationError;

#ifdef _WIN32
			__try
			{
				bError = !CustomDecal_Init( pWad, pCust->pBuffer, pResource->nDownloadSize, playernumber );
			}
			__except ( EXCEPTION_EXECUTE_HANDLER )
			{
				bError = TRUE;
			}
#else
			bError = !CustomDecal_Init( pWad, pCust->pBuffer, pResource->nDownloadSize, playernumber );
#endif

			if ( bError || pWad->lumpCount <= 0 )
				goto CustomizationError;

			if ( nLumps )
				*nLumps = pWad->lumpCount;

			pCust->bTranslated = TRUE;
			pCust->nUserData1 = 0;
			pCust->nUserData2 = pWad->lumpCount;

			if ( flags & FCUST_WIPEDATA )
			{
				Mem_Free( pWad->name );
				Mem_Free( pWad->cache );
				Mem_Free( pWad->lumps );
				Mem_Free( pCust->pInfo );
				pCust->pInfo = NULL;
			}
		}
	}

	if ( pCustomization )
		*pCustomization = pCust;

	pCust->pNext = pListHead->pNext;
	pListHead->pNext = pCust;

	return TRUE;

CustomizationError:
	if ( pCust->pBuffer )
		Mem_Free( pCust->pBuffer );
	if ( pCust->pInfo )
		Mem_Free( pCust->pInfo );
	Mem_Free( pCust );

	return FALSE;
}

int COM_SizeofResourceList( resource_t* pList, resourceinfo_t* ri )
{
	int nSize;
	resource_t* p;

	nSize = 0;
	Q_memset( ri, 0, sizeof( *ri ) );

	for ( p = pList->pNext; p != pList; p = p->pNext )
	{
		nSize += p->nDownloadSize;

		if ( p->type == t_model && p->nIndex == 1 )
			ri->info[t_world].size += p->nDownloadSize;
		else if ( (unsigned int)p->type < sizeof( ri->info ) / sizeof( ri->info[0] ) )
			ri->info[p->type].size += p->nDownloadSize;
	}

	return nSize;
}
