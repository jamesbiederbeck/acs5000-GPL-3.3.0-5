/******************************************************************************
 *
 * Copyright (c) 1999-2005 AppGate Network Security AB. All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code as
 * defined in and that are subject to the MindTerm Public Source License,
 * Version 2.0, (the 'License'). You may not use this file except in compliance
 * with the License.
 * 
 * You should have received a copy of the MindTerm Public Source License
 * along with this software; see the file LICENSE.  If not, write to
 * AppGate Network Security AB, Otterhallegatan 2, SE-41118 Goteborg, SWEDEN
 *
 *****************************************************************************/

package com.mindbright.terminal;

import java.awt.Container;

/**
 * Interface for terminal menu handlers. A terminal menu handler is
 * expected to handle the menus associated with a terminal window.
 *
 * @see TerminalWin
 */
public abstract class TerminalMenuHandler {
    /**
     * Set the name of this application. This name may be used in the
     * title of any dialogs popped up etc.
     *
     * @param titleName the application name for window titles
     */
    public abstract void setTitleName(String titleName);

    /**
     * Attach to a terminal window.
     *
     * @param term terminal window to attach to
     */
    public abstract void setTerminalWin(TerminalWin term);

    /**
     * Register a listener which is interested in menu events
     *
     * @param listener the interested listener
     */
    public abstract void setTerminalMenuListener(
        TerminalMenuListener listener);

    /**
     * Install the standard menus on the given frame. There is no need
     * to call setTerminalWin before calling this since the actual
     * terminal window is passed here as well.
     *
     * @param terminal terminal window to attach to
     * @param frame frame to install menus in
     */
    public abstract void addBasicMenus(TerminalWin terminal, Container frame);

    /**
     * Gets called when the selection state is changed
     *
     * @param selectionAvailable true if some text is selected
     */
    public abstract void updateSelection(boolean selectionAvailable);

    /**
     * Gets called when the contents of the menus might need updating.
     */
    public abstract void update();

    /**
     * Registers a popup menu.
     *
     * @param menu the popup menu. This is passed as an Object since
     * it may be either a <code>PopupMenu</code> or
     * <code>JPopupMenu</code>.
     */
    public abstract void setPopupMenu(Object menu);

    /**
     * Show the popup menu at the indicated position.
     *
     * @param x x-coordinate of position to show menu ay
     * @param y y-coordinate of position to show menu ay
     */
    public abstract void showPopupMenu(int x, int y);
}
