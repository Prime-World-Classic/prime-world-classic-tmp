#include "stdafx.h"
#include "RdpClientTransport.h"
#include "Network/RUDP/SockSrvWinsockBlocking.h"
#include "Network/RUDP/IRdp.h"
#include "Server/RdpTransport/RdpTransportChannel.h"
#include "Server/RdpTransport/RdpSocketFactory.h"
#include "Server/RdpTransport/RdpTransportMsg.h"
#include "System/RandomInterfaceImpl.h"
#include "RdpLoginClient.h"
#include "System/NiTimer.h"
#include "System/InlineProfiler.h"


static unsigned s_rdpLogEevents = ni_udp::RdpOptions::LogMajorEvents | ni_udp::RdpOptions::LogWarnings | ni_udp::RdpOptions::LogErrors;
REGISTER_VAR( "rdp_log_events", s_rdpLogEevents, STORAGE_NONE );

static int s_rdpLogicPriority = 1;
REGISTER_VAR( "rdp_logic_priority", s_rdpLogicPriority, STORAGE_NONE );

static int s_sockServPriority = 1;
REGISTER_VAR( "rdp_sock_server_priority", s_sockServPriority, STORAGE_NONE );

static int s_sockBufferSize = 65536;
REGISTER_VAR( "rdp_sock_buffer_size", s_sockBufferSize, STORAGE_NONE );

// Client-side RDP timing. The library defaults (ping 15 s, handshake 10 s) are
// sized for a long-lived server-side channel. A client inside a battle has a
// hard deadline: the game server keeps the reserved world snapshots for
// `game_fast_reconnect_reserve_time` (50 s in the pool config) after IT notices
// the drop, and the client only starts walking the pool after IT notices. With
// the defaults the client spends tens of seconds before the first attempt even
// fails, so the reconnect window is burned before the pool is tried.
static int s_rdpClPingPeriodMs = 5000;
REGISTER_VAR( "rdp_cl_ping_period", s_rdpClPingPeriodMs, STORAGE_NONE );

static int s_rdpClRetransmitLimit = 8;
REGISTER_VAR( "rdp_cl_retransmit_limit", s_rdpClRetransmitLimit, STORAGE_NONE );

// Retransmit backoff cap. The library caps it at 4 s, which stretches the
// "peer stopped answering" detection to ~15 s even with a modest retry limit.
// A client in a battle has a reservation window to fit into (see
// game_fast_reconnect_reserve_time), so it retransmits in a tighter cycle.
static int s_rdpClMaxRetransmitMs = 2000;
REGISTER_VAR( "rdp_cl_max_retransmit_time", s_rdpClMaxRetransmitMs, STORAGE_NONE );

static int s_rdpClHandshakeTotalMs = 5000;
REGISTER_VAR( "rdp_cl_handshake_timeout", s_rdpClHandshakeTotalMs, STORAGE_NONE );

static int s_rdpClHandshakeRetryMs = 2000;
REGISTER_VAR( "rdp_cl_handshake_retry", s_rdpClHandshakeRetryMs, STORAGE_NONE );

static int s_rdpClHandshakeTries = 3;
REGISTER_VAR( "rdp_cl_handshake_tries", s_rdpClHandshakeTries, STORAGE_NONE );


namespace rdp_transport
{

class ClientTransport::Worker : public threading::IThreadJob, public BaseObjectMT
{
  NI_DECLARE_REFCOUNT_CLASS_2( Worker, threading::IThreadJob, BaseObjectMT );
public:
  Worker( ClientTransport * _owner ) : owner( _owner ) {}
private:
  ClientTransport * owner;

  virtual void Work( volatile bool & isRunning )
  {
    NI_PROFILE_THREAD;

    while ( isRunning )
    {
      owner->ParallelPoll();
      Sleep( 100 );
    }
  }
};



ClientTransport::ClientTransport( const ni_udp::NetAddr & _bindAddr, unsigned _portSearchRange, Transport::MessageFactory * _msgFactory ) :
bindAddr( _bindAddr ),
portSearchRange( _portSearchRange )
{
  commonCtx.msgFactory = _msgFactory;
  commonCtx.schedule = 0;
  commonCtx.allocator = new HeapAllocator;
}



void ClientTransport::Cleanup()
{
  rdp = 0;
  sockServer = 0;

  loginClient = 0;
}



void ClientTransport::ParallelPoll()
{
  threading::MutexLock lock( mutex );

  if ( !loginClient )
    return;

  timer::Time now = timer::Now();

  loginClient->ParallelPoll( now );
}



int ClientTransport::GetUserId() const
{
  threading::MutexLock lock( mutex );

  if ( !loginClient )
    return 0;

  return loginClient->LoginSvcReply().uid;
}



StrongMT<Transport::IChannel> ClientTransport::OpenChannel( Transport::TServiceId interfaceId, unsigned int pingperiod, unsigned int to )
{
  threading::MutexLock lock( mutex );

  if ( !loginClient )
    return 0;

  if ( loginClient->State() != ELoginClientState::Ready )
    return 0;

  return loginClient->NewSvcChannel( commonCtx, interfaceId );
}



void ClientTransport::GetNewAcceptedChannels(vector<StrongMT<Transport::IChannel>> & _chnls)
{
}



void ClientTransport::Login( const Network::NetAddress & _loginServerAddress, const nstl::string & _login, const nstl::string & _password, const nstl::string & _sessionKey, const nstl::string & _playerKey, Login::LoginType::Enum _loginType )
{
  threading::MutexLock lock( initShutdownMutex );
  thread = 0;

  threading::MutexLock lock2( mutex );
  Cleanup();

  ni_udp::NetAddr loginSvcAddr;
  unsigned loginSvcMux = 0;
  if ( !ParseAddress( loginSvcAddr, loginSvcMux, _loginServerAddress.c_str() ) )
    return;

  if ( loginSvcAddr.Address() == INADDR_ANY )
    loginSvcAddr.Set( "localhost", loginSvcAddr.Port() );

  sockServer = new ni_udp::BlockingUdpSocketServer( s_sockServPriority, s_sockBufferSize );

  Strong<ni_rnd::Factory> rndFact = new ni_rnd::Factory;

  StrongMT<SocketFactory> sockFact = new SocketFactory( sockServer, bindAddr, portSearchRange, rndFact->Produce( timer::GetTicks() & 0xffffffffu ) );

  ni_udp::RdpOptions opts;
  opts.logEvents = s_rdpLogEevents;
  opts.logicThreadPriority = s_rdpLogicPriority;
  opts.pingPeriod = (float)s_rdpClPingPeriodMs / 1000.0f;
  opts.retransmitLimit = (unsigned)Max( 1, s_rdpClRetransmitLimit );
  opts.cc.maxRetransmitTime = (float)s_rdpClMaxRetransmitMs / 1000.0f;
  opts.handshakeTotalTimeout = (float)s_rdpClHandshakeTotalMs / 1000.0f;
  opts.handshakeRetryTimeout = (float)s_rdpClHandshakeRetryMs / 1000.0f;
  opts.handshakeMaxTries = (unsigned)Max( 1, s_rdpClHandshakeTries );

  rdp = ni_udp::NewRdpInstance();
  if ( !rdp->Init( sockFact, opts, rndFact->Produce( (unsigned)timer::GetTicks() ), new timer::RealTimer ) )
    return;

  loginClient = new LoginClient( rdp, loginSvcAddr, loginSvcMux, _login, _password, _sessionKey, _playerKey );

  thread = new threading::JobThread( new Worker( this ), "ClientTransport" );
}



void ClientTransport::Logout()
{
  threading::MutexLock lock( initShutdownMutex );
  thread = 0;

  threading::MutexLock lock2( mutex );
  Cleanup();
}



Login::ELoginResult::Enum ClientTransport::GetLoginResult() const
{
  threading::MutexLock lock( mutex );

  if ( loginClient )
  {
    ELoginClientState::Enum clSt = loginClient->State();
    if ( ( clSt == ELoginClientState::Ready ) || ( clSt == ELoginClientState::Failed ) )
      return loginClient->LoginSvcReply().code;
  }

  return Login::ELoginResult::NoResult;
}


newLogin::LoginReply ClientTransport::GetLoginReply() const
{
  threading::MutexLock lock( mutex );

  if ( loginClient )
    return loginClient->LoginSvcReply();

  return newLogin::LoginReply();
}



Transport::EStatus::Enum ClientTransport::GetStatus()
{
  threading::MutexLock lock( mutex );

  if ( !loginClient )
    return Transport::EStatus::NONE;

  ELoginClientState::Enum clSt = loginClient->State();

  if ( clSt == ELoginClientState::Ready )
    return Transport::EStatus::OK;

  if ( clSt == ELoginClientState::Failed )
    return Transport::EStatus::FAIL;

  return Transport::EStatus::NONE;
}



Transport::TServiceId ClientTransport::GetSessionPath() const
{
  threading::MutexLock lock( mutex );

  if ( !loginClient )
    return Transport::TServiceId();

  return loginClient->LoginSvcReply().welcomingSvcId.c_str();
}



Network::NetAddress ClientTransport::GetRelayAddress() const
{
  threading::MutexLock lock( mutex );

  if ( !loginClient )
    return Network::NetAddress();

  char buf[128] = "";
  loginClient->LoginSvcAddr().Str( buf, 128 );

  return buf;
}



Network::NetAddress ClientTransport::GetSecondaryRelayAddress() const
{
  return Network::NetAddress();
}

} //namesapce rdp_transport
