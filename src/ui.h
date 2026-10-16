#pragma once
#include <cairo.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Minimal software canvas on a cairo ARGB32 surface.
class Canvas
{
public:
	Canvas( int width, int height );
	~Canvas();
	Canvas( const Canvas & ) = delete;
	Canvas &operator=( const Canvas & ) = delete;

	int width() const { return m_w; }
	int height() const { return m_h; }
	cairo_t *cr() const { return m_cr; }

	void Clear( double r, double g, double b );
	void RoundRect( double x, double y, double w, double h, double radius, double r, double g, double b );
	void Text( const std::string &s, double x, double y, double size, double r, double g, double b, bool bold = false );
	double TextWidth( const std::string &s, double size, bool bold = false );
	// Tightly packed RGBA8, un-premultiplied (dest holds width*height*4 bytes).
	void CopyRGBA( uint8_t *dest ) const;
	bool WritePng( const char *path ) const;

private:
	int m_w, m_h;
	cairo_surface_t *m_surface;
	cairo_t *m_cr;
};

// Immediate-mode buttons, rebuilt on every redraw.
struct Button
{
	double x, y, w, h;
	std::string label;
	bool active;
	std::function<void()> onClick;
};

class ButtonPanel
{
public:
	void Clear() { m_buttons.clear(); }
	void Add( Button b ) { m_buttons.push_back( std::move( b ) ); }
	const std::vector<Button> &buttons() const { return m_buttons; }
	// Index of the button under (x, y) in canvas coordinates (origin top-left), or -1.
	int HitTest( double x, double y ) const;
	void Draw( Canvas &c, int hovered ) const;

private:
	std::vector<Button> m_buttons;
};
