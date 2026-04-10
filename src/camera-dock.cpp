#include "camera-dock.h"
#include "camera-client.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QScrollArea>
#include <QSettings>
#include <QLabel>
#include <QPushButton>

static const char *SETTINGS_ORG = "obs-bm-camera-control";
static const char *SETTINGS_APP = "camera";

CameraDockWidget::CameraDockWidget(QWidget *parent) : QDockWidget("BM Camera Control", parent)
{
	m_client = new CameraClient(this);
	connect(m_client, &CameraClient::connected, this, &CameraDockWidget::onConnected);
	connect(m_client, &CameraClient::connectFailed, this, &CameraDockWidget::onConnectFailed);
	connect(m_client, &CameraClient::disconnected, this, &CameraDockWidget::onDisconnected);
	connect(m_client, &CameraClient::presetsReceived, this,
		&CameraDockWidget::onPresetsReceived);

	setObjectName("BMCameraControlDock");
	setMinimumWidth(280);
	buildUI();
	setControlsEnabled(false);
}

CameraDockWidget::~CameraDockWidget() {}

// ── UI construction ───────────────────────────────────────────────────────────

void CameraDockWidget::buildUI()
{
	QScrollArea *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	QWidget *content = new QWidget(scroll);
	QVBoxLayout *root = new QVBoxLayout(content);
	root->setSpacing(6);
	root->setContentsMargins(6, 6, 6, 6);

	// ── Connection ────────────────────────────────────────────────────────
	{
		QGroupBox *grp = new QGroupBox("Camera", content);
		QVBoxLayout *vl = new QVBoxLayout(grp);

		QHBoxLayout *hl = new QHBoxLayout;
		m_hostnameEdit = new QLineEdit;
		m_hostnameEdit->setPlaceholderText("hostname or IP");
		m_hostnameEdit->setText(loadHostname());
		m_connectBtn = new QPushButton("Connect");
		hl->addWidget(m_hostnameEdit);
		hl->addWidget(m_connectBtn);
		vl->addLayout(hl);

		m_statusLabel = new QLabel("Not connected");
		m_statusLabel->setStyleSheet("color: gray;");
		vl->addWidget(m_statusLabel);

		root->addWidget(grp);

		connect(m_connectBtn, &QPushButton::clicked, this,
			&CameraDockWidget::onConnectClicked);
	}

	// ── Exposure ─────────────────────────────────────────────────────────
	{
		QGroupBox *grp = new QGroupBox("Exposure", content);
		QFormLayout *fl = new QFormLayout(grp);

		// Shutter
		QWidget *shutterRow = new QWidget;
		QHBoxLayout *shutterHl = new QHBoxLayout(shutterRow);
		shutterHl->setContentsMargins(0, 0, 0, 0);
		m_shutterSpin = new QSpinBox;
		m_shutterSpin->setRange(1, 16000);
		m_shutterSpin->setValue(50);
		m_shutterAngleMode = new QCheckBox("Angle");
		m_shutterAngleMode->setToolTip("When checked, value is shutter angle ×100 (e.g. 18000 = 180°).\nWhen unchecked, value is 1/N shutter speed.");
		shutterHl->addWidget(m_shutterSpin);
		shutterHl->addWidget(m_shutterAngleMode);
		fl->addRow("Shutter", shutterRow);

		connect(m_shutterSpin, &QSpinBox::editingFinished, this, [this]() {
			if (m_shutterAngleMode->isChecked())
				m_client->setShutterAngle(m_shutterSpin->value());
			else
				m_client->setShutterSpeed(m_shutterSpin->value());
		});

		// ISO
		m_isoSpin = new QSpinBox;
		m_isoSpin->setRange(100, 25600);
		m_isoSpin->setSingleStep(100);
		m_isoSpin->setValue(400);
		fl->addRow("ISO", m_isoSpin);
		connect(m_isoSpin, &QSpinBox::editingFinished, this,
			[this]() { m_client->setISO(m_isoSpin->value()); });

		// Gain
		m_gainSpin = new QSpinBox;
		m_gainSpin->setRange(-12, 36);
		m_gainSpin->setSingleStep(2);
		m_gainSpin->setValue(0);
		m_gainSpin->setSuffix(" dB");
		fl->addRow("Gain", m_gainSpin);
		connect(m_gainSpin, &QSpinBox::editingFinished, this,
			[this]() { m_client->setGain(m_gainSpin->value()); });

		// ND
		m_ndSpin = new QSpinBox;
		m_ndSpin->setRange(0, 12);
		m_ndSpin->setSingleStep(2);
		m_ndSpin->setValue(0);
		m_ndSpin->setPrefix("ND");
		fl->addRow("ND Filter", m_ndSpin);
		connect(m_ndSpin, &QSpinBox::editingFinished, this,
			[this]() { m_client->setNDStop(m_ndSpin->value()); });

		// Auto Exposure
		m_autoExposureCombo = new QComboBox;
		m_autoExposureCombo->addItem("Off", QStringList{"Off", ""});
		m_autoExposureCombo->addItem("Iris (continuous)",
					     QStringList{"Continuous", "Iris"});
		m_autoExposureCombo->addItem("Shutter (continuous)",
					     QStringList{"Continuous", "Shutter"});
		m_autoExposureCombo->addItem("Iris+Shutter (continuous)",
					     QStringList{"Continuous", "Iris,Shutter"});
		m_autoExposureCombo->addItem("One-shot Iris", QStringList{"OneShot", "Iris"});
		fl->addRow("Auto Exp.", m_autoExposureCombo);
		connect(m_autoExposureCombo, &QComboBox::currentIndexChanged, this,
			[this](int idx) {
				QStringList pair =
					m_autoExposureCombo->itemData(idx).toStringList();
				m_client->setAutoExposure(pair[0], pair[1]);
			});

		root->addWidget(grp);
	}

	// ── White Balance ─────────────────────────────────────────────────────
	{
		QGroupBox *grp = new QGroupBox("White Balance", content);
		QVBoxLayout *vl = new QVBoxLayout(grp);
		QFormLayout *fl = new QFormLayout;

		// Kelvin slider + spinbox (linked)
		QHBoxLayout *khl = new QHBoxLayout;
		m_wbSlider = new QSlider(Qt::Horizontal);
		m_wbSlider->setRange(2000, 8000);
		m_wbSlider->setSingleStep(100);
		m_wbSlider->setValue(5600);
		m_wbSpin = new QSpinBox;
		m_wbSpin->setRange(2000, 8000);
		m_wbSpin->setSingleStep(100);
		m_wbSpin->setValue(5600);
		m_wbSpin->setSuffix(" K");
		khl->addWidget(m_wbSlider);
		khl->addWidget(m_wbSpin);
		fl->addRow("Kelvin", khl);

		// Keep slider and spinbox in sync
		connect(m_wbSlider, &QSlider::valueChanged, m_wbSpin, &QSpinBox::setValue);
		connect(m_wbSpin, &QSpinBox::valueChanged, m_wbSlider, &QSlider::setValue);
		connect(m_wbSlider, &QSlider::sliderReleased, this,
			[this]() { m_client->setWhiteBalance(m_wbSpin->value()); });
		connect(m_wbSpin, &QSpinBox::editingFinished, this,
			[this]() { m_client->setWhiteBalance(m_wbSpin->value()); });

		// Tint
		m_tintSpin = new QSpinBox;
		m_tintSpin->setRange(-100, 100);
		m_tintSpin->setValue(0);
		fl->addRow("Tint", m_tintSpin);
		connect(m_tintSpin, &QSpinBox::editingFinished, this,
			[this]() { m_client->setWhiteBalanceTint(m_tintSpin->value()); });

		vl->addLayout(fl);

		// Auto WB button
		QPushButton *autoWBBtn = new QPushButton("Auto White Balance");
		connect(autoWBBtn, &QPushButton::clicked, this,
			&CameraDockWidget::onAutoWBClicked);
		vl->addWidget(autoWBBtn);

		// WB Presets
		QHBoxLayout *presetRow = new QHBoxLayout;
		struct WBPreset {
			const char *label;
			int kelvin;
			int tint;
		};
		static const WBPreset wbPresets[] = {{"Sun", 5600, 10},
						     {"Tungsten", 3200, 0},
						     {"Fluor.", 4000, 15},
						     {"Shade", 4500, 15},
						     {"Cloud", 6500, 10}};
		for (const auto &p : wbPresets) {
			QPushButton *btn = new QPushButton(p.label);
			btn->setToolTip(QString("%1 K").arg(p.kelvin));
			int k = p.kelvin;
			int t = p.tint;
			connect(btn, &QPushButton::clicked, this,
				[this, k, t]() { onWBPresetClicked(k, t); });
			presetRow->addWidget(btn);
		}
		vl->addLayout(presetRow);

		root->addWidget(grp);
	}

	// ── Color / Contrast ─────────────────────────────────────────────────
	{
		QGroupBox *grp = new QGroupBox("Color / Contrast", content);
		QFormLayout *fl = new QFormLayout(grp);

		m_contrastAdjust = new QDoubleSpinBox;
		m_contrastAdjust->setRange(0.0, 2.0);
		m_contrastAdjust->setSingleStep(0.05);
		m_contrastAdjust->setValue(1.0);
		m_contrastAdjust->setToolTip("1.0 = no change");
		fl->addRow("Contrast", m_contrastAdjust);

		m_contrastPivot = new QDoubleSpinBox;
		m_contrastPivot->setRange(0.0, 1.0);
		m_contrastPivot->setSingleStep(0.05);
		m_contrastPivot->setValue(0.5);
		fl->addRow("Pivot", m_contrastPivot);

		connect(m_contrastAdjust, &QDoubleSpinBox::editingFinished, this, [this]() {
			m_client->setContrast(m_contrastPivot->value(),
					      m_contrastAdjust->value());
		});
		connect(m_contrastPivot, &QDoubleSpinBox::editingFinished, this, [this]() {
			m_client->setContrast(m_contrastPivot->value(),
					      m_contrastAdjust->value());
		});

		m_hueSpin = new QSpinBox;
		m_hueSpin->setRange(-180, 180);
		m_hueSpin->setValue(0);
		m_hueSpin->setSuffix("°");
		fl->addRow("Hue", m_hueSpin);

		m_satSpin = new QSpinBox;
		m_satSpin->setRange(0, 200);
		m_satSpin->setValue(100);
		m_satSpin->setSuffix("%");
		fl->addRow("Saturation", m_satSpin);

		connect(m_hueSpin, &QSpinBox::editingFinished, this, [this]() {
			m_client->setColor(m_hueSpin->value(), m_satSpin->value() / 100.0);
		});
		connect(m_satSpin, &QSpinBox::editingFinished, this, [this]() {
			m_client->setColor(m_hueSpin->value(), m_satSpin->value() / 100.0);
		});

		root->addWidget(grp);
	}

	// ── Presets ───────────────────────────────────────────────────────────
	{
		QGroupBox *grp = new QGroupBox("Presets", content);
		QVBoxLayout *vl = new QVBoxLayout(grp);

		m_presetList = new QListWidget;
		m_presetList->setMaximumHeight(120);
		vl->addWidget(m_presetList);

		QHBoxLayout *btnRow = new QHBoxLayout;
		QPushButton *applyBtn = new QPushButton("Apply");
		QPushButton *refreshBtn = new QPushButton("Refresh");
		btnRow->addWidget(applyBtn);
		btnRow->addWidget(refreshBtn);
		vl->addLayout(btnRow);

		connect(applyBtn, &QPushButton::clicked, this,
			&CameraDockWidget::onApplyPresetClicked);
		connect(refreshBtn, &QPushButton::clicked, this,
			&CameraDockWidget::onRefreshPresetsClicked);

		root->addWidget(grp);
	}

	root->addStretch();
	scroll->setWidget(content);
	setWidget(scroll);
}

// ── slots ────────────────────────────────────────────────────────────────────

void CameraDockWidget::onConnectClicked()
{
	if (m_client->isConnected()) {
		m_client->disconnectFromCamera();
	} else {
		QString host = m_hostnameEdit->text().trimmed();
		if (host.isEmpty())
			return;
		saveHostname(host);
		m_statusLabel->setText("Connecting…");
		m_statusLabel->setStyleSheet("color: orange;");
		m_connectBtn->setEnabled(false);
		m_client->connectToCamera(host);
	}
}

void CameraDockWidget::onConnected()
{
	m_statusLabel->setText(QString("Connected: %1").arg(m_client->hostname()));
	m_statusLabel->setStyleSheet("color: green;");
	m_connectBtn->setText("Disconnect");
	m_connectBtn->setEnabled(true);
	setControlsEnabled(true);
	m_client->fetchPresets();
}

void CameraDockWidget::onConnectFailed(const QString &reason)
{
	m_statusLabel->setText(QString("Failed: %1").arg(reason));
	m_statusLabel->setStyleSheet("color: red;");
	m_connectBtn->setText("Connect");
	m_connectBtn->setEnabled(true);
	setControlsEnabled(false);
}

void CameraDockWidget::onDisconnected()
{
	m_statusLabel->setText("Not connected");
	m_statusLabel->setStyleSheet("color: gray;");
	m_connectBtn->setText("Connect");
	setControlsEnabled(false);
	m_presetList->clear();
}

void CameraDockWidget::onPresetsReceived(const QStringList &presets)
{
	m_presetList->clear();
	for (const QString &p : presets)
		m_presetList->addItem(p);
}

void CameraDockWidget::onApplyPresetClicked()
{
	QListWidgetItem *item = m_presetList->currentItem();
	if (item)
		m_client->applyPreset(item->text());
}

void CameraDockWidget::onRefreshPresetsClicked()
{
	m_client->fetchPresets();
}

void CameraDockWidget::onWBPresetClicked(int kelvin, int tint)
{
	m_wbSpin->setValue(kelvin);
	m_tintSpin->setValue(tint);
	m_client->setWhiteBalance(kelvin);
	m_client->setWhiteBalanceTint(tint);
}

void CameraDockWidget::onAutoWBClicked()
{
	m_client->doAutoWhiteBalance();
}

// ── helpers ───────────────────────────────────────────────────────────────────

void CameraDockWidget::setControlsEnabled(bool enabled)
{
	m_shutterSpin->setEnabled(enabled);
	m_shutterAngleMode->setEnabled(enabled);
	m_gainSpin->setEnabled(enabled);
	m_isoSpin->setEnabled(enabled);
	m_ndSpin->setEnabled(enabled);
	m_autoExposureCombo->setEnabled(enabled);
	m_wbSlider->setEnabled(enabled);
	m_wbSpin->setEnabled(enabled);
	m_tintSpin->setEnabled(enabled);
	m_contrastAdjust->setEnabled(enabled);
	m_contrastPivot->setEnabled(enabled);
	m_hueSpin->setEnabled(enabled);
	m_satSpin->setEnabled(enabled);
	m_presetList->setEnabled(enabled);
}

void CameraDockWidget::saveHostname(const QString &hostname)
{
	QSettings s(SETTINGS_ORG, SETTINGS_APP);
	s.setValue("hostname", hostname);
}

QString CameraDockWidget::loadHostname() const
{
	QSettings s(SETTINGS_ORG, SETTINGS_APP);
	return s.value("hostname", "").toString();
}
