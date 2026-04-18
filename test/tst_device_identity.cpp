/**
 * @file tst_device_identity.cpp
 * @brief Tests for DeviceIdentity: explicit and QSettings-based construction.
 */

#include "../src/core/device_identity.h"

#include <QSettings>
#include <QTest>

using namespace pairion::core;

class TestDeviceIdentity : public QObject {
    Q_OBJECT

  private slots:
    /// Clear QSettings before each test to ensure clean state.
    void init() {
        QSettings settings;
        settings.remove(QStringLiteral("device/id"));
        settings.remove(QStringLiteral("device/bearerToken"));
    }

    /// Verify explicit construction provides the given values.
    void explicitConstruction() {
        DeviceIdentity identity(QStringLiteral("dev-x"), QStringLiteral("tok-x"));
        QCOMPARE(identity.deviceId(), QStringLiteral("dev-x"));
        QCOMPARE(identity.bearerToken(), QStringLiteral("tok-x"));
    }

    /// Verify first-launch creates UUID credentials and persists them.
    void firstLaunchCreatesCredentials() {
        DeviceIdentity identity;
        QVERIFY(!identity.deviceId().isEmpty());
        QVERIFY(!identity.bearerToken().isEmpty());
        // UUIDs without braces are 36 chars
        QCOMPARE(identity.deviceId().length(), 36);
        QCOMPARE(identity.bearerToken().length(), 36);

        // Verify persisted in QSettings
        QSettings settings;
        QCOMPARE(settings.value(QStringLiteral("device/id")).toString(), identity.deviceId());
        QCOMPARE(settings.value(QStringLiteral("device/bearerToken")).toString(),
                 identity.bearerToken());
    }

    /// Verify second launch reuses existing credentials.
    void subsequentLaunchReusesCredentials() {
        DeviceIdentity identity1;
        QString savedId = identity1.deviceId();
        QString savedToken = identity1.bearerToken();

        DeviceIdentity identity2;
        QCOMPARE(identity2.deviceId(), savedId);
        QCOMPARE(identity2.bearerToken(), savedToken);
    }

    /// Verify partial state: device ID exists but bearer token missing.
    void partialStateRegeneratesToken() {
        QSettings settings;
        settings.setValue(QStringLiteral("device/id"), QStringLiteral("existing-id"));

        DeviceIdentity identity;
        QCOMPARE(identity.deviceId(), QStringLiteral("existing-id"));
        QVERIFY(!identity.bearerToken().isEmpty());
        QCOMPARE(identity.bearerToken().length(), 36);
    }

    /// Verify partial state: bearer token exists but device ID missing.
    void partialStateRegeneratesDeviceId() {
        QSettings settings;
        settings.setValue(QStringLiteral("device/bearerToken"), QStringLiteral("existing-tok"));

        DeviceIdentity identity;
        QVERIFY(!identity.deviceId().isEmpty());
        QCOMPARE(identity.deviceId().length(), 36);
        QCOMPARE(identity.bearerToken(), QStringLiteral("existing-tok"));
    }
};

QTEST_GUILESS_MAIN(TestDeviceIdentity)
#include "tst_device_identity.moc"
