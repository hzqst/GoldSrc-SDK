#ifndef VGUI_WIZARDPANEL_H
#define VGUI_WIZARDPANEL_H

#include "VGUI.h"
#include "VGUI_Panel.h"
#include "VGUI_Dar.h"

#include <cstddef>

namespace vgui
{

class Button;
class ActionSignal;

class VGUIAPI WizardPanel : public Panel
{
public:
	class VGUIAPI WizardPage : public Panel
	{
	public:
		WizardPage();
		WizardPage( int wide, int tall );
#if VGUI_ABI_COMPAT_MODE != 1
		// A WizardPage owns its texts; a copy gets its own.
		WizardPage( const WizardPage& other );
		virtual ~WizardPage() override;
#endif

	private:
		virtual void fireSwitchingToBackPageSignals();
		virtual void fireSwitchingToNextPageSignals();
		virtual void init();

	public:
		virtual void setBackWizardPage( WizardPage* wizardPage );
		virtual void setNextWizardPage( WizardPage* wizardPage );
		virtual WizardPage* getBackWizardPage();
		virtual WizardPage* getNextWizardPage();

		virtual bool isBackButtonEnabled();
		virtual bool isNextButtonEnabled();
		virtual bool isFinishedButtonEnabled();
		virtual bool isCancelButtonEnabled();
		virtual void setBackButtonEnabled( bool state );
		virtual void setNextButtonEnabled( bool state );
		virtual void setFinishedButtonEnabled( bool state );
		virtual void setCancelButtonEnabled( bool state );

		virtual bool isBackButtonVisible();
		virtual bool isNextButtonVisible();
		virtual bool isFinishedButtonVisible();
		virtual bool isCancelButtonVisible();
		virtual void setBackButtonVisible( bool state );
		virtual void setNextButtonVisible( bool state );
		virtual void setFinishedButtonVisible( bool state );
		virtual void setCancelButtonVisible( bool state );

		virtual void getBackButtonText( char* text, int textLen );
		virtual void getNextButtonText( char* text, int textLen );
		virtual void getFinishedButtonText( char* text, int textLen );
		virtual void getCancelButtonText( char* text, int textLen );
		virtual void setBackButtonText( const char* text );
		virtual void setNextButtonText( const char* text );
		virtual void setFinishedButtonText( const char* text );
		virtual void setCancelButtonText( const char* text );

		virtual void setWantedFocus( Panel* panel );
		virtual Panel* getWantedFocus();
		virtual void addSwitchingToBackPageSignal( ActionSignal* s );
		virtual void addSwitchingToNextPageSignal( ActionSignal* s );
		virtual void setTitle( const char* text );
		virtual void getTitle( char* buf, int bufLen );

	private:
		WizardPage*			_backWizardPage;
		WizardPage*			_nextWizardPage;

		bool				_backButtonEnabled;
		bool				_nextButtonEnabled;
		bool				_finishedButtonEnabled;
		bool				_cancelButtonEnabled;
		bool				_backButtonVisible;
		bool				_nextButtonVisible;
		bool				_finishedButtonVisible;
		bool				_cancelButtonVisible;
		char*				_backButtonText;
		char*				_nextButtonText;
		char*				_finishedButtonText;
		char*				_cancelButtonText;

		Dar<ActionSignal*>	_switchingToBackPageSignalDar;
		Dar<ActionSignal*>	_switchingToNextPageSignalDar;
		char*				_title;
		Panel*				_wantedFocus;

#if VGUI_ABI_COMPAT_MODE != 1
		void copyFrom( const WizardPage& other );
#endif

		friend class WizardPanel;

	public:
#if VGUI_ABI_COMPAT_MODE != 1
		WizardPage& operator=( const WizardPage& other );
#else
		WizardPage& operator=( const WizardPage& ) = default;
#endif
	};

	WizardPanel( int x, int y, int wide, int tall );

private:
	virtual void fireFinishedActionSignal();
	virtual void fireCancelledActionSignal();
	virtual void firePageChangedActionSignal();

public:
	virtual void setCurrentWizardPage( WizardPage* currentWizardPage );

protected:
	virtual void performLayout() override;

public:
	virtual void addFinishedActionSignal( ActionSignal* s );
	virtual void addCancelledActionSignal( ActionSignal* s );
	virtual void addPageChangedActionSignal( ActionSignal* s );

	virtual void doBack();
	virtual void doNext();
	virtual void getCurrentWizardPageTitle( char* buf, int bufLen );
	virtual WizardPage* getCurrentWizardPage();

private:
	Button*				_backButton;
	Button*				_nextButton;
	Button*				_finishedButton;
	Button*				_cancelButton;

	WizardPage*			_currentWizardPage;
	Dar<ActionSignal*>	_pageChangedActionSignalDar;

public:
	WizardPanel& operator=( const WizardPanel& ) = default;
};

}

#endif
