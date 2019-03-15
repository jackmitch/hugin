// -*- c-basic-offset: 4 -*-
/** @file LensTools.h
 *
 *  @brief some helper classes for graphes
 *
 *  @author T. Modes
 *
 */

/*
 *  This is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this software. If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#ifndef GRAPHTOOLS_H
#define GRAPHTOOLS_H

#include <hugin_shared.h>
#include "panoinc_WX.h"
#include "panoinc.h"
#include "wx/popupwin.h"

namespace wxGraphTools
{
/** simple popup to show graph */
class WXIMPEX GraphPopupWindow : public wxPopupTransientWindow
{
public:
    GraphPopupWindow(wxWindow* parent, wxBitmap bitmap);
protected:
    void OnLeftDown(wxMouseEvent &e);
    void OnRightDown(wxMouseEvent &e);
private:
    wxStaticBitmap* m_bitmapControl;
    DECLARE_CLASS(GraphPopupWindow)
};

/**help class to draw charts */
class WXIMPEX Graph
{
public:
    /** constructors, set size and background colour of resulting bitmap */
    Graph(int graphWidth, int graphHeight, wxColour backgroundColour);
    /** destructor */
    ~Graph();
    /** set where to draw the chart on the bitmap */
    void SetChartArea(int left, int top, int right, int bottom);
    /** set the real dimension of the chart */
    void SetChartDisplay(double xmin, double ymin, double xmax, double ymax);
    /** draws the grid with linesX lines in x-direction and linexY lines in y-direction */
    void DrawGrid(size_t linesX, size_t linesY);
    /** draws a line with the coordinates given in points */
    void DrawLine(std::vector<hugin_utils::FDiff2D> points, wxColour colour, int penWidth = 1);
    const wxBitmap GetGraph() const;
private:
    // prevent copying of class
    Graph(const Graph&);
    Graph& operator=(const Graph&);
    //helper function to transform coordinates from real world to bitmap
    int TransformX(double x);
    int TransformY(double y);
    // area to be drawn
    double m_xmin, m_xmax, m_ymin, m_ymax;
    // size of canvas
    int m_width, m_height;
    // chart area
    int m_left, m_top, m_right, m_bottom;
    // bitmap
    wxBitmap* m_bitmap;
    wxMemoryDC m_dc;
};

/** return wxBitmap with graph of distortion for given SrcPanoImage */
wxBitmap WXIMPEX GetDistortionGraph(const HuginBase::SrcPanoImage& srcImage);
}
#endif // GRAPHTOOLS_H
