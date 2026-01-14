#pragma once
#pragma comment(lib, "Qt6Sql.lib")
#include <qlineedit.h>
#include <QString>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQueryModel>
#include <QtSql/QSqlQuery>
class DataOperator
{
public:
    struct userInfo {
        QString account = nullptr;
        QString name = nullptr;
        QString id = nullptr;
        unsigned long long balance = 0;
        unsigned int errorCount = 0;
        QString errors = nullptr;
        unsigned long long maxSingleTransferAmount = 1000000;
        unsigned long long maxDailyTransferAmount = 5000000;
        unsigned long long dailyTransferTotal = 0;
    };
    explicit DataOperator();
    bool initDatabase();
    userInfo userLogin(const QString &account, const QString &password);
    userInfo getUserName(const QString &account);
    static DataOperator& getInstance();
    ~DataOperator();
    bool initTables();
    bool userExistCheck(const QString &account);
    bool userLockCheck(const QString &account);
    QString validateUserPassword(const QString &account, const QString &password);
    static unsigned long long balanceStringToNumber(const QString &balanceString);
    static QString balanceNumberToString(const unsigned long long &balanceNumber);
    QString createUser(const userInfo &user, const QString &password);
    QString updateUserInfo(const userInfo &user);
    QString updateBalance(const QString &account, long long amount);
    QString setUserPassword(userInfo user, const QString &newPassword);
    QString updateDailyTransferTotal(const QString &account, const unsigned long long &amount, bool isAdd);
private:
    QSqlDatabase db;
    enum class queryMode;
    QSqlQuery getNewQuery(const userInfo &user, queryMode mode, const QString &password = nullptr, const long long &balance = 0);
    userInfo queryUserInfo(const QString &account);
    QString resetDailyTransferTotal(const QString &account);
};
