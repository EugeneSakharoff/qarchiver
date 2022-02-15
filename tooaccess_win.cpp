#include "main.h"
#include "arcapplication.h"

#include <QDebug>
#include <QTextCodec>
#include <QDateTime>
#include <QMultiMap>

#include <qt_windows.h>

typedef BOOL (* TOOINI)();
typedef int (* ATTCH)( LPCSTR, quint8, HANDLE* );
typedef BOOL (* DETCH)( int );
typedef quint16 (* GVIBN)( LPCSTR );
typedef quint16 (* GVIBID)( long );
typedef BOOL (* GVVBID)( int, long, LPVOID, int*, FILETIME *, quint32* );
typedef BOOL (* GVVBN)( int, LPCSTR, LPVOID, int*, FILETIME *, quint32* );
typedef BOOL (* GVVBO)( int, quint16, LPVOID, int*, FILETIME *, quint32* );
typedef BOOL (* SVVBID)( int, long, LPVOID, int, FILETIME* );
typedef BOOL (* SVVBN)( int, LPCSTR, LPVOID, int, FILETIME* );
typedef BOOL (* SVVBO)( int, quint16, LPVOID, int, FILETIME* );
typedef quint16 (* GVC)();
typedef BOOL (* GVI)( quint16, VARINFO* );
typedef BOOL (* GFVN)( int, quint16*, quint8* );
typedef BOOL (* GNVN)( int, quint16* );
typedef BOOL (* WTCNG)( int );
typedef BOOL (* CANWT)( int );
typedef BOOL (* SVU)( int, quint16, FILETIME* );
typedef BOOL (* GQR)( int, quint16*, LPVOID, int*, FILETIME*, quint32*, quint8* );
typedef BOOL (* GCC)( int, quint16* );
typedef BOOL (* FIS)( int, BOOL );
typedef quint16 (* GVER)();
typedef BOOL (* SVVBOEX)( int, WORD, LPVOID, int, FILETIME *, BYTE );

ATTCH pTOOAttach;
DETCH pTOODetach;
GVIBN pGetVarIdxByName;
GVIBID pGetVarIdxByID;
GVVBID pGetVarValByID;
GVVBN pGetVarValByName;
GVVBO pGetVarValByOrd;
SVVBID pSetVarValByID;
SVVBN pSetVarValByName;
SVVBO pSetVarValByOrd;
GVC pGetVarCnt;
GVI pGetVarInfo;
GFVN pGetFirstVarNo;
GNVN pGetNextVarNo;
WTCNG pWaitChange;
CANWT pCancelWait;
SVU pSetVarUndefined;
GQR pGetQueuedResult;
GCC pGetChangesCount;
FIS pForceInitialStatus;
GVER pGetVer;
SVVBOEX pSetVarValByOrdEx;

const char* pTOOErrors[] =
{
  QT_TR_NOOP( "Задача не найдена" ), // -1
  QT_TR_NOOP( "Задача уже присоединена" ), // -2
  QT_TR_NOOP( "Ошибка выделения памяти под данные" ), // -3
  QT_TR_NOOP( "Ошибка выделения памяти под SWMRG" ), // -4
  QT_TR_NOOP( "Ошибка открытия SWMRG" ), // -5
  QT_TR_NOOP( "Ошибка выделения памяти под маску" ), // -6
  QT_TR_NOOP( "Ошибка открытия Event'а" ), // -7
  QT_TR_NOOP( "Ошибка открытия Mutex'а" ), // -8
  QT_TR_NOOP( "Ошибка посылки супервизору сообщения о присоединении" ), // -9
  QT_TR_NOOP( "Ошибка открытия разделяемой памяти очереди" ), // -10
  QT_TR_NOOP( "Ошибка отображения разделяемой памяти очереди" ) // -11
};

HINSTANCE g_hLibInst = NULL;
UINT g_TasksQuitMsg = 0;
HANDLE g_hEvtTOO = NULL;

QString getRegistryKeyName( const QString & strProjectName );
void lastErrorMessage( QString & errstr );
bool conLibrary( QString & errstr );
void disconLibrary();
void clearLibPointers();

QMultiMap<long, int> g_mapVarIdx;
extern QVector<MVARREC> g_varsList;

/////////////////////////////////////////////////////////////////////////////
// class MLibAccess
/////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------
MLibAccess::MLibAccess()
{
m_iTask = -1;
clearLibPointers();
m_pGetVarThread = new QaGetVarThread;

return;
}

//--------------------------------------------------------
MLibAccess::~MLibAccess()
{
delete m_pGetVarThread;

if ( pTOODetach != NULL && m_iTask >= 0 )
  {
  pTOODetach( m_iTask );
  m_iTask = -1;
  }

disconnectTOO();

return;
}

//--------------------------------------------------------
void MLibAccess::init()
{

return;
}

//--------------------------------------------------------
bool MLibAccess::connectTOO( QString & strErr )
{
strErr.clear();
return conLibrary( strErr );
}

//--------------------------------------------------------
void MLibAccess::disconnectTOO()
{
disconLibrary();

return;
}

//--------------------------------------------------------
bool MLibAccess::attachToTOO( const QString & strTaskName, QString & strErr )
{
QTextCodec* codec = QTextCodec::codecForName( "Windows-1251" );
QByteArray encodedString = codec->fromUnicode( strTaskName );

// g_hEvtTOO закроется в TOODetach(...)
if ( ( m_iTask = pTOOAttach( encodedString.constData(), (quint8)CMD_ATT_QUEUE, &g_hEvtTOO ) ) < 0 )
  {
  strErr = QObject::tr( "Ошибка связи с ТОО (задача %1):\n%2" ).arg(
                        strTaskName, QString( pTOOErrors[ -m_iTask - 1 ] ) );
  return false;
  }

return true;
}

//--------------------------------------------------------
bool MLibAccess::varIndexesFromTOO( QString & strErr )
{
QTextCodec* codec = QTextCodec::codecForName( "Windows-1251" );
g_mapVarIdx.clear();
for ( int iVar = 0; iVar < g_varsList.size(); ++iVar )
  {
  MVARREC & varRef = g_varsList[ iVar ];
  QByteArray encodedString = codec->fromUnicode( varRef.mvard.vname );
  quint16 uVarIdx = pGetVarIdxByName( encodedString.constData() );
  if ( uVarIdx == 0xffff )
    {
    strErr = QObject::tr( "Переменная '%1' не найдена в ТОО" ).arg( varRef.mvard.vname );
    return false;
    }

  if ( varRef.mvard.varIdx != -1 )
    {
    strErr = QObject::tr( "Многократное описание переменной '%1'" ).arg( varRef.mvard.vname );
    return false;
    }

  varRef.mvard.varIdx = (long)uVarIdx;
  g_mapVarIdx.insert( varRef.mvard.varIdx, iVar );

  // Переменные - параметры сообщений
  for ( int iVarEx = 0; iVarEx < varRef.argvars.size(); ++iVarEx )
    {
    MVARD & varRefEx = varRef.argvars[ iVarEx ];
    QByteArray encodedStringEx = codec->fromUnicode( varRefEx.vname );
    quint16 uVarIdxEx = pGetVarIdxByName( encodedStringEx.constData() );
    if ( uVarIdxEx == 0xffff )
      {
      strErr = QObject::tr( "Переменная '%1' не найдена в ТОО" ).arg( varRefEx.vname );
      return false;
      }

    varRefEx.varIdx = (long)uVarIdxEx;
    int iVarExIdx =  iVar | ( ( iVarEx + 1 ) << 16 );
    if ( !g_mapVarIdx.contains( varRefEx.varIdx, iVarExIdx ) ) g_mapVarIdx.insert( varRefEx.varIdx, iVarExIdx );
    }
  }

return true;
}

//--------------------------------------------------------
QVariant MLibAccess::curVal( const MVARREC* pv, int index ) const
{
char valBuf[ nMaxVarSz ];
int nSz;
FILETIME ftim;
quint32 dwSts;

quint16 uVarIndex = 0xffff;
int nType = 0;
if ( index < 0 ) return QVariant();
else if ( index == 0 )
  {
  uVarIndex = (quint16)pv->mvard.varIdx;
  nType = pv->mvard.nType;
  }
else if ( index > pv->argvars.size() ) return QVariant();
else
  {
  MVARD mvardRef = pv->argvars[ index - 1 ];
  uVarIndex = (quint16)mvardRef.varIdx;;
  nType = mvardRef.nType;
  }
if ( uVarIndex == 0xffff ) return QVariant();

nSz = nMaxVarSz;
pGetVarValByOrd( m_iTask, uVarIndex, valBuf, &nSz, &ftim, &dwSts );

return convVar( nType, valBuf, ( ( dwSts & S_UNDEFINED ) == 0 ) );
}

//--------------------------------------------------------
QStringList MLibAccess::getArchFileNames( const QString & strProjectName )
{
QString strRegKeyName = getRegistryKeyName( strProjectName );

HKEY hkey = NULL;
DWORD dwVtype, dwLeng;
WCHAR keyName[ 1024 ];
int lg = strRegKeyName.toWCharArray ( keyName );
keyName[ lg ] = (WCHAR)0;
if ( ::RegOpenKeyExW( HKEY_CURRENT_USER, keyName, 0, KEY_READ, &hkey ) != ERROR_SUCCESS ) return QStringList();

DWORD dwPathCnt;

dwLeng = sizeof( DWORD );
if ( ::RegQueryValueExW( hkey, L"PathCnt", 0, &dwVtype, reinterpret_cast<LPBYTE>( &dwPathCnt ), &dwLeng ) != ERROR_SUCCESS )
  {
  ::RegCloseKey( hkey );
  return QStringList();
  }

QStringList retList;

for ( int i = 0; i < (int)dwPathCnt; ++i )
  {
  dwLeng = MAX_PATH;
  WCHAR libFileName[ MAX_PATH ];
  QString strPath = QString( "path%1" ).arg( i );
  LONG rc = ::RegQueryValueExW( hkey, reinterpret_cast<const WCHAR*>( strPath.utf16() ),
                                     0, &dwVtype, reinterpret_cast<LPBYTE>( &libFileName ), &dwLeng );
  if ( rc != ERROR_SUCCESS || dwVtype != REG_SZ )
    {
    ::RegCloseKey( hkey );
    return QStringList();
    }
  retList << QString::fromWCharArray( libFileName );
  }

return retList;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------
QString getRegistryKeyName( const QString & strProjectName )
{
QString strKeyName;
QString strPrjName = strProjectName.trimmed();
if ( !strPrjName.isEmpty() ) strPrjName += "\\";
strKeyName = QString( "SOFTWARE\\SCADA\\%1ARCHIVE" ).arg( strPrjName );

return strKeyName;
}

//------------------------------------------------------
bool conLibrary( QString & errstr )
{
QString strpath, esr;
TOOINI pTOOInit;

errstr.clear();

strpath = "Linker.dll";
if ( g_hLibInst != NULL )
  {
  errstr = QObject::tr( "Библиотека '%1' уже загружена" ).arg( strpath );
  return false;
  }

g_hLibInst = ::LoadLibrary( (const wchar_t *)strpath.utf16() );
if ( g_hLibInst == NULL )
  {
  lastErrorMessage( esr );
  errstr = QObject::tr( "Ошибка загрузки библиотеки '%1'\n%2" ).arg( strpath, esr );
  return false;
  }

try
  {
  const char* lpszFuncNam;

  lpszFuncNam = "TOOInit";
  pTOOInit = (TOOINI)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pTOOInit == NULL ) throw lpszFuncNam;

  lpszFuncNam = "TOOAttachByTaskname";
  pTOOAttach = (ATTCH)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pTOOAttach == NULL ) throw lpszFuncNam;

  lpszFuncNam = "TOODetach";
  pTOODetach = (DETCH)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pTOODetach == NULL ) throw lpszFuncNam;

  lpszFuncNam = "GetVarIdxByName";
  pGetVarIdxByName = (GVIBN)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pGetVarIdxByName == NULL ) throw lpszFuncNam;

  lpszFuncNam = "GetVarIdxByID";
  pGetVarIdxByID = (GVIBID)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pGetVarIdxByID == NULL ) throw lpszFuncNam;

  lpszFuncNam = "GetVarValByID";
  pGetVarValByID = (GVVBID)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pGetVarValByID == NULL ) throw lpszFuncNam;

  lpszFuncNam = "GetVarValByName";
  pGetVarValByName = (GVVBN)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pGetVarValByName == NULL ) throw lpszFuncNam;

  lpszFuncNam = "GetVarValByOrd";
  pGetVarValByOrd = (GVVBO)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pGetVarValByOrd == NULL ) throw lpszFuncNam;

  lpszFuncNam = "SetVarValByID";
  pSetVarValByID = (SVVBID)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pSetVarValByID == NULL ) throw lpszFuncNam;

  lpszFuncNam = "SetVarValByName";
  pSetVarValByName = (SVVBN)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pSetVarValByName == NULL ) throw lpszFuncNam;

  lpszFuncNam = "SetVarValByOrd";
  pSetVarValByOrd = (SVVBO)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pSetVarValByOrd == NULL ) throw lpszFuncNam;

  lpszFuncNam = "SetVarValByOrdEx";
  pSetVarValByOrdEx = (SVVBOEX)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pSetVarValByOrdEx == NULL ) throw lpszFuncNam;

  lpszFuncNam = "GetVarCnt";
  pGetVarCnt = (GVC)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pGetVarCnt == NULL ) throw lpszFuncNam;

  lpszFuncNam = "GetVarInfo";
  pGetVarInfo = (GVI)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pGetVarInfo == NULL ) throw lpszFuncNam;

  lpszFuncNam = "GetFirstVarNo";
  pGetFirstVarNo = (GFVN)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pGetFirstVarNo == NULL ) throw lpszFuncNam;

  lpszFuncNam = "GetNextVarNo";
  pGetNextVarNo = (GNVN)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pGetNextVarNo == NULL ) throw lpszFuncNam;

  lpszFuncNam = "WaitChange";
  pWaitChange = (WTCNG)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pWaitChange == NULL ) throw lpszFuncNam;

  lpszFuncNam = "CancelWait";
  pCancelWait = (CANWT)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pCancelWait == NULL ) throw lpszFuncNam;

  lpszFuncNam = "SetVarUndefined";
  pSetVarUndefined = (SVU)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pSetVarUndefined == NULL ) throw lpszFuncNam;

  lpszFuncNam = "GetQueuedResult";
  pGetQueuedResult = (GQR)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pGetQueuedResult == NULL ) throw lpszFuncNam;

  lpszFuncNam = "GetChangesCount";
  pGetChangesCount = (GCC)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pGetChangesCount == NULL ) throw lpszFuncNam;

  lpszFuncNam = "ForceInitialStatus";
  pForceInitialStatus = (FIS)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pForceInitialStatus == NULL ) throw lpszFuncNam;

  lpszFuncNam = "GetVer";
  pGetVer = (GVER)::GetProcAddress( g_hLibInst, lpszFuncNam );
  if ( pGetVer == NULL ) throw lpszFuncNam;
  }
catch ( const char *er )
  {
  QString strErr( er );
  lastErrorMessage( esr );
  errstr = QObject::tr( "Ошибка ::GetProcAddress( \"%1\" )\n%2" ).arg( strErr, esr );
  clearLibPointers();
  disconLibrary();
  return false;
  }

quint16 wVer = pGetVer();
if ( wVer < 0x100 )
  {
  errstr = QObject::tr( "Неверная версия библиотеки связи %1\nТребуется версия не ниже 1.00" ).arg( strpath );
  clearLibPointers();
  disconLibrary();
  return false;
  }

if ( !pTOOInit() )
  {
  errstr = QObject::tr( "Ошибка инициализации библиотеки %1\n(возможно не загружено ТОО)" ).arg( strpath );
  clearLibPointers();
  disconLibrary();
  return false;
  }

return true;
}

//--------------------------------------------------------
void disconLibrary()
{
if ( g_hLibInst != NULL )
  {
  ::FreeLibrary( g_hLibInst );
  g_hLibInst = NULL;
  }
return;
}

//--------------------------------------------------------
void lastErrorMessage( QString & errstr )
{
LPWSTR lpMsgBuf;

::FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, ::GetLastError(),
      MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
      (LPWSTR)&lpMsgBuf, 0, NULL );

errstr = QString::fromWCharArray( lpMsgBuf );
::LocalFree( lpMsgBuf );
errstr.replace( "\r", "" );
errstr.replace( "\n", "" );

return;
}

//--------------------------------------------------------
void clearLibPointers()
{
pTOOAttach = NULL;
pTOODetach = NULL;
pGetVarIdxByName = NULL;
pGetVarIdxByID = NULL;
pGetVarValByID = NULL;
pGetVarValByName = NULL;
pGetVarValByOrd = NULL;
pSetVarValByID = NULL;
pSetVarValByName = NULL;
pSetVarValByOrd = NULL;
pGetVarCnt = NULL;
pGetVarInfo = NULL;
pGetFirstVarNo = NULL;
pGetNextVarNo = NULL;
pWaitChange = NULL;
pCancelWait = NULL;
pSetVarUndefined = NULL;
pGetQueuedResult = NULL;
pGetChangesCount = NULL;
pForceInitialStatus = NULL;
pGetVer = NULL;
return;
}

//--------------------------------------------------------
void MLibAccess::startGetVarThread()
{
m_pGetVarThread->start();

return;
}

//------------------------------------------------------
void MLibAccess::stopGetVarThread()
{
if ( m_iTask < 0 ) return; // Если задача даже не подсоединена к ТОО, то поток и подавно
                           // не был запущен

// Дождемся завершения потока получения переменных из ТОО
m_pGetVarThread->stopThread();
pCancelWait( m_iTask );
m_pGetVarThread->wait();

return;
}

/////////////////////////////////////////////////////////////////////////////
// class QaGetVarThread
/////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------
QaGetVarThread::QaGetVarThread( QObject* parent) : QThread( parent )
{
m_bActThread = true;
connect( this, SIGNAL( inpVarChanged( const VDATA & ) ), qApp,
               SLOT( onInpVarChanged( const VDATA & ) ), Qt::QueuedConnection );

return;
}

//--------------------------------------------------------
QaGetVarThread::~QaGetVarThread()
{
MLibAccess* la = static_cast<QxArcApplication*>( qApp )->m_pLibAccess;
m_bActThread = false;
if ( isRunning() && la->m_iTask >= 0 && pCancelWait != NULL )
  {
  pCancelWait( la->m_iTask );
  }
wait();

return;
}

//--------------------------------------------------------
void QaGetVarThread::run()
{
char valBuf[ nMaxVarSz ];
BOOL bRet;
int nSz;
FILETIME ftim;
quint32 dwSts;
quint8 byOvflFlg;
quint16 vNo;
QDateTime sourceTimeBeg( QDate( 1601, 1, 1 ), QTime( 0, 0, 0, 0 ), Qt::UTC );

MLibAccess* la = static_cast<QxArcApplication*>( qApp )->m_pLibAccess;
pForceInitialStatus( la->m_iTask, TRUE );

// Обработка изменившихся переменных
while ( m_bActThread )
  {
  // Ждать изменения переменных в ТОО
  quint32 dwRet = ::WaitForSingleObject( g_hEvtTOO, INFINITE );

  if ( dwRet == WAIT_FAILED )
    {
    m_bActThread = false;
    break;
    }

  if ( !m_bActThread ) break;

  while ( 1 )
    {
    if ( !m_bActThread ) break;
    nSz = nMaxVarSz;
    bRet = pGetQueuedResult( la->m_iTask, &vNo, valBuf, &nSz, &ftim, &dwSts, &byOvflFlg );
    if ( !bRet )
      {
      m_bActThread = FALSE;
      break;
      }
    if ( vNo == 0xffff ) break;
    if ( !g_mapVarIdx.contains( (long)vNo ) ) continue;
    QList<int> idxList = g_mapVarIdx.values( (long)vNo );

    QDateTime nowTime = QDateTime::currentDateTimeUtc();
    quint64 uoffs = ( (quint64)ftim.dwLowDateTime + ( (quint64)ftim.dwHighDateTime << 32 ) ) / 10000;
    QDateTime sourceTime = sourceTimeBeg.addMSecs( uoffs );

    VDATA vdata;

    memcpy( vdata.valBuf, valBuf, nMaxVarSz );
    vdata.bDefined = ( ( dwSts & S_UNDEFINED ) == 0 );
    vdata.nowTimeU64 = nowTime.toMSecsSinceEpoch();
    vdata.sourceTimeU64 = sourceTime.toMSecsSinceEpoch();

    for ( int idx : idxList )
      {
      vdata.index = idx;
      emit inpVarChanged( vdata );
      }
    }
  }

return;
}
