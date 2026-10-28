#include "LibrarySyncCrypto.h"

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#endif

namespace LibrarySyncCrypto {

bool encryptString(const QString &plainText, QByteArray *encryptedOut, QString *errorMessage)
{
    if (!encryptedOut) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Внутренняя ошибка шифрования.");
        }
        return false;
    }

#ifdef Q_OS_WIN
    const QByteArray input = plainText.toUtf8();
    DATA_BLOB inBlob;
    inBlob.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(input.constData()));
    inBlob.cbData = static_cast<DWORD>(input.size());

    DATA_BLOB outBlob;
    ZeroMemory(&outBlob, sizeof(outBlob));
    if (!CryptProtectData(&inBlob, L"VuzhykSync", nullptr, nullptr, nullptr, 0, &outBlob)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось зашифровать пароль (DPAPI).");
        }
        return false;
    }

    encryptedOut->clear();
    encryptedOut->append(reinterpret_cast<const char *>(outBlob.pbData), static_cast<int>(outBlob.cbData));
    LocalFree(outBlob.pbData);
    return true;
#else
    Q_UNUSED(plainText);
    if (errorMessage) {
        *errorMessage = QStringLiteral("Шифрование пароля поддерживается только в Windows.");
    }
    return false;
#endif
}

bool decryptString(const QByteArray &encrypted, QString *plainOut, QString *errorMessage)
{
    if (!plainOut) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Внутренняя ошибка расшифровки.");
        }
        return false;
    }

#ifdef Q_OS_WIN
    DATA_BLOB inBlob;
    inBlob.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(encrypted.constData()));
    inBlob.cbData = static_cast<DWORD>(encrypted.size());

    DATA_BLOB outBlob;
    ZeroMemory(&outBlob, sizeof(outBlob));
    if (!CryptUnprotectData(&inBlob, nullptr, nullptr, nullptr, nullptr, 0, &outBlob)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось расшифровать пароль.");
        }
        return false;
    }

    *plainOut = QString::fromUtf8(reinterpret_cast<const char *>(outBlob.pbData), static_cast<int>(outBlob.cbData));
    LocalFree(outBlob.pbData);
    return true;
#else
    Q_UNUSED(encrypted);
    if (errorMessage) {
        *errorMessage = QStringLiteral("Расшифровка пароля поддерживается только в Windows.");
    }
    return false;
#endif
}

bool verifyPassword(const QByteArray &encryptedPassword, const QString &password, QString *errorMessage)
{
    QString decrypted;
    if (!decryptString(encryptedPassword, &decrypted, errorMessage)) {
        return false;
    }
    return decrypted == password;
}

} // namespace LibrarySyncCrypto
