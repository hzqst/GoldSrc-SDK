#include "VGUI_ConfigWizard.h"
#include "DefaultBorder.h"

#include "VGUI_Button.h"
#include "VGUI_LineBorder.h"
#include "VGUI_LoweredBorder.h"
#include "VGUI_TreeFolder.h"

using namespace vgui;

ConfigWizard::ConfigWizard( int x, int y, int wide, int tall ) : Panel( x, y, wide, tall )
{
	setBorder( defaultBorder<LineBorder>() );
	_treeFolder = new TreeFolder( "DonkeyFoo", 10, 10 );
	_treeFolder->setParent( this );
	_treeFolder->setBorder( defaultBorder<LoweredBorder>() );

	_client = new Panel( 80, 30, 64, 64 );
	_client->setParent( this );
	_client->setBorder( defaultBorder<LineBorder>() );

	_okButton = new Button( "Ok", 80, 80, 60, 20 );
	_okButton->setParent( this );
	_cancelButton = new Button( "Cancel", 120, 80 );
	_cancelButton->setParent( this );
	_applyButton = new Button( "Apply", 160, 80 );
	_applyButton->setParent( this );
	_helpButton = new Button( "Help", 200, 80 );
	_helpButton->setParent( this );
}

void ConfigWizard::setSize( int wide, int tall )
{
	Panel::setSize( wide, tall );
	getPaintSize( wide, tall );

	const int margin = 10;
	const int folderWide = 160;
	const int clientLeft = folderWide + 2 * margin;
	const int footerTall = 60;
	const int buttonBottom = 25;
	const int buttonSpacing = 70;
	const int buttonRight = 80;

	_treeFolder->setBounds( margin, margin, folderWide, tall - footerTall );
	_client->setBounds( clientLeft, margin, wide - clientLeft - margin, tall - footerTall );
	_helpButton->setPos( wide - buttonRight, tall - buttonBottom );
	_applyButton->setPos( wide - buttonRight - buttonSpacing, tall - buttonBottom );
	_cancelButton->setPos( wide - buttonRight - 2 * buttonSpacing, tall - buttonBottom );
	_okButton->setPos( wide - buttonRight - 3 * buttonSpacing, tall - buttonBottom );
}

Panel* ConfigWizard::getClient()
{
	return _client;
}

TreeFolder* ConfigWizard::getFolder()
{
	return _treeFolder;
}
