//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#include <sstream>

#include <iostream>
#include <iomanip>
#include <math.h>
#include <stdio.h>

#include "gaggia.h"
#include "display.h"
#include "timing.h"
#include "logger.h"
#include "settings.h"
#include "singleton.h"

//-----------------------------------------------------------------------------

Display::Display()
    :_opened( false )
    ,_shutdown( false )
    ,_width( 320 )
    ,_height( 240 )
    ,_display( nullptr )
    ,_tempFont( nullptr )
    ,_targetTempFont( nullptr )
    ,_headerFont( nullptr )
    ,_flowFont( nullptr )
    ,_stateFont( nullptr )
    ,_infoFont( nullptr )
    ,_flowTimer( nullptr )
    ,_wasFlowing( false )
    ,_oldFlowVolume( 0.0 )
    ,_currentMode( DisplayMode::MainScreen )
    ,_run( false )
    ,_thread( nullptr )
    ,_mutex( nullptr )
{
    _open();
}

//-----------------------------------------------------------------------------

Display::~Display() {
    _close();
}

//-----------------------------------------------------------------------------

bool Display::ready() const {
    return _opened;
}

//-----------------------------------------------------------------------------

void Display::_open() {
    static const char *table[] = {
        "TSLIB_TSDEVICE=/dev/input/event0",
            "TSLIB_TSEVENTTYPE=INPUT",
            "TSLIB_CONFFILE=/etc/ts.conf",
            "TSLIB_CALIBFILE=/etc/pointercal",
            "SDL_FBDEV=/dev/fb1",
            "SDL_MOUSEDRV=TSLIB",
            "SDL_MOUSEDEV=/dev/input/event0",
            "SDL_NOMOUSE=1",
            "SDL_VIDEODRIVER=FBCON",
            0
    };

    for ( size_t i = 0; table[ i ] != 0; ++i ) {
        putenv( ( char* )table[ i ] );
    }
    
    if ( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
        LogError("Could not initialize SDL");
        _close();
        return;
    }

    const SDL_VideoInfo* videoInfo = SDL_GetVideoInfo();
   
    if ( videoInfo == nullptr ) {
        LogError("Could not obtain the video format for SDL");
        _close();
        return;
    }
    
    _width  = videoInfo->current_w;
    _height = videoInfo->current_h;

    _display = SDL_SetVideoMode( videoInfo->current_w, videoInfo->current_h, videoInfo->vfmt->BitsPerPixel, SDL_DOUBLEBUF );
    
    if ( _display == 0 ) {
        LogError( "Display could not be opened" );
        _close();
        return;
    }

    // initialise TTF
    if ( TTF_Init() < 0 ) {
        _close();
        return;
    }

    _tempFont = TTF_OpenFont( "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf", 72 );
    _targetTempFont = TTF_OpenFont( "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf", 32 );
    _headerFont = TTF_OpenFont( "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf", 32 );
    _flowFont = TTF_OpenFont( "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf", 36 );
    _stateFont = TTF_OpenFont( "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf", 40 );
    _infoFont = TTF_OpenFont( "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf", 16 );

    if ( _tempFont == nullptr || _targetTempFont == nullptr || _flowFont == nullptr || _stateFont == nullptr || _infoFont == nullptr ) {
        LogError( "TTF font could not initialize!" );
        _close();
        return;
    }
        
    // Hide mouse pointer
    SDL_ShowCursor( 0 );
    
     //Initialize PNG loading
    int imgFlags = IMG_INIT_PNG;
    if( !( IMG_Init( imgFlags ) & imgFlags ) ) {
        LogError( "SDL_image could not initialize!" );
        _close();
        return;
    }
    
   // _shutdownButton = new Button( 310 - 61 - 10, 10, "shutdown.png", _display->format );
    
    if ( !_initUIElements() ) {
        LogError("Could not initialize UI elements");
        _close();
        return;
    }

    _flowTimer = new Timer();
    _flowTimer->stop();

    _opened = true;
    _run = true;
    _mutex = new std::mutex;
    _thread = new std::thread( &Display::_worker, this );
}

//-----------------------------------------------------------------------------

void Display::_close() {
    if ( _run ) {
        _run = false;
        _thread->join();
        delete _thread;
    }

    for ( auto iter = _uiElements.begin(); iter != _uiElements.end(); ++iter ) {
        SDL_FreeSurface( iter->second->surface );
    }

    if ( _tempFont ) {
        TTF_CloseFont( _tempFont );
    }

    if ( _targetTempFont ) {
        TTF_CloseFont( _targetTempFont );
    }

    if ( _headerFont ) {
        TTF_CloseFont( _headerFont );
    }
    
    if ( _flowFont ) {
        TTF_CloseFont( _flowFont );
    }

    if ( _stateFont ) {
        TTF_CloseFont( _stateFont );
    }

    if ( _infoFont ) {
        TTF_CloseFont( _infoFont );
    }

    if ( _flowTimer ) {
        delete _flowTimer;
    }

    TTF_Quit();
    SDL_Quit();    
}

//-----------------------------------------------------------------------------

bool Display::getShutdown() const {
    std::lock_guard<std::mutex> lock( *_mutex );
    return _shutdown;
}

//-----------------------------------------------------------------------------

void Display::_handleClickEvent( int x, int y ) {
    if ( _currentMode == DisplayMode::MainScreen ) {
        if ( _clickedUIElement( _uiElements[ UIElementName::ButtonOneCup ], x, y )) {
            Singleton<Gaggia>::pointer()->extractOneCup();
        }
        else if ( _clickedUIElement( _uiElements[ UIElementName::ButtonTwoCups ], x, y )) {
            Singleton<Gaggia>::pointer()->extractTwoCups();
        }
        else if ( _clickedUIElement( _uiElements[ UIElementName::ButtonSteamActive ], x, y )) {
            const bool steam = Singleton<Gaggia>::pointer()->getSteamMode();
            Singleton<Gaggia>::pointer()->setSteamMode( !steam );
        }
        else if ( _clickedUIElement( _uiElements[ UIElementName::ButtonShutdown ], x, y )) {
            //std::lock_guard<std::mutex> lock( *_mutex );
            //_shutdown = true;
            _currentMode = DisplayMode::Shutdown;
        }
    }
    else if ( _currentMode == DisplayMode::Shutdown ) {
        if ( _clickedUIElement( _uiElements[ UIElementName::ButtonOkay ], x, y )) {
            std::lock_guard<std::mutex> lock( *_mutex );
            _shutdown = true;
        }
        else if ( _clickedUIElement( _uiElements[ UIElementName::ButtonCancel ], x, y )) {
            _currentMode = DisplayMode::MainScreen;
        }
    }
}

//-----------------------------------------------------------------------------

void Display::_worker() {
    SDL_Event event;

    while ( _run ) {
        while( SDL_PollEvent( &event ) ) {
            switch(event.type) {      
                case SDL_MOUSEBUTTONDOWN: {
                    break;
                }
        
                case SDL_MOUSEBUTTONUP: {         
                    _handleClickEvent( event.button.x, event.button.y );
                    break;
                }
          
                case SDL_MOUSEMOTION: {
                    break;
                }
            
                default: {
                }
            }
        }

        _render();
        delayms(10);
    }
}

//-----------------------------------------------------------------------------

void Display::_render() {
    if ( !_run ) {
        return;
    }

    const SDL_Color blackColor  = {   0,   0,   0, 255 };
    const SDL_Color whiteColor  = { 255, 255, 255, 255 };
    const SDL_Color greyColor  =  { 125, 125, 125, 255 };

    SDL_Rect displayRect = { 0, 0, static_cast<short unsigned int>( _width ), static_cast<short unsigned int>( _height ) };
    SDL_FillRect( _display, &displayRect, SDL_MapRGB( _display->format, 0, 0, 0 ) );

    if ( _currentMode == DisplayMode::MainScreen ) {
        const bool steamActive = Singleton<Gaggia>::pointer()->getSteamMode();
        const Gaggia::State::Value gaggiaState = Singleton<Gaggia>::pointer()->getState();
        const bool gaggiaHeating = Singleton<Gaggia>::pointer()->getPowerRegulator();

        _drawUIElement( _uiElements[ UIElementName::ButtonOneCup ] );
        _drawUIElement( _uiElements[ UIElementName::ButtonTwoCups ] );
        
        if ( steamActive ) {
            _drawUIElement( _uiElements[ UIElementName::ButtonSteamActive ] );
        }
        else {
            _drawUIElement( _uiElements[ UIElementName::ButtonSteamInactive ] );
        }

        _drawUIElement( _uiElements[ UIElementName::ButtonShutdown ] );

        const double currentTemperature = Singleton<Gaggia>::pointer()->getBoilerTemperature();
        const double targetTemperature  = Singleton<Gaggia>::pointer()->getBoilerTargetTemperature();    

        // Current temperature
        std::stringstream text;
        text << std::fixed << std::setprecision(1) << currentTemperature << "°";
        _drawText( _tempFont, 15, 10, text.str(), whiteColor, blackColor );

        // Target temperature
        if ( gaggiaHeating ) {
            text.str( std::string() );
            text << "/ " << std::fixed << std::setprecision(1) << targetTemperature << "°";
            _drawText( _targetTempFont, 60, 80, text.str(), greyColor, blackColor );
        }

        // Status text
        const std::string state = _getStatusText();
        _drawText( _infoFont, 15, 130, state, whiteColor, blackColor );

        bool showRemainingFlow = false;
        bool extraction = gaggiaState == Gaggia::State::Extracting || gaggiaState == Gaggia::State::ExtractingOneCup || gaggiaState == Gaggia::State::ExtractingTwoCups;

        if ( extraction && !_wasFlowing ) {
            _wasFlowing = true;
            _flowTimer->stop();
        }

        if ( !extraction && _wasFlowing) {
            _wasFlowing = false;
            _flowTimer->reset();
            _flowTimer->start();
        }

        if ( _flowTimer->isRunning() ) {
            if ( _flowTimer->getElapsed() > 10.0 ) {
                _flowTimer->stop();
                _oldFlowVolume = 0.0;
            }
            else {
                showRemainingFlow = true;
            } 
        }

        if ( extraction || showRemainingFlow ) { 
            double flowVolume = showRemainingFlow ? _oldFlowVolume : Singleton<Gaggia>::pointer()->getExtractionAmount();
            const double flowTime = Singleton<Gaggia>::pointer()->getExtractionTime();

            // Store flow volume
            if ( flowVolume != 0.0 ) {
                _oldFlowVolume = flowVolume;
            }

            if ( flowVolume == 0.0 ) {
                flowVolume = _oldFlowVolume;
            }

            //LogInfo("Extraction: V=" << flowVolume << ", T=" << flowTime);

            const SDL_Color textColor = showRemainingFlow ? greyColor : whiteColor;

            // Flow volume
            text.str( std::string() );
            text << std::fixed << std::setprecision(0) << flowVolume << " ml";
            _drawText( _flowFont, 205, 15, text.str(), textColor, blackColor );

            // Flow time
            text.str( std::string() );
            text << std::fixed << std::setprecision(0) << flowTime << " sec";
            _drawText( _flowFont, 205, 60, text.str(), textColor, blackColor );
        }
    }
    //else if ( _currentMode == DisplayMode::Temperature ) {
    //    _drawUIElement( _uiElements[ UIElementName::ButtonPlus ] );
    //    _drawUIElement( _uiElements[ UIElementName::ButtonMinus ] );
    //    _drawUIElement( _uiElements[ UIElementName::ButtonBack ] );

    //    // TODO
    //    double targetTemperature = 93.5;
    //    const std::string header = "Ziel Temperatur";
    //    std::stringstream text;
    //    text << std::fixed << std::setprecision(1) << targetTemperature;

    //    _drawText( _tempFont, 110, 60, text.str(), whiteColor, blackColor );
    //    _drawText( _targetTempFont, 40, 15, header, whiteColor, blackColor );
    //}
    else if ( _currentMode == DisplayMode::Shutdown ) {
        _drawUIElement( _uiElements[ UIElementName::ButtonOkay ] );
        _drawUIElement( _uiElements[ UIElementName::ButtonCancel ] );

        const std::string header = "Herunterfahren";
        _drawText( _headerFont, 40, 15, header, whiteColor, blackColor );
    }

    // Info text
    {
        const double systemTime = Singleton<Gaggia>::pointer()->getSystemTime();
        const int systemTimeMinutes = static_cast<int>( systemTime / 60 );

        std::stringstream text;
        text << "Betriebszeit: " << systemTimeMinutes;
        
        if ( systemTimeMinutes == 1 ) {
            text << " Minute";
        }
        else {
            text << " Minuten";
        }
        
        _drawText( _infoFont, 15, 218, text.str(), whiteColor, blackColor );

        const double tankState = Singleton<Gaggia>::pointer()->getWaterTankLevel();
        const double waterTankPercentage = tankState * 100.0;

        text.str( std::string() );
        text << "Tank: " << std::fixed << std::setprecision(0) << waterTankPercentage << "%";
        _drawText( _infoFont, 225, 218, text.str(), whiteColor, blackColor );
    }

    SDL_Flip( _display );
}

//-----------------------------------------------------------------------------

bool Display::_initUIElements() {
    const int buttonSize = 60;
    const int bottomButtonXSpace = 16;
    const int bottomButtonV = 155;

    const std::string prefix = Singleton<Settings>::pointer()->getPath() + "/";

    int bottomButtonX = 15;

    // Bottom 1 / 4: 30 ML, Okay, Plus

    if ( !_addUIElement( UIElementName::ButtonOneCup, prefix + "button_one_cup.png", bottomButtonX, bottomButtonV ) ) {
        return false;
    }

    if ( !_addUIElement( UIElementName::ButtonOkay, prefix + "button_okay.png", bottomButtonX, bottomButtonV ) ) {
        return false;
    }

    if ( !_addUIElement( UIElementName::ButtonPlus, prefix + "button_plus.png", bottomButtonX, bottomButtonV ) ) {
        return false;
    }

    bottomButtonX += ( buttonSize + bottomButtonXSpace );

    // Bottom 2 / 4: 60 ML, Minus

    if ( !_addUIElement( UIElementName::ButtonTwoCups, prefix + "button_two_cups.png", bottomButtonX, bottomButtonV ) ) {
        return false;
    }

    if ( !_addUIElement( UIElementName::ButtonMinus, prefix + "button_minus.png", bottomButtonX, bottomButtonV ) ) {
        return false;
    }

    // Bottom 3 / 4: Steam on / off

    bottomButtonX += ( buttonSize + bottomButtonXSpace );

    if ( !_addUIElement( UIElementName::ButtonSteamActive, prefix + "button_boiler_active.png", bottomButtonX, bottomButtonV ) ) {
        return false;
    }

    if ( !_addUIElement( UIElementName::ButtonSteamInactive, prefix + "button_boiler_inactive.png", bottomButtonX, bottomButtonV ) ) {
        return false;
    }

    bottomButtonX += ( buttonSize + bottomButtonXSpace );

    // Bottom 4 / 4: Back, Shutdown, Cancel

    if ( !_addUIElement( UIElementName::ButtonBack, prefix + "button_back.png", bottomButtonX, bottomButtonV ) ) {
        return false;
    }

    if ( !_addUIElement( UIElementName::ButtonShutdown, prefix + "button_shutdown.png", bottomButtonX, bottomButtonV ) ) {
        return false;
    }

    if ( !_addUIElement( UIElementName::ButtonCancel, prefix + "button_cancel.png", bottomButtonX, bottomButtonV ) ) {
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------

SDL_Surface* Display::_createTextSurface( TTF_Font* font, const std::string& text, SDL_Color foregroundColour, SDL_Color backgroundColour ) {
    if ( _display == nullptr ) {
        return nullptr;
    }

    return TTF_RenderText_Shaded( font, text.c_str(), foregroundColour, backgroundColour );
}

//-----------------------------------------------------------------------------

void Display::_drawText(TTF_Font* font, short x, short y, const std::string & text, SDL_Color foregroundColour, SDL_Color backgroundColour) {
    if ( _display == nullptr ) {
        return;
    }

    SDL_Surface *textSurface = _createTextSurface( font, text, foregroundColour, backgroundColour );
    
    if ( textSurface == nullptr ) {
        return;
    }

    SDL_Rect destRect = { x, y };
    SDL_BlitSurface( textSurface, 0, _display, &destRect );
    SDL_FreeSurface( textSurface );
}

//-----------------------------------------------------------------------------

bool Display::_addUIElement( const UIElementName::Value& name, const std::string& file, int x, int y ) {
    SDL_Surface* loadedImage = IMG_Load( file.c_str() );

    if ( loadedImage == NULL) {
        LogError( "Could not load image " << file.c_str() );
        return false;
    }

    // Convert surface to screen format
    SDL_Surface* surface = SDL_ConvertSurface( loadedImage, _display->format, 0 );

    if ( surface == nullptr ) {
        LogError( "Unable to optimize image! SDL Error: " << SDL_GetError() );
        SDL_FreeSurface( loadedImage );
        return false;
    }

    SDL_FreeSurface( loadedImage );
  
    SDL_Rect rect;

    rect.x = x;
    rect.y = y;
    rect.w = surface->w;
    rect.h = surface->h;
  
    UIElement* uiElement = new UIElement();
    uiElement->surface = surface;
    uiElement->rect = rect;

    _uiElements[ name ] = uiElement;

    return true;
}

//-----------------------------------------------------------------------------

void Display::_drawUIElement( UIElement* uiElement ) {
    if ( !uiElement ) {
        return;
    }
  
    SDL_Rect offset;
    offset.x = uiElement->rect.x;
    offset.y = uiElement->rect.y;
  
    SDL_BlitSurface( uiElement->surface, NULL, _display, &offset );
}

//-----------------------------------------------------------------------------

void Display::_drawUIElementPosition( UIElement* uiElement, int x, int y ) {
    if ( !uiElement ) {
        return;
    }
  
    SDL_Rect offset;
    offset.x = x;
    offset.y = y;
  
    SDL_BlitSurface( uiElement->surface, NULL, _display, &offset );
}

//-----------------------------------------------------------------------------

bool Display::_clickedUIElement( UIElement* uiElement, int x, int y ) {
    if ( !uiElement ) {
        return false;
    }
  
    return ( x >= uiElement->rect.x && x <= uiElement->rect.x + uiElement->rect.w && y >= uiElement->rect.y && y <= uiElement->rect.y + uiElement->rect.h );
}

//-----------------------------------------------------------------------------

std::string Display::_getStatusText() const {
    const Gaggia::State::Value state = Singleton<Gaggia>::pointer()->getState();
    std::stringstream text;

    switch ( state ) {
        case Gaggia::State::Deactivated: {
            text << "Boiler Deaktiviert";
            break;
        }

        case Gaggia::State::Heating: {
            const double heatingTimeLeft = Singleton<Gaggia>::pointer()->getHeatingRestTime();
            const int minutesLeft = static_cast<int>( heatingTimeLeft / 60 );

            text << "Vorheizen (" << minutesLeft;
            
            if ( minutesLeft == 1 ) {
                text << " Minute noch)";
            }
            else {
                text << " Minuten noch)";
            }
            
            break;
        }

        case Gaggia::State::Active: {
            text << "Bereit";
            break;
        }

        case Gaggia::State::Steam: {
            text << "Bereit (Dampf)";
            break;
        }

        case Gaggia::State::Extracting: {
            text << "Extraktion (manuell)";
            break;
        }

        case Gaggia::State::ExtractingOneCup: {
            text << "Extraktion (30 ml)";
            break;
        }

        case Gaggia::State::ExtractingTwoCups: {
            text << "Extraktion (60 ml)";
            break;
        }

        case Gaggia::State::Invalid:
        default: {
            text << "Fehler";
        }
    }

    return text.str();
}

//-----------------------------------------------------------------------------
