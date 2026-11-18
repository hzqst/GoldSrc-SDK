#include "VGUI_WizardPanel.h"

#include "VGUI_ActionSignal.h"
#include "VGUI_App.h"
#include "VGUI_Button.h"

#include <cstring>
#include <new>

namespace
{

using namespace vgui;

class FooBackHandler : public ActionSignal
{
public:
	FooBackHandler( WizardPanel* wizardPanel )
	{
		_wizardPanel = wizardPanel;
	}

	virtual void actionPerformed( Panel* panel ) override;

protected:
	WizardPanel*	_wizardPanel;
};

class FooNextHandler : public ActionSignal
{
public:
	FooNextHandler( WizardPanel* wizardPanel )
	{
		_wizardPanel = wizardPanel;
	}

	virtual void actionPerformed( Panel* panel ) override;

protected:
	WizardPanel*	_wizardPanel;
};

void FooBackHandler::actionPerformed( Panel* )
{
	_wizardPanel->doBack();
}

void FooNextHandler::actionPerformed( Panel* )
{
	_wizardPanel->doNext();
}

}

namespace vgui
{

WizardPanel::WizardPanel( int x, int y, int wide, int tall ) : Panel( x, y, wide, tall )
{
	_currentWizardPage = NULL;

	_backButton = new Button( "back", 20, 100 );
	_backButton->setParent( this );
	_backButton->addActionSignal( new FooBackHandler( this ) );

	_nextButton = new Button( "next", 80, 100 );
	_nextButton->setParent( this );
	_nextButton->addActionSignal( new FooNextHandler( this ) );

	_finishedButton = new Button( "finished", 120, 100 );
	_finishedButton->setParent( this );

	_cancelButton = new Button( "cancel", 180, 100 );
	_cancelButton->setParent( this );
}

void WizardPanel::addFinishedActionSignal( ActionSignal* s )
{
	_finishedButton->addActionSignal( s );
}

void WizardPanel::addCancelledActionSignal( ActionSignal* s )
{
	_cancelButton->addActionSignal( s );
}

void WizardPanel::fireFinishedActionSignal()
{
	_finishedButton->fireActionSignal();
}

void WizardPanel::fireCancelledActionSignal()
{
	_cancelButton->fireActionSignal();
}

void WizardPanel::firePageChangedActionSignal()
{
	for ( int i = 0; i < _pageChangedActionSignalDar.getCount(); ++i )
	{
		_pageChangedActionSignalDar[i]->actionPerformed( this );
	}
}

WizardPanel::WizardPage* WizardPanel::getCurrentWizardPage()
{
	return _currentWizardPage;
}

void WizardPanel::getCurrentWizardPageTitle( char* buf, int bufLen )
{
	_currentWizardPage->getTitle( buf, bufLen );
}

void WizardPanel::doNext()
{
	if ( _currentWizardPage == NULL )
	{
		return;
	}

	_currentWizardPage->fireSwitchingToNextPageSignals();
	setCurrentWizardPage( _currentWizardPage->getNextWizardPage() );
}

void WizardPanel::doBack()
{
	if ( _currentWizardPage == NULL )
	{
		return;
	}

	_currentWizardPage->fireSwitchingToBackPageSignals();
	setCurrentWizardPage( _currentWizardPage->getBackWizardPage() );
}

void WizardPanel::setCurrentWizardPage( WizardPage* currentWizardPage )
{
	if ( _currentWizardPage != NULL )
	{
		removeChild( _currentWizardPage );
	}

	_currentWizardPage = currentWizardPage;

	if ( _currentWizardPage != NULL )
	{
		_currentWizardPage->setParent( this );
		getApp()->requestFocus( _currentWizardPage->getWantedFocus() );
	}

	firePageChangedActionSignal();
	invalidateLayout( false );
}

void WizardPanel::performLayout()
{
	int wide;
	int tall;
	int x;
	int y;
	int buttonTall;
	char buf[256];

	getPaintSize( wide, tall );

	_backButton->setVisible( false );
	_nextButton->setVisible( false );
	_finishedButton->setVisible( false );
	_cancelButton->setVisible( false );

	if ( _currentWizardPage != NULL )
	{
		buttonTall = _backButton->getTall();
		_currentWizardPage->setBounds( 2, 2, wide - 4, tall - buttonTall - 8 );

		x = wide - 4;
		y = tall - buttonTall - 2;

		_currentWizardPage->getCancelButtonText( buf, 256 );
		_cancelButton->setText( buf );
		_cancelButton->setEnabled( _currentWizardPage->isCancelButtonEnabled() );
		_cancelButton->setVisible( _currentWizardPage->isCancelButtonVisible() );
		_cancelButton->setPos( x - _cancelButton->getWide(), y );
		if ( _currentWizardPage->isCancelButtonVisible() )
		{
			x -= _cancelButton->getWide() + 4;
		}

		_currentWizardPage->getFinishedButtonText( buf, 256 );
		_finishedButton->setText( buf );
		_finishedButton->setEnabled( _currentWizardPage->isFinishedButtonEnabled() );
		_finishedButton->setVisible( _currentWizardPage->isFinishedButtonVisible() );
		_finishedButton->setPos( x - _finishedButton->getWide(), y );
		if ( _currentWizardPage->isFinishedButtonVisible() )
		{
			x -= _finishedButton->getWide() + 4;
		}

		_currentWizardPage->getNextButtonText( buf, 256 );
		_nextButton->setText( buf );
		_nextButton->setEnabled( _currentWizardPage->isNextButtonEnabled() );
		_nextButton->setVisible( _currentWizardPage->isNextButtonVisible() );
		_nextButton->setPos( x - _nextButton->getWide(), y );
		if ( _currentWizardPage->isNextButtonVisible() )
		{
			x -= _nextButton->getWide() + 4;
		}

		_currentWizardPage->getBackButtonText( buf, 256 );
		_backButton->setText( buf );
		_backButton->setEnabled( _currentWizardPage->isBackButtonEnabled() );
		_backButton->setVisible( _currentWizardPage->isBackButtonVisible() );
		_backButton->setPos( x - _backButton->getWide(), y );
		if ( _currentWizardPage->isBackButtonVisible() )
		{
			x -= _backButton->getWide() + 4;
		}
	}
}

void WizardPanel::addPageChangedActionSignal( ActionSignal* s )
{
	_pageChangedActionSignalDar.putElement( s );
}

WizardPanel::WizardPage::WizardPage() : Panel( 0, 0, 64, 64 )
{
	init();
}

WizardPanel::WizardPage::WizardPage( int wide, int tall ) : Panel( 0, 0, wide, tall )
{
	init();
}

#if VGUI_ABI_COMPAT_MODE != 1
WizardPanel::WizardPage::WizardPage( const WizardPage& other ) : Panel( other )
{
	init();
	copyFrom( other );
}

WizardPanel::WizardPage::~WizardPage()
{
	delete[] _backButtonText;
	delete[] _nextButtonText;
	delete[] _finishedButtonText;
	delete[] _cancelButtonText;
	delete[] _title;
}

WizardPanel::WizardPage& WizardPanel::WizardPage::operator=( const WizardPage& other )
{
	if ( this != &other )
	{
		Panel::operator=( other );
		copyFrom( other );
	}

	return *this;
}

// The signals and the linked pages are not owned and stay shared.
void WizardPanel::WizardPage::copyFrom( const WizardPage& other )
{
	_backWizardPage = other._backWizardPage;
	_nextWizardPage = other._nextWizardPage;
	_backButtonEnabled = other._backButtonEnabled;
	_nextButtonEnabled = other._nextButtonEnabled;
	_finishedButtonEnabled = other._finishedButtonEnabled;
	_cancelButtonEnabled = other._cancelButtonEnabled;
	_backButtonVisible = other._backButtonVisible;
	_nextButtonVisible = other._nextButtonVisible;
	_finishedButtonVisible = other._finishedButtonVisible;
	_cancelButtonVisible = other._cancelButtonVisible;
	setBackButtonText( other._backButtonText );
	setNextButtonText( other._nextButtonText );
	setFinishedButtonText( other._finishedButtonText );
	setCancelButtonText( other._cancelButtonText );
	_switchingToBackPageSignalDar = other._switchingToBackPageSignalDar;
	_switchingToNextPageSignalDar = other._switchingToNextPageSignalDar;
	setTitle( other._title );
	_wantedFocus = other._wantedFocus;
}
#endif

void WizardPanel::WizardPage::fireSwitchingToBackPageSignals()
{
	for ( int i = 0; i < _switchingToBackPageSignalDar.getCount(); ++i )
	{
		_switchingToBackPageSignalDar[i]->actionPerformed( this );
	}
}

void WizardPanel::WizardPage::fireSwitchingToNextPageSignals()
{
	for ( int i = 0; i < _switchingToNextPageSignalDar.getCount(); ++i )
	{
		_switchingToNextPageSignalDar[i]->actionPerformed( this );
	}
}

void WizardPanel::WizardPage::init()
{
	_nextWizardPage = NULL;
	_backButtonText = NULL;
	_nextButtonText = NULL;
	_finishedButtonText = NULL;
	_cancelButtonText = NULL;
	_wantedFocus = NULL;
	_backWizardPage = NULL;
	_backButtonEnabled = false;
	_nextButtonEnabled = false;
	_finishedButtonEnabled = false;
	_cancelButtonEnabled = true;
	_backButtonVisible = true;
	_nextButtonVisible = true;
	_finishedButtonVisible = true;
	_cancelButtonVisible = true;
	_title = NULL;
	setTitle( "Untitled" );
	setBackButtonText( "<< Back" );
	setNextButtonText( "Next >>" );
	setFinishedButtonText( "Finished" );
	setCancelButtonText( "Cancel" );
}

void WizardPanel::WizardPage::setBackWizardPage( WizardPage* wizardPage )
{
	_backWizardPage = wizardPage;
	setBackButtonEnabled( wizardPage != NULL );
}

void WizardPanel::WizardPage::setNextWizardPage( WizardPage* wizardPage )
{
	_nextWizardPage = wizardPage;
	setNextButtonEnabled( wizardPage != NULL );
}

WizardPanel::WizardPage* WizardPanel::WizardPage::getBackWizardPage()
{
	return _backWizardPage;
}

WizardPanel::WizardPage* WizardPanel::WizardPage::getNextWizardPage()
{
	return _nextWizardPage;
}

bool WizardPanel::WizardPage::isBackButtonEnabled()
{
	return _backButtonEnabled;
}

bool WizardPanel::WizardPage::isNextButtonEnabled()
{
	return _nextButtonEnabled;
}

bool WizardPanel::WizardPage::isFinishedButtonEnabled()
{
	return _finishedButtonEnabled;
}

bool WizardPanel::WizardPage::isCancelButtonEnabled()
{
	return _cancelButtonEnabled;
}

void WizardPanel::WizardPage::setBackButtonEnabled( bool state )
{
	_backButtonEnabled = state;
}

void WizardPanel::WizardPage::setNextButtonEnabled( bool state )
{
	_nextButtonEnabled = state;
}

void WizardPanel::WizardPage::setFinishedButtonEnabled( bool state )
{
	_finishedButtonEnabled = state;
}

void WizardPanel::WizardPage::setCancelButtonEnabled( bool state )
{
	_cancelButtonEnabled = state;
}

bool WizardPanel::WizardPage::isBackButtonVisible()
{
	return _backButtonVisible;
}

bool WizardPanel::WizardPage::isNextButtonVisible()
{
	return _nextButtonVisible;
}

bool WizardPanel::WizardPage::isFinishedButtonVisible()
{
	return _finishedButtonVisible;
}

bool WizardPanel::WizardPage::isCancelButtonVisible()
{
	return _cancelButtonVisible;
}

void WizardPanel::WizardPage::setBackButtonVisible( bool state )
{
	_backButtonVisible = state;
}

void WizardPanel::WizardPage::setNextButtonVisible( bool state )
{
	_nextButtonVisible = state;
}

void WizardPanel::WizardPage::setFinishedButtonVisible( bool state )
{
	_finishedButtonVisible = state;
}

void WizardPanel::WizardPage::setCancelButtonVisible( bool state )
{
	_cancelButtonVisible = state;
}

void WizardPanel::WizardPage::getBackButtonText( char* text, int textLen )
{
	vgui_strcpy( text, textLen, _backButtonText );
}

void WizardPanel::WizardPage::getNextButtonText( char* text, int textLen )
{
	vgui_strcpy( text, textLen, _nextButtonText );
}

void WizardPanel::WizardPage::getFinishedButtonText( char* text, int textLen )
{
	vgui_strcpy( text, textLen, _finishedButtonText );
}

void WizardPanel::WizardPage::getCancelButtonText( char* text, int textLen )
{
	vgui_strcpy( text, textLen, _cancelButtonText );
}

void WizardPanel::WizardPage::setBackButtonText( const char* text )
{
	delete[] _backButtonText;
	_backButtonText = vgui_strdup( text );
}

void WizardPanel::WizardPage::setNextButtonText( const char* text )
{
	delete[] _nextButtonText;
	_nextButtonText = vgui_strdup( text );
}

void WizardPanel::WizardPage::setFinishedButtonText( const char* text )
{
	delete[] _finishedButtonText;
	_finishedButtonText = vgui_strdup( text );
}

void WizardPanel::WizardPage::setCancelButtonText( const char* text )
{
	delete[] _cancelButtonText;
	_cancelButtonText = vgui_strdup( text );
}

void WizardPanel::WizardPage::setWantedFocus( Panel* panel )
{
	_wantedFocus = panel;
}

Panel* WizardPanel::WizardPage::getWantedFocus()
{
	return _wantedFocus;
}

void WizardPanel::WizardPage::addSwitchingToBackPageSignal( ActionSignal* s )
{
	_switchingToBackPageSignalDar.putElement( s );
}

void WizardPanel::WizardPage::addSwitchingToNextPageSignal( ActionSignal* s )
{
	_switchingToNextPageSignalDar.putElement( s );
}

void WizardPanel::WizardPage::setTitle( const char* text )
{
	delete[] _title;
	_title = vgui_strdup( text );
}

void WizardPanel::WizardPage::getTitle( char* buf, int bufLen )
{
	vgui_strcpy( buf, bufLen, _title );
}

}
