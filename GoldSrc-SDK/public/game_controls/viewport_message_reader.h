#pragma once

#include <cstddef>
#include <cstring>

// Network messages must be complete before a viewport mutates client state.
class ViewportMessageReader
{
public:
	ViewportMessageReader( const void* data, int size ) :
		m_data( static_cast<const unsigned char*>( data ) ),
		m_size( size >= 0 ? static_cast<size_t>( size ) : 0 ), m_position( 0 ),
		m_valid( size >= 0 && ( data || size == 0 ) ) {}

	int ReadByte()
	{
		if ( !m_valid || m_position >= m_size )
		{
			m_valid = false;
			return -1;
		}
		return m_data[m_position++];
	}

	int ReadShort()
	{
		const int low = ReadByte();
		const int high = ReadByte();
		if ( !m_valid )
			return -1;
		const int value = low | ( high << BitsPerByte );
		return value >= SignedShortLimit ? value - ShortRange : value;
	}

	const char* ReadString()
	{
		if ( !m_valid || m_position >= m_size )
		{
			m_valid = false;
			return "";
		}
		const unsigned char* start = m_data + m_position;
		const void* end = std::memchr( start, 0, m_size - m_position );
		if ( !end )
		{
			m_valid = false;
			return "";
		}
		m_position += static_cast<const unsigned char*>( end ) - start + 1;
		return reinterpret_cast<const char*>( start );
	}

	bool IsValid() const { return m_valid; }

private:
	static constexpr int BitsPerByte = 8;
	static constexpr int SignedShortLimit = 0x8000;
	static constexpr int ShortRange = 0x10000;
	const unsigned char* m_data;
	size_t m_size;
	size_t m_position;
	bool m_valid;
};
