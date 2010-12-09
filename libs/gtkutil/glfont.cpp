/*
Copyright (C) 2001-2006, William Joseph.
All Rights Reserved.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "glfont.h"
#include "igl.h"

// generic string printing with call lists
class GLFontCallList: public GLFont
{
	GLuint m_displayList;
	int m_pixelHeight;
	int m_pixelAscent;
	int m_pixelDescent;
	public:
	GLFontCallList(GLuint displayList, int asc, int desc, int pixelHeight) : m_displayList(displayList), m_pixelHeight(pixelHeight), m_pixelAscent(asc), m_pixelDescent(desc)
	{
	}
	virtual ~GLFontCallList()
	{
		glDeleteLists(m_displayList, 256);
	}
	void printString(const char *s)
	{
		GlobalOpenGL().m_glListBase(m_displayList);
		GlobalOpenGL().m_glCallLists(GLsizei(strlen(s)), GL_UNSIGNED_BYTE, reinterpret_cast<const GLubyte*>(s));
	}
	virtual int getPixelAscent() const
	{
		return m_pixelAscent;
	}
	virtual int getPixelDescent() const
	{
		return m_pixelDescent;
	}
	virtual int getPixelHeight() const
	{
		return m_pixelHeight;
	}
};

#ifdef _WIN32
	#include <windows.h>
#endif
#include "debugging/debugging.h"

// LordHavoc: this is code for direct Xlib bitmap character fetching, as an
// alternative to requiring gtkglarea, it was created due to a lack of this
// package on SuSE 9.x but this package is now commonly shipping in Linux
// distributions so this code may be unnecessary, feel free however to enable
// it when building packages for distros that do not ship with that package,
// or if you just prefer less dependencies...
#if 0

#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <GL/glx.h>

GLFont *glfont_create(const char* font_string)
{
  GLuint font_list_base;
  XFontStruct *fontInfo;
  Display *dpy = GDK_DISPLAY ();
  unsigned int i, first, last, firstrow, lastrow;
  int maxchars;
  int firstbitmap;

  fontInfo = XLoadQueryFont (dpy, "-*-fixed-*-*-*-*-8-*-*-*-*-*-*-*");
  if (fontInfo == NULL)
  {
    // try to load other fonts
    fontInfo = XLoadQueryFont (dpy, "-*-fixed-*-*-*-*-*-*-*-*-*-*-*-*");

    // any font will do !
    if (fontInfo == NULL)
      fontInfo = XLoadQueryFont(dpy, "-*-*-*-*-*-*-*-*-*-*-*-*-*-*");

    if (fontInfo == NULL)
      ERROR_MESSAGE("couldn't create font");
  }

  first = (int)fontInfo->min_char_or_byte2;
  last = (int)fontInfo->max_char_or_byte2;
  firstrow = (int)fontInfo->min_byte1;
  lastrow = (int)fontInfo->max_byte1;
  /*
   * How many chars in the charset
   */
  maxchars = 256 * lastrow + last;
  font_list_base = glGenLists(maxchars+1);
  if (font_list_base == 0)
  {
    ERROR_MESSAGE( "couldn't create font" );
  }

  /*
   * Get offset to first char in the charset
   */
  firstbitmap = 256 * firstrow + first;
  /*
   * for each row of chars, call glXUseXFont to build the bitmaps.
   */

  for(i=firstrow; i<=lastrow; i++)
  {
    glXUseXFont(fontInfo->fid, firstbitmap, last-first+1, font_list_base+firstbitmap);
    firstbitmap += 256;
  }

/*    *height = fontInfo->ascent + fontInfo->descent;
    *width = fontInfo->max_bounds.width;  */
  return new GLFontCallList(font_list_base, fontInfo->ascent, fontInfo->descent, fontInfo->ascent + fontInfo->descent);
}

#elif 0

#include <gtk/gtkglwidget.h>

GLFont *glfont_create(const char* font_string)
{
  GLuint font_list_base = glGenLists (256);
  gint font_height = 0, font_ascent = 0, font_descent = 0;

  PangoFontDescription* font_desc = pango_font_description_from_string (font_string);

  PangoFont* font = gdk_gl_font_use_pango_font (font_desc, 0, 256, font_list_base);

  if(font == 0)
  {
	  pango_font_description_free (font_desc);
	  font_desc = pango_font_description_from_string ("fixed 8");
	  font = gdk_gl_font_use_pango_font (font_desc, 0, 256, font_list_base);
  }

  if(font == 0)
  {
	  pango_font_description_free (font_desc);
	  font_desc = pango_font_description_from_string ("courier new 8");
	  font = gdk_gl_font_use_pango_font (font_desc, 0, 256, font_list_base);
  }

  if(font != 0)
  {
    PangoFontMetrics* font_metrics = pango_font_get_metrics (font, 0);

    font_ascent = pango_font_metrics_get_ascent (font_metrics);
	font_descent = pango_font_metrics_get_descent (font_metrics);
	font_height = font_ascent + font_descent;

    font_ascent = PANGO_PIXELS (font_ascent);
    font_descent = PANGO_PIXELS (font_descent);
    font_height = PANGO_PIXELS (font_height);

    pango_font_metrics_unref (font_metrics);
  }

  pango_font_description_free (font_desc);

  // fix for pango/gtkglext metrix bug
  if(font_height > 256)
	  font_height = 16;

  return new GLFontCallList(font_list_base, font_ascent, font_descent, font_height);
}
#else

// new font code ripped from ZeroRadiant

#include <pango/pangoft2.h>
#include <pango/pango-utils.h>

class GLFontInternal: public GLFont
{
	const char *font_string;
	int font_height;
	int font_ascent;
	int font_descent;
	int y_offset_bitmap_render_pango_units;
	PangoContext *ft2_context;
	PangoFontMap *fontmap;

	public:
	GLFontInternal(const char *_font_string): font_string(_font_string)
	{
		PangoFontDescription *font_desc;
		PangoLayout *layout;
		PangoRectangle log_rect;
		int font_ascent_pango_units;
		int font_descent_pango_units;

#if !PANGO_VERSION_CHECK(1,22,0)
		ft2_context = pango_ft2_get_context(96, 96);
#else
		fontmap = pango_ft2_font_map_new();
		//pango_ft2_font_map_set_resolution(PANGO_FT2_FONT_MAP(fontmap), 96, 96);
		ft2_context = pango_font_map_create_context(fontmap);
#endif

		font_desc = pango_font_description_from_string(font_string);
		//pango_font_description_set_size(font_desc, 10 * PANGO_SCALE);
		pango_context_set_font_description(ft2_context, font_desc);
		pango_font_description_free(font_desc);
		// TODO fallback to fixed 8, courier new 8

		layout = pango_layout_new(ft2_context);

#ifdef FONT_SIZE_WORKAROUND
		pango_layout_set_width(layout, -1); // -1 no wrapping.  All text on one line.
		pango_layout_set_text(layout, "The quick brown fox jumped over the lazy sleeping dog's back then sat on a tack.", -1); // -1 null-terminated string.
#endif

#if !PANGO_VERSION_CHECK(1,22,0)
		PangoLayoutIter *iter;  
		iter = pango_layout_get_iter(layout);
		font_ascent_pango_units = pango_layout_iter_get_baseline(iter);
		pango_layout_iter_free(iter);
#else
		font_ascent_pango_units = pango_layout_get_baseline(layout);
#endif
		pango_layout_get_extents(layout, NULL, &log_rect);
		g_object_unref(G_OBJECT(layout));
		font_descent_pango_units = log_rect.height - font_ascent_pango_units;

		font_ascent = PANGO_PIXELS_CEIL(font_ascent_pango_units);
		font_descent = PANGO_PIXELS_CEIL(font_descent_pango_units);
		font_height = font_ascent + font_descent;
		y_offset_bitmap_render_pango_units = (font_ascent * PANGO_SCALE) - font_ascent_pango_units;
	}

	virtual ~GLFontInternal()
	{
		g_object_unref(G_OBJECT(ft2_context));
		g_object_unref(G_OBJECT(fontmap));
	}

	// Renders the input text at the current location with the current color.
	// The X position of the current location is used to place the left edge of the text image,
	// where the text image bounds are defined as the logical extents of the line of text.
	// The Y position of the current location is used to place the bottom of the text image.
	// You should offset the Y position by the amount returned by gtk_glwidget_font_descent()
	// if you want to place the baseline of the text image at the current Y position.
	// Note: A problem with this function is that if the lower left corner of the text falls
	// just a hair outside of the viewport (meaning the current raster position is invalid),
	// then no text will be rendered.  The solution to this is a very hacky one.  You can search
	// Google for "glDrawPixels clipping".
	virtual void printString(const char *s)
	{
		// The idea for this code initially came from the font-pangoft2.c example that comes with GtkGLExt.

		PangoLayout *layout;
		PangoRectangle log_rect;
		FT_Bitmap bitmap;
		unsigned char *begin_bitmap_buffer;
		GLfloat color[4];
		GLint previous_unpack_alignment;
		GLboolean previous_blend_enabled;
		GLint previous_blend_func_src;
		GLint previous_blend_func_dst;
		GLfloat previous_red_bias;
		GLfloat previous_green_bias;
		GLfloat previous_blue_bias;
		GLfloat previous_alpha_scale;

		layout = pango_layout_new(ft2_context);
		pango_layout_set_width(layout, -1); // -1 no wrapping.  All text on one line.
		pango_layout_set_text(layout, s, -1); // -1 null-terminated string.
		pango_layout_get_extents(layout, NULL, &log_rect);

		if (log_rect.width > 0 && log_rect.height > 0) {
			bitmap.rows = font_ascent + font_descent;
			bitmap.width = PANGO_PIXELS_CEIL(log_rect.width);
			bitmap.pitch = -bitmap.width; // Rendering it "upside down" for OpenGL.
			begin_bitmap_buffer = (unsigned char *) g_malloc(bitmap.rows * bitmap.width);
			memset(begin_bitmap_buffer, 0, bitmap.rows * bitmap.width);
			bitmap.buffer = begin_bitmap_buffer + (bitmap.rows - 1) * bitmap.width; // See pitch above.
			bitmap.num_grays = 0xff;
			bitmap.pixel_mode = FT_PIXEL_MODE_GRAY;
			pango_ft2_render_layout_subpixel(&bitmap, layout, -log_rect.x,
					y_offset_bitmap_render_pango_units);
			GlobalOpenGL().m_glGetFloatv(GL_CURRENT_COLOR, color);

			// Save state.  I didn't see any OpenGL push/pop operations for these.
			// Question: Is saving/restoring this state necessary?  Being safe.
			GlobalOpenGL().m_glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous_unpack_alignment);
			previous_blend_enabled = GlobalOpenGL().m_glIsEnabled(GL_BLEND);
			GlobalOpenGL().m_glGetIntegerv(GL_BLEND_SRC, &previous_blend_func_src);
			GlobalOpenGL().m_glGetIntegerv(GL_BLEND_DST, &previous_blend_func_dst);
			GlobalOpenGL().m_glGetFloatv(GL_RED_BIAS, &previous_red_bias);
			GlobalOpenGL().m_glGetFloatv(GL_GREEN_BIAS, &previous_green_bias);
			GlobalOpenGL().m_glGetFloatv(GL_BLUE_BIAS, &previous_blue_bias);
			GlobalOpenGL().m_glGetFloatv(GL_ALPHA_SCALE, &previous_alpha_scale);

			GlobalOpenGL().m_glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			GlobalOpenGL().m_glEnable(GL_BLEND);
			GlobalOpenGL().m_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			GlobalOpenGL().m_glPixelTransferf(GL_RED_BIAS, color[0]);
			GlobalOpenGL().m_glPixelTransferf(GL_GREEN_BIAS, color[1]);
			GlobalOpenGL().m_glPixelTransferf(GL_BLUE_BIAS, color[2]);
			GlobalOpenGL().m_glPixelTransferf(GL_ALPHA_SCALE, color[3]);

			GlobalOpenGL().m_glDrawPixels(bitmap.width, bitmap.rows,
					GL_ALPHA, GL_UNSIGNED_BYTE, begin_bitmap_buffer);
			g_free(begin_bitmap_buffer);

			// Restore state in reverse order of how we set it.
			GlobalOpenGL().m_glPixelTransferf(GL_ALPHA_SCALE, previous_alpha_scale);
			GlobalOpenGL().m_glPixelTransferf(GL_BLUE_BIAS, previous_blue_bias);
			GlobalOpenGL().m_glPixelTransferf(GL_GREEN_BIAS, previous_green_bias);
			GlobalOpenGL().m_glPixelTransferf(GL_RED_BIAS, previous_red_bias);
			GlobalOpenGL().m_glBlendFunc(previous_blend_func_src, previous_blend_func_dst);
			if (!previous_blend_enabled) { GlobalOpenGL().m_glDisable(GL_BLEND); }
			GlobalOpenGL().m_glPixelStorei(GL_UNPACK_ALIGNMENT, previous_unpack_alignment);
		}

		g_object_unref(G_OBJECT(layout));
	}

	virtual int getPixelAscent() const
	{
		return font_ascent;
	}
	virtual int getPixelDescent() const
	{
		return font_descent;
	}
	virtual int getPixelHeight() const
	{
		return font_height;
	}
};

GLFont *glfont_create(const char* font_string)
{
	return new GLFontInternal(font_string);
}

#endif
