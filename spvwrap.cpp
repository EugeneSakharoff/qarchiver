#include "spvwrap.h"
#include "main.h"
#include "arcapplication.h"

#include <QTextCodec>

#include <poll.h>

/////////////////////////////////////////////////////////////////////////////
// class CSpv
/////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------
CSpv::CSpv() // c'tor
{
m_bAttached = false;

return;
}

//--------------------------------------------------------
CSpv::~CSpv() // d'tor
{
detachTrioTask();
return;
}

//--------------------------------------------------------
void CSpv::stopGetVarThread() const
{
MLibAccess* la = static_cast<QxArcApplication*>( qApp )->m_pLibAccess;
la->stopGetVarThread();

return;
}

//--------------------------------------------------------
bool CSpv::isGetVarThreadRunning() const
{
MLibAccess* la = static_cast<QxArcApplication*>( qApp )->m_pLibAccess;

return la->m_pGetVarThread->isRunning();
}

//--------------------------------------------------------
void CSpv::detachTrioTask()
{
if ( m_bAttached )
  {
  try
    {
    ::TUAPI_DetachTrioTask();
    }
  catch ( ... )
    {
    }
  m_bAttached = false;
  }

return;
}

////--------------------------------------------------------
//int CSpv::attachTrioTask( uint32_t dwFlags, const QString & strTaskName, QString & strErr )
//{
//int nRet = -1;

//Q_ASSERT( !m_bAttached );
//strErr.clear();
//try
//  {
//  wchar_t szTaskName[ MAX_TASK_NAME_LG + 1 ];

//  strTaskName.toWCharArray( szTaskName );
//  szTaskName[ MAX_TASK_NAME_LG ] = 0;
//  nRet = ::TUAPI_AttachTrioTask( (TUAPIe_TaskClass)dwFlags, szTaskName );
//  if ( nRet == 0 ) m_bAttached = true;
//  else strErr = errorTextForm( nRet );
//  }
//catch ( std::exception & e )
//  {
//  strErr = QString::fromUtf8( e.what() );
////  stopGetVarThread();
//  }
//catch ( ... )
//  {
//  strErr = QStringLiteral( "Неизвестная ошибка" );
////  stopGetVarThread();
//  }

//return nRet;
//}

//--------------------------------------------------------
int CSpv::attachTrioTask( uint32_t dwFlags, const QString & strTaskName, QString & strErr )
{
int nRet = -1;

Q_ASSERT( !m_bAttached );
strErr.clear();
try
  {
  nRet = ::TUAPI_AttachTrioTask( (TUAPIe_TaskClass)dwFlags, strTaskName.toUtf8().constData() );
  if ( nRet == 0 ) m_bAttached = true;
  else strErr = errorTextForm( nRet );
  }
catch ( std::exception & e )
  {
  nRet = -1;
  strErr = QString::fromUtf8( e.what() );
  }
catch ( ... )
  {
  nRet = -1;
  strErr = QStringLiteral( "Неизвестная ошибка: TUAPI_AttachTrioTask exception" );
  }

return nRet;
}

//--------------------------------------------------------
QString CSpv::errorTextForm( int nErrorCode )
{
QString strErrStr;

try
  {
  strErrStr = QString::fromUtf8( ::TUAPI_ErrorTextForm( nErrorCode ) );
  }
catch ( ... )
  {
  }

return strErrStr;
}

//--------------------------------------------------------
long CSpv::derefToVarKey( long iSRPPOID, const QString & sName, int iDerefFlags )
{
long lKey = -1;

if ( !m_bAttached ) return -1;
try
  {
//  lKey = ::TUAPI_DerefToVarKey( iSRPPOID, sName.toUtf8().constData(), iDerefFlags );
  lKey = ::TUAPI_DerefToVarKey( iSRPPOID,
         QTextCodec::codecForName( QByteArray( "IBM 866" ) )->fromUnicode( sName ).constData(), iDerefFlags );
  }
catch ( ... )
  {
  }

return lKey;
}

////--------------------------------------------------------
//long CSpv::propertiesOperation( void* pComlexResult, long iID, const QString & sName, int iCmd )
//{
//long lKey = -1;

//if ( !m_bAttached ) return -1;
//try
//  {
//  wchar_t szVarName[ MAX_VAR_NAME_LG + 1 ];

//  sName.toWCharArray( szVarName );
//  szVarName[ MAX_VAR_NAME_LG ] = 0;

//  lKey = ::TUAPI_PropertiesOperation( pComlexResult, iID, szVarName, iCmd );
//  }
//catch ( ... )
//  {
////  stopGetVarThread();
//  }

//return lKey;
//}

//--------------------------------------------------------
long CSpv::propertiesOperation( void* pComlexResult, long iID, const QString & sName, int iCmd )
{
long lKey = -1;

if ( !m_bAttached ) return -1;

try
  {
//  lKey = ::TUAPI_PropertiesOperation( pComlexResult, iID, sName.toUtf8().constData(), iCmd );
  lKey = ::TUAPI_PropertiesOperation( pComlexResult, iID,
         QTextCodec::codecForName( QByteArray( "IBM 866" ) )->fromUnicode( sName ).constData(), iCmd );
  }
catch ( ... )
  {
  }

return lKey;
}

//--------------------------------------------------------
long CSpv::waitForNewVar( void* pComplexVal, TUAPIe_W4NV_cmd eCmd )
{
long lKey = -1;

if ( !m_bAttached ) return -1;

try
  {
  if ( eCmd == TUAPIci_W4NV_BLOCKING_WAIT )
    {
    struct pollfd poll_fd;

    if ( !inputQueueContol( &poll_fd, TUAPIci_IQS_Get_pollfd ) ) return -1;

    while ( 1 )
      {
      int nRet = poll( &poll_fd, 1, 100 );
      if ( nRet < 0 || !isGetVarThreadRunning() ) return -1;
      if ( nRet > 0 ) break;
      }
    }

  lKey = ::TUAPI_WaitForNewVar( pComplexVal, eCmd );
  }
catch ( ... )
  {
//  stopGetVarThread();
  }

return lKey;
}

//--------------------------------------------------------
bool CSpv::waitSynchroStart( QString & strErr )
{
int nRet = -1;

strErr.clear();
if ( !m_bAttached )
  {
  strErr = QStringLiteral( "Задача не присоединена к ТОО" );
  return false;
  }

try
  {
  nRet = ::TUAPI_AttachTrioTask( (TUAPIe_TaskClass)-1, NULL );
  if ( nRet < 0 ) strErr = errorTextForm( nRet );
  }
catch ( std::exception & e )
  {
  nRet = -1;
  strErr = QString::fromUtf8( e.what() );
  }
catch ( ... )
  {
  nRet = -1;
  strErr = QStringLiteral( "Неизвестная ошибка: TUAPI_AttachTrioTask exception" );
  }

return nRet >= 0;
}

//--------------------------------------------------------
bool CSpv::universalWriteVar( TUAPI_key iVarKey, const void* pValue, int iVarState, int iFlags )
{
int nRet = -1;
char szVarName[ 64 ];

if ( !m_bAttached ) return false;

try
  {
  nRet = ::TUAPI_UniversalWriteVar( iVarKey, pValue, iVarState, iFlags );

//  char varName[ 64 ]; // !!!!!!!!!!!!!!!!!
//  propertiesOperation( varName, iVarKey, NULL, TUAPIci_PO_ByVarKey | TUAPIci_PO_GetVarName ); // !!!!!!!!!!!!!!!!!
//  varName[ 32 ] = 0; // !!!!!!!!!!!!!!!!!
//  qDebug() << varName << nRet;  // !!!!!!!!!!!!!!!!!

  if ( nRet < 0 )
    {
    QString strErrStr = errorTextForm( nRet );
    propertiesOperation( szVarName, iVarKey, NULL, TUAPIci_PO_ByVarKey | TUAPIci_PO_GetVarName );
    szVarName[ 32 ] = 0;

//    qDebug() << szVarName << nRet;  // !!!!!!!!!!!!!!!!!

    QString strErr;

    strErr += szVarName;
    strErr += QStringLiteral( "  iVarKey=" );
    strErr += QString::number( iVarKey );
    strErr += QStringLiteral( "  Error UniversalWriteVar = " );
    strErr += strErrStr;
    strErr += QStringLiteral( "\n" );

//    protocol( strErr, log_warn );
    printConsole( strErr );
    }
  }
catch ( std::exception & e )
  {
  propertiesOperation( szVarName, iVarKey, NULL, TUAPIci_PO_ByVarKey | TUAPIci_PO_GetVarName );
  szVarName[ 32 ] = 0;

  QString strErr;

  strErr += szVarName;
  strErr += QStringLiteral( "  iVarKey=" );
  strErr += QString::number( iVarKey );
  strErr += QStringLiteral( "  UniversalWriteVar exception:\n" );
  strErr += QString::fromUtf8( e.what() );
  strErr += QStringLiteral( "\n" );

//  protocol( strErr, log_warn );
  printConsole( strErr );
  }

return nRet >= 0;
}

//--------------------------------------------------------
bool CSpv::inputQueueContol( struct pollfd* p_pollfd, TUAPIe_IQS_Cmd nCmd )
{
int nRet = -1;

if ( !m_bAttached ) return false;

try
  {
  nRet = ::TUAPI_InputQueueContol( p_pollfd, nCmd );
  }
catch ( ... )
  {
  }

return nRet >= 0;
}
