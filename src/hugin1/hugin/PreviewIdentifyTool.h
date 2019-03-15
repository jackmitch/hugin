// -*- c-basic-offset: 4 -*-
/** @file PreviewIdentifyTool.cpp
 *
 *  @author James Legg
 *
 *  This program is free software; you can redistribute it and/or
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

#ifndef _PREVIEWIDENTIFYTOOL_H
#define _PREVIEWIDENTIFYTOOL_H

#include "Tool.h"
#include <set>
#include <vector>

class GLPreviewFrame;

/** Visually connect the image numbers with the image on the preview.
 * There are two ways it does this:
 * -# When the user moves the mouse pointer over a image button, the image is
 *    highlighted in the preview.
 * -# When the user moves the mouse pointer over the preview, the image under
 *    the pointer are highlighted with matching colours over the buttons.
 * 
 * The highlighted images are drawn on top of the other images, with a
 * coloured border.
 * If the mouse is over exactly two images, a click opens the control point
 * editor with those two images shown.
 */
class PreviewIdentifyTool : public Tool
{
public:
    /** constructor
      @param helper pointer to ToolHelper class for managing Tool notifications
      @param owner pointer to GLPreviewFrame, needed for updating the color of the image buttons
      @param showNumbers true, if the image number should be shown above the images,
        this image show code works only for the preview, not for the panosphere and mosaic plane
        so pass the correct parameter depending on the underlying GLRenderer/GLViewer class
    */
    PreviewIdentifyTool(ToolHelper *helper, GLPreviewFrame *owner, bool showNumbers);
    ~PreviewIdentifyTool();
    void Activate();
    void ImagesUnderMouseChangedEvent();
    void AfterDrawImagesEvent();
    bool BeforeDrawImageEvent(unsigned int image);
    /** Notification for when moving the mouse on an image button.
     * @param image the image number of the image the mouse is on.
     */
    void ShowImageNumber(unsigned int image);
    /// Notification for when moving the mouse off an image button.
    void StopShowingImages();
    /// Show control point editor if mouse is over two images.
    void MouseButtonEvent(wxMouseEvent & e);

    void MouseMoveEvent(double x, double y, wxMouseEvent & e);
    
    void KeypressEvent(int keycode, int modifiers, int pressed);
    
    void setConstantOn(bool constant_on_in);
    
    void UpdateWithNewImageSet(std::set<unsigned int> new_image_set);
    void ForceRedraw();
private:
    /// Generate a colour given how many colours we need and an index.
    void HighlightColour(unsigned int index, unsigned int count,
                        unsigned char &red, unsigned char &green,
                        unsigned char &blue);
    static bool texture_created;
    /// OpenGL texture name for the circular border texture.
    static unsigned int circle_border_tex;
    /// OpenGL texture name for the rectangular border texture.
    static unsigned int rectangle_border_tex;
    /// OpenGL texture name for the font texture
    static unsigned int font_tex;
    /// OpenGL call list id for font
    static unsigned int font_list;
    /// glyph width for font
    static std::vector<int> m_glyphWidth;

    /// Set of image numbers of the images we are displaying highlighted.
    std::set<unsigned int> m_image_set;
    GLPreviewFrame *m_preview_frame;
    /// The image the user last placed their mouse over the button for
    unsigned int m_mouse_over_image;
    bool m_mouse_is_over_button;

    void StopUpdating();
    void ContinueUpdating();

    bool m_stopUpdating;
    //user has clicked and is holding left button while over panorama
    bool m_holdLeft;
    
    bool m_holdControl;

    bool m_constantOn;
    bool m_showNumbers;
};

#endif

