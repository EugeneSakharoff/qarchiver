#ifndef MAIN_H
#define MAIN_H

#include <QCoreApplication>
#include <QList>
#include <QThread>
#include <QVariant>
#include <QSettings>


#include <limits>

#ifdef Q_OS_WIN
  #include <qt_windows.h>
//  #include <limits>
  void enableSysMenuClose( HWND hWnd, bool bEnable );
#endif

//#define DEBUGNODB

void printConsole( const QString & str );



#define nVarNameLg  32 // Max length of a variable name
#define nMaxVarSz  200 // Max length of a string and an array-type variable
#define nPrjNamLg   32 // Max length of a project name
#define nTaskNamLg  16 // Max length of a task name
#define nSpcPrmLg  128 // Max length of a specialized task parameter
#define nMaxDescrSz  128 // Max length of a variable description string
#define nMaxUstTxtLen  128 // Max ust. text length

const quint32 S_UNDEFINED = 1; // Флаг неопределенного значения переменной
const quint32 S_INITIAL = 256; // Флаг начального значения переменной
const quint32 S_NOSEND = 128; // Флаг запрета рассылки соседним каналам
const quint32 S_THISCHAN = 1024; // Признак того, что переменная установлена задачей данного канала

#define __countof( array ) ( sizeof( array ) / sizeof( array[ 0 ] ) )

#if QT_VERSION < QT_VERSION_CHECK( 5, 15, 0 )
#define EtKeepEmptyParts QString::KeepEmptyParts
#define EtSkipEmptyParts QString::SkipEmptyParts
#else
#define EtKeepEmptyParts Qt::KeepEmptyParts
#define EtSkipEmptyParts Qt::SkipEmptyParts
#endif

enum
{
CMD_ATT_QUERY = 1, // Присоединение по опросу
CMD_ATT_MASK, // Присоединение по маске
CMD_ATT_QUEUE, // Присоединение с буферизацией, извещение по событию
CMD_RESERVED0,
CMD_RESERVED1,
CMD_RESERVED2,
CMD_RESERVED3,
CMD_RESERVED4,
CMD_DETACH
};

// Variables types enum
enum
{
VRT_REAL4 = 0,
VRT_LOGICAL,
VRT_BYTE,
VRT_SHORT,
VRT_TIMER,
VRT_STRING,
VRT_LONG,
VRT_BINARY,
VRT_VOID = 100
};

// Variables attributes enum
enum
{
IID_INVAR = 0,
IID_OUTVAR,
IID_LCLVAR,
IID_INLVLVAR
};

typedef struct tagVARINFO
{
char vName[ nVarNameLg ]; // Имя переменной
short vType; // Тип переменной
long lID; // Идентификатор
quint16 nDstTasks; // Количество задач - приемников изменений
} VARINFO;

#pragma pack( 1 )

// File header structure for ARCHIVE task
typedef struct tagTSK_FILE_HDR
{
char synch[ 2 ];
char copyright[ 50 ];
char prjname[ nPrjNamLg ];
quint16 version;
char taskname[ nTaskNamLg ];
quint16 nkeys;
qint32 lTimeStamp;
char szActArchVar[ nVarNameLg ];
char szTime2ArchVar[ nVarNameLg ];
quint8 byActMode;
char reserved[ 28 ];
} TSK_FILE_HDR;

// Archive name structure for ARCHIVE task
#define nArchNameLg 64
typedef struct tagAUXINF
  {
  char archname[ nArchNameLg ];
  quint32 uPeriodReq;
  quint32 uMandatoryReq;
  quint32 uLimitMb;
  quint32 uLimitHr;
  quint32 uCheckLim;
  quint8 byChkDiff;
  quint8 byChkPeriodReq;
  quint8 byChkMandatory;
  quint8 byChkLimitMB;
  quint8 reserved[ 32 ];
  } AUXINF;

// Variable description structure for ARCHIVE task
typedef struct tagARCHVAR
  {
  char vName[ nVarNameLg ]; // Variable name
  short vType; // Variable type
  quint32 lVarID; // Variable ID
  char lpszDescription[ nMaxDescrSz ]; // Variable description
  quint16 wUstNum;
  quint8 byNDigits; // Number of digits after decimal point
  char lpszUnits[ 32 ]; // Measuring units
//  char lpszExtMsg[ nMaxUstTxtLen ]; // Extended message
  char reserved1[ nMaxUstTxtLen ]; // Reserved
//  quint32 uExtMsgType; // Extended message type
  quint32 reserved2; // Reserved
  quint8 byExtVarsNum; // Number of variables - parameters of extended message
//  quint8 byUseStaticText; // Use static text flag (lpszExtMsg) for extended message
  quint8 reserved3; // Reserved
  quint8 reserved[ 31 ];
  } ARCHVAR;

// USTAVKA description structure for a new ALARM task
typedef struct tagUSTAVEX
  {
  quint32 uUstVal;
  short nUstType;
  char sText[ nMaxUstTxtLen ];
  quint8 reserved[ 16 ];
  } USTAVEX;

// Ext message variable description structure for an ARCHIVE task
typedef struct tagARCMSGVAREX
  {
  char vName[ nVarNameLg ]; // Variable name
  short vType; // Variable type
  int32_t lVarID; // Variable ID
  quint8 byNDigits; // Number of digits after decimal point
  char lpszUnits[ 32 ]; // Measuring units
  int procChanges; // Archive if changed flag
  quint8 reserved[ 32 ];
  } ARCMSGVAREX;

#pragma pack()

/////////////////////////////////////////////////////////////////////////////
// struct MVARD
/////////////////////////////////////////////////////////////////////////////

struct MVARD
{
QString vname; // Имя переменной
int nType; // Тип переменной
long varIdx; // Номер/ключ переменной в ТОО
QString strDescription; // Описание переменной
QString strGroup;
int iGroupIdx;
quint8 byNDigits; // Количество знаков после запятой
QString strUnits; // Единицы измерения
bool bProcChanges;
QVariant curVal;
qint64 nowTimeU64;
qint64 sourceTimeU64;
double dblPrevVal;
qint64 prevNowTimeU64;
qint64 prevSourceTimeU64;
double dblMin;
double dblMax;
int iArchId; // Идентификатор переменной в БД архива
//MVARD() { varIdx = -1; nType = VRT_LONG; bProcChanges = true; nowTimeU64 = 0; sourceTimeU64 = 0;
//          prevNowTimeU64 = 0; prevSourceTimeU64 = 0; dblPrevVal = std::numeric_limits<double>::quiet_NaN();
//          dblMin = std::numeric_limits<double>::max(); dblMax = std::numeric_limits<double>::min(); iArchId = 0; }
MVARD() : nType( VRT_LONG ), varIdx( -1 ), bProcChanges( true ), nowTimeU64( 0 ), sourceTimeU64( 0 ),
		  dblPrevVal( std::numeric_limits<double>::quiet_NaN() ), prevNowTimeU64( 0 ), prevSourceTimeU64( 0 ),
		  dblMin( std::numeric_limits<double>::max() ), dblMax( std::numeric_limits<double>::min() ), iArchId( 0 ) {}
};

/////////////////////////////////////////////////////////////////////////////
// struct MVARD
/////////////////////////////////////////////////////////////////////////////

struct MUSTAV
{
quint32 uUstVal; // Значение уставки, при котором генерируется сообщение
                 // ( Если uUstVal == std::numeric_limits<quint32>::max() - 1, то при любом значении )
QString strText; // Текст сообщения
int nUstType; // Тип сообщения (информационное, аварийное и т.д.)
bool operator==( quint32 uVal ) const { return ( uUstVal == uVal ); }
};

/////////////////////////////////////////////////////////////////////////////
// struct MVARREC
/////////////////////////////////////////////////////////////////////////////

struct MVARREC
{
MVARD mvard;
QList<MVARD> argvars;
QList<MUSTAV> ustlist;
};

/////////////////////////////////////////////////////////////////////////////
// struct VDATA
/////////////////////////////////////////////////////////////////////////////

struct VDATA
{
int index;
//QVariant curVal; // Текущее значение
char valBuf[ nMaxVarSz ];
bool bDefined;
qint64 nowTimeU64; // Момент прихода изменившейся переменной
qint64 sourceTimeU64; // Момент изменения переменной в источнике
};

const quint32 ucFreeUst = std::numeric_limits<quint32>::max() - 1;

QVariant convVar( int nType, const char* valBuf, bool bDefined );

/////////////////////////////////////////////////////////////////////////////
// class QaGetVarThread
/////////////////////////////////////////////////////////////////////////////

class QaGetVarThread : public QThread
{
    Q_OBJECT

protected:
    bool m_bActThread;

public:
    QaGetVarThread( QObject* parent = NULL );
    virtual ~QaGetVarThread();

public:
    void stopThread() { m_bActThread = false; }

public:
    virtual void run();

signals:
    void inpVarChanged( const VDATA & vdata );
};

/////////////////////////////////////////////////////////////////////////////
// class MLibAccess
/////////////////////////////////////////////////////////////////////////////

class MLibAccess
{
public:
    int m_iTask;
    QaGetVarThread* m_pGetVarThread;

public:
    MLibAccess();
    virtual ~MLibAccess();

public:
    void init();
    bool connectTOO( QString & strErr );
    void disconnectTOO();
    bool attachToTOO( const QString & strTaskName, QString & strErr );
    void startGetVarThread();
    void stopGetVarThread();
    bool varIndexesFromTOO( QString & strErr );
    QVariant curVal( const MVARREC* pv, int index ) const;
    static QStringList getArchFileNames( const QString & strProjectName );
#ifdef Q_OS_UNIX
    bool waitSynchroStart( QString & strErr );
#endif
};

bool hasOneOf( const QString & str, const char* s, QChar & illChar );



#endif // MAIN_H
