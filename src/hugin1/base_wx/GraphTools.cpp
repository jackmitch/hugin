// -*- c-basic-offset: 4 -*-
/** @file GraphTools.cpp
 *
 *  @brief some helper classes for graphes
 *
 *  @author T. Modes
 *
 */

/*  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this software. If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "panoinc_WX.h"
#include "GraphTools.h"
#include "panotools/PanoToolsInterface.h"

namespace wxGraphTools
{
IMPLEMENT_CLASS(GraphPopupWindow, wxPopupTransientWindow)

GraphPopupWindow::GraphPopupWindow(wxWindow* parent, wxBitmap bitmap) : wxPopupTransientWindow(parent)
{
    wxPanel* panel = new wxPanel(this);
    m_bitmapControl = new wxStaticBitmap(panel, wxID_ANY, bitmap);
    panel->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(GraphPopupWindow::OnLeftDown), NULL, this);
    m_bitmapControl->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(GraphPopupWindow::OnLeftDown), NULL, this);
    m_bitmapControl->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(GraphPopupWindow::OnRightDown), NULL, this);
    wxBoxSizer* topsizer = new wxBoxSizer(wxHORIZONTAL);
    topsizer->Add(m_bitmapControl);
    panel->SetSizer(topsizer);
    topsizer->Fit(panel);
    topsizer->Fit(this);
};

void GraphPopupWindow::OnLeftDown(wxMouseEvent &e)
{
    Dismiss();
};

void GraphPopupWindow::OnRightDown(wxMouseEvent &e)
{
    wxConfigBase* config = wxConfigBase::Get();
    wxFileDialog dlg(this,
        _("Save graph"),
        config->Read(wxT("/actualPath"), wxT("")), wxT(""),
        _("Bitmap (*.bmp)|*.bmp|PNG-File (*.png)|*.png"),
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT, wxDefaultPosition);
    dlg.SetDirectory(config->Read(wxT("/actualPath"), wxT("")));
    dlg.SetFilterIndex(config->Read(wxT("/lastImageTypeIndex"), 0l));
    if (dlg.ShowModal() == wxID_OK)
    {
        config->Write(wxT("/actualPath"), dlg.GetDirectory());  // remember for later
        wxFileName filename(dlg.GetPath());
        int imageType = dlg.GetFilterIndex();
        config->Write(wxT("/lastImageTypeIndex"), imageType);
        if (!filename.HasExt())
        {
            switch (imageType)
            {
                case 1:
                    filename.SetExt(wxT("png"));
                    break;
                case 0:
                default:
                    filename.SetExt(wxT("bmp"));
                    break;
            };
        };
        if (filename.FileExists())
        {
            int d = wxMessageBox(wxString::Format(_("File %s exists. Overwrite?"), filename.GetFullPath().c_str()),
                _("Save image"), wxYES_NO | wxICON_QUESTION);
            if (d != wxYES)
            {
                return;
            }
        }
        switch (imageType)
        {
            case 1:
                m_bitmapControl->GetBitmap().SaveFile(filename.GetFullPath(), wxBITMAP_TYPE_PNG);
                break;
            case 0:
            default:
                m_bitmapControl->GetBitmap().SaveFile(filename.GetFullPath(), wxBITMAP_TYPE_BMP);
                break;
        };
    };
};

/*help class to draw charts */
Graph::Graph(int graphWidth, int graphHeight, wxColour backgroundColour)
{
    m_width = graphWidth;
    m_height = graphHeight;
    //create bitmap
    m_bitmap = new wxBitmap(m_width, m_height);
    m_dc.SelectObject(*m_bitmap);
    m_dc.SetBackground(wxBrush(backgroundColour));
    m_dc.Clear();
    //draw border
    m_dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT)));
    m_dc.SetBrush(*wxTRANSPARENT_BRUSH);
    m_dc.DrawRectangle(0, 0, m_width, m_height);
    SetChartArea(0, 0, m_width, m_height);
    SetChartDisplay(0, 0, 1, 1);
};

/** destructor */
Graph::~Graph()
{
    m_dc.SelectObject(wxNullBitmap);
};

/** set where to draw the chart on the bitmap */
void Graph::SetChartArea(int left, int top, int right, int bottom)
{
    m_dc.DestroyClippingRegion();
    m_left = left;
    m_top = top;
    m_right = right;
    m_bottom = bottom;
    m_dc.SetClippingRegion(m_left - 1, m_top - 1, m_right - m_left + 2, m_bottom - m_top + 2);
};

/** set the real dimension of the chart */
void Graph::SetChartDisplay(double xmin, double ymin, double xmax, double ymax)
{
    m_xmin = xmin;
    m_ymin = ymin;
    m_xmax = xmax;
    m_ymax = ymax;
};

/** draws the grid with linesX lines in x-direction and linexY lines in y-direction */
void Graph::DrawGrid(size_t linesX, size_t linesY)
{
    m_dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT)));
    for (size_t i = 0; i < linesX; i++)
    {
        int x = TransformX((double)i*(m_xmax - m_xmin) / (linesX - 1) + m_xmin);
        m_dc.DrawLine(x, m_top, x, m_bottom);
    };
    for (size_t i = 0; i < linesY; i++)
    {
        int y = TransformY((double)i*(m_ymax - m_ymin) / (linesY - 1) + m_ymin);
        m_dc.DrawLine(m_left, y, m_right, y);
    };
};

/** draws a line with the coordinates given in points */
void Graph::DrawLine(std::vector<hugin_utils::FDiff2D> points, wxColour colour, int penWidth)
{
    if (points.size() < 2)
    {
        return;
    };
    wxPoint *polygonPoints = new wxPoint[points.size()];
    for (size_t i = 0; i < points.size(); i++)
    {
        polygonPoints[i].x = TransformX(points[i].x);
        polygonPoints[i].y = TransformY(points[i].y);
    };
    m_dc.SetPen(wxPen(colour, penWidth));
    m_dc.DrawLines(points.size(), polygonPoints);
    delete[]polygonPoints;
};

const wxBitmap Graph::GetGraph() const
{
    return *m_bitmap;
};

int Graph::TransformX(double x)
{
    return hugin_utils::round((x - m_xmin) / (m_xmax - m_xmin)*(m_right - m_left) + m_left);
};

int Graph::TransformY(double y)
{
    return hugin_utils::round(m_bottom - (y - m_ymin) / (m_ymax - m_ymin)*(m_bottom - m_top));
};

wxBitmap GetDistortionGraph(const HuginBase::SrcPanoImage& srcImage)
{
#define NRPOINTS 100
    HuginBase::PanoramaOptions opts;
    opts.setHFOV(srcImage.getHFOV());
    opts.setProjection(HuginBase::PanoramaOptions::RECTILINEAR);
    opts.setWidth(srcImage.getWidth());
    opts.setHeight(srcImage.getHeight());
    HuginBase::PTools::Transform transform;
    transform.createTransform(srcImage, opts);

    const double minLength = std::min<double>(srcImage.getWidth(), srcImage.getHeight()) / 2.0;
    const double maxR = std::sqrt(static_cast<double>(srcImage.getWidth()*srcImage.getWidth() + srcImage.getHeight()*srcImage.getHeight())) / (2.0*minLength);
    //draw graph
    Graph graph(300, 200, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
    graph.SetChartArea(10, 10, 290, 190);
    graph.SetChartDisplay(0, -0.1, maxR, 0.1);
    graph.DrawGrid(6, 7);
    std::vector<hugin_utils::FDiff2D> points;
    // draw helper lines
    // help line at r=1
    points.push_back(hugin_utils::FDiff2D(1, -0.1));
    points.push_back(hugin_utils::FDiff2D(1, 0.1));
    graph.DrawLine(points, wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT), 1);
    points.clear();
    // helper line at r=width/height (for landscape images), maximal radius in horizontal (or vertical) direction
    // chart goes to r for diagonal
    const double rMaxHorzVert = std::max<double>(srcImage.getWidth(), srcImage.getHeight()) / (2.0*minLength);
    points.push_back(hugin_utils::FDiff2D(rMaxHorzVert, -0.1));
    points.push_back(hugin_utils::FDiff2D(rMaxHorzVert, 0.1));
    graph.DrawLine(points, wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT), 1);
    points.clear();
    // now draw distortion graph
    double r = 0;
    for (size_t i = 0; i < NRPOINTS; i++)
    {
        double x, y;
        if (transform.transform(x, y, r*minLength, 0))
        {
            points.push_back(hugin_utils::FDiff2D(r, x / minLength - r));
        };
        r += maxR / (NRPOINTS - 1);
    };
    graph.DrawLine(points, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT), 2);
    // return bitmap
    return graph.GetGraph();
};

} // namespace