基于qt的atm项目作业demo
内容

运用面向对象程序设计思想，基于命令行界面设计并实现一个ATM模拟程序，模拟常见的ATM功能。

要求

ATM模拟程序能够完成ATM的主要功能，包括：
①显示欢迎词及提示信息；②用户插卡，ATM验证用户账号及密码有效性，输入错误3次即被锁卡；③余额查询：初始余额为10000元；④取款功能：每次取款余额为100的整数倍，有单笔和单日金额限制；⑤转账功能：可将本账户中的存款转入其它账户，转入账户账号需两次输入确认；⑥修改密码：密码为6位数字，新密码需要两次输入确认；⑦退卡。
设计实现命令行界面，界面应友好、方便操作。参考界面如图1所示。程序所涉及到的用户资料、银行帐户、存取款记录等信息保存在数据文件中。其中银行账户的格式如下：

1	需求分析
该程序需要对用户提供友好界面，结合目前学习的C++相关编程技术，决定采用QT Widget进行GUI设计。
该程序涉及大量信息的读写，考虑到单机程序较少有并发读写+要求保存在数据文件，因此决定使用轻量的sqlite3进行数据存储。
单笔限额和单日限额应根据每个账户风控情况进行单独设置，也应当存储在用户数据。
所有金额都应该精确到分，即两位小数。考虑到小数存储可能会有精度损失，应当以分为单位，存为整数，但在用户界面仍应当设置为2位小数。
对用户输入需要进行校验，主要是：长度和特殊校验。身份证应该符合GB 11643-1999校验，银行卡号应该符合Luhn算法。输入的金额应当是0-2位小数。部分采用正则表达式进行匹配。
为保证密码安全，即使遇到数据泄露也不泄漏密码，本项目采用bcrypt进行密码加密，使用libbcrypt(Trusch,2019)库进行加密，在数据库存储密文且无法逆向解密，确保安全。
由于各个地方都需要使用到用户信息的全部和部分，因此设计一个通用的结构体，标准化存储相关数据。出于安全考虑，且大部分业务无需使用密码，因此结构体不包含密码，密码单独传参。
2	系统设计
2.1	用户界面流程：

2.2	数据库设计：
字段名	数据类型	说明
account	CHAR(19)	主键,账户号码，必须为19位数字字符串
name	VARCHAR(20)	用户姓名，最大长度20字符
id	CHAR(18)	身份证号码，必须为18位
password_crypted	CHAR(60)	加密密码，固定60位值
balance	BIGINT	账户余额，必须≥0（单位：分）
error_cout	INTEGER	密码错误次数，范围0-3
max_single_transfer_amount	BIGINT	单笔最大转账限额（单位：分）
max_daily_transfer_amount	BIGINT	每日最大转账限额（单位：分）
daily_transfer_total	BIGINT	当日已转账总额（单位：分）
last_transfer_date	DATE	最后转账日期
此外，设计了dataOperator::userInfo结构体，用于传递相关信息，
2.3	程序设计：
UI	类名	说明
accountpanel.ui	AccountPanel	用户登录后的信息和操作窗口
inputDialog.ui		自定义的提示窗口，在其他需要在消息中输入数据的函数内调用，无单独类控制。
mainwindow.ui	MainWindow	登录窗口
userinfooperator.ui	UserInfoOperator	开户和修改信息的面板
此外还使用DataOperator进行数据库操作，使用ValidCheck命名空间挂载验证函数。
3	编译情况
编译平台：macOS 26.2 (25C56) Apple M4
编译工具：Apple clang version 17.0.0 (clang-1700.6.3.2), cmake version 4.2.1, Qt 6.10.0 for macOS
4 感谢  
TRUSCH. (2019). libbcrypt [软件源代码]. GitHub 仓库. https://github.com/trusch/libbcrypt.git




