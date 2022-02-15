#include "main.h"
#include "arcapplication.h"
#include "database.h"

#include <QLocale>
#include <QTranslator>
#include <QTextCodec>
#include <QUdpSocket>
#include <QNetworkInterface>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QProcess>
#include <QTimer>

#ifdef Q_OS_UNIX
  #include <unistd.h>
  #include <signal.h>
#endif

#include <string>
#include <iostream>

//#include <pqxx/transaction>
//#include <pqxx/connection>

//using namespace pqxx;

const int ncMaxUnitsSz = 32;

QString g_strTaskName;
QString g_strPrjName;
QString g_strArchName;

QVector<MVARREC> g_varsList;

#ifdef Q_OS_WIN
bool checkPrjName( const QString & strParVal );
#endif
bool readFile( const QString & fileName, QString & strErr );
bool initTOO( QString & strErr );

//BOOL WINAPI CtrlHandler( DWORD fdwCtrlType );

// D:\PostgreSQL\13\data\postgresql.conf
// lc_messages

const char* DB_USER = "postgres";      // Имя пользователя
const char* DB_PASSWORD = "postgres";  // Пароль

//--------------------------------------------------------
int main( int argc, char* argv[] )
{
setlocale( LC_ALL, "Russian" );
setlocale( LC_NUMERIC, "English" );

qRegisterMetaType<VDATA>( "VDATA" );

QxArcApplication app( argc, argv );

QLocale::setDefault( QLocale( QLocale::Russian, QLocale::RussianFederation ) );

const char* tr_names[] =
{
  ":translations/qt_ru.qm",
  ":translations/qtbase_ru.qm",
  nullptr
};

for ( int i = 0; tr_names[ i ] != nullptr; ++i )
  {
  QTranslator* qt_translator = new QTranslator( &app );
  if ( qt_translator->load( tr_names[ i ] ) )
    {
    app.installTranslator( qt_translator );
    }
  }

QCoreApplication::setApplicationVersion( QStringLiteral( "1.0" ) );

QString strAppDescription = QObject::tr( "Приложение архивации\n" );
printConsole( strAppDescription + QString( "\n\n" ) );

//qDebug() << hex << (short)u'D' << (short)u'S' << (short)u'N' << (short)u'U'; // !!!!!!!!!!!!!!!!!!

try
  {
  QCommandLineParser parser;

  parser.addHelpOption();
  parser.addVersionOption();

  QStringList optlist;

  optlist << QStringLiteral( "f" ) << QStringLiteral( "file" );
  QCommandLineOption fileOption( optlist, QObject::tr( "Задать имя файла описания архива" ), QStringLiteral( "file" ) );
  parser.addOption( fileOption );

#ifdef Q_OS_WIN
  optlist.clear();
  optlist << QStringLiteral( "p" ) << QStringLiteral( "project" );
  QCommandLineOption projectOption( optlist, QObject::tr( "Задать имя проекта" ), QStringLiteral( "project" ) );
  parser.addOption( projectOption );
#endif

  parser.process( app );

#ifdef Q_OS_WIN
  // Имя проекта
  QString strProjName = parser.value( projectOption ).trimmed();
  if ( !strProjName.isEmpty() )
    {
    if ( !checkPrjName( strProjName ) )
      {
      QString strErr = QObject::tr( "Неверное имя проекта '%1'\n" ).arg( strProjName );
      throw std::runtime_error( strErr.toUtf8().constData() );
      }
    }
#endif
  // Имя файла описания архива
  QString fileName = parser.value( fileOption ).trimmed();
  if ( fileName.isEmpty() )
    {
    #ifdef Q_OS_UNIX
    QString strErr = QObject::tr( "Не задано имя файла описания архива\n" ) + parser.helpText();
    throw std::runtime_error( strErr.toUtf8().constData() );
    #else
    QStringList pathList = MLibAccess::getArchFileNames( strProjName );
    if ( pathList.isEmpty() )
      {
      QString strErr = QObject::tr( "Описание архива отсутствует или ошибки чтения реестра\n" );
      throw std::runtime_error( strErr.toUtf8().constData() );
      }
    if ( pathList.size() == 1 ) fileName = pathList.first();
    else
      {
      // Перезапустить индивидуальные экземпляры приложения архиватора для каждого файла описания архива
      QString appPath = QCoreApplication::applicationFilePath();
      for ( const QString & strPath : pathList )
        {
        QStringList arguments;
        arguments << QString( "-f" ) << strPath;
        if ( !strProjName.isEmpty() ) arguments << QString( "-p" ) << strProjName;
        if ( !QProcess::startDetached( appPath, arguments ) )
          {
          QString strErr = QObject::tr( "Ошибка перезапуска приложения архивации\n" );
          throw std::runtime_error( strErr.toUtf8().constData() );
          }
        }
      return 0;
      }
    #endif
    }

  if ( !fileName.toLower().endsWith( QStringLiteral( ".bin" ) ) )
    {
    fileName += QStringLiteral( ".bin" );
    }

  fileName = QFileInfo( fileName ).absoluteFilePath();
  QFileInfo fi( fileName );
  if ( !fi.exists() )
    {
    QString strErr = QObject::tr( "Файл описания архива '%1' не найден" ).arg( fileName );
    throw std::runtime_error( strErr.toUtf8().constData() );
    }

  if ( !fi.isFile() )
    {
    QString strErr = QObject::tr( "'%1' не является файлом" ).arg( fileName );
    throw std::runtime_error( strErr.toUtf8().constData() );
    }

  if ( fi.size() == 0 )
    {
    QString strErr = QObject::tr( "Пустой файл '%1'" ).arg( fileName );
    throw std::runtime_error( strErr.toUtf8().constData() );
    }

  QString str_Err;
  //qDebug() << fileName;
  if ( !readFile( fileName, str_Err ) )
    {
    throw std::runtime_error( str_Err.toUtf8().constData() );
    }

  if ( !initTOO( str_Err ) )
    {
    throw std::runtime_error( str_Err.toUtf8().constData() );
    }

  // <Имя БД архива><Имя пользователя архива><Пароль доступа к архиву>
  QStringList saDbArgs = g_strArchName.split( QChar( '@' ), EtKeepEmptyParts );

  if ( saDbArgs.isEmpty() )
    {
    QString strErr = QObject::tr( "Неверное имя/параметры архива" );
    throw std::runtime_error( strErr.toUtf8().constData() );
    }

  QString strArchName = saDbArgs.first().trimmed().toLower();
  if ( strArchName.isEmpty() )
    {
    QString strErr = QObject::tr( "Неверное имя/параметры архива" );
    throw std::runtime_error( strErr.toUtf8().constData() );
    }

  QString strUserName = DB_USER;
  if ( saDbArgs.size() >= 2 )
    {
    strUserName = saDbArgs.at( 1 ).trimmed().toLower();
    if ( strUserName.isEmpty() )
      {
      QString strErr = QObject::tr( "Неверное имя пользователя для архива '%1'" ).arg( strArchName );
      throw std::runtime_error( strErr.toUtf8().constData() );
      }
    }

  QString strPassword = DB_PASSWORD;
  if ( saDbArgs.size() >= 3 )
    {
    strPassword = saDbArgs.at( 2 ).trimmed().toLower();
    if ( strPassword.isEmpty() )
      {
      QString strErr = QObject::tr( "Неверный пароль для архива '%1'" ).arg( strArchName );
      throw std::runtime_error( strErr.toUtf8().constData() );
      }
    }

#ifndef DEBUGNODB
  // Подключиться к/создать БД архива
  if ( !connect_database( strArchName, strUserName, strPassword, true, str_Err ) )
    {
    throw std::runtime_error( str_Err.toUtf8().constData() );
    }

  // Обновить/создать таблицу описания переменных
  if ( !populateGroupsTable( str_Err ) )
    {
    throw std::runtime_error( str_Err.toUtf8().constData() );
    }


  // Обновить/создать таблицу описания переменных
  if ( !populateDescriptionTable( str_Err ) )
    {
    throw std::runtime_error( str_Err.toUtf8().constData() );
    }

  // Подготовить SQL-запросы для записи в архив изменений переменных
  if ( !prepareRunQueries( str_Err ) )
    {
    throw std::runtime_error( str_Err.toUtf8().constData() );
    }
#endif
  }
catch ( const std::exception & e )
  {
  printConsole( QString::fromUtf8( e.what() ) + QChar( '\n' ) );
//  qDebug() <<QString::fromUtf8( e.what() ) + QChar( '\n' );
  return 1;
  }



////CREATE DATABASE test
////    WITH
////    OWNER = postgres
////    ENCODING = 'UTF8'
////    CONNECTION LIMIT = -1;

//// In your program, during startup connect to the template1 or postgres databases that're always available in a PostgreSQL
//// install and issue a SELECT 1 FROM pg_database WHERE datname = ? and as the first parameter pass the desired database name.

//// Check the result set that is returned. If a row is returned then database exists, you're done, no further action required.
//// If no row is returned then the database doesn't exist and you need to create it, so:

//// Issue a CREATE DATABASE mydatabasename; with any desired options like OWNER, ENCODING, etc per the manual to create the database
//// its self. The new database will be empty.

//// Populate the database either by connecting to the new database in your application and sending a sequence of SQL commands
//// from your application directly, or by invoking the psql command on the shell to read a sql script file and send that to the database.
//// I'd generally prefer to run the SQL directly within my application.

//try
//  {
//  const std::string dbstring = "dbname = postgres user = postgres password = root application_name = qarchiver "
////  const std::string dbstring = "dbname = postgres user = root password = root application_name = qarchiver "
//                               "hostaddr = 127.0.0.1 port = 5432";
//  pqxx::connection C( dbstring );
//  if ( C.is_open() )
//    {
//    QTextCodec* codec = QTextCodec::codecForName( "Windows-1251" );
//    QString s = codec->toUnicode( C.dbname() );
//    QString str = QObject::tr( "Opened database successfully: %1\n" ).arg( s );
//    printConsole( str );

//    C.set_client_encoding( std::string( "UNICODE" ) );

//    pqxx::work W( C );

////    pqxx::result R;

////    R = W.exec( "SET CLIENT_ENCODING TO 'UNICODE'" );
////    R. status == PGRES_COMMAND_OK;

//    pqxx::result R = W.exec( "SELECT 1 FROM pg_database WHERE datname = 'test'" );
////    pqxx::result R = W.exec( "SELECT 1 FROM pg_database WHERE datname = test'" );
//    W.commit();

////    QString sSz = QString::number( R.size() );
////    printConsole( sSz + "\n" );

//    if ( R.size() == 0 )
//      {

//      }
//    }
//  else
//    {
//    QString strErr = QObject::tr( "Can't open database\n" );
//    printConsole( strErr );
//    return 1;
//    }
//   C.disconnect();
//   }
//catch ( const pqxx::broken_connection & e )
//  {
////  QTextCodec* codec = QTextCodec::codecForName( "Windows-1251" );
////  QString strErrWin1251 = QStringLiteral( "Windows 1251 " ) + codec->toUnicode( e.what() ) + QStringLiteral( "\n" );
////  printConsole( strErrWin1251 );

////  QString strErrUtf8 = QStringLiteral( "UTF-8. " ) + QString::fromUtf8( e.what() ) + QStringLiteral( "\n" );
////  printConsole( strErrUtf8 );

//  QString strErr = QString::fromLocal8Bit( e.what() ) + QStringLiteral( "\n" );
//  printConsole( strErr );

//  return 1;
//  }
//catch ( const std::exception & e )
//  {
////  QTextCodec* codec = QTextCodec::codecForName( "Windows-1251." );
////  QString strErrWin1251 = QStringLiteral( "Windows 1251 " ) + codec->toUnicode( e.what() ) + QStringLiteral( "\n" );
////  printConsole( strErrWin1251 );

////  QString strErrUtf8 = QStringLiteral( "UTF-8. " ) + QString::fromUtf8( e.what() ) + QStringLiteral( "\n" );
////  printConsole( strErrUtf8 );

//  QString strErr = QString::fromUtf8( e.what() ) + QStringLiteral( "\n" );
//  printConsole( strErr );

//  return 1;
//  }





//if ( retCode != 0 )
//  {
//  #ifdef Q_OS_WIN
//  HWND hWnd = ::GetConsoleWindow();
//  ::ShowWindow( hWnd, SW_SHOW );
//  #endif

//  getchar();
//  }

//return retCode;

#ifdef Q_OS_WIN
HANDLE hStdInput = ::GetStdHandle( STD_INPUT_HANDLE );
::SetConsoleMode( hStdInput, 0 );

//::SetConsoleCtrlHandler( NULL, TRUE );
//::SetConsoleCtrlHandler( (PHANDLER_ROUTINE)CtrlHandler, TRUE );

HWND hWnd = ::GetConsoleWindow();
enableSysMenuClose( hWnd, false );
//::ShowWindow( hWnd, SW_HIDE );
#else
signal( SIGINT, SIG_IGN );
signal( SIGHUP, SIG_IGN );
signal( SIGTERM, SIG_IGN );
signal( SIGABRT, SIG_IGN );
#endif

QTimer::singleShot( 0, &app, SLOT( initApp() ) );
return app.exec();
}

//--------------------------------------------------------
void printConsole( const QString & str )
{
#ifdef Q_OS_WIN
HWND hWnd = ::GetConsoleWindow();
::ShowWindow( hWnd, SW_SHOW );
#endif

std::wcout << str.toStdWString();

return;
}

//LC_MESSAGES 	ru_RU.UTF-8
//LANG			ru_RU.UTF-8

//bool QPSQLDriverPrivate::setEncodingUtf8()
//{
//    PGresult *result = exec("SET CLIENT_ENCODING TO 'UNICODE'");
//    int status = PQresultStatus(result);
//    PQclear(result);
//    return status == PGRES_COMMAND_OK;
//}

//void QPSQLDriverPrivate::setDatestyle()
//{
//    PGresult *result = exec("SET DATESTYLE TO 'ISO'");
//    int status =  PQresultStatus(result);
//    if (status != PGRES_COMMAND_OK)
//        qWarning("%s", PQerrorMessage(connection));
//    PQclear(result);
//}

//void QPSQLDriverPrivate::setByteaOutput()
//{
//    if (pro >= QPSQLDriver::Version9) {
//        // Server version before QPSQLDriver::Version9 only supports escape mode for bytea type,
//        // but bytea format is set to hex by default in PSQL 9 and above. So need to force the
//        // server to use the old escape mode when connects to the new server.
//        PGresult *result = exec("SET bytea_output TO escape");
//        int status = PQresultStatus(result);
//        if (status != PGRES_COMMAND_OK)
//            qWarning("%s", PQerrorMessage(connection));
//        PQclear(result);
//    }
//}

//--------------------------------------------------------
bool hasOneOf( const QString & str, const char* s, QChar & illChar )
{
for ( int i = 0; s[ i ] != 0; ++i )
  {
  QChar c = QChar( s[ i ] );
  if ( str.contains( c ) )
    {
    illChar = c;
    return true;
    }
  }

return false;
}

#ifdef Q_OS_WIN
//------------------------------------------------------
bool checkPrjName( const QString & strParVal )
{
QString prjName = strParVal.trimmed();
if ( prjName.size() > nPrjNamLg ) return false;
QChar illChar;
if ( hasOneOf( prjName, "/*\\:\t%", illChar ) ) return false;

return true;
}
#endif

//--------------------------------------------------------
bool readFile( const QString & fileName, QString & strErr )
{
QByteArray ba;
QFile file( fileName );

if ( !file.open( QFile::ReadOnly ) )
  {
  strErr = QObject::tr( "Ошибка открытия файла '%1' :\n%2" ).arg( fileName ).arg( file.errorString() );
  return false;
  }

ba = file.readAll();
if ( ba.isEmpty() )
  {
  strErr = QObject::tr( "Ошибка чтения файла '%1' :\n%2" ).arg( fileName ).arg( file.errorString() );
  file.close();
  return false;
  }

file.close();

const TSK_FILE_HDR* pHdr = (TSK_FILE_HDR*)ba.constData();

const AUXINF* pArch = (const AUXINF*)( pHdr + 1 );
const ARCHVAR* pArchVar = (const ARCHVAR*)( pArch + 1 );

if ( memcmp( pHdr->synch, "AR", 2 ) != 0 )
  {
  strErr = QObject::tr( "Файл '%1' не является описанием архива" ).arg( fileName );
  return false;
  }

if ( pHdr->version < 0x0105 )
  {
  strErr = QObject::tr( "Неверная весия описания архива: 0x%1" ).arg( pHdr->version, 4, 16, QChar( '0' ) );
  return false;
  }

char szTaskName[ nTaskNamLg + 1 ];
memcpy( szTaskName, pHdr->taskname, nTaskNamLg );
szTaskName[ nTaskNamLg ] = 0;

char szPrjName[ nPrjNamLg + 1 ];
memcpy( szPrjName, pHdr->prjname, nPrjNamLg );
szPrjName[ nPrjNamLg ] = 0;

char szArchName[ nArchNameLg + 1 ];
memcpy( szArchName, pArch->archname, nArchNameLg );
szArchName[ nArchNameLg ] = 0;

QTextCodec* codec = QTextCodec::codecForName( "IBM 866" );
g_strTaskName = codec->toUnicode( szTaskName );
g_strPrjName = codec->toUnicode( szPrjName );
g_strArchName = codec->toUnicode( szArchName );

int nVarCount = (int)pHdr->nkeys;
g_varsList.clear();

for( int iv = 0; iv < nVarCount; ++iv )
  {
  MVARREC mvarrec;

  // Имя переменной
  char szVarName[ nVarNameLg + 1 ];
  memcpy( szVarName, pArchVar->vName, nVarNameLg );
  szVarName[ nVarNameLg ] = 0;
  mvarrec.mvard.vname = codec->toUnicode( szVarName );

  // Тип переменной
  mvarrec.mvard.nType = pArchVar->vType;

  // Номер/ключ переменной в ТОО
  mvarrec.mvard.varIdx =-1;

  // Описание переменной
  char szDescription[ nMaxDescrSz + 1 ];
  memcpy( szDescription, pArchVar->lpszDescription, nMaxDescrSz );
  szDescription[ nMaxDescrSz ] = 0;
  mvarrec.mvard.strDescription = codec->toUnicode( szDescription );

  // Количество знаков после запятой
  mvarrec.mvard.byNDigits = pArchVar->byNDigits;

  // Единицы измерения
  char szUnits0[ ncMaxUnitsSz + 1 ];
  memcpy( szUnits0, pArchVar->lpszUnits, ncMaxUnitsSz );
  szUnits0[ ncMaxUnitsSz ] = 0;
  mvarrec.mvard.strUnits = codec->toUnicode( szUnits0 );

  quint16 wUstNum = pArchVar->wUstNum;
  quint8 byExtVarsNum = pArchVar->byExtVarsNum;

  //qDebug().noquote() << mvarrec.mvard.vname << mvarrec.mvard.strDescription << mvarrec.mvard.strUnits
  //                 << "Type =" << mvarrec.mvard.nType << "NDigits =" << mvarrec.mvard.byNDigits; // !!!!!!!!!!!!!!!!!!

  ++pArchVar;

  mvarrec.argvars.clear();
  if ( byExtVarsNum != 0 )
    {
    const ARCMSGVAREX* pVar = reinterpret_cast<const ARCMSGVAREX*>( pArchVar );
    for ( quint8 ui1 = 0; ui1 < byExtVarsNum; ++ui1 )
      {
      MVARD mvardEx;

      // Имя переменной
      char szVarNameEx[ nVarNameLg + 1 ];
      memcpy( szVarNameEx, pVar->vName, nVarNameLg );
      szVarNameEx[ nVarNameLg ] = 0;
      mvardEx.vname = codec->toUnicode( szVarNameEx );

      // Тип переменной
      mvardEx.nType = pVar->vType;

      // Номер/ключ переменной в ТОО
      mvardEx.varIdx = -1;

      // Количество знаков после запятой
      mvardEx.byNDigits = pVar->byNDigits;

      // Единицы измерения
      char szUnits[ ncMaxUnitsSz + 1 ];
      memcpy( szUnits, pVar->lpszUnits, ncMaxUnitsSz );
      szUnits[ ncMaxUnitsSz ] = 0;
      mvardEx.strUnits = codec->toUnicode( szUnits );

      // Флаг необходимости генерировать сообщение при изменении этой переменной
      mvardEx.bProcChanges = pVar->procChanges;

//      qDebug().noquote() << mvardEx.vname << mvardEx.bProcChanges; // !!!!!!!!!!!!!!!!!!

      mvarrec.argvars << mvardEx;
      ++pVar;

      }
    pArchVar = reinterpret_cast<const ARCHVAR*>( pVar );
    }

  mvarrec.ustlist.clear();
  if ( wUstNum != 0 )
    {
    const USTAVEX* pUst = reinterpret_cast<const USTAVEX*>( pArchVar );
    for ( quint8 ui2 = 0; ui2 < wUstNum; ++ui2 )
      {
      MUSTAV mustav;

      // Значение уставки, при котором генерируется сообщение
      mustav.uUstVal = pUst->uUstVal;

      // Текст сообщения
      char szText[ nMaxUstTxtLen + 1 ];
      memcpy( szText, pUst->sText, nMaxUstTxtLen );
      szText[ nMaxUstTxtLen ] = 0;
      mustav.strText = codec->toUnicode( szText );

      // Тип сообщения (информационное, аварийное и т.д.)
      mustav.nUstType = pUst->nUstType;

//      qDebug().noquote() << mustav.strText; // !!!!!!!!!!!!!!!!!!

      mvarrec.ustlist << mustav;
      ++pUst;
      }
    pArchVar = reinterpret_cast<const ARCHVAR*>( pUst );
    }


  //qDebug().noquote() << "*****" << mvarrec.mvard.vname << mvarrec.mvard.varIdx; // !!!!!!!!!!!!!!!!!!

  g_varsList << mvarrec;
  }

return true;
}

//--------------------------------------------------------
bool initTOO( QString & strErr )
{
MLibAccess* la = static_cast<QxArcApplication*>( qApp )->m_pLibAccess;
if ( !la->connectTOO( strErr ) ) return false;

// Подсоединиться к ТОО
if ( !la->attachToTOO( g_strTaskName, strErr ) ) return false;

// Получить индексы переменных в ТОО
if ( !la->varIndexesFromTOO( strErr ) ) return false;

#ifdef Q_OS_UNIX
if ( !la->waitSynchroStart( strErr ) ) return false;
#endif

return true;
}

#ifdef Q_OS_WIN
//--------------------------------------------------------
void enableSysMenuClose( HWND hWnd, bool bEnable )
{
if ( hWnd == NULL ) return;

HMENU mnu = ::GetSystemMenu( hWnd, FALSE );
UINT nState = ::GetMenuState( mnu, SC_CLOSE, MF_BYCOMMAND );
if ( nState == (UINT)-1 || ( nState & ( bEnable ? MF_ENABLED : MF_GRAYED ) ) != 0 )
  {
  //Revert menu first
  //It was probably not reverted before
  ::GetSystemMenu( hWnd, TRUE );
  mnu = ::GetSystemMenu( hWnd, FALSE );
  }

//And enable/disable it now
if ( ::EnableMenuItem( mnu, SC_CLOSE, MF_BYCOMMAND | ( bEnable ? MF_ENABLED : MF_GRAYED ) ) != -1 )
  {
  //Redraw menu
  ::DrawMenuBar( hWnd );
  }

return;
}
#endif

//--------------------------------------------------------
QVariant convVar( int nType, const char* valBuf, bool bDefined )
{
QVariant val;

switch ( nType )
  {
  case VRT_REAL4:
  val = bDefined ? QVariant( (double)( *(float*)valBuf) ) : QVariant();
  break;

  case VRT_LOGICAL:
    {
    bool b = ( *(quint8*)valBuf != 0 );
    val = bDefined ? QVariant( b ) : QVariant();
    }
  break;

  case VRT_BYTE:
  val = bDefined ? QVariant( (quint8)(*(quint8*)valBuf) ) : QVariant();
  break;

  case VRT_SHORT:
  val = bDefined ? QVariant( (qint16)(*(qint16*)valBuf) ) : QVariant();
  break;

  case VRT_STRING:
  {
  if ( bDefined )
    {
    QTextCodec* codec = QTextCodec::codecForName( "Windows-1251" );
    val = QVariant( codec->toUnicode( valBuf ) );
    }
  else val = QVariant();
  }
  break;

  case VRT_LONG:
  val = bDefined ? QVariant( (qint32)(*(qint32*)valBuf) ) : QVariant();
  break;
  }

return val;
}
