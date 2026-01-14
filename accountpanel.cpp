#include "accountpanel.h"

#include <QMessageBox>

#include "ui_accountpanel.h"
#include <QString>
#include "userinfooperator.h"
#include "ui_inputdialog.h"
#include <QRegularExpressionValidator>
#include <utility>
#include <QTimer>

#include "validcheck.h"

AccountPanel::AccountPanel(DataOperator::userInfo  user, QString password,QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::AccountPanel)
    , user(std::move(user))
    , password(std::move(password))
{
    ui->setupUi(this);
    //设置用户信息
    displayUserInfo();
    //自动刷新用户信息
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &AccountPanel::refresh);
    timer->setInterval(20000);
    timer->start();
}
void AccountPanel::displayUserInfo() const {
    ui->welcome->setText(user.name+"，欢迎您使用TDM ATM");
    ui->account->setText(user.account);
    ui->name->setText(user.name);
    ui->maxDailyTransfer->setText(DataOperator::balanceNumberToString(user.maxDailyTransferAmount));
    ui->maxSingleTransfer->setText(DataOperator::balanceNumberToString(user.maxSingleTransferAmount));
    ui->todayTotalTransfer->setText(DataOperator::balanceNumberToString(user.dailyTransferTotal));
    ui->balance->setText(DataOperator::balanceNumberToString(user.balance));
    ui->id->setText(user.id.left(3) + QString(11, '*') + user.id.right(4));
}
void AccountPanel::refresh() {
    //todo：发现自动刷新有些卡顿，把data操作弄到子线程里会好一些
    auto instance = DataOperator::getInstance();
    user = instance.userLogin(user.account,password);
    if (user.errors != nullptr) {
        QMessageBox::warning(this, "错误", "用户信息获取失败："+user.errors);
        this->close();
    }
    if (instance.validateUserPassword(user.account,password) != nullptr) {
        QMessageBox::warning(this, "错误", "用户验证失败，请重新登录！");
        this->close();
    }
    displayUserInfo();
}
AccountPanel::~AccountPanel()
{
    ui->account->clear();
    ui->name->clear();
    ui->id->clear();
    ui->balance->clear();
    ui->welcome->clear();
    delete ui;
}

void AccountPanel::on_exit_clicked()
{
    this->destroy();
}


void AccountPanel::on_modifyInfo_clicked()
{
    UserInfoOperator o(this,UserInfoOperator::userMode::modifyUser,user);
    o.setWindowTitle("修改信息");
    o.exec();
    o.deleteLater();
    refresh();
}


void AccountPanel::on_setPassword_clicked()
{
    QDialog dialog;
    Ui::inputDialog inputUI;
    inputUI.setupUi(&dialog);
    dialog.setWindowTitle("修改密码");
    inputUI.notice->setText("请输入新密码：");
    inputUI.input->setEchoMode(QLineEdit::Password);
    inputUI.input->setValidator(new QRegularExpressionValidator(QRegularExpression(NUMBER_REGEXP)));
    inputUI.input->setMaxLength(6);
    if (dialog.exec() == QDialog::Rejected) return;
    QString newPassword = inputUI.input->text();
    if (newPassword.length() != 6) {
        QMessageBox::warning(this, "错误", "密码长度必须为6位！");
        return;
    }
    inputUI.notice->setText("请再次输入密码：");
    inputUI.input->clear();
    inputUI.input->setEchoMode(QLineEdit::Password);
    inputUI.input->setMaxLength(6);
    if (dialog.exec() == QDialog::Rejected) return;
    if (newPassword != inputUI.input->text()) {
        QMessageBox::warning(this, "错误", "密码不匹配！");
        return;
    }
    //修改密码操作
    auto instance = DataOperator::getInstance();
    const auto result = instance.setUserPassword(user,newPassword);
    if (result != nullptr) {
        QMessageBox::information(this, "成功", "密码修改失败:"+result);
    } else {
        QMessageBox::warning(this, "错误", "密码修改成功，请重新登录！");
        this->close();
    }
}


void AccountPanel::on_transferBalance_clicked()
{
    //获取转账账户
    QDialog dialog;
    Ui::inputDialog inputUI;
    inputUI.setupUi(&dialog);
    dialog.setWindowTitle("转账");
    inputUI.notice->setText("请输入对方账户：");
    inputUI.input->setValidator(new QRegularExpressionValidator(QRegularExpression(NUMBER_REGEXP)));
    inputUI.input->setMaxLength(19);
    if (dialog.exec() == QDialog::Rejected) return;
    auto objectAccount = inputUI.input->text();
    if (objectAccount.length() != 19 || !ValidCheck::isValidLuhn(objectAccount,19)) {
        QMessageBox::warning(this, "错误", "对方账户格式错误！");
        return;
    }
    inputUI.notice->setText("请再次输入对方账户：");
    inputUI.input->clear();
    if (dialog.exec() == QDialog::Rejected) return;
    if (objectAccount != inputUI.input->text()) {
        QMessageBox::warning(this, "错误", "两次输入不匹配！");
        return;
    }
    if (objectAccount == user.account) {
        QMessageBox::warning(this, "错误", "不能给自己转账！");
        return;
    }
    //获取转账金额
    inputUI.notice->setText("请输入转账金额：");
    inputUI.input->clear();
    inputUI.input->setValidator(new QRegularExpressionValidator(QRegularExpression(AMOUNT_REGEXP)));
    if (dialog.exec() == QDialog::Rejected) return;
    const auto amount = DataOperator::balanceStringToNumber(inputUI.input->text());
    //获取对方姓名
    auto instance = DataOperator::getInstance();
    auto objectUser = instance.getUserName(objectAccount);
    if (objectUser.errors != nullptr) {
        QMessageBox::warning(this, "错误", objectUser.errors);
        return;
    }
    //获取密码确认
    inputUI.input->setEchoMode(QLineEdit::Password);
    inputUI.notice->setText("即将向"+objectUser.name+"转账"+DataOperator::balanceNumberToString(amount)+"，请在下方输入密码确认：");
    inputUI.input->setMaxLength(6);
    inputUI.input->clear();
    if (dialog.exec() == QDialog::Rejected) return;
    if (inputUI.input->text() != password) {
        QMessageBox::warning(this, "错误", "密码错误，转账取消！");
        return;
    }
    //执行转账操作
    if (amount > user.maxSingleTransferAmount) {
        QMessageBox::warning(this, "错误", "转账金额超过单笔限额！");
        return;
    }
    if (user.dailyTransferTotal + amount > user.maxDailyTransferAmount) {
        QMessageBox::warning(this, "错误", "转账金额超过当日限额！");
        return;
    }
    if (amount > user.balance) {
        QMessageBox::warning(this, "错误", "余额不足，转账取消！");
        return;
    }
    //更新当日转账总额
    auto result = instance.updateDailyTransferTotal(user.account,amount, true);
    if (result != nullptr) {
        QMessageBox::warning(this, "错误", result);
        return;
    }
    result = instance.updateBalance(user.account,-amount);
    if (result != nullptr) {
        QMessageBox::warning(this, "错误", result);
        return;
    }
    result = instance.updateBalance(objectAccount,amount);
    if (result != nullptr) {
        //回滚
        instance.updateBalance(user.account,amount);
        QMessageBox::warning(this, "错误", "对方账户存储失败，转账取消:"+result);
        instance.updateDailyTransferTotal(user.account,amount, false);
    }
    QMessageBox::information(this, "成功", "转账成功！");
    refresh();
}


void AccountPanel::on_withdrawBalance_clicked()
{
    QDialog dialog;
    Ui::inputDialog inputUI;
    inputUI.setupUi(&dialog);
    dialog.setWindowTitle("取钱");
    inputUI.notice->setText("请输入数额：");
    inputUI.input->setValidator(new QRegularExpressionValidator(QRegularExpression("^[0-9]*$")));
    if (dialog.exec() == QDialog::Rejected) return;
    auto amount = inputUI.input->text().toLongLong();
    inputUI.input->setEchoMode(QLineEdit::Password);
    inputUI.notice->setText("即将取款："+QString::number(amount)+"，请在下方输入密码确认：");
    inputUI.input->clear();
    inputUI.input->setMaxLength(6);
    if (dialog.exec() == QDialog::Rejected) return;
    if (inputUI.input->text() != password) {
        QMessageBox::warning(this, "错误", "密码错误，转账取消！");
        return;
    }
    auto instance = DataOperator::getInstance();
    if (amount % 100) {
        QMessageBox::warning(this, "错误", "金额不是100的整数倍！");
        return;
    }
    const auto result = instance.updateBalance(user.account,-amount*100);
    if (result != nullptr) {
        QMessageBox::warning(this, "错误", result);
    } else {
        QMessageBox::information(this, "成功", "取款成功！");
        displayUserInfo();
    }
    refresh();
}