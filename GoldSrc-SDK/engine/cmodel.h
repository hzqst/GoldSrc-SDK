#pragma once

#include "const.h"
#include "gl_model.h"

void		CM_CalcPAS( model_t* model );
void		CM_FreePAS();
byte*		CM_LeafPVS( int leafnum );
byte*		CM_LeafPAS( int leafnum );
void		CM_DecompressPVS( byte* input, byte* decompressed, int byteCount );
qboolean	CM_HeadnodeVisible( mnode_t* node, byte* visibility, int* firstVisibleLeaf );

inline mnode_t* Mod_DrawingHullNode( const model_t* model, int index )
{
	if ( !model || model->type != mod_brush || !model->nodes || index < 0 || index >= model->numnodes )
		return nullptr;
	return &model->nodes[index];
}
