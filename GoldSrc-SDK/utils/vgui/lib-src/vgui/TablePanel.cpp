#include "VGUI_TablePanel.h"

#include "VGUI_ChangeSignal.h"
#include "VGUI_Color.h"
#include "VGUI_HeaderPanel.h"
#include "VGUI_InputSignal.h"
#include "VGUI_RepaintSignal.h"
#include "VGUI_Scheme.h"

#include <string.h>

#define COLUMN_EXTENT_BITS	12
#define COLUMN_EXTENT_MASK	((1 << COLUMN_EXTENT_BITS) - 1)

namespace vgui
{

class FooVGuiTablePanelHandler : public ChangeSignal, public InputSignal, public RepaintSignal
{
public:
	FooVGuiTablePanelHandler( TablePanel* table ) : _table( table )
	{
	}

	virtual void valueChanged( Panel* panel ) override;

	virtual void cursorMoved( int x, int y, Panel* panel ) override;
	virtual void cursorEntered( Panel* panel ) override;
	virtual void cursorExited( Panel* panel ) override;
	virtual void mousePressed( MouseCode code, Panel* panel ) override;
	virtual void mouseDoublePressed( MouseCode code, Panel* panel ) override;
	virtual void mouseReleased( MouseCode code, Panel* panel ) override;
	virtual void mouseWheeled( int delta, Panel* panel ) override;
	virtual void keyPressed( KeyCode code, Panel* panel ) override;
	virtual void keyTyped( KeyCode code, Panel* panel ) override;
	virtual void keyReleased( KeyCode code, Panel* panel ) override;
	virtual void keyFocusTicked( Panel* panel ) override;

	virtual void panelRepainted( Panel* panel ) override;

private:
	TablePanel*	_table;
};

void FooVGuiTablePanelHandler::valueChanged( Panel* panel )
{
	HeaderPanel* header = dynamic_cast<HeaderPanel*>( panel );
	int columnCount = _table->getColumnCount();
	for ( int i = 0; i < header->getSectionCount(); ++i )
	{
		if ( i >= columnCount )
		{
			break;
		}

		int x0;
		int x1;
		header->getSectionExtents( i, x0, x1 );
		_table->setColumnExtents( i, x0, x1 );
	}
}

void FooVGuiTablePanelHandler::cursorMoved( int, int, Panel* )
{
}

void FooVGuiTablePanelHandler::cursorEntered( Panel* )
{
}

void FooVGuiTablePanelHandler::cursorExited( Panel* )
{
}

void FooVGuiTablePanelHandler::mousePressed( MouseCode code, Panel* panel )
{
	_table->privateMousePressed( code, panel );
}

void FooVGuiTablePanelHandler::mouseDoublePressed( MouseCode code, Panel* panel )
{
	_table->privateMouseDoublePressed( code, panel );
}

void FooVGuiTablePanelHandler::mouseReleased( MouseCode, Panel* )
{
}

void FooVGuiTablePanelHandler::mouseWheeled( int, Panel* )
{
}

void FooVGuiTablePanelHandler::keyPressed( KeyCode, Panel* )
{
}

void FooVGuiTablePanelHandler::keyTyped( KeyCode code, Panel* panel )
{
	_table->privateKeyTyped( code, panel );
}

void FooVGuiTablePanelHandler::keyReleased( KeyCode, Panel* )
{
}

void FooVGuiTablePanelHandler::keyFocusTicked( Panel* )
{
}

void FooVGuiTablePanelHandler::panelRepainted( Panel* )
{
	_table->repaint();
}

TablePanel::TablePanel( int x, int y, int wide, int tall, int columnCount ) : Panel( x, y, wide, tall )
#if defined( _WIN32 )
	, _columnDar( 4 )
#endif
{
	setGridSize( 2, 2 );
	setGridVisible( false, false );
	setColumnCount( columnCount );

	setFgColor( Color( Scheme::SchemeColor::sc_black ) );

	_selectedCell[0] = -1;
	_selectedCell[1] = -1;
#if defined( _WIN32 )
	_mouseOverCell[0] = 0;
	_mouseOverCell[1] = 0;
#else
	_mouseOverCell[0] = -1;
	_mouseOverCell[1] = -1;
#endif
	_editableCell[0] = -1;
	_editableCell[1] = -1;

	_fakeInputPanel = new Panel();
	_editableCellPanel = NULL;
	_columnSelectionEnabled = false;
	_rowSelectionEnabled = true;
	_cellSelectionEnabled = true;
	_cellEditingEnabled = true;
	_virtualSize[0] = wide;
	_virtualSize[1] = tall;

	addInputSignal( new FooVGuiTablePanelHandler( this ) );
}

#if VGUI_ABI_COMPAT_MODE != 1
TablePanel::TablePanel( const TablePanel& other ) : Panel( other )
{
	_fakeInputPanel = new Panel();
	copyFrom( other );
}

// The fake input panel is only a child while isWithinTraverse runs.
TablePanel::~TablePanel()
{
	delete _fakeInputPanel;
}

// Keeps this panel's own fake input panel.
TablePanel& TablePanel::operator=( const TablePanel& other )
{
	if ( this != &other )
	{
		Panel::operator=( other );
		copyFrom( other );
	}

	return *this;
}

// Like the original copy, the editing cell panel stays shared.
void TablePanel::copyFrom( const TablePanel& other )
{
	_columnDar = other._columnDar;
	memcpy( _gridVisible, other._gridVisible, sizeof( _gridVisible ) );
	_gridWide = other._gridWide;
	_gridTall = other._gridTall;
	memcpy( _selectedCell, other._selectedCell, sizeof( _selectedCell ) );
	memcpy( _mouseOverCell, other._mouseOverCell, sizeof( _mouseOverCell ) );
	memcpy( _editableCell, other._editableCell, sizeof( _editableCell ) );
	_columnSelectionEnabled = other._columnSelectionEnabled;
	_rowSelectionEnabled = other._rowSelectionEnabled;
	_cellSelectionEnabled = other._cellSelectionEnabled;
	_editableCellPanel = other._editableCellPanel;
	memcpy( _virtualSize, other._virtualSize, sizeof( _virtualSize ) );
	_cellEditingEnabled = other._cellEditingEnabled;
}
#endif

void TablePanel::setCellEditingEnabled( bool state )
{
	_cellEditingEnabled = state;
}

void TablePanel::setGridSize( int wide, int tall )
{
	_gridWide = wide;
	_gridTall = tall;
}

void TablePanel::setGridVisible( bool horizontal, bool )
{
	_gridVisible[0] = horizontal;
	_gridVisible[1] = horizontal;
}

int TablePanel::getColumnCount()
{
	return _columnDar.getCount();
}

void TablePanel::setColumnExtents( int column, int x0, int x1 )
{
	if ( column >= 0 && column < _columnDar.getCount() )
	{
		_columnDar.setElementAt( ( x0 << COLUMN_EXTENT_BITS ) + x1, column );
	}
	repaint();
}

void TablePanel::getSelectedCell( int& column, int& row )
{
	column = _selectedCell[0];
	row = _selectedCell[1];
}

void TablePanel::setHeaderPanel( HeaderPanel* headerPanel )
{
	headerPanel->addChangeSignal( new FooVGuiTablePanelHandler( this ) );
	headerPanel->fireChangeSignal();
	repaint();
}

void TablePanel::setColumnSelectionEnabled( bool state )
{
	_columnSelectionEnabled = state;
	repaint();
}

void TablePanel::setRowSelectionEnabled( bool state )
{
	_rowSelectionEnabled = state;
	repaint();
}

void TablePanel::setCellSectionEnabled( bool state )
{
	_cellSelectionEnabled = state;
	repaint();
}

void TablePanel::stopCellEditing()
{
	if ( _editableCellPanel != NULL )
	{
		_editableCellPanel->setParent( NULL );
		_editableCellPanel = NULL;
		_editableCell[0] = -1;
		_editableCell[1] = -1;
	}
}

void TablePanel::getVirtualSize( int& wide, int& tall )
{
	wide = _virtualSize[0];
	tall = _virtualSize[1];
}

void TablePanel::setSelectedCell( int column, int row )
{
	if ( _selectedCell[0] != column || _selectedCell[1] != row )
	{
		repaint();
		stopCellEditing();
	}
	_selectedCell[0] = column;
	_selectedCell[1] = row;
}

void TablePanel::setEditableCell( int column, int row )
{
	if ( _editableCell[0] != column || _editableCell[1] != row )
	{
		stopCellEditing();
		_editableCellPanel = startCellEditing( column, row );
		if ( _editableCellPanel != NULL )
		{
			_editableCellPanel->setParent( this );
		}
	}
	_editableCell[0] = column;
	_editableCell[1] = row;
}

void TablePanel::setColumnCount( int columnCount )
{
	_columnDar.ensureCapacity( columnCount );
	_columnDar.setCount( columnCount );
}

void TablePanel::paint()
{
	const int before = _gridWide / 2 - 1;
	const int after = _gridWide - before;
	int wide;
	int tall;
	getPaintSize( wide, tall );

	Color color;
	getFgColor( color );
	int r;
	int g;
	int b;
	int a;
	color.getColor( r, g, b, a );

	int gridRight;
	if ( _gridVisible[1] )
	{
		gridRight = 0;
		for ( int column = 0; column < _columnDar.getCount(); ++column )
		{
			int packed = _columnDar[column];
			gridRight = packed & COLUMN_EXTENT_MASK;
			drawSetColor( r, g, b, a );
			drawFilledRect( gridRight - after, 0, gridRight + before, tall );
		}
	}
	else
	{
		gridRight = wide;
	}

	if ( _gridVisible[0] )
	{
		int top = 0;
		for ( int row = 0; row < getRowCount(); ++row )
		{
			drawSetColor( r, g, b, a );
			drawFilledRect( 0, top, gridRight, top + _gridTall );
			top += _gridTall + getCellTall( row );
		}
	}

	_virtualSize[0] = _virtualSize[1] = 0;
	for ( int column = 0; column < _columnDar.getCount(); ++column )
	{
		int packed = _columnDar[column];
		int left = before + ( ( packed >> COLUMN_EXTENT_BITS ) & COLUMN_EXTENT_MASK );
		int right = ( packed & COLUMN_EXTENT_MASK ) - after;
		if ( right > _virtualSize[0] )
		{
			_virtualSize[0] = right;
		}

		int top = _gridTall;
		for ( int row = 0; row < getRowCount(); ++row )
		{
			bool columnSelected = _columnSelectionEnabled && _selectedCell[0] == column;
			bool rowSelected = _rowSelectionEnabled && _selectedCell[1] == row;
			bool cellSelected = _cellSelectionEnabled && _selectedCell[0] == column && _selectedCell[1] == row;
			if ( _editableCell[0] == column && _editableCell[1] == row )
			{
				if ( _editableCellPanel != NULL )
				{
					_editableCellPanel->setBounds( left, top, right - left, getCellTall( row ) );
					_editableCellPanel->repaint();
					_editableCellPanel->solveTraverse();
				}
			}
			else
			{
				Panel* renderer = getCellRenderer( column, row, columnSelected, rowSelected, cellSelected );
				if ( renderer != NULL )
				{
					renderer->setParent( this );
					renderer->setBounds( left, top, right - left, getCellTall( row ) );
					renderer->repaint();
					renderer->solveTraverse();
					renderer->paintTraverse();
					renderer->setParent( NULL );
				}
			}

			top += _gridTall + getCellTall( row );
			if ( top > _virtualSize[1] )
			{
				_virtualSize[1] = right;
			}
		}
	}
}

Panel* TablePanel::isWithinTraverse( int x, int y )
{
	Panel* hitPanel = Panel::isWithinTraverse( x, y );
	if ( hitPanel != this )
	{
		return hitPanel;
	}

	const int before = _gridWide / 2 - 1;
	const int after = _gridWide - before;
	for ( int column = 0; column < _columnDar.getCount(); ++column )
	{
		int packed = _columnDar[column];
		int left = before + ( ( packed >> COLUMN_EXTENT_BITS ) & COLUMN_EXTENT_MASK );
		int right = ( packed & COLUMN_EXTENT_MASK ) - after;
		int top = _gridTall;
		for ( int row = 0; row < getRowCount(); ++row )
		{
			_fakeInputPanel->setParent( this );
			_fakeInputPanel->setBounds( left, top, right - left, getCellTall( row ) );
			_fakeInputPanel->solveTraverse();
			Panel* hit = _fakeInputPanel->isWithinTraverse( x, y );
			_fakeInputPanel->setParent( NULL );
			if ( hit == _fakeInputPanel )
			{
				_mouseOverCell[1] = row;
				_mouseOverCell[0] = column;
				return this;
			}

			top += _gridTall + getCellTall( row );
		}
	}

	return this;
}

void TablePanel::privateMousePressed( MouseCode, Panel* )
{
	if ( _cellEditingEnabled )
	{
		setSelectedCell( _mouseOverCell[0], _mouseOverCell[1] );
		requestFocus();
	}
}

void TablePanel::privateMouseDoublePressed( MouseCode, Panel* )
{
	if ( _cellEditingEnabled )
	{
		setSelectedCell( _mouseOverCell[0], _mouseOverCell[1] );
		int column;
		int row;
		getSelectedCell( column, row );
		setEditableCell( column, row );
	}
}

void TablePanel::privateKeyTyped( KeyCode code, Panel* )
{
	if ( !_cellEditingEnabled )
	{
		return;
	}

	int column;
	int row;
	getSelectedCell( column, row );

	switch ( code )
	{
	case KeyCode::KEY_ENTER:
		setEditableCell( column, row );
		return;

	case KeyCode::KEY_UP:
		row = row - 1;
		if ( row < 0 )
		{
			row = 0;
		}
		setSelectedCell( column, row );
		return;

	case KeyCode::KEY_LEFT:
		column = column - 1;
		if ( column < 0 )
		{
			column = 0;
		}
		setSelectedCell( column, row );
		break;

	case KeyCode::KEY_DOWN:
		row = row + 1;
		setSelectedCell( column, row );
		return;

	case KeyCode::KEY_RIGHT:
		column = column + 1;
		setSelectedCell( column, row );
		break;

	default:
		return;
	}
}

}
