#include "main.h"
#include "database.h"
#include "arcapplication.h"

#include <QNetworkInterface>

extern QVector<MVARREC> g_varsList;

const quint16 quitPort = 35144;
const quint16 quitPortS = quitPort + 1;

const int ncTimerPeriod = 100; // млс

const uint dwcPlain = 0;
const uint dwcSubscript = 1;
const uint dwcSuperScript = 2;
const uint dwcUnderline = 3;
const uint dwcMask = 3;
const uint dwcBold = 4;
const uint dwcItalic = 8;

const uint dwcSubscriptF = 1;
const uint dwcSuperScriptF = 2;
const uint dwcUnderlineF = 4;
const uint dwcBoldF = 8;
const uint dwcItalicF = 16;

const char* bmStart = "\t# ";
const char* bmEnd = " @\t";

// <b>...</b>       Bold
// <i>...</i>       Italics
// <u>...</u>       Underline
// <sup>...</sup> 	Superscript
// <sub>...</sub>	Subscript

uint uAtrrF[] = { dwcSubscriptF, dwcSuperScriptF, dwcUnderlineF, dwcBoldF, dwcItalicF };
const int ncAttrSz = (int)__countof( uAtrrF );
const uint ucTypesSz = 17;
int types[ ucTypesSz ] = { -1 };

const char* begTags[ ncAttrSz ] =
{
"<sub>",
"<sup>",
"<u>",
"<b>",
"<i>"
};

const char* endTags[ ncAttrSz ] =
{
"</sub>",
"</sup>",
"</u>",
"</b>",
"</i>"
};

int attrToType( uint uAttr ) { Q_ASSERT( uAttr < ucTypesSz ); return types[ uAttr ]; }
//uint typeToAttr( int nType ) { Q_ASSERT( nType >= 0 && nType < ncAttrSz ); return uAtrrF[ nType ]; }
//inline QString begTagFromType( int nType ) { return QString( begTags[ nType ] ); }
//inline QString endTagFromType( int nType ) { return QString( endTags[ nType ] ); }
inline QString begTagFromAttr( uint uAttr ) { return QString( begTags[ attrToType( uAttr ) ] ); }
inline QString endTagFromAttr( uint uAttr ) { return QString( endTags[ attrToType( uAttr ) ] ); }

/////////////////////////////////////////////////////////////////////////////
// class QxArcApplication
/////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------
QxArcApplication::QxArcApplication( int & argc, char** argv ) : QCoreApplication( argc, argv ), m_timerId( 0 )
{
for ( int i = 0; i < ncAttrSz; ++i )
  {
  types[ uAtrrF[ i ] ] = i;
  }

m_pLibAccess = new MLibAccess;
m_quitSocket.bind( QHostAddress::Any, quitPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint );
connect( &m_quitSocket, SIGNAL( readyRead() ), this, SLOT( readQuitDatagram() ), Qt::QueuedConnection );
connect( this, SIGNAL( aboutToQuit() ), this, SLOT( onAboutToQuit() ) );

return;
}

//--------------------------------------------------------
QxArcApplication::~QxArcApplication()
{
delete m_pLibAccess;
//qDebug() << "d'tor QxArcApplication"; // !!!!!!!!!!!!!!!!!!!!!!!

closeDb();

#ifdef Q_OS_WIN
HWND hWnd = ::GetConsoleWindow();
enableSysMenuClose( hWnd, true );
#endif

return;
}

//--------------------------------------------------------
void QxArcApplication::initApp()
{
m_timerId = startTimer( ncTimerPeriod, Qt::PreciseTimer );
m_pLibAccess->startGetVarThread();

return;
}

//--------------------------------------------------------
void QxArcApplication::readQuitDatagram()
{
QByteArray datagram;

quint64 sz = m_quitSocket.pendingDatagramSize();

datagram.resize( ( sz == 0 ) ? 1 : sz );
QHostAddress address;
quint16 port;
m_quitSocket.readDatagram( datagram.data(), sz, &address, &port );

// Закрытие приложения возможно только с определенного порта
if ( port != quitPortS ) return;

 // Закрытие приложения возможно только с локального IP-адреса
QList<QHostAddress> list = QNetworkInterface::allAddresses();
bool bFound = false;
for( int i = 0; i < list.size(); ++i )
  {
  if ( list.at( i ).protocol() == QAbstractSocket::IPv4Protocol &&
       list.at( i ).toIPv4Address() == address.toIPv4Address() )
    {
    bFound = true;
    break;
    }
  }
if ( !bFound ) return;

if ( datagram.startsWith( "QUIT" ) )
  {
  quit();
  }

return;
}

//--------------------------------------------------------
void QxArcApplication::onAboutToQuit()
{
//qDebug() << "onAboutToQuit()"; // !!!!!!!!!!!!!!!!!!!

m_pLibAccess->stopGetVarThread();
killTimer( m_timerId );

return;
}

//--------------------------------------------------------
void QxArcApplication::onInpVarChanged( const VDATA & vdata )
{
//qDebug() << "onInpVarChanged"; // !!!!!!!!!!!!!!!!!!!

int iVidx = vdata.index & 0xffff;
MVARREC & mvarrec = g_varsList[ iVidx ];

int iExIdx = ( vdata.index >> 16 );

if ( iExIdx == 0 )
  {
  mvarrec.mvard.curVal = convVar( mvarrec.mvard.nType, vdata.valBuf, vdata.bDefined );
  mvarrec.mvard.nowTimeU64 = vdata.nowTimeU64;
  mvarrec.mvard.sourceTimeU64 = vdata.sourceTimeU64;

  m_changedVars.insert( iVidx );

 updateMinMaxVals( mvarrec.mvard );
 }
else if ( iExIdx >= 1 && iExIdx <= mvarrec.argvars.size() )
  {
  MVARD & mvardRef = mvarrec.argvars[ iExIdx - 1 ];

  mvardRef.curVal = convVar( mvardRef.nType, vdata.valBuf, vdata.bDefined );

//  qDebug() << "onInpVarChanged" << mvardRef.curVal; // !!!!!!!!!!!!!!!!!!!

  mvardRef.nowTimeU64 = vdata.nowTimeU64;
  mvardRef.sourceTimeU64 = vdata.sourceTimeU64;

  if ( mvardRef.bProcChanges ) m_changedVars.insert( iVidx );

  updateMinMaxVals( mvardRef );
  }

return;
}

//--------------------------------------------------------
void QxArcApplication::updateMinMaxVals( MVARD & mvardRef )
{
if ( mvardRef.curVal.isValid() && mvardRef.curVal.canConvert( QVariant::Double ) )
  {
  double dblVal = mvardRef.curVal.toDouble();
  if ( dblVal < mvardRef.dblMin ) mvardRef.dblMin = dblVal;
  if ( dblVal > mvardRef.dblMax ) mvardRef.dblMax = dblVal;
  }

return;
}

//--------------------------------------------------------
void QxArcApplication::takePrevVal( MVARD & mvardRef, double dblVal, double & dblPrevVal, qint64 & prevNowTimeU64, qint64 & prevSourceTimeU64 )
{
dblPrevVal = mvardRef.dblPrevVal;
prevNowTimeU64 = mvardRef.prevNowTimeU64;
prevSourceTimeU64 = mvardRef.prevSourceTimeU64;

mvardRef.dblPrevVal = dblVal;
mvardRef.prevNowTimeU64 = mvardRef.nowTimeU64;
mvardRef.prevSourceTimeU64 = mvardRef.sourceTimeU64;

return;
}

//--------------------------------------------------------
void QxArcApplication::takeMinMaxVal( MVARD & mvardRef, double dblVal, double & dblMin, double & dblMax )
{
dblMin = mvardRef.dblMin;
dblMax = mvardRef.dblMax;
mvardRef.dblMin = dblVal;
mvardRef.dblMax = dblVal;

return;
}

//------------------------------------------------------
void QxArcApplication::timerEvent( QTimerEvent* event )
{
Q_UNUSED( event )

for ( int idx : m_changedVars )
  {
  MVARREC & mvarrec = g_varsList[ idx ];
  QVariant curVal = mvarrec.mvard.curVal;

  if (settings.value("DEBUG").toBool()) qDebug()<<mvarrec.mvard.vname;

  if ( mvarrec.ustlist.isEmpty() ) // Если список уставок пуст - числа архивируем как значение, тексты - как сообщения
    {
    if ( mvarrec.mvard.nType == VRT_STRING ) // Сообщение
      {
      // Для текстовых сообщений не обрабатываем неопределенные значения
      if ( curVal.isValid() )

        {
        QString strRes = processMessage( curVal.toString(), mvarrec );
        strRes = makeHtml( strRes);

#ifndef DEBUGNODB
        add_message_to_db( mvarrec.mvard.iArchId, strRes, mvarrec.mvard.nowTimeU64 );
#else
        printConsole( tr( "'add_message_to_db:' message='%1' time=%2\n" )
                                                .arg( strRes ).arg( mvarrec.mvard.nowTimeU64 ) );
#endif
        }
      }
    else // Значение
      {
      double dblVal = mvarrec.mvard.curVal.isValid() ?
                      mvarrec.mvard.curVal.toDouble() : std::numeric_limits<double>::quiet_NaN();

      // TODO: Сохранять предыдущее значение и время
      double dblPrevVal;
      qint64 prevNowTimeU64, prevSourceTimeU64;
      takePrevVal( mvarrec.mvard, dblVal, dblPrevVal, prevNowTimeU64, prevSourceTimeU64 );

      // TODO: Сохранять минимальное и максимальное значение за период накопления
      double dblMin, dblMax;
      takeMinMaxVal( mvarrec.mvard, dblVal, dblMin, dblMax );

#ifndef DEBUGNODB
      add_value_change_to_db( mvarrec.mvard.iArchId, dblVal, mvarrec.mvard.nowTimeU64 );
#else
      printConsole( tr( "'add_value_change_to_db:' id=%1 value=%2 time=%3\n" )
                    .arg( mvarrec.mvard.iArchId ).arg( dblVal ).arg( mvarrec.mvard.nowTimeU64 ) );
#endif
      }
    }
  else
    {
    double dblVal = mvarrec.mvard.curVal.isValid() ?
                    mvarrec.mvard.curVal.toDouble() : std::numeric_limits<double>::quiet_NaN();

    // TODO: Сохранять предыдущее значение и время
    double dblPrevVal;
    qint64 prevNowTimeU64, prevSourceTimeU64;
    takePrevVal( mvarrec.mvard, dblVal, dblPrevVal, prevNowTimeU64, prevSourceTimeU64 );

    // TODO: Сохранять минимальное и максимальное значение за период накопления
    double dblMin, dblMax;
    takeMinMaxVal( mvarrec.mvard, dblVal, dblMin, dblMax );

#ifndef DEBUGNODB
    add_value_change_to_db( mvarrec.mvard.iArchId, dblVal, mvarrec.mvard.nowTimeU64 );
#else
    printConsole( tr( "'add_value_change_to_db:' id=%1 value=%2 time=%3\n" )
                  .arg( mvarrec.mvard.iArchId ).arg( dblVal ).arg( mvarrec.mvard.nowTimeU64 ) );
#endif

    QString strMsg;
    // Для текстовых сообщений не обрабатываем неопределенные значения
    if ( curVal.isValid() )
      {
      // Если требуется обрабатывать любое изменение
      if ( mvarrec.ustlist.first().uUstVal == ucFreeUst )
          strMsg = mvarrec.ustlist.first().strText;
      else
        {
        // Найдем текст сообщения, соответствующий значению переменной
//        for ( const MUSTAV & mu : mvarrec.ustlist )
//          {
//          if ( mu.uUstVal == curVal.toUInt() )
//            {
//            strMsg = mu.strText;
//            break;
//            }
//          }
		QList<MUSTAV> & ustlistRef = mvarrec.ustlist;
		QList<MUSTAV>::iterator it = std::find( ustlistRef.begin(), ustlistRef.end(), curVal.toUInt() );
		if ( it != ustlistRef.end() ) strMsg = it->strText;
		}
      }

    if ( !strMsg.isEmpty() )
      {
      if (settings.value("DEBUG").toBool())qDebug() << mvarrec.mvard.vname << "Message:" << strMsg; // !!!!!!!!!!!!!!!!!!!!!!
//      mvarrec.mvard
//      mvarrec.argvars
      QString strRes = processMessage( strMsg, mvarrec );
//      qDebug().noquote() << mvarrec.mvard.vname << "Before makeHtml:" << strRes; // !!!!!!!!!!!!!!!!!!!!!!
      strRes = makeHtml( strRes );
//      qDebug().noquote() << "After makeHtml:" << strRes; // !!!!!!!!!!!!!!!!!!!!!!

#ifndef DEBUGNODB
      add_message_to_db( mvarrec.mvard.iArchId,strRes, mvarrec.mvard.nowTimeU64 );
#else
      printConsole( tr( "'add_message_to_db:' message='%1' time=%2\n" )
                                              .arg( strRes ).arg( mvarrec.mvard.nowTimeU64 ) );
#endif
      }
    }

//  mvarrec.ustlist.size()

//  qDebug() << "Process" << mvarrec.mvard.vname << mvarrec.argvars.size(); // !!!!!!!!!!!!!!!!!!!
//  for ( const MVARD & mvardRef : mvarrec.argvars )
//    {
//    qDebug() << mvardRef.vname; // !!!!!!!!!!!!!!!!!!!!
//    }
  }

m_changedVars.clear();

return;
}

//------------------------------------------------------
// Формирование сообщения, используя значения переменных - параметров
// @<цифра> (например, @1, @2, …) - вывод значения соответствующей переменной с числом знаков после
//                                  запятой равным числу, следующему за знаком “@”.
// @D - вывод значения в формате по умолчанию, определенном в словаре глобальных переменных.
// @S - вставлять значение как строку.
// @N - пропустить переменную (для уставок с разным количеством переменных).
// @@ - вставить одиночный символ @ в выводимую строку.
// @U - вывод единиц измерения, определенных в словаре глобальных переменных.
QString QxArcApplication::processMessage( const QString & strMsg, const MVARREC & mvarrec ) const
{
QString strOut;

int iVar = 0;
bool bAt = false; // Флаг '@'
for ( int ic = 0; ic < strMsg.size(); ++ic )
  {
  QChar ch = strMsg.at( ic );
  ushort uChCode = ch.toUpper().unicode();

  if ( bAt )
    {
    bAt = false;

    switch ( uChCode )
      {
      case u'0':
      case u'1':
      case u'2':
      case u'3':
      case u'4':
      case u'5':
      case u'6':
      case u'7':
      case u'8':
      case u'9':
        {
        if ( iVar >= 0 && iVar < mvarrec.argvars.size() )
          {
          MVARD mvardRef = mvarrec.argvars[ iVar ];
          int nDigits = (int)( uChCode - u'0' );
          strOut += convVarVal( mvardRef.curVal, nDigits, false );
          }
        else strOut += "<Error>";
        ++iVar;
        }
      break;

      case u'D':
        {
        if ( iVar >= 0 && iVar < mvarrec.argvars.size() )
          {
          MVARD mvardRef = mvarrec.argvars[ iVar ];
          int nDigits = (int)mvardRef.byNDigits;
          strOut += convVarVal( mvardRef.curVal, nDigits, false );
          }
        else strOut += "<Error>";
        ++iVar;
        }
      break;

      case u'S':
        {
        if ( iVar >= 0 && iVar < mvarrec.argvars.size() )
          {
          MVARD mvardRef = mvarrec.argvars[ iVar ];
          int nDigits = (int)mvardRef.byNDigits;
          strOut += convVarVal( mvardRef.curVal, nDigits, false );
          }
        else strOut += "<Error>";
        ++iVar;
        }
      break;

      case u'N':
      ++iVar;
      break;

      case u'U':
        {
        if ( iVar <= 0 ) strOut +=  mvarrec.mvard.strUnits;
        else if ( iVar >= mvarrec.argvars.size() ) strOut += "<Error>";
        else strOut += mvarrec.argvars[ iVar - 1 ].strUnits;
        }
      break;

      default:
      strOut += ch;
      break;
      }
    }
  else if ( ch == QChar( '@' ) ) bAt = true;
  else strOut += ch;
  }

return strOut;
}

//------------------------------------------------------
// Преобразование значения в строку
QString QxArcApplication::convVarVal( const QVariant & var, int nDigits, bool bString ) const
{
QString strText;
const char* digits[] = { "", ".0", ".00", ".000", ".0000", ".00000", ".000000", ".0000000",
                                                                     ".00000000", ".000000000" };
nDigits = (std::min)( nDigits, (int)__countof( digits ) - 1 );

switch ( var.type() )
  {
  case QVariant::Bool:
  strText = var.toBool() ? QObject::tr( "1" ) : QObject::tr( "0" );
  strText += digits[ nDigits ];
  break;

  case QVariant::Int:
  strText = QString::number( var.toInt() );
  strText += digits[ nDigits ];
  break;

  case QVariant::UInt:
  strText = QString::number( var.toUInt() );
  strText += digits[ nDigits ];
  break;

  case QVariant::LongLong:
  strText = QString::number( var.toLongLong() );
  strText += digits[ nDigits ];
  break;

  case QVariant::ULongLong:
  strText = QString::number( var.toULongLong() );
  strText += digits[ nDigits ];
  break;

  case QVariant::Double:
  strText = QString::number( var.toDouble(), 'f', nDigits );
  break;

  case QVariant::Char:
  strText = QString::number( var.toChar().digitValue() );
  strText += digits[ nDigits ];
  break;

  case QVariant::String:
  if ( bString )
    {
    QString str = var.toString();
    str.remove( QChar( '\\' ) );
    strText += str;
    }
  else
    {
    bool bOk;
    double val = var.toString().toDouble( &bOk );
    strText = bOk ? QString::number( val, 'f', nDigits ) : QObject::tr( "???" );
    }
  break;

  default:
  strText = QObject::tr( "???" );
  break;
  }

return strText;
}

//------------------------------------------------------
uint QxArcApplication::codeFromChar( QChar ch ) const
{
QString str = ch;
bool bOk;
uint uCode = str.toUInt( &bOk, 16 );
if ( !bOk ) return (uint)-1;
else
  {
  uint uCodeF = 0;
  switch ( uCode & dwcMask )
    {
    case dwcPlain: // Обычный шрифт (м.б. курсив и/или жирный)
    break;

    case dwcSubscript: // Подстрочные символы
    uCodeF |= dwcSubscriptF;
    break;

    case dwcSuperScript: // Надстрочные символы
    uCodeF |= dwcSuperScriptF;
    break;

    case dwcUnderline: // Обычный шрифт с подчеркиванием
    uCodeF |= dwcUnderlineF;
    break;

    default:
    break;
    }

  if ( ( uCode & dwcItalic ) != 0 ) uCodeF |= dwcItalicF;
  if ( ( uCode & dwcBold ) != 0 ) uCodeF |= dwcBoldF;

  return uCodeF;
  }
}

//------------------------------------------------------
QString QxArcApplication::makeHtml( const QString & strText ) const
{
QString strMsg = strText;
strMsg += QStringLiteral( "\\0" );

bool bBackSlash = false; // Флаг '\'
uint uPrevCodeF = 0;
QVector<uint> codeStack( 1, 0 );

for ( int ic = 0; ic < strMsg.size(); )
  {
  QChar ch = strMsg.at( ic );

  if ( !bBackSlash )
    {
     if ( ch == QChar( '\\' ) ) bBackSlash = true;
    ++ic;
    }
  else
    {
    bBackSlash = false;
    strMsg.remove( --ic, 2 );

    if ( ch == QChar( '\\' ) )
      {
      const char* htmlBackSlash = "&#92;";
      strMsg.insert( ic, htmlBackSlash );
      ic += (int)strlen( htmlBackSlash );

      continue;
      }

    uint uCodeF = codeFromChar( ch );
    if ( uCodeF == (uint)-1 )
      {
      QString strError = QStringLiteral( "<Error>" );
      strMsg.insert( ic, strError );
      ic += strError.size();

      continue;
      }

    // ----------------------
    // Конец блока

    uint uEndedF = uPrevCodeF & ~uCodeF; // Атрибуты символа, закончившие действие
    if ( uEndedF != 0 )
      {
      while ( ( uPrevCodeF & uEndedF ) != 0 )
        {
        while ( uEndedF != 0 )
          {
          uint uAttr = codeStack.takeLast();
          QString strEndTag = endTagFromAttr( uAttr );
          strMsg.insert( ic, strEndTag );
          ic += strEndTag.size();
          uEndedF &= ~uAttr;
          uPrevCodeF &= ~uAttr;
          }
        }
      }

    // ----------------------
    // Начало блока

    uint uStartedF = ~uPrevCodeF & uCodeF; // Атрибуты символа, начавшие действие
    if ( uStartedF != 0 )
      {
      for ( int i = 0; i < ncAttrSz; ++i )
        {
        uint uAttrBit = ( 1 << i );
        if ( ( uStartedF & uAttrBit ) != 0 )
          {
          codeStack.push_back( uAttrBit );
          QString strBegTag = begTagFromAttr( uAttrBit );
          strMsg.insert( ic, strBegTag );
          ic += strBegTag.size();
          }
        }

      uPrevCodeF |= uStartedF;
      }
    }
  } // for ( int ic

return strMsg;
}

//<sup>qqq<i>qqqqq</i>q</sup><sub>qqqqq<b>qqqq</b></sub><u>qqq<b>qqqqq</b>qqqqq<b>q</u>qqqqqqqqqqq</b><i>qqqqqqqqqq</i>qqqqqqqqqqq
//sup>qqq<i>qqqqq</i>q</sup><sub>qqqqq<b>qqqq</b></sub><u>qqq<b>qqqqq</b>qqqqq<b>q</b></u><b>qqqqqqqqqqq</b><i>qqqqqqqqqq</i>qqqqqqqqqqq
