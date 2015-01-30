//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#ifndef __DISPLAY_H__
#define __DISPLAY_H__

//-----------------------------------------------------------------------------

#include <string>
#include <map>
#include <thread>
#include <mutex>

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_image.h>

//#include "button.h"

//-----------------------------------------------------------------------------
 // Forward decls

class Timer;

//-----------------------------------------------------------------------------

class Display {
public:
    Display();
    ~Display();

    bool getShutdown() const;
    bool ready() const;
    
private:
    struct DisplayMode {
        enum Value {
            MainScreen,
            Temperature,
            Shutdown
        };
    };

    struct UIElementName {
        enum Value {
            /*ButtonBGDefault,
            ButtonBGClicked,
            ButtonBGTriggered,*/
            /*Button30ML,
            Button60ML,*/
            ButtonOneCup,
            ButtonTwoCups,
            ButtonShutdown,
            ButtonSteamActive,
            ButtonSteamInactive,
            ButtonBack,
            ButtonCancel,
            ButtonOkay,
            ButtonMinus,
            ButtonPlus
        };
    };

    struct UIElement {
        SDL_Surface* surface;
        SDL_Rect rect;
    };

    void _open();
    void _close();
    void _worker();
    void _render();
    void _handleClickEvent( int x, int y );

    bool _initUIElements();

    void _drawText( TTF_Font* font, short x, short y, const std::string& text, SDL_Color foregroundColour, SDL_Color backgroundColour );
    SDL_Surface* _createTextSurface( TTF_Font* font, const std::string& text, SDL_Color foregroundColour, SDL_Color backgroundColour );

    bool _addUIElement( const UIElementName::Value& name, const std::string& file, int x = 0, int y = 0 );
    void _drawUIElement( UIElement* uiElement );
    void _drawUIElementPosition( UIElement* uiElement, int x, int y );
    bool _clickedUIElement( UIElement* uiElement, int x, int y );

    std::string _getStatusText() const;

    bool _opened;
    bool _shutdown;

    int _width;
    int    _height;
    SDL_Surface* _display;

    TTF_Font* _tempFont;
    TTF_Font* _targetTempFont;
    TTF_Font* _headerFont;
    TTF_Font* _flowFont;
    TTF_Font* _stateFont;
    TTF_Font* _infoFont;

    std::map<UIElementName::Value, UIElement*> _uiElements;

    // Flow meter animation
    Timer* _flowTimer;
    bool _wasFlowing;
    double _oldFlowVolume;
    
    DisplayMode::Value _currentMode;

    bool _run;
    std::thread* _thread;
    std::mutex* _mutex;
};

//-----------------------------------------------------------------------------

#endif // __DISPLAY_H__
