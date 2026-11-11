//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================


#include "voice_gain.h"

#define AG_FIX_SHIFT 7

void CAutoGain::Reset( int blockSize, float maxGain, float avgToMaxVal, float scale )
{
	m_BlockSize = blockSize;
	m_MaxGain = maxGain;
	m_AvgToMaxVal = avgToMaxVal;

	m_CurBlockOffset = 0;
	m_CurTotal = 0;
	m_CurMax = 0;

	m_Scale = scale;

	m_CurrentGain = 1.0f;
	m_NextGain = 1.0f;
	m_FixedCurrentGain = 1 << AG_FIX_SHIFT;
	m_GainMultiplier = 0;
}

CAutoGain::CAutoGain()
{
	Reset( 128, 5.0f, 0.5f, 1.0f );
}

void CAutoGain::ProcessSamples( short* pSamples, int nSamples )
{
	while ( nSamples )
	{
		int nToProcess = m_BlockSize - m_CurBlockOffset;
		if ( nToProcess > nSamples )
			nToProcess = nSamples;

		for ( int i = 0; i < nToProcess; i++ )
		{
			int sample = pSamples[i];

			// Update running totals..
			int absSample = sample < 0 ? -sample : sample;
			m_CurTotal += absSample;
			if ( absSample > m_CurMax )
				m_CurMax = absSample;

			// Apply gain on this sample.
			AGFixed gain = m_FixedCurrentGain + i * m_GainMultiplier;
			int newVal = ( sample * gain ) >> AG_FIX_SHIFT;
			if ( newVal < -32768 )
				newVal = -32768;
			if ( newVal > 32767 )
				newVal = 32767;
			pSamples[i] = (short)newVal;
		}

		pSamples += nToProcess;
		nSamples -= nToProcess;

		// Did we just end a block? Update our next gain.
		if ( m_CurBlockOffset % m_BlockSize == 0 )
		{
			m_CurrentGain = m_NextGain;
			m_FixedCurrentGain = (AGFixed)( (double)m_CurrentGain * ( 1 << AG_FIX_SHIFT ) );

			int avg = m_CurTotal / m_BlockSize;
			float maxVal = (float)( m_CurMax - avg ) * m_AvgToMaxVal + (float)avg;
			float gain = 32767.0f / maxVal;
			if ( gain > m_MaxGain )
				gain = m_MaxGain;

			m_CurTotal = 0;
			m_CurMax = 0;

			m_NextGain = gain * m_Scale;
			float gainStep = ( m_NextGain - m_CurrentGain ) / (float)( m_BlockSize - 1 );
			m_GainMultiplier = (AGFixed)( (double)gainStep * ( 1 << AG_FIX_SHIFT ) );
		}
	}
}
