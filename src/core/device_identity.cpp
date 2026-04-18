/**
 * @file device_identity.cpp
 * @brief Implementation of DeviceIdentity with QSettings-based credential persistence.
 */

#include "device_identity.h"

#include "constants.h"

#include <QSettings>
#include <QUuid>

namespace pairion::core {

DeviceIdentity::DeviceIdentity(QObject *parent) : QObject(parent) {
    loadOrCreate();
}

DeviceIdentity::DeviceIdentity(const QString &deviceId, const QString &bearerToken, QObject *parent)
    : QObject(parent), m_deviceId(deviceId), m_bearerToken(bearerToken) {}

const QString &DeviceIdentity::deviceId() const {
    return m_deviceId;
}

const QString &DeviceIdentity::bearerToken() const {
    return m_bearerToken;
}

void DeviceIdentity::loadOrCreate() {
    QSettings settings;
    m_deviceId = settings.value(QStringLiteral("device/id")).toString();
    m_bearerToken = settings.value(QStringLiteral("device/bearerToken")).toString();

    if (m_deviceId.isEmpty()) {
        m_deviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue(QStringLiteral("device/id"), m_deviceId);
    }
    if (m_bearerToken.isEmpty()) {
        m_bearerToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue(QStringLiteral("device/bearerToken"), m_bearerToken);
    }
}

} // namespace pairion::core
