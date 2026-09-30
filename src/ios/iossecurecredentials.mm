#include "iossecurecredentials.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include <QString>

namespace {
NSString *const serviceName = @"com.w9wdx.qk4phone.qrz";
NSString *const accountName = @"api-key";

NSMutableDictionary *baseQuery() {
    return [@{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: serviceName,
        (__bridge id)kSecAttrAccount: accountName
    } mutableCopy];
}

bool setError(QString *error, OSStatus status, const QString &operation) {
    if (error) {
        CFStringRef description = SecCopyErrorMessageString(status, nullptr);
        const QString detail = description ? QString::fromCFString(description) : QString::number(status);
        if (description)
            CFRelease(description);
        *error = QStringLiteral("%1: %2").arg(operation, detail);
    }
    return false;
}
}

QString IosSecureCredentials::readQrzApiKey(QString *error) {
    NSMutableDictionary *query = baseQuery();
    query[(__bridge id)kSecReturnData] = @YES;
    query[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;
    CFTypeRef result = nullptr;
    const OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
    if (status == errSecItemNotFound)
        return {};
    if (status != errSecSuccess) {
        setError(error, status, QStringLiteral("Cannot read the QRZ API key securely"));
        return {};
    }
    NSData *data = (__bridge_transfer NSData *)result;
    NSString *value = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    if (!value) {
        if (error)
            *error = QStringLiteral("The saved QRZ API key is not valid text.");
        return {};
    }
    return QString::fromNSString(value);
}

bool IosSecureCredentials::writeQrzApiKey(const QString &key, QString *error) {
    NSData *data = [key.toNSString() dataUsingEncoding:NSUTF8StringEncoding];
    NSMutableDictionary *query = baseQuery();
    NSDictionary *update = @{(__bridge id)kSecValueData: data};
    OSStatus status = SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)update);
    if (status == errSecItemNotFound) {
        query[(__bridge id)kSecValueData] = data;
        query[(__bridge id)kSecAttrAccessible] = (__bridge id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly;
        status = SecItemAdd((__bridge CFDictionaryRef)query, nullptr);
    }
    return status == errSecSuccess
        || setError(error, status, QStringLiteral("Cannot save the QRZ API key securely"));
}

bool IosSecureCredentials::clearQrzApiKey(QString *error) {
    const OSStatus status = SecItemDelete((__bridge CFDictionaryRef)baseQuery());
    return status == errSecSuccess || status == errSecItemNotFound
        || setError(error, status, QStringLiteral("Cannot remove the QRZ API key"));
}
