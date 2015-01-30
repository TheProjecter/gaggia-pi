//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_image.h>

#include "button.h"
#include "logger.h"
#include "singleton.h"

//-----------------------------------------------------------------------------

Button::Button(int x, int y, const std::string& file, SDL_PixelFormat* displayFormat) 
    :_buttonSurface( nullptr )
    ,_rect( nullptr )
    ,_good( false )
{
    // Temporary storage for the image that's loaded
    SDL_Surface* loadedImage = IMG_Load( file.c_str() );

    if ( loadedImage == NULL) {
        LogError( "Could not load image " << file.c_str() );
        return;
    }

    // Convert surface to screen format
    _buttonSurface = SDL_ConvertSurface( loadedImage, displayFormat, 0 );

    if( _buttonSurface == nullptr ) {
        LogError( "Unable to optimize image! SDL Error: " << SDL_GetError() );
        return;
    }

    SDL_FreeSurface( loadedImage );
  
    _rect = new SDL_Rect();

    _rect->x = x;
    _rect->y = y;
    _rect->w = _buttonSurface->w;
    _rect->h = _buttonSurface->h;
  
    _good = true;
}

//-----------------------------------------------------------------------------

Button::~Button() {
    if ( _good ) {
        SDL_FreeSurface( _buttonSurface );
    }
    if ( _rect ) {
        delete _rect;
    }
}

//-----------------------------------------------------------------------------

bool Button::isClicked(int x, int y) const {
    if ( !_good ) {
        return false;
    }
  
    return ( x >= _rect->x && x <= _rect->x + _rect->w && y >= _rect->y && y <= _rect->y + _rect->h );
}

//-----------------------------------------------------------------------------

void Button::draw(SDL_Surface* destination) const {
    if ( !_good ) {
        return;
    }
  
    SDL_Rect offset;
    offset.x = _rect->x;
    offset.y = _rect->y;
  
    SDL_BlitSurface( _buttonSurface, NULL, destination, &offset );
}

//-----------------------------------------------------------------------------
