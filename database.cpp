#include "main.h"
#include "database.h"

#include <QFileInfo>
#include <QSettings>

extern QVector<MVARREC> g_varsList;

QSqlDatabase db;

const char* DB_SERVICE = "template1";  // Имя шаблона БД, на основании которого создается БД архива

const char* DB_DRIVER = "QPSQL";       // Имя драйвера

const char* VALUES_TABLE = "vals";               // Таблица значений
const char* DESCRIPTIONS_TABLE = "descriptions"; // Таблица описаний
const char* MESSAGES_TABLE = "messages";         // Таблица сообщений
const char* GROUPS_TABLE = "groups";

const char* ID_COLUMN = "id";                   // Колонка первичного ключа (одинакова для всех таблиц)
const char* VARID_COLUMN = "varid";             // Колонка идентификатора переменной в таблице значений
const char* NAME_COLUMN = "name";               // Имя переменной
const char* DESCRIPTION_COLUMN = "description"; // Описание переменной
const char* UNITS_COLUMN = "units";             // Единицы измерения
const char* GROUP_COLUMN = "ingroup";           // Группа переменной
const char* DATETIME_COLUMN = "time";           // Время изменения
const char* PRECISION_COLUMN = "precision";     // Точность пердставления
const char* VALUE_COLUMN = "val";               // Значение
const char* MESSAGE_COLUMN = "message";         // Текст сообщения

const char*  DEFAULT_DATETIME_FORMAT = "dd-MM-yyyy hh:mm:ss";

bool open_database( const QString & db_name, const QString & strUserName,
                                             const QString & strPassword, QString & strErr );
QSqlQuery prepare_update_descriptions( bool & bOk );
QSqlQuery prepare_insert_into_descriptions( bool & bOk );
QSqlQuery prepare_insert_into_groups( bool & bOk );
QSqlQuery prepare_insert_into_values( bool & bOk );
QSqlQuery prepare_insert_into_messages( bool & bOk );
bool updateRecord( QSqlQuery & updateQuery,
                   const QString & units, int precision, int group,
                   const QString & description, int id );
bool insertRecord( QSqlQuery & insertQuery, const QString & name,
                   const QString & units, int precision, int group,
                   const QString & description, int & id );
bool insertIntoGroups( QSqlQuery & insertQuery, const QString & name, int & id );

QSqlQuery insert_value_change;
QSqlQuery insert_message;

//--------------------------------------------------------
// Cоединение с БД, либо создание, если БД не существует
bool connect_database( const QString & db_name, const QString & strUserName,
                       const QString & strPassword, bool bCreate, QString & strErr )
{
strErr.clear();
QString strDbName = db_name.toLower();

// Получим имя ini-файла
QString strSettingsPath = QCoreApplication::applicationFilePath();
QFileInfo fi( strSettingsPath );
QString ext = fi.suffix();
strSettingsPath = strSettingsPath.left( strSettingsPath.size() - ext.size() );
if ( !strSettingsPath.endsWith( QChar( '.' ) ) ) strSettingsPath += QChar( '.' );
strSettingsPath += QString( "ini" );

// Из ini-файла считаем имя и пароль для подключения к системной базе данных (шаблону БД)
const char* szKeyUserName = "USERNAME";
const char* szKeyPassword = "PASSWORD";
QSettings settings( strSettingsPath, QSettings::IniFormat );
settings.setIniCodec( "UTF-8" );

bool bNeedSync = false;

QString strIniUserName = settings.value( QString( szKeyUserName ) ).toString().trimmed();
if ( strIniUserName.isEmpty() )
  {
  strIniUserName = QStringLiteral( "postgres" );
  settings.setValue( QString( szKeyUserName ), QVariant( strIniUserName ) );
  bNeedSync = true;
  }

QString strIniPassword = settings.value( QString( szKeyPassword ) ).toString().trimmed();
if ( strIniPassword.isEmpty() )
  {
  strIniPassword = QStringLiteral( "postgres" );
  settings.setValue( QString( szKeyPassword ), QVariant( strIniPassword ) );
  bNeedSync = true;
  }

if ( bNeedSync ) settings.sync();

QSqlDatabase dbTemplate = QSqlDatabase::addDatabase( DB_DRIVER, DB_SERVICE );
dbTemplate.setUserName( strIniUserName );
dbTemplate.setPassword( strIniPassword );
if ( !dbTemplate.open() )
  {
  strErr = QObject::tr( "Ошибка открытия системной БД: %1" ).arg( dbTemplate.lastError().text() );
  return false;
  }

QSqlQuery query( dbTemplate );

// Проверить существование записи пользователя
if ( !query.exec( QString( "SELECT 1 FROM pg_roles WHERE rolname = '%1'" ).arg( strUserName ) ) )
  {
  strErr = QObject::tr( "Ошибка чтения системной БД: %1" ).arg( dbTemplate.lastError().text() );
  dbTemplate.close();
  return false;
  }

if ( query.size() < 1 ) // Если запись пользователя не найдена, создать её
  {
  if ( !bCreate )
    {
    strErr = QObject::tr( "Пользователь с именем '%1' не найден" ).arg( strUserName );
    dbTemplate.close();
    return false;
    }

  if ( !query.exec( QString( "CREATE USER %1 WITH PASSWORD '%2'" ).arg( strUserName ).arg( strPassword ) ) )
    {
    strErr = QObject::tr( "Ошибка создания записи пользователя '%1': %2" )
                                           .arg( strUserName ).arg( query.lastError().text() );
    dbTemplate.close();
    return false;
    }
  }

// Проверить существование БД архива
if ( !query.exec( QString( "SELECT 1 FROM pg_database WHERE datname = '%1'" ).arg( strDbName ) ) )
  {
  strErr = QObject::tr( "Ошибка чтения системной БД: %1" ).arg( dbTemplate.lastError().text() );
  dbTemplate.close();
  return false;
  }

if ( query.size() < 1 ) // Если БД с заданным именем не найдена, создать её и предоставить пользователю доступ к ней
  {
  if ( !bCreate )
    {
    strErr = QObject::tr( "БД архива '%1' не найдена" ).arg( strDbName );
    dbTemplate.close();
    return false;
    }

  if ( !query.exec( QString( "CREATE DATABASE %1 WITH OWNER = DEFAULT ENCODING = 'UTF8' TABLESPACE = pg_default "
                             "CONNECTION LIMIT = -1;" ).arg( strDbName ) ) )
    {
    strErr = QObject::tr( "Ошибка создания БД '%1': %2" ).arg( strDbName ).arg( query.lastError().text() );
    dbTemplate.close();
    return false;
    }

  if ( !query.exec( QString( "GRANT ALL PRIVILEGES ON DATABASE %1 TO %2" ).arg( strDbName ).arg( strUserName ) ) )
    {
    strErr = QObject::tr( "Ошибка предоставления привилегий доступа к БД '%1' пользователю '%2': %3" )
                                  .arg( strDbName ).arg( strUserName ).arg( query.lastError().text() );
    dbTemplate.close();
    return false;
    }
  }

dbTemplate.close();

if ( !open_database( db_name, strUserName, strPassword, strErr ) ) return false;

// Структура таблицы groups: id, имя,
const QString GROUPS_STRUCTURE = QStringLiteral( "(" ) + QString( ID_COLUMN ) +
                                 QStringLiteral( " serial primary key, " ) +
                                 QString( NAME_COLUMN ) + QStringLiteral( " varchar(256)) " );

if ( !create_table( GROUPS_TABLE, GROUPS_STRUCTURE, strErr ) )
  {
  db.close();
  return false;
  }



// Структура таблицы vals (изменения значений переменных): id, id_переменной,
// дата_и_время, значение, предыдущее значение
const QString VALUES_STRUCTURE = QStringLiteral( "(" ) + QString( ID_COLUMN ) +
                                 QStringLiteral( " serial primary key, " ) +
                                 QString( VARID_COLUMN ) + QStringLiteral( " int , " ) +
                                 QString( DATETIME_COLUMN ) +
                                 QStringLiteral( " timestamp, " ) + QString( VALUE_COLUMN ) +
                                 QStringLiteral( " float)" );

if ( !create_table( VALUES_TABLE, VALUES_STRUCTURE, strErr ) )
  {
  db.close();
  return false;
  }

// Cтруктура таблицы messages (сообщения): id, текст_сообщения, дата_и_время
const QString MESSAGES_STRUCTURE =
      QStringLiteral( "(" ) + QString( ID_COLUMN ) + QStringLiteral( " serial primary key, " ) +
      QString( VARID_COLUMN ) + QStringLiteral( " int , " ) +
      QString( MESSAGE_COLUMN ) + QStringLiteral( " varchar(256), " ) +
      QString( DATETIME_COLUMN ) + QStringLiteral( " timestamp)" );
if ( !create_table( MESSAGES_TABLE, MESSAGES_STRUCTURE, strErr ) )
  {
  db.close();
  return false;
  }

// Структура таблицы descriptions (описание переменных): id, имя_переменной,
// ед.изменеия, точность представления, группа
const QString DESCRIPTIONS_STRUCTURE = QStringLiteral( "(" ) + QString( ID_COLUMN ) + QStringLiteral( " serial primary key, " ) +
                                                               QString( NAME_COLUMN ) + QStringLiteral( " varchar(20) UNIQUE, " ) +
                                                               QString( UNITS_COLUMN ) + QStringLiteral( " varchar(20), " ) +
                                                               QString( PRECISION_COLUMN ) + QStringLiteral( " smallint, " ) +
                                                               QString( GROUP_COLUMN ) + QStringLiteral( " int, " ) +
                                                               QString( DESCRIPTION_COLUMN ) + QStringLiteral( " varchar(100))" );
if ( !create_table( DESCRIPTIONS_TABLE, DESCRIPTIONS_STRUCTURE, strErr ) )
  {
  db.close();
  return false;
  }

return true; // Соединение удалось
}

//--------------------------------------------------------
void closeDb()
{
if ( db.isOpen() )
  {
  insert_value_change.clear();
  insert_message.clear();
  db.close();
  }

return;
}

//--------------------------------------------------------
// внутренняя функция для установки соединения с БД
bool open_database( const QString & db_name, const QString & strUserName,
                                             const QString & strPassword, QString & strErr )
{
strErr.clear();

// Создание и инициализация QSqlDatabase
db = QSqlDatabase::addDatabase( DB_DRIVER, db_name );
db.setDatabaseName( db_name );
db.setUserName( strUserName );
db.setPassword( strPassword );
if ( !db.open() )
  { // Если соединение не удалось
  strErr = QObject::tr( "Ошибка открытия БД '%1': %2" ).arg( db_name ).arg( db.lastError().text() );
  return false;
  }

return true;
}

//--------------------------------------------------------
// Создание таблицы с заданной структурой столбцов
bool create_table( const QString & table, const QString & structure, QString & strErr )
{
strErr.clear();
QSqlQuery query( db );
if ( !query.exec( QStringLiteral( "CREATE TABLE IF NOT EXISTS " ) + table +
                                                   QStringLiteral( " " ) + structure ) )
  { // Если запрос не был выполнен
  strErr = QObject::tr( "Ошибка создания таблицы '%1': %2" ).arg( table, query.lastError().text() );
  return false;
  }
else return true;
}

//--------------------------------------------------------
QSqlQuery prepare_update_descriptions( bool & bOk )
{
QSqlQuery q( db );
bOk = q.prepare( QStringLiteral( "UPDATE " ) + QString( DESCRIPTIONS_TABLE ) +
                 QStringLiteral( " SET " ) + QString( UNITS_COLUMN ) +
                 QStringLiteral( "=:units," ) +
                 QString( PRECISION_COLUMN ) + QStringLiteral( "=:presision," ) +
                 QString( GROUP_COLUMN ) + QStringLiteral( "=:group," ) +
                 QString( DESCRIPTION_COLUMN ) + QStringLiteral( "=:description" ) +
                 QStringLiteral( " WHERE " ) + QString( ID_COLUMN ) + QStringLiteral( " = " ) +
                 QStringLiteral( ":Id" ) );

return q;
}

//--------------------------------------------------------
QSqlQuery prepare_insert_into_descriptions( bool & bOk )
{
QSqlQuery q( db );
bOk = q.prepare( QStringLiteral( "INSERT INTO " ) + QString( DESCRIPTIONS_TABLE ) + QStringLiteral( "(" ) +
                 QString( NAME_COLUMN ) + QStringLiteral( "," ) + QString( UNITS_COLUMN ) +
                 QStringLiteral( "," ) + QString( PRECISION_COLUMN ) +
                 QStringLiteral( "," ) + QString( GROUP_COLUMN ) + QStringLiteral( "," ) +
                 QString( DESCRIPTION_COLUMN ) +
                 QStringLiteral( ") VALUES (:name, :units, :precision, :group, :description)" ) +
                 QStringLiteral( " RETURNING " ) + QString( ID_COLUMN ) );

return q;
}


//--------------------------------------------------------
QSqlQuery prepare_insert_into_groups( bool & bOk )
{
QSqlQuery q( db );
bOk = q.prepare( QStringLiteral( "INSERT INTO " ) + QString( GROUPS_TABLE ) + QStringLiteral( "(" ) +
                 QString( NAME_COLUMN ) + QStringLiteral( ") VALUES (:name)" ) +
                 QStringLiteral( " RETURNING " ) + QString( ID_COLUMN ) );

return q;
}

//--------------------------------------------------------
QSqlQuery prepare_insert_into_values( bool & bOk )
{
QSqlQuery q( db );
bOk = q.prepare( QStringLiteral( "INSERT INTO " ) + QString( VALUES_TABLE ) + QStringLiteral( "(" ) +
                 QString( VARID_COLUMN ) + QStringLiteral( "," ) +
                 QString( DATETIME_COLUMN ) + QStringLiteral( "," ) + QString( VALUE_COLUMN ) +
                 QStringLiteral( ") VALUES (:varid, :dt, :val)" ) );
return q;
}

//--------------------------------------------------------
QSqlQuery prepare_insert_into_messages( bool & bOk )
{
QSqlQuery q( db );
bOk = q.prepare( QStringLiteral( "INSERT INTO " ) + QString( MESSAGES_TABLE ) + QStringLiteral( "(" ) +
                 QString( VARID_COLUMN ) + QStringLiteral( "," ) +
                 QString( MESSAGE_COLUMN ) + QStringLiteral( "," ) +
                 QString( DATETIME_COLUMN ) + QStringLiteral( ") VALUES (:varid, :msg, :dt)" ) );
return q;
}

//--------------------------------------------------------
bool updateRecord( QSqlQuery & updateQuery,
                   const QString & units, int precision, int group,
                   const QString & description, int id )
{
updateQuery.bindValue( ":units", QVariant( units ) );
updateQuery.bindValue( ":precision", QVariant( precision ) );
updateQuery.bindValue( ":group", QVariant( group ) );
updateQuery.bindValue( ":description", QVariant( description ) );
updateQuery.bindValue( ":Id", QVariant( id ) );

return updateQuery.exec();
}


//--------------------------------------------------------
bool insertRecord( QSqlQuery & insertQuery, const QString & name,
                   const QString & units, int precision, int group,
                   const QString & description, int & id )
{
insertQuery.bindValue( ":name", QVariant( name ) );
insertQuery.bindValue( ":units", QVariant( units ) );
insertQuery.bindValue( ":precision", QVariant( precision ) );
insertQuery.bindValue( ":group", QVariant( group ) );
insertQuery.bindValue( ":description", QVariant( description ) );

id = 0;
bool bRet = insertQuery.exec();
if ( bRet )
  {
  bRet = insertQuery.next();
  if ( bRet ) id = insertQuery.value( 0 ).toInt();
  }

return bRet;
}

//--------------------------------------------------------
bool insertIntoGroups( QSqlQuery & insertQuery, const QString & name, int & id )
{
insertQuery.bindValue( ":name", QVariant( name ) );
id = 0;
bool bRet = insertQuery.exec();
if ( bRet )
  {
  bRet = insertQuery.next();
  if ( bRet ) id = insertQuery.value( 0 ).toInt();
  }

return bRet;
}


//--------------------------------------------------------
void add_message_to_db(int idx, const QString & message, qint64 time )
{
//qDebug() << "MESSAGE:  " << message;
insert_message.bindValue( ":varid", QVariant( idx ) );
insert_message.bindValue( ":msg", QVariant( message ) );
insert_message.bindValue( ":dt", QVariant( QDateTime::fromMSecsSinceEpoch( time ).toString(DEFAULT_DATETIME_FORMAT) ) );
insert_message.exec();

return;
}

//--------------------------------------------------------
void add_value_change_to_db( int id, double value, qint64 time )
{
//qDebug() << "VALUE:  " << id << value;
insert_value_change.bindValue( ":varid", QVariant( id ) );
insert_value_change.bindValue( ":dt", QVariant( QDateTime::fromMSecsSinceEpoch( time ).toString(DEFAULT_DATETIME_FORMAT) ) );
insert_value_change.bindValue( ":val", QVariant( value ) );
insert_value_change.exec();

return;
}

//--------------------------------------------------------
bool populateGroupsTable( QString & strErr )
{
QSqlQuery select_query( db );
if ( !select_query.exec( QStringLiteral( "SELECT " ) + QString( ID_COLUMN ) +
                  QStringLiteral( ", " ) + QString( NAME_COLUMN ) +
                  QStringLiteral( " FROM " ) + QString( GROUPS_TABLE ) ) )
  {
  strErr = QObject::tr( "Ошибка чтения таблицы групп: %1" )
                                               .arg( select_query.lastError().text() );
  return false;
  }

QMap<QString, int> idxMap;
while ( select_query.next() )
  {
  idxMap[ select_query.value( 1 ).toString().toLower() ] = select_query.value( 0 ).toInt();
  }

bool bOk1;
QSqlQuery insert_query = prepare_insert_into_groups( bOk1 );
if ( !bOk1 )
  {
  strErr = QObject::tr( "Ошибка подготовки запроса вставки записи: %1" )
                                               .arg( insert_query.lastError().text() );
  return false;
  }

for ( int iVar = 0; iVar < g_varsList.size(); ++iVar )
  {
  MVARREC & varRef = g_varsList[ iVar ];
  if (varRef.mvard.strGroup.isEmpty()) varRef.mvard.strGroup = "default";
  QString strGroupLowerCase = varRef.mvard.strGroup.toLower();
  if ( idxMap.contains( strGroupLowerCase ) )
    {
    varRef.mvard.iGroupIdx = idxMap.value( strGroupLowerCase );
    }
  else
    {
    int iGroupIdx=0;
    if ( !insertIntoGroups(insert_query, varRef.mvard.strGroup, iGroupIdx ) )
      {
      strErr = QObject::tr( "Ошибка добавления записи: %1" ).arg( insert_query.lastError().text() );
      return false;
      }
    varRef.mvard.iArchId = iGroupIdx;
    idxMap[ strGroupLowerCase ] = iGroupIdx;
    }

  // Переменные - параметры сообщений
  for ( int iVarEx = 0; iVarEx < varRef.argvars.size(); ++iVarEx )
    {
    MVARD & varRefEx = varRef.argvars[ iVarEx ];

    if (varRefEx.strGroup.isEmpty()) varRefEx.strGroup = "default";
    QString strGroupExLowerCase = varRefEx.strGroup.toLower();

    if ( !idxMap.contains( strGroupExLowerCase ) )
      {
      int iGroupIdx=0;
      if ( !insertIntoGroups(insert_query, varRefEx.strGroup, iGroupIdx ) )
        {
        strErr = QObject::tr( "Ошибка добавления записи: %1" ).arg( insert_query.lastError().text() );
        return false;
        }
      varRefEx.iGroupIdx = iGroupIdx;
      idxMap[ strGroupExLowerCase ] = iGroupIdx;
      }
    }
  }

return true;
}

//--------------------------------------------------------
bool populateDescriptionTable( QString & strErr )
{
QSqlQuery select_query( db );
if ( !select_query.exec( QStringLiteral( "SELECT " ) + QString( ID_COLUMN ) +
                  QStringLiteral( ", " ) + QString( NAME_COLUMN ) +
                  QStringLiteral( " FROM " ) + QString( DESCRIPTIONS_TABLE ) ) )
  {
  strErr = QObject::tr( "Ошибка чтения таблицы описания переменных: %1" )
                                               .arg( select_query.lastError().text() );
  return false;
  }

QMap<QString, int> idxMap;
while ( select_query.next() )
  {
  idxMap[ select_query.value( 1 ).toString().toLower() ] = select_query.value( 0 ).toInt();
  }

bool bOk1;
QSqlQuery insert_query = prepare_insert_into_descriptions( bOk1 );
if ( !bOk1 )
  {
  strErr = QObject::tr( "Ошибка подготовки запроса вставки записи: %1" )
                                               .arg( insert_query.lastError().text() );
  return false;
  }

bool bOk2;
QSqlQuery update_query = prepare_update_descriptions( bOk2 );
if ( !bOk2 )
  {
  strErr = QObject::tr( "Ошибка подготовки запроса обновления записи: %1" )
                                               .arg( update_query.lastError().text() );
  return false;
  }

for ( int iVar = 0; iVar < g_varsList.size(); ++iVar )
  {
  MVARREC & varRef = g_varsList[ iVar ];
  QString strVarNameLowerCase = varRef.mvard.vname.toLower();
  if ( idxMap.contains( strVarNameLowerCase ) )
    {
    varRef.mvard.iArchId = idxMap.value( strVarNameLowerCase );
    if ( !updateRecord( update_query,
                        varRef.mvard.strUnits, varRef.mvard.byNDigits, varRef.mvard.iGroupIdx,
                        varRef.mvard.strDescription, varRef.mvard.iArchId ) )
      {
      strErr = QObject::tr( "Ошибка обновления записи: %1" ).arg( update_query.lastError().text() );
      return false;
      }
    }
  else
    {
    int iArchId;
    if ( !insertRecord( insert_query, varRef.mvard.vname,
                        varRef.mvard.strUnits, varRef.mvard.byNDigits, varRef.mvard.iGroupIdx,
                        varRef.mvard.strDescription, iArchId ) )
      {
      strErr = QObject::tr( "Ошибка добавления записи: %1" ).arg( insert_query.lastError().text() );
      return false;
      }

    varRef.mvard.iArchId = iArchId;
    idxMap[ strVarNameLowerCase ] = iArchId;
    }

  // Переменные - параметры сообщений
  for ( int iVarEx = 0; iVarEx < varRef.argvars.size(); ++iVarEx )
    {
    MVARD & varRefEx = varRef.argvars[ iVarEx ];

    QString strVarNameExLowerCase = varRefEx.vname.toLower();
//    if ( idxMap.contains( strVarNameExLowerCase ) )
//      {
//      varRefEx.iArchId = idxMap.value( strVarNameExLowerCase );
//      if ( !updateRecord( update_query,
//                          varRefEx.strUnits, varRefEx.byNDigits, QString(),
//                          varRefEx.strDescription, varRefEx.iArchId ) )
//        {
//        strErr = QObject::tr( "Ошибка обновления записи: %1" ).arg( update_query.lastError().text() );
//        return false;
//        }
//      }
//    else
    if ( !idxMap.contains( strVarNameExLowerCase ) )
      {
      int iArchId;
      if ( !insertRecord( insert_query, varRefEx.vname,
                          varRefEx.strUnits, varRefEx.byNDigits, varRefEx.iGroupIdx,
                          varRefEx.strDescription, iArchId ) )
        {
        strErr = QObject::tr( "Ошибка добавления записи: %1" ).arg( insert_query.lastError().text() );
        return false;
        }

      varRefEx.iArchId = iArchId;
      idxMap[ strVarNameExLowerCase ] = iArchId;
      }
    }
  }

return true;
}

//--------------------------------------------------------
bool prepareRunQueries( QString & strErr )
{
strErr.clear();
bool bOk;
insert_value_change = prepare_insert_into_values( bOk );
if ( !bOk )
  {
  strErr = QObject::tr( "Ошибка подготовки запроса вставки значений: %1" )
                                               .arg( insert_value_change.lastError().text() );
  return false;
  }

insert_message = prepare_insert_into_messages( bOk );
if ( !bOk )
  {
  strErr = QObject::tr( "Ошибка подготовки запроса вставки сообщений: %1" )
                                               .arg( insert_message.lastError().text() );
  return false;
  }

return true;
}
