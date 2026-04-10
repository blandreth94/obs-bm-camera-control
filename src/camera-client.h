#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkReply>

// Async HTTP client for the Blackmagic Camera REST API.
// All write methods fire-and-forget; readiness is signalled via connected()/connectFailed().
class CameraClient : public QObject {
	Q_OBJECT

public:
	explicit CameraClient(QObject *parent = nullptr);

	// Try to reach camera at hostname (e.g. "Camera-1.local" or "192.168.1.100").
	// Emits connected() or connectFailed() when done.
	void connectToCamera(const QString &hostname);
	void disconnectFromCamera();

	bool isConnected() const { return m_connected; }
	QString hostname() const { return m_hostname; }

	// Exposure
	void setShutterSpeed(int denominator); // e.g. 50 → 1/50
	void setShutterAngle(int hundredths); // e.g. 18000 → 180.0°
	void setGain(int db);
	void setISO(int iso);
	void setNDStop(int stop);
	void setAutoExposure(const QString &mode, const QString &type);

	// White balance
	void setWhiteBalance(int kelvin);
	void setWhiteBalanceTint(int tint);
	void doAutoWhiteBalance();

	// Color / contrast
	void setContrast(double pivot, double adjust);
	void setColor(double hue, double saturation); // hue in degrees (-180..+180)

	// Presets
	void fetchPresets(); // emits presetsReceived()
	void applyPreset(const QString &filename);

signals:
	void connected();
	void connectFailed(const QString &reason);
	void disconnected();
	void presetsReceived(const QStringList &presets);
	void errorOccurred(const QString &message);

private:
	void putJson(const QString &endpoint, const QByteArray &json);
	void get(const QString &endpoint, std::function<void(const QByteArray &)> callback);
	QString baseUrl() const;

	QNetworkAccessManager *m_nam;
	QString m_hostname;
	bool m_connected = false;
};
