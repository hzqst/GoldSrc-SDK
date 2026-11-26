#ifndef VGUI_TABLEPANEL_H
#define VGUI_TABLEPANEL_H

#include "VGUI_Panel.h"
#include "VGUI_Dar.h"

namespace vgui
{

class HeaderPanel;
class FooVGuiTablePanelHandler;
class FooTablePanelHeaderSignal;

class VGUIAPI TablePanel : public Panel
{
#if VGUI_ABI_COMPAT_MODE == 1
	friend struct VGuiCompatAccess;
#endif
	friend class FooVGuiTablePanelHandler;
	friend class FooTablePanelHeaderSignal;

public:
	TablePanel( int x, int y, int wide, int tall, int columnCount );
#if VGUI_ABI_COMPAT_MODE != 1
	// A TablePanel owns the panel it hit-tests cells with; a copy gets its own.
	TablePanel( const TablePanel& other );
	virtual ~TablePanel() override;
#endif

	virtual void setCellEditingEnabled( bool state );
	virtual void setColumnCount( int columnCount );
	virtual void setGridVisible( bool horizontal, bool vertical );
	virtual void setGridSize( int wide, int tall );
	virtual int getColumnCount();
	virtual void setColumnExtents( int column, int x0, int x1 );

	virtual void setSelectedCell( int column, int row );
	virtual void getSelectedCell( int& column, int& row );
	virtual void setHeaderPanel( HeaderPanel* headerPanel );
	virtual void setColumnSelectionEnabled( bool state );
	virtual void setRowSelectionEnabled( bool state );
	virtual void setCellSectionEnabled( bool state );

	virtual void setEditableCell( int column, int row );
	virtual void stopCellEditing();
	virtual int getRowCount() = 0;
	virtual int getCellTall( int row ) = 0;
	virtual Panel* getCellRenderer( int column, int row, bool columnSelected, bool rowSelected, bool cellSelected ) = 0;
	virtual Panel* startCellEditing( int column, int row ) = 0;

	virtual void getVirtualSize( int& wide, int& tall ) override;

protected:
	virtual void paint() override;
	virtual Panel* isWithinTraverse( int x, int y ) override;

private:
	virtual void privateMousePressed( MouseCode code, Panel* panel );
	virtual void privateMouseDoublePressed( MouseCode code, Panel* panel );
	virtual void privateKeyTyped( KeyCode code, Panel* panel );

	Dar<int>	_columnDar;
	bool		_gridVisible[2];
	int			_gridWide;
	int			_gridTall;

	int			_selectedCell[2];
	int			_mouseOverCell[2];
	int			_editableCell[2];
	Panel*		_fakeInputPanel;
	bool		_columnSelectionEnabled;
	bool		_rowSelectionEnabled;
	bool		_cellSelectionEnabled;
	Panel*		_editableCellPanel;
	int			_virtualSize[2];
	bool		_cellEditingEnabled;
#if VGUI_ABI_COMPAT_MODE != 1
	// The change signal added to the header, until the table is given another.
	FooTablePanelHeaderSignal*	_headerSignal;
#endif

#if VGUI_ABI_COMPAT_MODE != 1
	void copyFrom( const TablePanel& other );
#endif

public:
#if VGUI_ABI_COMPAT_MODE != 1
	TablePanel& operator=( const TablePanel& other );
#else
	TablePanel& operator=( const TablePanel& ) = default;
#endif
};

}

#endif
