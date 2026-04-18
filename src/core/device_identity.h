#pragma once

/**
 * @file device_identity.h
 * @brief Manages the device's unique identity and bearer token via QSettings.
 *
 * On first launch, generates a UUID device ID and bearer token and persists
 * them in QSettings. On subsequent launches, reads the stored values.
 */

#include <QObject>
#include <QString>

namespace pairion::core {

/**
 * @brief Loads or creates device credentials from QSettings.
 *
 * For testing, use the two-argument constructor to provide explicit values.
 */
class DeviceIdentity : public QObject {
    Q_OBJECT
  public:
    /**
     * @brief Construct a DeviceIdentity that loads from QSettings.
     * @param parent QObject parent.
     */
    explicit DeviceIdentity(QObject *parent = nullptr);

    /**
     * @brief Construct a DeviceIdentity with explicit credentials (for testing).
     * @param deviceId Pre-set device ID.
     * @param bearerToken Pre-set bearer token.
     * @param parent QObject parent.
     */
    DeviceIdentity(const QString &deviceId, const QString &bearerToken, QObject *parent = nullptr);

    /// The device's unique identifier.
    const QString &deviceId() const;

    /// The bearer token used for WebSocket authentication.
    const QString &bearerToken() const;

  private:
    void loadOrCreate();

    QString m_deviceId;
    QString m_bearerToken;
};

} // namespace pairion::core
