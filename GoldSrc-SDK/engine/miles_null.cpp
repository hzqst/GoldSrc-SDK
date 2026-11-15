// Placeholder for the Miles Sound System on Linux, where the SDK does not ship
// the library. Every call fails or does nothing, so CCDAudio reports that MP3
// playback is unavailable (see cd.cpp).

extern "C"
{
	typedef void* HDIGDRIVER;
	typedef void* HSTREAM;
	typedef void* HSAMPLE;

	int AIL_startup()
	{
		return 0;
	}

	void AIL_shutdown()
	{
	}

	char* AIL_set_redist_directory( const char* )
	{
		return nullptr;
	}

	HDIGDRIVER AIL_open_generic_digital_driver( int, int, int, int, int )
	{
		return nullptr;
	}

	int RADSS_OalInstallDriver( int, int )
	{
		return 0;
	}

	void AIL_close_digital_driver( HDIGDRIVER )
	{
	}

	HSTREAM AIL_open_stream( HDIGDRIVER, const char*, int )
	{
		return nullptr;
	}

	void AIL_close_stream( HSTREAM )
	{
	}

	void AIL_start_stream( HSTREAM )
	{
	}

	void AIL_pause_stream( HSTREAM, int )
	{
	}

	int AIL_stream_status( HSTREAM )
	{
		return 0;
	}

	void AIL_set_stream_loop_count( HSTREAM, int )
	{
	}

	void AIL_set_stream_user_data( HSTREAM, unsigned int, int )
	{
	}

	HSAMPLE AIL_stream_sample_handle( HSTREAM )
	{
		return nullptr;
	}

	int AIL_sample_playback_rate( HSAMPLE )
	{
		return 0;
	}

	void AIL_set_sample_volume_pan( HSAMPLE, float, float )
	{
	}

	int AIL_set_preference( unsigned int, int )
	{
		return 0;
	}
}
