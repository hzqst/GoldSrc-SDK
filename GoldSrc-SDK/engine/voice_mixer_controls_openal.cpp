#include "voice_mixer_controls.h"

class CMixerControls : public IMixerControls
{
public:
	~CMixerControls() override;
	void Release() override;
	bool GetValue_Float( Control control, float& value ) override;
	bool SetValue_Float( Control control, float value ) override;
	bool SelectMicrophoneForWaveInput() override;
	virtual const char* GetMixerName();
};

CMixerControls::~CMixerControls()
{
}

void CMixerControls::Release()
{
	delete this;
}

bool CMixerControls::GetValue_Float( Control, float& )
{
	return false;
}

bool CMixerControls::SetValue_Float( Control, float )
{
	return false;
}

bool CMixerControls::SelectMicrophoneForWaveInput()
{
	return false;
}

const char* CMixerControls::GetMixerName()
{
	return "Linux";
}

IMixerControls* GetMixerControls()
{
	return new CMixerControls;
}
