#pragma once

extern bool r_legacyBeamColors;

// Limit compatibility decoding to the draw of an explicitly marked beam.
class ScopedLegacyBeamColors
{
public:
	explicit ScopedLegacyBeamColors( bool enabled ) : m_previous( r_legacyBeamColors )
	{
		r_legacyBeamColors = enabled;
	}
	~ScopedLegacyBeamColors() { r_legacyBeamColors = m_previous; }
	ScopedLegacyBeamColors( const ScopedLegacyBeamColors& ) = delete;
	ScopedLegacyBeamColors& operator=( const ScopedLegacyBeamColors& ) = delete;

private:
	bool m_previous;
};

inline float R_DecodeBeamColor( float color )
{
	return r_legacyBeamColors ? color * 255.0f : color;
}

inline float R_ClampBeamVertexColor( float color )
{
	if ( !r_legacyBeamColors )
		return color;
	return color < 0.0f ? 0.0f : ( color > 1.0f ? 1.0f : color );
}
