#include "dataoperator.h"
#include "include/libbcrypt/include/bcrypt/BCrypt.hpp"
#include <QSqlError>
#include <QDir>
#include <QStandardPaths>
DataOperator::DataOperator() {
    db = QSqlDatabase::addDatabase("QSQLITE");
    //找一个可以存储数据库的位置
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    qDebug("Database path: %s",dir.path().toStdString().c_str());
    if (!dir.exists()) dir.mkpath(".");  // 创建目录
    db.setDatabaseName(dir.filePath("database.db"));
    initDatabase();
}

bool DataOperator::initDatabase() {
    return db.open();
}

enum class DataOperator::queryMode {createUser,updateUserInfo,
        setUserPassword,userExistCheck,
        getUserInfo,updateBalance,updateLimits
    };
//生成各类sql查询参数
QSqlQuery DataOperator::getNewQuery(const userInfo& user, const queryMode mode, const QString& password,const long long &balance) {
    if (!db.isOpen()) initDatabase();
    auto query = QSqlQuery(db);
    switch (mode) {
        case queryMode::createUser:
            query.prepare("insert into users (account, name, id, password_crypted, balance, error_count, max_single_transfer_amount, max_daily_transfer_amount, daily_transfer_total, last_transfer_date) "
                  "values (:account, :name, :id, :password_crypted, :balance, :error_count, :max_single_transfer_amount, :max_daily_transfer_amount, :daily_transfer_total, :last_transfer_date)");
            query.bindValue(":account", user.account);
            query.bindValue(":name", user.name);
            query.bindValue(":id", user.id);
            query.bindValue(":password_crypted", QString::fromStdString(BCrypt::generateHash(password.toStdString())));
            query.bindValue(":balance", user.balance);
            query.bindValue(":error_count", user.errorCount);
            query.bindValue(":max_single_transfer_amount", user.maxSingleTransferAmount);
            query.bindValue(":max_daily_transfer_amount",user.maxDailyTransferAmount);
            query.bindValue(":daily_transfer_total", user.dailyTransferTotal);
            query.bindValue(":last_transfer_date", QDate::currentDate());
            break;
        case queryMode::updateUserInfo:
            query.prepare("update users set name = :name, id = :id, balance = :balance, error_count = :error_count,"
                          " max_single_transfer_amount = :max_single_transfer_amount,"
                          " max_daily_transfer_amount = :max_daily_transfer_amount,"
                          //更新不应该清零
                          " daily_transfer_total = COALESCE(:daily_transfer_total, daily_transfer_total)"
                          " where account = :account;");
            query.bindValue(":account", user.account);
            query.bindValue(":balance", user.balance);
            query.bindValue(":name", user.name);
            query.bindValue(":id", user.id);
            query.bindValue(":error_count", user.errorCount);
            query.bindValue(":max_single_transfer_amount", user.maxSingleTransferAmount);
            query.bindValue(":max_daily_transfer_amount",user.maxDailyTransferAmount);
            query.bindValue(":daily_transfer_total", (user.dailyTransferTotal == 0 ? QVariant() : user.dailyTransferTotal));
            break;
        case queryMode::setUserPassword:
            query.prepare("update users set password_crypted = :password_crypted, error_count = :error_count where account = :account and name = :name and id = :id;");
            query.bindValue(":account", user.account);
            query.bindValue(":password_crypted", QString::fromStdString(BCrypt::generateHash(password.toStdString())));
            query.bindValue(":name", user.name);
            query.bindValue(":id", user.id);
            query.bindValue(":error_count", user.errorCount);
            break;
        case queryMode::getUserInfo: case queryMode::userExistCheck:
            query.prepare("select * from users where account = :account;");
            query.bindValue(":account", user.account);
            break;
        case queryMode::updateBalance:
            query.prepare("update users set balance = balance + :balance where account = :account");
            query.bindValue(":account", user.account);
            query.bindValue(":balance", balance);
            break;
        case queryMode::updateLimits:
            query.prepare("update users set daily_transfer_total = :daily_transfer_total, last_transfer_date = :last_transfer_date where account = :account;");
            query.bindValue(":account", user.account);
            query.bindValue(":daily_transfer_total", user.dailyTransferTotal);
            //只要有过更新就表明是今天更新的，就算今天没有转账，也更新为今天的日期（总额清零，不影响）
            query.bindValue(":last_transfer_date", QDate::currentDate());
    }
    return query;
}
//此处用于生成一个唯一的Instance，在其他任何地方都可以调用
DataOperator& DataOperator::getInstance(){
    static DataOperator instance;
    return instance;
}
DataOperator::~DataOperator() {
    db.close();
}
//检查表信息是否存在，不存在就创建新表
bool DataOperator::initTables() {
    if (!db.isOpen()) initDatabase();
    auto query = QSqlQuery(db);
    query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='users';");
    if (!query.next()) {
        // 表不存在，直接创建
        return query.exec(
            "CREATE TABLE IF NOT EXISTS users ("
            "    account CHAR(19) PRIMARY KEY CHECK("
            "        length(account) = 19 AND "
            "        account GLOB '[0-9]*'"
            "    ),"
            "    name VARCHAR(20) CHECK(length(name) <= 20),"
            "    id CHAR(18) CHECK(length(id) = 18),"
            "    password_crypted CHAR(60) NOT NULL CHECK(length(password_crypted) = 60),"
            "    balance BIGINT DEFAULT 0 CHECK(balance >= 0),"
            "    error_count INTEGER DEFAULT 0 CHECK(error_count >= 0 AND error_count <= 3),"
            "    max_single_transfer_amount BIGINT DEFAULT 1000000 CHECK(max_single_transfer_amount >= 0),"
            "    max_daily_transfer_amount BIGINT DEFAULT 5000000 CHECK(max_daily_transfer_amount >= 0),"
            "    daily_transfer_total BIGINT DEFAULT 0 CHECK(daily_transfer_total >= 0),"
            "    last_transfer_date DATE"
            ");"
        );
    }
    return true;
}
//判断该用户是否存在
bool DataOperator::userExistCheck(const QString &account) {
    userInfo user;
    user.account = account;
    auto query = getNewQuery(user, queryMode::userExistCheck);
    query.exec();
    return query.next();
}
//判断用户是否被锁定
bool DataOperator::userLockCheck(const QString &account) {
    auto user = queryUserInfo(account);
    if (user.errors != nullptr) return true; //查询出问题,视为锁定
    return user.errorCount >= 3;
}
//获取用户信息
DataOperator::userInfo DataOperator::queryUserInfo(const QString &account) {
    userInfo user;
    //如果用户不存在
    if (!userExistCheck(account)) {
        user.errors = "用户不存在";
        return user;
    }
    user.account = account;
    auto query = getNewQuery(user, queryMode::getUserInfo);
    //如果没成功
    if (!query.exec()) {
        user.errors = "数据库查询失败：" + query.lastError().text();
        return user;
    }
    //查询不到数据
    if (!query.next()) {
        user.errors = "数据库无法查询该用户记录：" + query.lastError().text();
        return user;
    }
    //查询到了，处理最大转账日期
    QDate lastTransferDate = query.value("last_transfer_date").toDate();
    //不是同一天，重置日转账总额
    if (lastTransferDate != QDate::currentDate()) {
        user.errors = resetDailyTransferTotal(account); //触发更新
        if (user.errors != nullptr) return user;
    }
    user = {
        .account = account,
        .name = query.value("name").toString(),
        .id = query.value("id").toString(),
        .balance = query.value("balance").toULongLong(),
        .errorCount = query.value("error_count").toUInt(),
        .errors = nullptr,
        .maxSingleTransferAmount = query.value("max_single_transfer_amount").toULongLong(),
        .maxDailyTransferAmount = query.value("max_daily_transfer_amount").toULongLong(),
        .dailyTransferTotal = query.value("daily_transfer_total").toULongLong(),
    };
    return user;
}
//创建用户
QString DataOperator::createUser(const userInfo &user, const QString &password) {
    if (userExistCheck(user.account)) {
        //用户存在，返回错误
        return "用户已存在";
    }
    auto query = getNewQuery(user, queryMode::createUser, password);
    return query.exec() ? nullptr : "数据库存储失败：" + query.lastError().text();
}
//更新用户信息
QString DataOperator::updateUserInfo(const userInfo &user) {
    if (!userExistCheck(user.account)) {
        //用户不存在，返回错误
        return "用户已存在";
    }
    auto query = getNewQuery(user, queryMode::updateUserInfo);
    return query.exec() ? nullptr : "数据库存储失败：" + query.lastError().text();
}
//设置用户密码
QString DataOperator::setUserPassword(userInfo user, const QString &newPassword) {
    if (!userExistCheck(user.account)) return "用户不存在";
    user.errorCount = 0; //重置错误计数
    auto query = getNewQuery(user, queryMode::setUserPassword, newPassword);
    return query.exec() ? nullptr : "数据库存储失败，可能信息不正确？：" + query.lastError().text();
}
//验证用户密码
QString DataOperator::validateUserPassword(const QString &account,const QString &password) {
    if (!userExistCheck(account)) return "用户不存在";
    if (userLockCheck(account)) return "账户已锁定";
    userInfo user = queryUserInfo(account);
    if (user.errors != nullptr) return user.errors;
    auto query = getNewQuery(user, queryMode::getUserInfo);
    if (!query.exec()) return "数据库查询失败：" + query.lastError().text();
    if (!query.next()) return "数据库无法查询该用户记录：" + query.lastError().text();
    //验证密码
    if (BCrypt::validatePassword(password.toStdString(), query.value("password_crypted").toString().toStdString())) {
        //验证成功
        user.errorCount = 0;
        updateUserInfo(user);
        return nullptr;
    }
    //验证失败
    user.errorCount++;
    updateUserInfo(user);
    return "密码错误";
}
//获取用户登录的信息
DataOperator::userInfo DataOperator::userLogin(const QString &account, const QString &password) {
    //验证密码
    const QString passwordValidationError = validateUserPassword(account, password);
    if (passwordValidationError != nullptr) {
        userInfo user;
        user.errors = passwordValidationError;
        return user;
    }
    //获取用户信息
    return queryUserInfo(account);
}
DataOperator::userInfo DataOperator::getUserName(const QString &account) {
    if (!userExistCheck(account)) {}
    auto user = queryUserInfo(account);
    return userInfo{
    .account = account,
    //部分隐藏姓名，长度小于2的隐藏左侧字符，长度大于2的隐藏中间字符
    .name = user.name.length() <= 2 ? QString(user.name.length()-1,'*') + user.name.right(1)
        : user.name.left(1)+ QString(user.name.length()-2,'*') + user.name.right(1),
    .id = nullptr,
    .balance = 0,
    .errorCount = 0,
    .errors = user.errors};
}
QString DataOperator::updateBalance(const QString &account,const long long amount)
{
    if (!userExistCheck(account)) {
        //用户不存在，返回错误
        return "用户不存在";
    }
    userInfo user;
    user.account = account;
    auto query = getNewQuery(user, queryMode::updateBalance,nullptr,amount);
    return query.exec() ? nullptr : "数据库存储失败：" + query.lastError().text();
}
//清零用户今日转账总额
QString DataOperator::resetDailyTransferTotal(const QString &account) {
    if (!userExistCheck(account)) {
        //用户不存在，返回错误
        return "用户不存在";
    }
    userInfo user;
    user.account = account;
    user.dailyTransferTotal = 0;
    auto query = getNewQuery(user, queryMode::updateLimits);
    return query.exec() ? nullptr : "数据库存储失败：" + query.lastError().text();
}
//更新用户转账限额情况，但是在原有基础上加减
QString DataOperator::updateDailyTransferTotal(const QString &account, const unsigned long long &amount,bool isAdd) {
    if (!userExistCheck(account)) {
        //用户不存在，返回错误
        return "用户不存在";
    }
    auto user = queryUserInfo(account);
    if (user.errors != nullptr) return user.errors;
    if (isAdd) user.dailyTransferTotal += amount;
    else {
        if (user.dailyTransferTotal < amount) user.dailyTransferTotal = 0;
        else user.dailyTransferTotal -= amount;
    }
    auto query = getNewQuery(user, queryMode::updateLimits);
    return query.exec() ? nullptr : "数据库存储失败：" + query.lastError().text();
}

//将带小数点的balance字符串，乘以100转为小数
unsigned long long DataOperator::balanceStringToNumber(const QString &balanceString) {
    if (balanceString.isEmpty()) return 0;
    const int dotIndex = balanceString.indexOf('.');
    if (dotIndex == -1) return balanceString.toULongLong() * 100; // 纯整数
    // 带小数,小数部分必须是1或2位
    QString integerPart = balanceString.left(dotIndex);
    QString decimalPart = balanceString.mid(dotIndex + 1);
    // 将小数部分补全到2位
    if (decimalPart.length() == 1) decimalPart += '0';
    return integerPart.toULongLong() * 100 + decimalPart.toInt();
}
//将小数转为带小数点的字符串
QString DataOperator::balanceNumberToString(const unsigned long long &balanceNumber) {
    auto integerPart = QString::number(balanceNumber / 100);
    auto decimalPart = QString::number(balanceNumber % 100);
    auto length = decimalPart.length();
    //有小数情况
    if (length == 1) return integerPart + ".0" + decimalPart;
    if (length == 2) return integerPart + "." + decimalPart;
    //无小数情况
    return integerPart;
}