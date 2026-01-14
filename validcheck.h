#pragma once
#include <QString>
#define AMOUNT_REGEXP "^[0-9]+(.[0-9]{1,2})?$"
#define NUMBER_REGEXP "^[0-9]*$"
#define ID_REGEXP "^[0-9]{17}[0-9Xx]$"
#define NAME_REGEXP "^([a-zA-Z0-9\u4e00-\u9fa5\u00b7]{1,10})$"
namespace ValidCheck {
    bool isValidLuhn(const QString &number, qsizetype length);
    bool isValidID(const QString& id);
}
