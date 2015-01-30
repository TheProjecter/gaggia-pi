//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#ifndef __BUTTON_H__
#define __BUTTON_H__

//-----------------------------------------------------------------------------

#include <string>

//-----------------------------------------------------------------------------
// Forward decls

struct SDL_PixelFormat;
struct SDL_Surface;
struct SDL_Rect;

//-----------------------------------------------------------------------------

class Button {
public:
    Button(int x, int y, const std::string& file, SDL_PixelFormat* displayFormat);
    ~Button();

    bool isClicked(int x, int y) const;
    void draw(SDL_Surface* destination) const;
  
private:
    SDL_Surface* _buttonSurface;
    SDL_Rect* _rect;
    bool _good;
  
}; // Button

//-----------------------------------------------------------------------------

#endif //__BUTTON_H__
