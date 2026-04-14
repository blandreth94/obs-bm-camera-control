#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <functional>

// Async HTTP client for the Blackmagic Camera REST API.
// Write methods are fire-and-forget. Poll() fetches all readable settings
// and emits individual signals so the UI can sync without fighting user input.
class CameraClient : public QObject {
	Q_OBJECT

public:
	explicit CameraClient(QObject *parent = nullptr);

	void connectToCamera(const QString &hostname);
	void disconnectFromCamera();

	bool isConnected() const { return m_connected; }
	QString hostname() const { return m_hostname; }

	// Poll all readable settings; emits the signals below on success.
	void poll();

	// Exposure (write)
	void setShutterSpeed(int denominator);
	void setShutterAngle(int hundredths);
	void setGain(int db);
	void setISO(int iso);
	void setNDStop(int stop);
	void setAutoExposure(const QString &mode, const QString &type);

	// White balance (write)
	void setWhiteBalance(int kelvin);
	void setWhiteBalanceTint(int tint);
	void doAutoWhiteBalance();

	// Color / contrast (write)
	void setContrast(double pivot, double adjust);
	void setColor(double hueDegrees, double saturation);

	// Presets (write + fetch)
	void fetchPresets();
	void applyPreset(const QString &filename);

signals:
	// Connection
	void connected();
	void connectFailed(const QString &reason);
	void disconnected();
	void errorOccurred(const QString &message);

	// Poll results — emitted after each poll() call
	void shutterReceived(int value, bool isAngle);
	void isoReceived(int iso);
	void gainReceived(int db);
	void ndReceived(int stop);
	void wbReceived(int kelvin);
	void tintReceived(int tint);
	void contrastReceived(double pivot, double adjust);
	void colorReceived(double hueDegrees, double saturation);
	void presetsReceived(const QStringList &presets);

private:
	void putJson(const QString &endpoint, const QByteArray &json);
	void get(const QString &endpoint, std::function<void(const QByteArray &)> callback);
	void silentGet(const QString &endpoint, std::function<void(const QByteArray &)> callback);
	QString baseUrl() const;

	QNetworkAccessManager *m_nam;
	QString m_hostname;
	bool m_connected = false;
};
