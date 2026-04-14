#include "camera-client.h"

#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <functional>

CameraClient::CameraClient(QObject *parent) : QObject(parent)
{
	m_nam = new QNetworkAccessManager(this);
}

QString CameraClient::baseUrl() const
{
	return QString("http://%1/control/api/v1").arg(m_hostname);
}

void CameraClient::connectToCamera(const QString &hostname)
{
	m_hostname = hostname;
	m_connected = false;

	// Probe /system to verify reachability
	get("/system", [this](const QByteArray &data) {
		Q_UNUSED(data)
		m_connected = true;
		emit connected();
	});
}

void CameraClient::disconnectFromCamera()
{
	m_connected = false;
	m_hostname.clear();
	emit disconnected();
}

// ── helpers ──────────────────────────────────────────────────────────────────

void CameraClient::putJson(const QString &endpoint, const QByteArray &json)
{
	if (!m_connected)
		return;

	QNetworkRequest req(QUrl(baseUrl() + endpoint));
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	QNetworkReply *reply = m_nam->put(req, json);
	connect(reply, &QNetworkReply::finished, reply, [this, reply, endpoint]() {
		if (reply->error() != QNetworkReply::NoError) {
			emit errorOccurred(
				QString("PUT %1 failed: %2").arg(endpoint, reply->errorString()));
		}
		reply->deleteLater();
	});
}

void CameraClient::get(const QString &endpoint,
		       std::function<void(const QByteArray &)> callback)
{
	QNetworkRequest req(QUrl(baseUrl() + endpoint));

	QNetworkReply *reply = m_nam->get(req);
	connect(reply, &QNetworkReply::finished, reply,
		[this, reply, endpoint, callback = std::move(callback)]() {
			if (reply->error() != QNetworkReply::NoError) {
				emit connectFailed(reply->errorString());
			} else {
				callback(reply->readAll());
			}
			reply->deleteLater();
		});
}

// Silent GET for polling — ignores errors rather than emitting connectFailed
void CameraClient::silentGet(const QString &endpoint,
			     std::function<void(const QByteArray &)> callback)
{
	if (!m_connected)
		return;
	QNetworkRequest req(QUrl(baseUrl() + endpoint));
	QNetworkReply *reply = m_nam->get(req);
	connect(reply, &QNetworkReply::finished, reply,
		[reply, callback = std::move(callback)]() {
			if (reply->error() == QNetworkReply::NoError)
				callback(reply->readAll());
			reply->deleteLater();
		});
}

// ── exposure ─────────────────────────────────────────────────────────────────

void CameraClient::setShutterSpeed(int denominator)
{
	QJsonObject obj;
	obj["shutterSpeed"] = denominator;
	putJson("/video/shutter", QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void CameraClient::setShutterAngle(int hundredths)
{
	QJsonObject obj;
	obj["shutterAngle"] = hundredths;
	putJson("/video/shutter", QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void CameraClient::setGain(int db)
{
	QJsonObject obj;
	obj["gain"] = db;
	putJson("/video/gain", QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void CameraClient::setISO(int iso)
{
	QJsonObject obj;
	obj["iso"] = iso;
	putJson("/video/iso", QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void CameraClient::setNDStop(int stop)
{
	QJsonObject obj;
	obj["stop"] = stop;
	putJson("/video/ndFilter", QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void CameraClient::setAutoExposure(const QString &mode, const QString &type)
{
	QJsonObject obj;
	obj["mode"] = mode;
	obj["type"] = type;
	putJson("/video/autoExposure", QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

// ── white balance ─────────────────────────────────────────────────────────────

void CameraClient::setWhiteBalance(int kelvin)
{
	QJsonObject obj;
	obj["whiteBalance"] = kelvin;
	putJson("/video/whiteBalance", QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void CameraClient::setWhiteBalanceTint(int tint)
{
	QJsonObject obj;
	obj["whiteBalanceTint"] = tint;
	putJson("/video/whiteBalanceTint", QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void CameraClient::doAutoWhiteBalance()
{
	putJson("/video/whiteBalance/doAuto", "{}");
}

// ── color / contrast ──────────────────────────────────────────────────────────

void CameraClient::setContrast(double pivot, double adjust)
{
	QJsonObject obj;
	obj["pivot"] = pivot;
	obj["adjust"] = adjust;
	putJson("/colorCorrection/contrast", QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void CameraClient::setColor(double hueDegrees, double saturation)
{
	// API expects hue as -1.0..+1.0 mapped from -180..+180
	QJsonObject obj;
	obj["hue"] = hueDegrees / 180.0;
	obj["saturation"] = saturation;
	putJson("/colorCorrection/color", QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

// ── poll ──────────────────────────────────────────────────────────────────────

void CameraClient::poll()
{
	// Shutter — camera reports either shutterSpeed or shutterAngle, not both
	silentGet("/video/shutter", [this](const QByteArray &data) {
		QJsonObject obj = QJsonDocument::fromJson(data).object();
		if (obj.contains("shutterAngle"))
			emit shutterReceived(obj["shutterAngle"].toInt(), true);
		else if (obj.contains("shutterSpeed"))
			emit shutterReceived(obj["shutterSpeed"].toInt(), false);
	});

	silentGet("/video/iso", [this](const QByteArray &data) {
		QJsonObject obj = QJsonDocument::fromJson(data).object();
		if (obj.contains("iso"))
			emit isoReceived(obj["iso"].toInt());
	});

	silentGet("/video/gain", [this](const QByteArray &data) {
		QJsonObject obj = QJsonDocument::fromJson(data).object();
		if (obj.contains("gain"))
			emit gainReceived(obj["gain"].toInt());
	});

	silentGet("/video/ndFilter", [this](const QByteArray &data) {
		QJsonObject obj = QJsonDocument::fromJson(data).object();
		if (obj.contains("stop"))
			emit ndReceived(obj["stop"].toInt());
	});

	silentGet("/video/whiteBalance", [this](const QByteArray &data) {
		QJsonObject obj = QJsonDocument::fromJson(data).object();
		if (obj.contains("whiteBalance"))
			emit wbReceived(obj["whiteBalance"].toInt());
	});

	silentGet("/video/whiteBalanceTint", [this](const QByteArray &data) {
		QJsonObject obj = QJsonDocument::fromJson(data).object();
		if (obj.contains("whiteBalanceTint"))
			emit tintReceived(obj["whiteBalanceTint"].toInt());
	});

	silentGet("/colorCorrection/contrast", [this](const QByteArray &data) {
		QJsonObject obj = QJsonDocument::fromJson(data).object();
		if (obj.contains("pivot") && obj.contains("adjust"))
			emit contrastReceived(obj["pivot"].toDouble(),
					      obj["adjust"].toDouble());
	});

	silentGet("/colorCorrection/color", [this](const QByteArray &data) {
		QJsonObject obj = QJsonDocument::fromJson(data).object();
		if (obj.contains("hue") && obj.contains("saturation"))
			// Convert hue from -1..+1 back to degrees
			emit colorReceived(obj["hue"].toDouble() * 180.0,
					   obj["saturation"].toDouble());
	});
}

// ── presets ───────────────────────────────────────────────────────────────────

void CameraClient::fetchPresets()
{
	get("/presets", [this](const QByteArray &data) {
		QJsonDocument doc = QJsonDocument::fromJson(data);
		QJsonArray arr = doc.object()["presets"].toArray();
		QStringList list;
		for (const QJsonValue v : arr)
			list << v.toString();
		emit presetsReceived(list);
	});
}

void CameraClient::applyPreset(const QString &filename)
{
	QJsonObject obj;
	obj["preset"] = filename;
	putJson("/presets/active", QJsonDocument(obj).toJson(QJsonDocument::Compact));
}
