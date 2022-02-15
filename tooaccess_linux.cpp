#include "main.h"
//#include "mainwindow.h"
//#include "mdichild.h"
//#include "etglobals.h"
#include "spvwrap.h"

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <QDebug>
#include <QTextCodec>
#include <QDateTime>
#include <QMultiMap>

extern QVector<MVARREC> g_varsList;

CSpv spv;
QMultiMap<long, int> g_mapVarIdx; // Индекс переменной в ТОО в индекс переменной в массиве g_varsList

/////////////////////////////////////////////////////////////////////////////
// class MLibAccess
/////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------
MLibAccess::MLibAccess()
{
m_iTask = -1;
m_pGetVarThread = new QaGetVarThread;

return;
}

//--------------------------------------------------------
MLibAccess::~MLibAccess()
{
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

return true;
}

//--------------------------------------------------------
void MLibAccess::disconnectTOO()
{
stopGetVarThread();
spv.detachTrioTask();

return;
}

//--------------------------------------------------------
bool MLibAccess::attachToTOO( const QString & strTaskName, QString & strErr )
{
QString errstr;

int nRet = spv.attachTrioTask( (uint32_t)TUAPIci_TASKCLASS_ASYNC_DUPLEX_IO, strTaskName, errstr );
if ( nRet != 0 )
  {
  strErr = QObject::tr( "Ошибка связи с ТОО (задача %1):\n%2\nВозможно не загружено ТОО" ).arg( strTaskName, errstr );
  return false;
  }

return true;
}

//--------------------------------------------------------
bool MLibAccess::waitSynchroStart( QString & strErr )
{
QString errstr;

// Ожидать запуска
if ( !spv.waitSynchroStart( errstr ) )
  {
  spv.detachTrioTask();
  strErr = QObject::tr( "Ошибка WaitSynchroStart: '%1'" ).arg( errstr );
  return false;
  }

return true;
}

//--------------------------------------------------------
bool MLibAccess::varIndexesFromTOO( QString & strErr )
{
g_mapVarIdx.clear();
for ( int iVar = 0; iVar < g_varsList.size(); ++iVar )
  {
  MVARREC & varRef = g_varsList[ iVar ];

  long lVarKey = spv.derefToVarKey( -1, varRef.mvard.vname, TUAPIci_D2VK_DF_BYNAME );

  if ( lVarKey < 0 )
    {
    strErr = QObject::tr( "Ошибка поиска переменной '%1' в ТОО: %2 (%3)" )
                      .arg( varRef.mvard.vname ).arg( spv.errorTextForm( lVarKey ) ).arg( lVarKey );

    return false;
    }

  if ( varRef.mvard.varIdx != -1 )
    {
    strErr = QObject::tr( "Многократное описание переменной '%1'" ).arg( varRef.mvard.vname );

    return false;
    }

  varRef.mvard.varIdx = lVarKey;

  long lTOOIndex = spv.propertiesOperation( NULL, -1, varRef.mvard.vname, TUAPIci_PO_GetVarTOOIndex | TUAPIci_PO_ByVarName );
  if ( lTOOIndex < 0 )
    {
    strErr = QObject::tr( "Ошибка поиска переменной '%1' в ТОО: %2 (%3)" )
                          .arg( varRef.mvard.vname ).arg( spv.errorTextForm( lTOOIndex ) ).arg( lTOOIndex );

    return false;
    }

  g_mapVarIdx.insert( lTOOIndex, iVar );

  // Переменные - параметры сообщений
  for ( int iVarEx = 0; iVarEx < varRef.argvars.size(); ++iVarEx )
    {
    MVARD & varRefEx = varRef.argvars[ iVarEx ];

    long lVarKeyEx = spv.derefToVarKey( -1, varRefEx.vname, TUAPIci_D2VK_DF_BYNAME );

    if ( lVarKeyEx < 0 )
      {
      strErr = QObject::tr( "Ошибка поиска переменной '%1' в ТОО: %2 (%3)" )
                            .arg( varRefEx.vname ).arg( spv.errorTextForm( lVarKeyEx ) ).arg( lVarKeyEx );

      return false;
      }

    varRefEx.varIdx = lVarKeyEx;

    long lTOOIndexEx = spv.propertiesOperation( NULL, -1, varRefEx.vname, TUAPIci_PO_GetVarTOOIndex | TUAPIci_PO_ByVarName );
    if ( lTOOIndexEx < 0 )
      {
      strErr = QObject::tr( "Ошибка поиска переменной '%1' в ТОО: %2 (%3)" )
                            .arg( varRefEx.vname ).arg( spv.errorTextForm( lTOOIndexEx ) ).arg( lTOOIndexEx );

      return false;
      }

    int iVarExIdx =  iVar | ( ( iVarEx + 1 ) << 16 );
    if ( !g_mapVarIdx.contains( lTOOIndexEx, iVarExIdx ) ) g_mapVarIdx.insert( lTOOIndexEx, iVarExIdx );
    }
  }

return true;
}

//--------------------------------------------------------
QVariant MLibAccess::curVal( const MVARREC* pv, int index ) const
{
char valBuf[ MAX_VARIABLESIZE + 1 ];

long lVarIndex = -1;
int nType = 0;
if ( index < 0 ) return QVariant();
else if ( index == 0 )
  {
  lVarIndex = pv->mvard.varIdx;
  nType = pv->mvard.nType;
  }
else if ( index > pv->argvars.size() ) return QVariant();
else
  {
  MVARD mvardRef = pv->argvars[ index - 1 ];
  lVarIndex = mvardRef.varIdx;;
  nType = mvardRef.nType;
  }
if ( lVarIndex == -1 ) return QVariant();

// Возвращает статус если >=0 или ошибку, если < 0
int status = ::TUAPI_UniversalReadVar( lVarIndex, valBuf, nullptr, 0 );
valBuf[ MAX_VARIABLESIZE ] = 0;

return ( status < 0 ) ? QVariant() : convVar( nType, valBuf, (bool)::TUAPI_IsStatusGood( status ) );
}

//--------------------------------------------------------
// В данном варианте имя файла описания архива будет взято из командной строки
QStringList MLibAccess::getArchFileNames( const QString & strProjectName )
{
Q_UNUSED( strProjectName )

return QStringList();
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
delete m_pGetVarThread;
m_pGetVarThread = nullptr;

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
if ( isRunning() )
  {
  m_bActThread = false;

  // Инициировать завершение потока, разблокировав функцию ожидания изменения переменных
  struct pollfd poll_fd;
  if ( spv.inputQueueContol( &poll_fd, TUAPIci_IQS_Get_pollfd ) )
    {
    shutdown( poll_fd.fd, SHUT_RDWR );
    close( poll_fd.fd );
    }

  wait();
  }

return;
}

//--------------------------------------------------------
void QaGetVarThread::run()
{
QDateTime sourceTimeBeg( QDate( 1970, 1, 1 ), QTime( 0, 0, 0, 0 ), Qt::UTC );

// Обработка изменившихся переменных
// Главный цикл
while ( m_bActThread )
  {
  TUAPIst_Time stTime;
  long lvNo = spv.waitForNewVar( &stTime, TUAPIci_W4NV_BLOCKING_WAIT ); // lvNo - индекс переменной в ТОО
  if ( !m_bActThread ) break;
  if ( lvNo < 0 ) continue;

  if ( !g_mapVarIdx.contains( lvNo ) ) continue;

  char valBuf[ MAX_VARIABLESIZE + 1 ];
  long lRet = spv.waitForNewVar( valBuf, TUAPIci_W4NV_GET_VALUE ); // lRet - индекс переменной в ТОО
  if ( !m_bActThread ) break;
  if ( lRet < 0 ) continue;

  // Индекс переменной в ТОО в список индексов переменной в массиве g_varsList (плюс переменные-параметры)
  QList<int> idxList = g_mapVarIdx.values( lvNo );
  bool bDefined = (bool)::TUAPI_IsStatusGood( lRet );

//  printConsole( QString( "Run %1\n" ).arg( lvNo ) ); // !!!!!!!!!!!!!!!!!!!!

  valBuf[ MAX_VARIABLESIZE ] = 0;

  QDateTime nowTime = QDateTime::currentDateTimeUtc();
  quint64 uoffs = (quint64)stTime.time * 1000ULL + (quint64)stTime.millitm;
  QDateTime sourceTime = sourceTimeBeg.addMSecs( uoffs );

  VDATA vdata;

  memcpy( vdata.valBuf, valBuf, (std::min)( MAX_VARIABLESIZE + 1, nMaxVarSz ) );
  vdata.bDefined = bDefined;
  vdata.nowTimeU64 = nowTime.toMSecsSinceEpoch();
  vdata.sourceTimeU64 = sourceTime.toMSecsSinceEpoch();

  for ( int idx : idxList )
    {
    vdata.index = idx;
    emit inpVarChanged( vdata );
    }
  } // Конец главного цикла

return;
}
