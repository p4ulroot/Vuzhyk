#pragma once

#include <QByteArray>
#include <QString>

namespace LibrarySyncCrypto {

bool encryptString(const QString &plainText, QByteArray *encryptedOut, QString *errorMessage = nullptr);
bool decryptString(const QByteArray &encrypted, QString *plainOut, QString *errorMessage = nullptr);
bool verifyPassword(const QByteArray &encryptedPassword, const QString &password, QString *errorMessage = nullptr);

} // namespace LibrarySyncCrypto
