/*
vgui_surf.cpp - main vgui layer
Copyright (C) 2011 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

In addition, as a special exception, the author gives permission
to link the code of this program with VGUI library developed by
Valve, L.L.C ("Valve"). You must obey the GNU General Public License
in all respects for all of the code used other than VGUI library.
If you modify this file, you may extend this exception to your
version of the file, but you are not obligated to do so. If
you do not wish to do so, delete this exception statement
from your version.

*/

#include <ctype.h>
#include "vgui_main.h"

#define MAX_PAINT_STACK	16
#define FONT_SIZE		512
#define FONT_PAGES	8

struct FontInfo
{
	int	id;
	int	pageCount;
	int	pageForChar[256];
	int	bindIndex[FONT_PAGES];
	float	texCoord[256][FONT_PAGES];
	int	contextCount;
};

static int staticContextCount = 0;
static char staticRGBA[FONT_SIZE * FONT_SIZE * 4];
static Font* staticFont = NULL;
static Dar<Font*> staticFontDar;
static FontInfo* staticFontInfo;
static Dar<FontInfo*> staticFontInfoDar;
static PaintStack paintStack[MAX_PAINT_STACK];
static int staticPaintStackPos = 0;

#define ColorIndex( c )((( c ) - '0' ) & 7 )

CEngineSurface :: CEngineSurface( Panel *embeddedPanel ):SurfaceBase( embeddedPanel )
{
	_embeddedPanel = embeddedPanel;
	_drawColor[0] = _drawColor[1] = _drawColor[2] = _drawColor[3] = 255;
	_drawTextColor[0] = _drawTextColor[1] = _drawTextColor[2] = _drawTextColor[3] = 255;

	_surfaceExtents[0] = _surfaceExtents[1] = 0;
	//_surfaceExtents[2] = menu.globals->scrWidth;
	//_surfaceExtents[3] = menu.globals->scrHeight;
	embeddedPanel->getSize(_surfaceExtents[2], _surfaceExtents[3]);
	_drawTextPos[0] = _drawTextPos[1] = 0;

	staticFont = NULL;
	staticFontDar.setCount( 0 );
	staticFontInfo = NULL;
	staticFontInfoDar.setCount( 0 );
	staticPaintStackPos = 0;
	staticContextCount++;

	_translateX = _translateY = 0;
	_currentTexture = 0;
}

CEngineSurface :: ~CEngineSurface( void )
{
	g_api->DrawShutdown ();
}

Panel *CEngineSurface :: getEmbeddedPanel( void )
{
	return _embeddedPanel;
}

bool CEngineSurface :: hasFocus( void )
{
	// What differs when window does not has focus?
	//return host.state != HOST_NOFOCUS;
	return true;
}
	
void CEngineSurface :: setCursor( Cursor *cursor )
{
	_currentCursor = cursor;
	g_api->CursorSelect( (VGUI_DefaultCursor)cursor->getDefaultCursor() );
}

void CEngineSurface :: SetupPaintState( const PaintStack *paintState )
{
	_translateX = paintState->iTranslateX;
	_translateY = paintState->iTranslateY;
	SetScissorRect( paintState->iScissorLeft, paintState->iScissorTop,
		paintState->iScissorRight, paintState->iScissorBottom );
}

void CEngineSurface :: InitVertex( vpoint_t &vertex, int x, int y, float u, float v )
{
	vertex.point[0] = x + _translateX;
	vertex.point[1] = y + _translateY;
	vertex.coord[0] = u;
	vertex.coord[1] = v;
}

int CEngineSurface :: createNewTextureID( void )
{
	return g_api->GenerateTexture();
}

void CEngineSurface :: drawSetColor( int r, int g, int b, int a )
{
	_drawColor[0] = r;
	_drawColor[1] = g;
	_drawColor[2] = b;
	_drawColor[3] = a;
}

void CEngineSurface :: drawSetTextColor( int r, int g, int b, int a )
{
	_drawTextColor[0] = r;
	_drawTextColor[1] = g;
	_drawTextColor[2] = b;
	_drawTextColor[3] = a;
}

void CEngineSurface :: drawFilledRect( int x0, int y0, int x1, int y1 )
{
	vpoint_t rect[2];
	vpoint_t clippedRect[2];

	if( _drawColor[3] >= 255 ) return;

	InitVertex( rect[0], x0, y0, 0, 0 );
	InitVertex( rect[1], x1, y1, 0, 0 );

	// fully clipped?
	if( !ClipRect( rect[0], rect[1], &clippedRect[0], &clippedRect[1] ))
		return;	

	g_api->SetupDrawingRect( _drawColor );	
	g_api->EnableTexture( false );
	g_api->DrawQuad( &clippedRect[0], &clippedRect[1] );
	g_api->EnableTexture( true );
}

void CEngineSurface :: drawOutlinedRect( int x0, int y0, int x1, int y1 )
{
	if( _drawColor[3] >= 255 ) return;

	drawFilledRect( x0, y0, x1, y0 + 1 );		// top
	drawFilledRect( x0, y1 - 1, x1, y1 );		// bottom
	drawFilledRect( x0, y0 + 1, x0 + 1, y1 - 1 );	// left
	drawFilledRect( x1 - 1, y0 + 1, x1, y1 - 1 );	// right
}
	
void CEngineSurface :: drawSetTextFont( Font *font )
{
	staticFont = font;

	if( font )
	{
		bool	buildFont = false;

		staticFontInfo = NULL;

		for( int i = 0; i < staticFontInfoDar.getCount(); i++ )
		{
			if( staticFontInfoDar[i]->id == font->getId( ))
			{
				staticFontInfo = staticFontInfoDar[i];
				if( staticFontInfo->contextCount != staticContextCount )
					buildFont = true;
			}
		}

		if( !staticFontInfo || buildFont )
		{
			staticFontInfo = new FontInfo;
			staticFontInfo->id = 0;
			staticFontInfo->pageCount = 0;
			staticFontInfo->bindIndex[0] = 0;
			staticFontInfo->bindIndex[1] = 0;
			staticFontInfo->bindIndex[2] = 0;
			staticFontInfo->bindIndex[3] = 0;
			memset( staticFontInfo->pageForChar, 0, sizeof( staticFontInfo->pageForChar ));
			staticFontInfo->contextCount = -1;
			staticFontInfo->id = staticFont->getId();
			staticFontInfoDar.putElement( staticFontInfo );
			staticFontInfo->contextCount = staticContextCount;

			int currentPage = 0;
			int x = 0, y = 0;

			memset( staticRGBA, 0, sizeof( staticRGBA ));

			for( int i = 0; i < 256; i++ )
			{
				int abcA, abcB, abcC;
				staticFont->getCharABCwide( i, abcA, abcB, abcC );

				int wide = abcB;

				if( isspace( i )) continue;

				int tall = staticFont->getTall();

				if( x + wide + 1 > FONT_SIZE )
				{
					x = 0;
					y += tall + 1;
				}

				if( y + tall + 1 > FONT_SIZE )
				{
					if( !staticFontInfo->bindIndex[currentPage] )
						staticFontInfo->bindIndex[currentPage] = createNewTextureID();
					drawSetTextureRGBA( staticFontInfo->bindIndex[currentPage], staticRGBA, FONT_SIZE, FONT_SIZE );
					currentPage++;

					if( currentPage == FONT_PAGES )
						break;

					memset( staticRGBA, 0, sizeof( staticRGBA ));
					x = y = 0;
				}

				staticFont->getCharRGBA( i, x, y, FONT_SIZE, FONT_SIZE, (byte *)staticRGBA );
				staticFontInfo->pageForChar[i] = currentPage;
				staticFontInfo->texCoord[i][0] = (float)((double)x / (double)FONT_SIZE );
				staticFontInfo->texCoord[i][1] = (float)((double)y / (double)FONT_SIZE );
				staticFontInfo->texCoord[i][2] = (float)((double)(x + wide)/(double)FONT_SIZE );
				staticFontInfo->texCoord[i][3] = (float)((double)(y + tall)/(double)FONT_SIZE );
				x += wide + 1;
			}

			if( currentPage != FONT_PAGES )
			{
				if( !staticFontInfo->bindIndex[currentPage] )
					staticFontInfo->bindIndex[currentPage] = createNewTextureID();
				drawSetTextureRGBA( staticFontInfo->bindIndex[currentPage], staticRGBA, FONT_SIZE, FONT_SIZE );
			}
			staticFontInfo->pageCount = currentPage + 1;
		}
	}
}

void CEngineSurface :: drawSetTextPos( int x, int y )
{
	_drawTextPos[0] = x;
	_drawTextPos[1] = y;
}

void CEngineSurface :: drawPrintChar( int x, int y, int wide, int tall, float s0, float t0, float s1, float t1, int color[4], bool additive )
{
	vpoint_t	ul, lr;

	ul.point[0] = x;
	ul.point[1] = y;
	lr.point[0] = x + wide;
	lr.point[1] = y + tall;

	// gets at the texture coords for this character in its texture page
	ul.coord[0] = s0;
	ul.coord[1] = t0;
	lr.coord[0] = s1;
	lr.coord[1] = t1;

	vpoint_t clippedRect[2];

	if( !ClipRect( ul, lr, &clippedRect[0], &clippedRect[1] ))
		return;

	if (additive)
	{
		if (g_api->SetupDrawingTextAdditive)
			g_api->SetupDrawingTextAdditive(color);
	}
	else
	{
		g_api->SetupDrawingText(color);
	}

	g_api->DrawQuad( &clippedRect[0], &clippedRect[1] ); // draw the letter
}

void CEngineSurface :: drawPrintText( const char* text, int textLen )
{
	//return;
	static bool hasColor = 0;
	static int numColor = 7;

	if( !text || !staticFont || _drawTextColor[3] >= 255 )
		return;

	int x = _drawTextPos[0] + _translateX;
	int y = _drawTextPos[1] + _translateY;

	int tall = staticFont->getTall();

	int j, iTotalWidth = 0;
	int curTextColor[4];

	//  HACKHACK: allow color strings in VGUI
	if( numColor != 7 )
	{
		for( j = 0; j < 3; j++ ) // grab predefined color
			curTextColor[j] = g_api->GetColor(numColor,j);
	}
	else
	{
		for( j = 0; j < 3; j++ ) // revert default color
			curTextColor[j] = _drawTextColor[j];
	}
	curTextColor[3] = _drawTextColor[3]; // copy alpha

	if( textLen == 1 )
	{
		if( *text == '^' )
		{
			hasColor = true;
			return; // skip '^'
		}
		else if( hasColor && isdigit( *text ))
		{
			numColor = ColorIndex( *text );
			hasColor = false; // handled
			return; // skip colornum
		}
		else hasColor = false;
	}
	for( int i = 0; i < textLen; i++ )
	{
		int curCh = g_api->ProcessUtfChar( (unsigned char)text[i] );
		if( !curCh )
		{
			continue;
		}

		int abcA, abcB, abcC;

		staticFont->getCharABCwide( curCh, abcA, abcB, abcC );

		float s0 = staticFontInfo->texCoord[curCh][0];
		float t0 = staticFontInfo->texCoord[curCh][1];
		float s1 = staticFontInfo->texCoord[curCh][2];
		float t1 = staticFontInfo->texCoord[curCh][3];
		int wide = abcB;

		iTotalWidth += abcA;
		drawSetTexture( staticFontInfo->bindIndex[staticFontInfo->pageForChar[curCh]] );
		drawPrintChar( x + iTotalWidth, y, wide, tall, s0, t0, s1, t1, curTextColor, false );
		iTotalWidth += wide + abcC;
	}

	_drawTextPos[0] += iTotalWidth;
}

void CEngineSurface :: drawSetTextureRGBA( int id, const char* rgba, int wide, int tall )
{
	g_api->UploadTexture( id, rgba, wide, tall );
}
	
void CEngineSurface :: drawSetTexture( int id )
{
	g_api->BindTexture( id );
	_currentTexture = id;
}
	
void CEngineSurface :: drawTexturedRect( int x0, int y0, int x1, int y1 )
{
	vpoint_t rect[2];
	vpoint_t clippedRect[2];

	InitVertex( rect[0], x0, y0, 0, 0 );
	InitVertex( rect[1], x1, y1, 1, 1 );

	// fully clipped?
	if( !ClipRect( rect[0], rect[1], &clippedRect[0], &clippedRect[1] ))
		return;	

	g_api->SetupDrawingImage( _drawColor );	
	g_api->DrawQuad( &clippedRect[0], &clippedRect[1] );
}
	
void CEngineSurface :: pushMakeCurrent( Panel* panel, bool useInsets )
{
	int insets[4] = { 0, 0, 0, 0 };
	int absExtents[4];
	int clipRect[4];

	if( useInsets )
		panel->getInset( insets[0], insets[1], insets[2], insets[3] );
	panel->getAbsExtents( absExtents[0], absExtents[1], absExtents[2], absExtents[3] );
	panel->getClipRect( clipRect[0], clipRect[1], clipRect[2], clipRect[3] );

	PaintStack *paintState = &paintStack[staticPaintStackPos];

	assert( staticPaintStackPos < MAX_PAINT_STACK );

	paintState->m_pPanel = panel;

	// determine corrected top left origin
	paintState->iTranslateX = insets[0] + absExtents[0];
	paintState->iTranslateY = insets[1] + absExtents[1];
	// setup clipping rectangle for scissoring
	paintState->iScissorLeft = clipRect[0];
	paintState->iScissorTop = clipRect[1];
	paintState->iScissorRight = clipRect[2];
	paintState->iScissorBottom = clipRect[3];

	SetupPaintState( paintState );
	staticPaintStackPos++;
}
	
void CEngineSurface :: popMakeCurrent( Panel *panel )
{
	int top = staticPaintStackPos - 1;

	// more pops that pushes?
	assert( top >= 0 );

	// didn't pop in reverse order of push?
	assert( paintStack[top].m_pPanel == panel );

	staticPaintStackPos--;

	if( top > 0 ) SetupPaintState( &paintStack[top-1] );
}

bool CEngineSurface :: setFullscreenMode( int wide, int tall, int bpp )
{
	// NOTE: Xash3D always working in 32-bit mode
	// Skip it now. VGUI cannot change video modes
	return false;
}
	
void CEngineSurface :: setWindowedMode( void )
{
	// Skip it now. VGUI cannot change video modes
	/*
	Cvar_SetFloat( "fullscreen", 0.0f );
	*/
}

void CEngineSurface :: PushMakeCurrent(int insets[4], int absExtents[4], int clipRect[4])
{
	PaintStack *paintState = &paintStack[staticPaintStackPos];

	assert( staticPaintStackPos < MAX_PAINT_STACK );

	// determine corrected top left origin
	paintState->iTranslateX = insets[0] + absExtents[0];
	paintState->iTranslateY = insets[1] + absExtents[1];
	// setup clipping rectangle for scissoring
	paintState->iScissorLeft = clipRect[0];
	paintState->iScissorTop = clipRect[1];
	paintState->iScissorRight = clipRect[2];
	paintState->iScissorBottom = clipRect[3];

	SetupPaintState( paintState );
	staticPaintStackPos++;
}

void CEngineSurface :: PopMakeCurrent()
{
	int top = staticPaintStackPos - 1;

	// more pops that pushes?
	assert( top >= 0 );

	staticPaintStackPos--;

	if( top > 0 ) SetupPaintState( &paintStack[top-1] );
}

void CEngineSurface :: DrawSetColor(int r, int g, int b, int a)
{
	drawSetColor(r, g, b, 255 - a);
}

void CEngineSurface :: DrawFilledRect(int x0, int y0, int x1, int y1)
{
	drawFilledRect(x0, y0, x1, y1);
}

void CEngineSurface :: DrawOutlinedRect(int x0, int y0, int x1, int y1)
{
	drawOutlinedRect(x0, y0, x1, y1);
}

void CEngineSurface :: DrawSetTextFont(int font)
{
	if (font < 0 || font >= staticFontDar.getCount())
		return;

	drawSetTextFont(staticFontDar[font]);
}

void CEngineSurface :: DrawSetTextColor(int r, int g, int b, int a)
{
	drawSetTextColor(r, g, b, 255 - a);
}

void CEngineSurface :: DrawGetTextColor(int &r, int &g, int &b, int &a)
{
	r = _drawTextColor[0];
	g = _drawTextColor[1];
	b = _drawTextColor[2];
	a = 255 - _drawTextColor[3];
}

void CEngineSurface :: DrawSetTextPos(int x, int y)
{
	drawSetTextPos(x, y);
}

void CEngineSurface :: DrawGetTextPos(int &x, int &y)
{
	x = _drawTextPos[0];
	y = _drawTextPos[1];
}

void CEngineSurface :: DrawPrintText(const wchar_t *text, int textLen)
{
	// convert to single byte for now
	char *singleByteText = new char[textLen + 1];
	int singleByteTextLen = 0;

	for (int i = 0; i < textLen; ++i)
	{
		int curCh = g_api->ProcessUtfChar(text[i]);
		if (!curCh)
			continue;
		
		singleByteText[singleByteTextLen++] = curCh;
	}

	drawPrintText(singleByteText, singleByteTextLen);
}

void CEngineSurface :: DrawUnicodeChar(wchar_t wch, bool additive)
{
	int curCh = g_api->ProcessUtfChar(wch);
	if (!curCh)
		return;

	int x = _drawTextPos[0] + _translateX;
	int y = _drawTextPos[1] + _translateY;

	int abcA, abcB, abcC;
	staticFont->getCharABCwide( curCh, abcA, abcB, abcC );

	float s0 = staticFontInfo->texCoord[curCh][0];
	float t0 = staticFontInfo->texCoord[curCh][1];
	float s1 = staticFontInfo->texCoord[curCh][2];
	float t1 = staticFontInfo->texCoord[curCh][3];
	int wide = abcB;
	int tall = staticFont->getTall();

	drawSetTexture(staticFontInfo->bindIndex[staticFontInfo->pageForChar[curCh]]);
	drawPrintChar(x + abcA, y, wide, tall, s0, t0, s1, t1, _drawTextColor, additive);
}

void CEngineSurface :: DrawSetTextureFile(int id, const char *filename)
{
	if (g_api->UploadTextureFile)
	{
		drawSetTexture(id);
		g_api->UploadTextureFile(id, filename);
	}
}

void CEngineSurface :: DrawSetTextureRGBA(int id, const unsigned char *rgba, int wide, int tall)
{
	drawSetTextureRGBA(id, (const char *)rgba, wide, tall);
}

void CEngineSurface :: DrawSetTexture(int id)
{
	drawSetTexture(id);
}

void CEngineSurface :: DrawGetTextureSize(int id, int &wide, int &tall)
{
	g_api->BindTexture(id);

	int _wide, _tall;
	g_api->GetTextureSizes(&_wide, &_tall);
	wide = _wide;
	tall = _tall;

	g_api->BindTexture(_currentTexture);
}

void CEngineSurface :: DrawTexturedRect(int x0, int y0, int x1, int y1)
{
	drawTexturedRect(x0, y0, x1, y1);
}

int CEngineSurface :: CreateNewTextureID()
{
	return createNewTextureID();
}

bool CEngineSurface :: DeleteTextureByID(int id)
{
	return false;
}

void CEngineSurface :: DrawUpdateRegionTextureBGRA(int nTextureID, int x, int y, const unsigned char *pchData, int wide, int tall)
{
	if (g_api->UploadTextureBlockBGRA)
	{
		// IDK why valve does this
		drawSetTexture(nTextureID);
		int texWide, texTall;
		g_api->GetTextureSizes(&texWide, &texTall);
		g_api->UploadTextureBlockBGRA(nTextureID, 0, y, pchData + 4 * y * texWide, texWide, tall);
	}
}

void CEngineSurface :: DrawSetTextureBGRA(int id, const unsigned char *bgra, int wide, int tall)
{
	if (g_api->UploadTextureBGRA)
		g_api->UploadTextureBGRA(id, (const char *)bgra, wide, tall);
}

int CEngineSurface :: CreateFont()
{
	staticFontDar.addElement(nullptr);
	return staticFontDar.getCount() - 1;
}

bool CEngineSurface :: AddGlyphSetToFont(int font, const char *fontName, int tall, int weight, int flags)
{
	if (font < 0 || font >= staticFontDar.getCount() || staticFontDar[font] != nullptr)
		return false;

	staticFontDar.setElementAt(new Font(fontName, tall, 0, 0.0f, weight, flags & FONTFLAG_ITALIC, flags & FONTFLAG_UNDERLINE, flags & FONTFLAG_STRIKEOUT, flags & FONTFLAG_SYMBOL), font);
	return true;
}

bool CEngineSurface :: AddCustomFontFile(const char *fontFileName)
{
	return false;
}

int CEngineSurface :: GetFontTall(int font)
{
	if (font < 0 || font >= staticFontDar.getCount() || staticFontDar[font] == nullptr)
		return 0;

	return staticFontDar[font]->getTall();
}

void CEngineSurface :: GetCharABCwide(int font, int ch, int &a, int &b, int &c)
{
	if (font < 0 || font >= staticFontDar.getCount() || staticFontDar[font] == nullptr)
		return;

	return staticFontDar[font]->getCharABCwide(ch, a, b, c);
}

int CEngineSurface :: GetCharacterWidth(int font, int ch)
{
	if (font < 0 || font >= staticFontDar.getCount() || staticFontDar[font] == nullptr)
		return 0;

	int a, b, c;
	staticFontDar[font]->getCharABCwide(ch, a, b, c);
	return a + b + c;
}

void CEngineSurface :: GetTextSize(int font, const wchar_t *text, int &wide, int &tall)
{
	if (font < 0 || font >= staticFontDar.getCount() || staticFontDar[font] == nullptr)
		return;

	tall = staticFontDar[font]->getTall();
	wide = 0;

	for (; *text != 0; ++text)
	{
		int a, b, c;
		staticFontDar[font]->getCharABCwide(*text, a, b, c);
		wide += a + b + c;
	}
}

void CEngineSurface :: GetScreenSize(int &wide, int &tall)
{
	_embeddedPanel->getSize(wide, tall);
}

void CEngineSurface :: SetCursor(int cursor)
{
	_embeddedPanel->setCursor(new Cursor((Cursor::DefaultCursor)cursor));
}

int CEngineSurface :: GetCursor()
{
	return _embeddedPanel->getCursor()->getDefaultCursor();
}

void CEngineSurface :: GetCursorPos(int &x, int &y)
{
	GetMousePos(x, y);
}
