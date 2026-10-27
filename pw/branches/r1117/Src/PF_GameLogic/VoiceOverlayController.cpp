#include "stdafx.h"
#include "VoiceOverlayController.h"
#include "NivalInput/Binds.h"
#include "NivalInput/InputCommandInstance.h"
#include "NivalInput/ControlsSet.h"
#include "NivalInput/HwInputInterface.h"
#include "ChatController.h"
#include "System/Texts.h"

#include <winsock2.h>
#include <stdio.h>
#include <stdarg.h>

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW( IOC_VENDOR, 12 )
#endif

namespace NGameX
{

namespace
{
  // launcher content/modules/voiceGameBridge.js
  const unsigned short LAUNCHER_PORT = 34981;
  const float POLL_PERIOD = 0.1f;
  // No answer for this long: launcher is gone (or too old), hide the overlay.
  const float SILENCE_TIMEOUT = 2.0f;
  const int MAX_DATAGRAM = 8192;
  const int MAX_LINE = 512;

  const char szOverlayCommand[] = "VoiceOverlayCmd";

  // Diagnostics: Bin\voice_overlay.log, rewritten every game start. Only rare events
  // (unlock key, show/hide, Flash replies), never the 10 Hz polling.
  void DebugLog( const char * format, ... )
  {
    static bool started = false;
    FILE * file = fopen( "voice_overlay.log", started ? "a" : "w" );
    if ( !file )
      return;
    started = true;

    SYSTEMTIME time;
    ::GetLocalTime( &time );
    fprintf( file, "%02d:%02d:%02d.%03d ", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds );

    va_list args;
    va_start( args, format );
    vfprintf( file, format, args );
    va_end( args );
    fputc( '\n', file );
    fclose( file );
  }

  // Skips `count` space-separated fields; the rest of the line is a nick that may contain spaces.
  const char * SkipFields( const char * p, int count )
  {
    for ( int i = 0; i < count && *p; ++i )
    {
      while ( *p && *p != ' ' )
        ++p;
      while ( *p == ' ' )
        ++p;
    }
    return p;
  }

  wstring FromUtf8( const char * text )
  {
    wstring result;
    NStr::UTF8ToUnicode( &result, string( text ) );
    return result;
  }

  template<class T>
  bool SamePeers( const nstl::vector<T> & a, const nstl::vector<T> & b )
  {
    if ( a.size() != b.size() )
      return false;
    for ( int i = 0; i < a.size(); ++i )
      if ( !( a[i] == b[i] ) )
        return false;
    return true;
  }


  bool IsGameWindowActive()
  {
    HWND foreground = ::GetForegroundWindow();
    if ( !foreground )
      return false;
    DWORD processId = 0;
    ::GetWindowThreadProcessId( foreground, &processId );
    return processId == ::GetCurrentProcessId();
  }

  // Virtual key of a NivalInput control (NivalInput/HwInputKeys.inl names); 0 if unknown.
  int VirtualKeyOf( const string & name )
  {
    if ( name.size() == 1 )
    {
      const char c = name[0];
      if ( ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) )
        return c;
      switch ( c )
      {
        case '`': return VK_OEM_3;
        case '-': return VK_OEM_MINUS;
        case '=': return VK_OEM_PLUS;
        case '[': return VK_OEM_4;
        case ']': return VK_OEM_6;
        case ';': return VK_OEM_1;
        case '\'': return VK_OEM_7;
        case ',': return VK_OEM_COMMA;
        case '.': return VK_OEM_PERIOD;
        case '/': return VK_OEM_2;
        case '\\': return VK_OEM_5;
      }
      return 0;
    }
    if ( name.size() >= 2 && name.size() <= 3 && name[0] == 'F' && name[1] >= '1' && name[1] <= '9' )
    {
      const int n = atoi( name.c_str() + 1 );
      return n >= 1 && n <= 15 ? VK_F1 + n - 1 : 0;
    }
    if ( name.size() == 5 && name.compare( 0, 4, "NUM_" ) == 0 && name[4] >= '0' && name[4] <= '9' )
      return VK_NUMPAD0 + ( name[4] - '0' );

    struct Named { const char * name; int vk; };
    static const Named table[] = {
      { "LSHIFT", VK_LSHIFT }, { "RSHIFT", VK_RSHIFT }, { "LCTRL", VK_LCONTROL }, { "RCTRL", VK_RCONTROL },
      { "LALT", VK_LMENU }, { "RALT", VK_RMENU }, { "LWIN", VK_LWIN }, { "RWIN", VK_RWIN },
      { "SPACE", VK_SPACE }, { "TAB", VK_TAB }, { "ENTER", VK_RETURN }, { "ESC", VK_ESCAPE },
      { "BACKSPACE", VK_BACK }, { "CAPITAL", VK_CAPITAL }, { "UP", VK_UP }, { "DOWN", VK_DOWN },
      { "LEFT", VK_LEFT }, { "RIGHT", VK_RIGHT }, { "HOME", VK_HOME }, { "END", VK_END },
      { "PG_UP", VK_PRIOR }, { "PG_DOWN", VK_NEXT }, { "INSERT", VK_INSERT }, { "DELETE", VK_DELETE },
      { "MOUSE_BUTTON0", VK_LBUTTON }, { "MOUSE_BUTTON1", VK_RBUTTON }, { "MOUSE_BUTTON2", VK_MBUTTON },
      { "MOUSE_BUTTON3", VK_XBUTTON1 }, { "MOUSE_BUTTON4", VK_XBUTTON2 },
    };
    for ( int i = 0; i < sizeof( table ) / sizeof( table[0] ); ++i )
      if ( name == table[i].name )
        return table[i].vk;
    return 0;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VoiceOverlayController::VoiceOverlayController( UI::FlashContainer2 * flashWnd, const char* className, bool _pollUnlockKey ) :
  FlashInterface( flashWnd, className ),
  socketHandle( INVALID_SOCKET ),
  socketFailed( false ),
  winsockStarted( false ),
  pollTimer( 0.f ),
  silenceTime( 0.f ),
  shown( false ),
  pttHeld( false ),
  unlocked( false ),
  pollUnlockKey( _pollUnlockKey ),
  hintShown( false ),
  hasCurrent( false )
{
  flashWnd->AddFSListner( szOverlayCommand, this );
  Input::Binds * binds = Input::BindsManager::Instance()->GetBinds();
  DebugLog( "controller created, unlock bind %s", binds && binds->FindCommand( "voice_overlay_unlock_on" ) ? "present" : "MISSING" );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Players keep their own input_new.cfg, so a new bind never reaches them from the
// default config. Define it once; the in-game preferences then save it with the rest.
void VoiceOverlayController::EnsureDefaultUnlockBind()
{
  Input::Binds * binds = Input::BindsManager::Instance()->GetBinds();
  if ( !binds || binds->FindCommand( "voice_overlay_unlock_on" ) )
    return;

  NGlobal::RunCommand( L"bindsection adventure_screen", L"input" );
  NGlobal::RunCommand( L"bind voice_overlay_unlock_on 'SHIFT'", L"input" );
  NGlobal::RunCommand( L"bind !voice_overlay_unlock_off 'SHIFT'", L"input" );
  NGlobal::RunCommand( L"bindsection", L"input" );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoiceOverlayController::NotifySmartCast( const char * talentId, bool on )
{
  if ( !talentId || !*talentId || strlen( talentId ) > 64 )
    return;
  for ( const char * p = talentId; *p; ++p )
    if ( !( ( *p >= 'a' && *p <= 'z' ) || ( *p >= 'A' && *p <= 'Z' ) || ( *p >= '0' && *p <= '9' ) || *p == '_' || *p == '-' ) )
      return;
  OpenSocket();
  char text[96];
  _snprintf_s( text, sizeof( text ), _TRUNCATE, "smartcast %s %d", talentId, on ? 1 : 0 );
  Send( text );
  DebugLog( "smartcast %s %d", talentId, on ? 1 : 0 );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoiceOverlayController::SetUnlocked( bool _unlocked )
{
  DebugLog( "unlock key %s (was %d, shown %d)", _unlocked ? "down" : "up", (int)unlocked, (int)shown );
  if ( unlocked == _unlocked )
    return;
  unlocked = _unlocked;
  if ( shown )
    CallMethod( "SetVoiceOverlayInteractive", unlocked );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VoiceOverlayController::~VoiceOverlayController()
{
  if ( pttHeld )
    Send( "ptt 0" );
  CloseSocket();
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool VoiceOverlayController::OpenSocket()
{
  if ( socketHandle != INVALID_SOCKET )
    return true;
  if ( socketFailed )
    return false;

  WSADATA wsaData;
  if ( ::WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) != 0 )
  {
    socketFailed = true;
    return false;
  }
  winsockStarted = true;

  SOCKET s = ::socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
  if ( s == INVALID_SOCKET )
  {
    socketFailed = true;
    CloseSocket();
    return false;
  }

  u_long nonBlocking = 1;
  ::ioctlsocket( s, FIONBIO, &nonBlocking );

  // Without a launcher every send gets an ICMP "port unreachable", which Windows reports
  // as WSAECONNRESET on the next recvfrom. Turn that off: silence is handled by a timeout.
  BOOL reportReset = FALSE;
  DWORD bytesReturned = 0;
  ::WSAIoctl( s, SIO_UDP_CONNRESET, &reportReset, sizeof( reportReset ), NULL, 0, &bytesReturned, NULL, NULL );

  sockaddr_in local;
  memset( &local, 0, sizeof( local ) );
  local.sin_family = AF_INET;
  local.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
  local.sin_port = 0;
  if ( ::bind( s, (const sockaddr *)&local, sizeof( local ) ) != 0 )
  {
    ::closesocket( s );
    socketFailed = true;
    CloseSocket();
    return false;
  }

  socketHandle = s;
  return true;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoiceOverlayController::CloseSocket()
{
  if ( socketHandle != INVALID_SOCKET )
  {
    ::closesocket( (SOCKET)socketHandle );
    socketHandle = INVALID_SOCKET;
  }
  if ( winsockStarted )
  {
    ::WSACleanup();
    winsockStarted = false;
  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoiceOverlayController::Send( const char * text )
{
  if ( socketHandle == INVALID_SOCKET )
    return;

  sockaddr_in target;
  memset( &target, 0, sizeof( target ) );
  target.sin_family = AF_INET;
  target.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
  target.sin_port = htons( LAUNCHER_PORT );
  ::sendto( (SOCKET)socketHandle, text, (int)strlen( text ), 0, (const sockaddr *)&target, sizeof( target ) );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoiceOverlayController::Update( float timeDelta )
{
  if ( !OpenSocket() )
    return;

  ReceiveAll();
  UpdatePushToTalk();
  if ( pollUnlockKey )
  {
    const bool held = IsUnlockBindHeld();
    if ( held != unlocked )
      SetUnlocked( held );
  }

  pollTimer += timeDelta;
  silenceTime += timeDelta;

  if ( pollTimer >= POLL_PERIOD )
  {
    pollTimer = 0.f;
    Send( "poll" );
    // Launcher drops a held push-to-talk if it stops hearing about it (game crash).
    if ( pttHeld )
      Send( "ptt 1" );
  }

  if ( shown && silenceTime > SILENCE_TIMEOUT )
    HideOverlay();
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoiceOverlayController::ReceiveAll()
{
  char buffer[MAX_DATAGRAM + 1];

  for ( int i = 0; i < 32; ++i )
  {
    sockaddr_in from;
    int fromLen = sizeof( from );
    const int size = ::recvfrom( (SOCKET)socketHandle, buffer, MAX_DATAGRAM, 0, (sockaddr *)&from, &fromLen );
    if ( size <= 0 )
      return;

    if ( from.sin_addr.s_addr != htonl( INADDR_LOOPBACK ) || from.sin_port != htons( LAUNCHER_PORT ) )
      continue;

    buffer[size] = 0;

    State state;
    if ( !ParseState( buffer, size, state ) )
      continue;

    silenceTime = 0.f;
    ApplyState( state );
  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool VoiceOverlayController::ParseState( const char * data, int size, State & state )
{
  bool header = false;
  const char * p = data;
  const char * end = data + size;

  while ( p < end )
  {
    const char * lineEnd = p;
    while ( lineEnd < end && *lineEnd != '\n' )
      ++lineEnd;

    char line[MAX_LINE];
    const int lineLen = (int)( lineEnd - p );
    const int len = lineLen < MAX_LINE - 1 ? lineLen : MAX_LINE - 1;
    memcpy( line, p, len );
    line[len] = 0;
    if ( len > 0 && line[len - 1] == '\r' )
      line[len - 1] = 0;
    p = lineEnd + 1;

    if ( !header )
    {
      if ( strcmp( line, "PWV 1" ) != 0 )
        return false;
      header = true;
      continue;
    }

    if ( strncmp( line, "cfg ", 4 ) == 0 )
    {
      int enabled = 0, radio = 0;
      char keys[64] = { 0 };
      float x = -1.f, y = -1.f;
      if ( sscanf_s( line + 4, "%d %d %63s %f %f", &enabled, &radio, keys, (unsigned)sizeof( keys ), &x, &y ) < 3 )
        continue;
      state.enabled = enabled != 0;
      state.radioMode = radio != 0;
      state.posX = x;
      state.posY = y;
      state.pttKeys.clear();
      for ( const char * k = keys; *k && *k != '-'; )
      {
        const int vk = atoi( k );
        if ( vk > 0 && vk < 256 )
          state.pttKeys.push_back( vk );
        while ( *k && *k != ',' )
          ++k;
        if ( *k == ',' )
          ++k;
      }
    }
    else if ( strncmp( line, "me ", 3 ) == 0 )
    {
      int mic = 0, speaking = 0;
      if ( sscanf_s( line + 3, "%d %d", &mic, &speaking ) != 2 )
        continue;
      state.micState = mic;
      state.meSpeaking = speaking != 0;
      state.myName = FromUtf8( SkipFields( line + 3, 2 ) );
    }
    else if ( strncmp( line, "p ", 2 ) == 0 )
    {
      Peer peer;
      if ( sscanf_s( line + 2, "%d %d %d", &peer.id, &peer.flags, &peer.volume ) != 3 )
        continue;
      peer.name = FromUtf8( SkipFields( line + 2, 3 ) );
      state.peers.push_back( peer );
    }
  }

  return header;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoiceOverlayController::ApplyState( const State & state )
{
  // Keys are needed by push-to-talk even if nothing visible changed.
  const bool firstState = !hasCurrent;
  const bool visible = state.enabled;

  if ( firstState && visible )
    // The saved position is applied once; after that the overlay owns it (dragging).
    CallMethod( "SetVoiceOverlayPosition", state.posX, state.posY );

  const bool changed = firstState
    || current.micState != state.micState
    || current.meSpeaking != state.meSpeaking
    || current.myName != state.myName
    || !SamePeers( current.peers, state.peers );

  current = state;
  hasCurrent = true;

  if ( !visible )
  {
    HideOverlay();
    return;
  }

  if ( changed || !shown )
  {
    nstl::vector<int> ids, flags, volumes;
    nstl::vector<wstring> names;
    for ( int i = 0; i < state.peers.size(); ++i )
    {
      ids.push_back( state.peers[i].id );
      flags.push_back( state.peers[i].flags );
      volumes.push_back( state.peers[i].volume );
      names.push_back( state.peers[i].name );
    }
    CallMethod( "SetVoiceOverlayState", state.micState, state.meSpeaking, state.myName, ids, flags, volumes, names );
  }

  if ( !shown )
  {
    CallMethod( "SetVoiceOverlayInteractive", unlocked );
    CallMethod( "ShowVoiceOverlay", true );
    shown = true;
    ShowUnlockHint();
    DebugLog( "show: mic %d, peers %d", state.micState, state.peers.size() );
  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoiceOverlayController::HideOverlay()
{
  if ( !shown )
    return;
  CallMethod( "ShowVoiceOverlay", false );
  shown = false;
  DebugLog( "hide" );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool VoiceOverlayController::IsPushToTalkHeld() const
{
  if ( !hasCurrent || !current.enabled || !current.radioMode || current.pttKeys.empty() )
    return false;
  if ( !IsGameWindowActive() )
    return false;
  for ( int i = 0; i < current.pttKeys.size(); ++i )
    if ( !( ::GetAsyncKeyState( current.pttKeys[i] ) & 0x8000 ) )
      return false;
  return true;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoiceOverlayController::SetChat( ChatUiController * _chat )
{
  chat = _chat;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// "Shift", "Ctrl+Q", "Shift / Mouse4": key sets of the unlock bind; left and right modifiers merge.
wstring VoiceOverlayController::GetUnlockKeyText()
{
  Input::Binds * binds = Input::BindsManager::Instance()->GetBinds();
  if ( !binds || !binds->HwInput() )
    return wstring( L"Shift" );
  Input::ICommandInstance * command = binds->FindCommand( "voice_overlay_unlock_on" );
  if ( !command )
    return wstring( L"Shift" );

  nstl::vector<wstring> variants;
  Input::TControlsSet & sets = static_cast<Input::ICommandInstanceImplementation *>( command )->AccessControlsSet();
  for ( Input::TControlsSet::iterator it = sets.begin(); it != sets.end(); ++it )
  {
    if ( !IsValid( *it ) )
      continue;
    vector<int> controls;
    (*it)->GetControls( controls );
    wstring text;
    for ( int i = 0; i < controls.size(); ++i )
    {
      string name = binds->HwInput()->ControlName( controls[i] );
      if ( name == "LSHIFT" || name == "RSHIFT" )
        name = "Shift";
      else if ( name == "LCTRL" || name == "RCTRL" )
        name = "Ctrl";
      else if ( name == "LALT" || name == "RALT" )
        name = "Alt";
      else if ( name.compare( 0, 12, "MOUSE_BUTTON" ) == 0 && name.size() == 13 )
        name = NStr::StrFmt( "Mouse%d", name[12] - '0' + 1 );
      if ( !text.empty() )
        text += L"+";
      text += NStr::ToUnicode( name );
    }
    if ( text.empty() )
      continue;
    bool known = false;
    for ( int i = 0; i < variants.size() && !known; ++i )
      known = variants[i] == text;
    if ( !known )
      variants.push_back( text );
  }

  wstring result;
  for ( int i = 0; i < variants.size(); ++i )
  {
    if ( i )
      result += L" / ";
    result += variants[i];
  }
  return result.empty() ? wstring( L"Shift" ) : result;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoiceOverlayController::ShowUnlockHint()
{
  if ( hintShown || !IsValid( chat ) )
    return;
  hintShown = true;
  const wstring key = GetUnlockKeyText();
  wstring text;
  if ( GetLanguageShortCode() == "en" )
    text = wstring( L"Hold \"" ) + key + L"\" + click the voice button to mute/unmute an ally.";
  else
    text = wstring( L"\u0417\u0430\u0436\u043c\u0438\u0442\u0435 \"" ) + key
      + L"\" + \u043a\u043b\u0438\u043a \u043f\u043e \u043a\u043d\u043e\u043f\u043a\u0435 \u0432\u043e\u0439\u0441\u0430, \u0447\u0442\u043e\u0431\u044b \u0437\u0430\u0433\u043b\u0443\u0448\u0438\u0442\u044c/\u0432\u0435\u0440\u043d\u0443\u0442\u044c \u0441\u043e\u044e\u0437\u043d\u0438\u043a\u0430.";
  chat->AddChatMessage( text, NDb::CHATCHANNEL_SYSTEM );
  DebugLog( "unlock hint sent" );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// The bind lives in the adventure_screen section, which is inactive on the loading screen, so
// its events never come there. Its keys are read here instead: any key set of the bind fully held.
bool VoiceOverlayController::IsUnlockBindHeld()
{
  if ( !IsGameWindowActive() )
    return false;
  Input::Binds * binds = Input::BindsManager::Instance()->GetBinds();
  if ( !binds || !binds->HwInput() )
    return false;
  Input::ICommandInstance * command = binds->FindCommand( "voice_overlay_unlock_on" );
  if ( !command )
    return false;
  // Binds keeps only implementations (Binds::TCommands).
  Input::TControlsSet & sets = static_cast<Input::ICommandInstanceImplementation *>( command )->AccessControlsSet();
  for ( Input::TControlsSet::iterator it = sets.begin(); it != sets.end(); ++it )
  {
    if ( !IsValid( *it ) )
      continue;
    vector<int> controls;
    (*it)->GetControls( controls );
    bool all = !controls.empty();
    for ( int i = 0; all && i < controls.size(); ++i )
    {
      const int vk = VirtualKeyOf( binds->HwInput()->ControlName( controls[i] ) );
      all = vk != 0 && ( ::GetAsyncKeyState( vk ) & 0x8000 ) != 0;
    }
    if ( all )
      return true;
  }
  return false;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoiceOverlayController::UpdatePushToTalk()
{
  const bool held = IsPushToTalkHeld();
  if ( held == pttHeld )
    return;
  pttHeld = held;
  Send( held ? "ptt 1" : "ptt 0" );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoiceOverlayController::OnFSCommand( UI::FlashContainer2* _wnd, const char* listenerID, const char* args, const wchar_t * argsW )
{
  if ( strcmp( listenerID, szOverlayCommand ) != 0 || !args )
    return;

  if ( strncmp( args, "dbg ", 4 ) == 0 )
  {
    DebugLog( "flash: %.200s", args + 4 );
    return;
  }

  // Flash builds the launcher command itself; pass through only the known ones.
  const bool known = strncmp( args, "mute ", 5 ) == 0
    || strcmp( args, "mic" ) == 0
    || strncmp( args, "vol ", 4 ) == 0
    || strncmp( args, "pos ", 4 ) == 0;
  if ( !known || strlen( args ) > 64 )
    return;

  Send( args );
}

}
