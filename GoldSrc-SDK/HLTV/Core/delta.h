#pragma once

class BitBuffer;

typedef struct delta_s delta_t;
typedef struct delta_definition_s delta_definition_t;
typedef struct delta_definition_list_s delta_definition_list_t;
typedef struct delta_link_s delta_link_t;

#define DT_BYTE				1
#define DT_SHORT			2
#define DT_FLOAT			4
#define DT_INTEGER			8
#define DT_ANGLE			16
#define DT_TIMEWINDOW_8		32
#define DT_TIMEWINDOW_BIG	64
#define DT_STRING			128
#define DT_SIGNED			( 1u << 31 )

typedef struct
{
	int		sendcount;
	int		receivedcount;
} delta_stats_t;

typedef struct delta_description_s
{
	int				fieldType;
	char			fieldName[32];
	int				fieldOffset;
	short			fieldSize;
	int				significant_bits;
	float			premultiply;
	float			postmultiply;
	short			flags;
	delta_stats_t	stats;
} delta_description_t;

typedef void ( *encoder_t )( delta_s* pFields, const unsigned char* from, const unsigned char* to );

struct delta_s
{
	int					dynamic;
	int					fieldCount;
	char				conditionalencodename[32];
	encoder_t			conditionalencode;
	delta_description_t*	pdd;
};

typedef struct delta_encoder_s
{
	delta_encoder_s*	next;
	char*				name;
	encoder_t			conditionalencode;
} delta_encoder_t;

typedef struct
{
	int		numbase;
	int		num;
	bool	remove;
	bool	custom;
	bool	newbl;
	int		newblindex;
	bool	full;
	int		offset;
	bool	instanced_baseline;
} deltacallback_t;

typedef struct delta_registry_s
{
	delta_registry_s*	next;
	char*				name;
	delta_s*			pdesc;
} delta_registry_t;

extern "C"
{
	extern delta_t				g_MetaDelta;
	extern delta_registry_t*	g_deltaregistry;
	extern delta_s*				g_peventdelta;
	extern delta_s*				g_pweapondelta;
	extern delta_s*				g_pclientdelta;
	extern delta_s*				g_pcustomentitydelta;
	extern delta_s*				g_pentitydelta;
	extern delta_s*				g_pplayerdelta;
	extern bool					g_large_Time_Buffers;
	extern double				g_delta_Time;
}

delta_description_t*	DELTA_FindField( delta_s* pFields, const char* pszField );
int						DELTA_FindFieldIndex( delta_s* pFields, const char* fieldname );
void					DELTA_SetField( delta_s* pFields, const char* fieldname );
void					DELTA_UnsetField( delta_s* pFields, const char* fieldname );
void					DELTA_SetFieldByIndex( delta_s* pFields, int fieldNumber );
void					DELTA_UnsetFieldByIndex( delta_s* pFields, int fieldNumber );
void					DELTA_ClearFlags( delta_s* pFields );
int						DELTA_CountSendFields( delta_s* pFields );
void					DELTA_MarkSendFields( unsigned char* from, unsigned char* to, delta_s* pFields );
void					DELTA_SetSendFlagBits( delta_s* pFields, int* bits, int* bytecount );
void					DELTA_WriteMarkedFields( BitBuffer* stream, unsigned char* from, unsigned char* to,
							delta_s* pFields );
int						DELTA_CheckDelta( unsigned char* from, unsigned char* to, delta_s* pFields );
void					DELTA_WriteHeader( BitBuffer* stream, deltacallback_t* header );
int						DELTA_WriteDelta( BitBuffer* stream, unsigned char* from, unsigned char* to, bool force,
							delta_s* pFields, deltacallback_t* header );
int						DELTA_ParseDelta( BitBuffer* stream, unsigned char* from, unsigned char* to, delta_s* pFields,
							int bufferSize );
int						DELTA_TestDelta( unsigned char* from, unsigned char* to, delta_s* pFields );

void					DELTA_AddEncoder( char* name, encoder_t conditionalencode );
void					DELTA_ClearEncoders();
encoder_t				DELTA_LookupEncoder( char* name );

int						DELTA_CountLinks( delta_link_t* plinks );
void					DELTA_ReverseLinks( delta_link_t** plinks );
void					DELTA_ClearLinks( delta_link_t** plinks );
delta_s*				DELTA_BuildFromLinks( delta_link_t** pplinks );
int						DELTA_FindOffset( int count, delta_definition_t* pdef, char* fieldname );
bool					DELTA_ParseType( delta_description_t* pdelta, char** pstream );
bool					DELTA_ParseField( int count, delta_definition_t* pdefinition, delta_link_t* pField,
							char** pstream );
void					DELTA_FreeDescription( delta_s** ppdesc );

void					DELTA_AddDefinition( char* name, delta_definition_t* pdef, int numelements );
void					DELTA_ClearDefinitions();
delta_definition_t*		DELTA_FindDefinition( char* name, int* count );

void					DELTA_SkipDescription( char** pstream );
bool					DELTA_ParseOneField( char** ppstream, delta_link_t** pplist, int count,
							delta_definition_t* pdefinition );
bool					DELTA_ParseDescription( char* name, delta_s** ppdesc, char* pstream );
bool					DELTA_Load( char* name, delta_s** ppdesc, char* pszFile );

void					DELTA_RegisterDescription( char* name );
void					DELTA_ClearRegistrations();
delta_s**				DELTA_LookupRegistration( char* name );

void					DELTA_ClearStats( delta_s* p );
void					DELTA_Init();
void					DELTA_UpdateDescriptions();
void					DELTA_Shutdown();
void					DELTA_SetTime( double time );
void					DELTA_SetLargeTimeBufferSize( bool bigBuffers );
