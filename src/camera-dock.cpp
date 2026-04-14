#include "camera-dock.h"
#include "camera-client.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QScrollArea>
#include <QSettings>
#include <QPushButton>
#include <QSizePolicy>

static const char *SETTINGS_ORG = "obs-bm-camera-control";
static const char *SETTINGS_APP = "camera";
static const int POLL_INTERVAL_MS = 2000;

// ── constructor ───────────────────────────────────────────────────────────────

CameraControlWidget::CameraControlWidget(QWidget *parent) : QWidget(parent)
{
	m_client = new CameraClient(this);
	connect(m_client, &CameraClient::connected, this, &CameraControlWidget::onConnected);
	connect(m_client, &CameraClient::connectFailed, this,
		&CameraControlWidget::onConnectFailed);
	connect(m_client, &CameraClient::disconnected, this, &CameraControlWidget::onDisconnected);
	connect(m_client, &CameraClient::presetsReceived, this,
		&CameraControlWidget::onPresetsReceived);
	connect(m_client, &CameraClient::shutterReceived, this,
		&CameraControlWidget::onShutterReceived);
	connect(m_client, &CameraClient::isoReceived, this, &CameraControlWidget::onISOReceived);
	connect(m_client, &CameraClient::gainReceived, this, &CameraControlWidget::onGainReceived);
	connect(m_client, &CameraClient::ndReceived, this, &CameraControlWidget::onNDReceived);
	connect(m_client, &CameraClient::wbReceived, this, &CameraControlWidget::onWBReceived);
	connect(m_client, &CameraClient::tintReceived, this, &CameraControlWidget::onTintReceived);
	connect(m_client, &CameraClient::contrastReceived, this,
		&CameraControlWidget::onContrastReceived);
	connect(m_client, &CameraClient::colorReceived, this,
		&CameraControlWidget::onColorReceived);

	m_pollTimer = new QTimer(this);
	m_pollTimer->setInterval(POLL_INTERVAL_MS);
	connect(m_pollTimer, &QTimer::timeout, m_client, &CameraClient::poll);

	buildUI();
	setControlsEnabled(false);
}

CameraControlWidget::~CameraControlWidget() {}

// ── responsive layout ─────────────────────────────────────────────────────────

void CameraControlWidget::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	bool wide = event->size().width() >= WIDE_THRESHOLD;
	if (wide != m_wideMode) {
		m_wideMode = wide;
		relayout(wide);
	}
}

// Remove all four groups from the grid and re-add them in the right arrangement.
// Wide:  [Exposure | WB   ]    Narrow:  [Exposure]
//        [Color   | Presets]             [WB      ]
//                                        [Color   ]
//                                        [Presets ]
void CameraControlWidget::relayout(bool wide)
{
	// Detach everything (doesn't delete widgets)
	while (m_groupGrid->count())
		m_groupGrid->takeAt(0);

	if (wide) {
		m_groupGrid->setColumnStretch(0, 1);
		m_groupGrid->setColumnStretch(1, 1);
		m_groupGrid->addWidget(m_exposureGroup, 0, 0);
		m_groupGrid->addWidget(m_wbGroup, 0, 1);
		m_groupGrid->addWidget(m_colorGroup, 1, 0);
		m_groupGrid->addWidget(m_presetsGroup, 1, 1);
	} else {
		m_groupGrid->setColumnStretch(0, 1);
		m_groupGrid->setColumnStretch(1, 0);
		m_groupGrid->addWidget(m_exposureGroup, 0, 0);
		m_groupGrid->addWidget(m_wbGroup, 1, 0);
		m_groupGrid->addWidget(m_colorGroup, 2, 0);
		m_groupGrid->addWidget(m_presetsGroup, 3, 0);
	}
}

// ── UI construction ───────────────────────────────────────────────────────────

void CameraControlWidget::buildUI()
{
	QScrollArea *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	QWidget *content = new QWidget(scroll);
	QVBoxLayout *root = new QVBoxLayout(content);
	root->setSpacing(6);
	root->setContentsMargins(6, 6, 6, 6);

	// ── Connection (always full width) ────────────────────────────────────
	{
		QGroupBox *grp = new QGroupBox("Camera", content);
		QGridLayout *gl = new QGridLayout(grp);
		gl->setSpacing(4);

		m_hostnameEdit = new QLineEdit;
		m_hostnameEdit->setPlaceholderText("hostname or IP");
		m_hostnameEdit->setText(loadHostname());
		m_connectBtn = new QPushButton("Connect");
		m_connectBtn->setFixedWidth(90);
		m_statusLabel = new QLabel("Not connected");
		m_statusLabel->setStyleSheet("color: gray; font-size: 11px;");

		gl->addWidget(m_hostnameEdit, 0, 0);
		gl->addWidget(m_connectBtn, 0, 1);
		gl->addWidget(m_statusLabel, 1, 0, 1, 2);

		root->addWidget(grp);
		connect(m_connectBtn, &QPushButton::clicked, this,
			&CameraControlWidget::onConnectClicked);
	}

	// ── Group grid (rearranged by relayout()) ─────────────────────────────
	m_groupGrid = new QGridLayout;
	m_groupGrid->setSpacing(6);

	// ── Exposure group ────────────────────────────────────────────────────
	{
		m_exposureGroup = new QGroupBox("Exposure", content);
		m_exposureGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		QGridLayout *gl = new QGridLayout(m_exposureGroup);
		gl->setSpacing(4);
		gl->setColumnStretch(1, 1);
		int row = 0;

		// Shutter + angle toggle
		m_shutterSpin = new QSpinBox;
		m_shutterSpin->setRange(1, 36000);
		m_shutterSpin->setValue(50);
		m_shutterAngleMode = new QCheckBox("°");
		m_shutterAngleMode->setToolTip(
			"Unchecked: 1/N shutter speed\nChecked: angle ×100 (18000 = 180°)");
		QHBoxLayout *shutHL = new QHBoxLayout;
		shutHL->setContentsMargins(0, 0, 0, 0);
		shutHL->setSpacing(2);
		shutHL->addWidget(m_shutterSpin, 1);
		shutHL->addWidget(m_shutterAngleMode);
		QWidget *shutW = new QWidget;
		shutW->setLayout(shutHL);
		gl->addWidget(new QLabel("Shutter"), row, 0);
		gl->addWidget(shutW, row++, 1);
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
		gl->addWidget(new QLabel("ISO"), row, 0);
		gl->addWidget(m_isoSpin, row++, 1);
		connect(m_isoSpin, &QSpinBox::editingFinished, this,
			[this]() { m_client->setISO(m_isoSpin->value()); });

		// Gain
		m_gainSpin = new QSpinBox;
		m_gainSpin->setRange(-12, 36);
		m_gainSpin->setSingleStep(2);
		m_gainSpin->setValue(0);
		m_gainSpin->setSuffix(" dB");
		gl->addWidget(new QLabel("Gain"), row, 0);
		gl->addWidget(m_gainSpin, row++, 1);
		connect(m_gainSpin, &QSpinBox::editingFinished, this,
			[this]() { m_client->setGain(m_gainSpin->value()); });

		// ND
		m_ndSpin = new QSpinBox;
		m_ndSpin->setRange(0, 12);
		m_ndSpin->setSingleStep(2);
		m_ndSpin->setValue(0);
		m_ndSpin->setPrefix("ND");
		gl->addWidget(new QLabel("ND Filter"), row, 0);
		gl->addWidget(m_ndSpin, row++, 1);
		connect(m_ndSpin, &QSpinBox::editingFinished, this,
			[this]() { m_client->setNDStop(m_ndSpin->value()); });

		// Auto Exposure
		m_autoExposureCombo = new QComboBox;
		m_autoExposureCombo->addItem("Off", QStringList{"Off", ""});
		m_autoExposureCombo->addItem("Iris", QStringList{"Continuous", "Iris"});
		m_autoExposureCombo->addItem("Shutter", QStringList{"Continuous", "Shutter"});
		m_autoExposureCombo->addItem("Iris+Shutter",
					     QStringList{"Continuous", "Iris,Shutter"});
		m_autoExposureCombo->addItem("1-shot Iris", QStringList{"OneShot", "Iris"});
		gl->addWidget(new QLabel("Auto"), row, 0);
		gl->addWidget(m_autoExposureCombo, row++, 1);
		connect(m_autoExposureCombo, &QComboBox::currentIndexChanged, this,
			[this](int idx) {
				QStringList pair =
					m_autoExposureCombo->itemData(idx).toStringList();
				m_client->setAutoExposure(pair[0], pair[1]);
			});
	}

	// ── White Balance group ───────────────────────────────────────────────
	{
		m_wbGroup = new QGroupBox("White Balance", content);
		m_wbGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		QVBoxLayout *vl = new QVBoxLayout(m_wbGroup);
		vl->setSpacing(4);

		// Kelvin slider spans full group width
		m_wbSlider = new QSlider(Qt::Horizontal);
		m_wbSlider->setRange(2000, 8000);
		m_wbSlider->setSingleStep(100);
		m_wbSlider->setValue(5600);
		vl->addWidget(m_wbSlider);

		// Kelvin spinbox + Tint
		QGridLayout *gl = new QGridLayout;
		gl->setSpacing(4);
		gl->setColumnStretch(1, 1);

		m_wbSpin = new QSpinBox;
		m_wbSpin->setRange(2000, 8000);
		m_wbSpin->setSingleStep(100);
		m_wbSpin->setValue(5600);
		m_wbSpin->setSuffix(" K");
		gl->addWidget(new QLabel("Kelvin"), 0, 0);
		gl->addWidget(m_wbSpin, 0, 1);

		m_tintSpin = new QSpinBox;
		m_tintSpin->setRange(-100, 100);
		m_tintSpin->setValue(0);
		gl->addWidget(new QLabel("Tint"), 1, 0);
		gl->addWidget(m_tintSpin, 1, 1);
		vl->addLayout(gl);

		connect(m_wbSlider, &QSlider::valueChanged, m_wbSpin, &QSpinBox::setValue);
		connect(m_wbSpin, &QSpinBox::valueChanged, m_wbSlider, &QSlider::setValue);
		connect(m_wbSlider, &QSlider::sliderReleased, this,
			[this]() { m_client->setWhiteBalance(m_wbSpin->value()); });
		connect(m_wbSpin, &QSpinBox::editingFinished, this,
			[this]() { m_client->setWhiteBalance(m_wbSpin->value()); });
		connect(m_tintSpin, &QSpinBox::editingFinished, this,
			[this]() { m_client->setWhiteBalanceTint(m_tintSpin->value()); });

		QPushButton *autoWBBtn = new QPushButton("Auto WB");
		connect(autoWBBtn, &QPushButton::clicked, this,
			&CameraControlWidget::onAutoWBClicked);
		vl->addWidget(autoWBBtn);

		// WB presets — wrap to next row naturally in a flow-style grid
		struct WBPreset { const char *label; int kelvin; int tint; };
		static const WBPreset wbPresets[] = {{"Sun", 5600, 10},
						     {"Tungsten", 3200, 0},
						     {"Fluor.", 4000, 15},
						     {"Shade", 4500, 15},
						     {"Cloud", 6500, 10}};
		QHBoxLayout *presetRow = new QHBoxLayout;
		presetRow->setSpacing(3);
		for (const auto &p : wbPresets) {
			QPushButton *btn = new QPushButton(p.label);
			btn->setToolTip(QString("%1 K").arg(p.kelvin));
			btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
			int k = p.kelvin, t = p.tint;
			connect(btn, &QPushButton::clicked, this,
				[this, k, t]() { onWBPresetClicked(k, t); });
			presetRow->addWidget(btn);
		}
		vl->addLayout(presetRow);
	}

	// ── Color / Contrast group ────────────────────────────────────────────
	{
		m_colorGroup = new QGroupBox("Color / Contrast", content);
		m_colorGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		QGridLayout *gl = new QGridLayout(m_colorGroup);
		gl->setSpacing(4);
		gl->setColumnStretch(1, 1);
		int row = 0;

		m_contrastAdjust = new QDoubleSpinBox;
		m_contrastAdjust->setRange(0.0, 2.0);
		m_contrastAdjust->setSingleStep(0.05);
		m_contrastAdjust->setValue(1.0);
		m_contrastAdjust->setToolTip("1.0 = no change");
		gl->addWidget(new QLabel("Contrast"), row, 0);
		gl->addWidget(m_contrastAdjust, row++, 1);

		m_contrastPivot = new QDoubleSpinBox;
		m_contrastPivot->setRange(0.0, 1.0);
		m_contrastPivot->setSingleStep(0.05);
		m_contrastPivot->setValue(0.5);
		gl->addWidget(new QLabel("Pivot"), row, 0);
		gl->addWidget(m_contrastPivot, row++, 1);

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
		gl->addWidget(new QLabel("Hue"), row, 0);
		gl->addWidget(m_hueSpin, row++, 1);

		m_satSpin = new QSpinBox;
		m_satSpin->setRange(0, 200);
		m_satSpin->setValue(100);
		m_satSpin->setSuffix("%");
		gl->addWidget(new QLabel("Saturation"), row, 0);
		gl->addWidget(m_satSpin, row++, 1);

		connect(m_hueSpin, &QSpinBox::editingFinished, this, [this]() {
			m_client->setColor(m_hueSpin->value(), m_satSpin->value() / 100.0);
		});
		connect(m_satSpin, &QSpinBox::editingFinished, this, [this]() {
			m_client->setColor(m_hueSpin->value(), m_satSpin->value() / 100.0);
		});
	}

	// ── Presets group ─────────────────────────────────────────────────────
	{
		m_presetsGroup = new QGroupBox("Presets", content);
		m_presetsGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		QVBoxLayout *vl = new QVBoxLayout(m_presetsGroup);
		vl->setSpacing(4);

		m_presetList = new QListWidget;
		vl->addWidget(m_presetList);

		QHBoxLayout *btnRow = new QHBoxLayout;
		QPushButton *applyBtn = new QPushButton("Apply");
		QPushButton *refreshBtn = new QPushButton("Refresh");
		btnRow->addWidget(applyBtn);
		btnRow->addWidget(refreshBtn);
		vl->addLayout(btnRow);

		connect(applyBtn, &QPushButton::clicked, this,
			&CameraControlWidget::onApplyPresetClicked);
		connect(refreshBtn, &QPushButton::clicked, this,
			&CameraControlWidget::onRefreshPresetsClicked);
	}

	// Start in narrow (single-column) mode — relayout() will upgrade if needed
	relayout(false);
	root->addLayout(m_groupGrid);
	root->addStretch();

	scroll->setWidget(content);

	QVBoxLayout *outerLayout = new QVBoxLayout(this);
	outerLayout->setContentsMargins(0, 0, 0, 0);
	outerLayout->addWidget(scroll);
}

// ── connection slots ──────────────────────────────────────────────────────────

void CameraControlWidget::onConnectClicked()
{
	if (m_client->isConnected()) {
		m_pollTimer->stop();
		m_client->disconnectFromCamera();
	} else {
		QString host = m_hostnameEdit->text().trimmed();
		if (host.isEmpty())
			return;
		saveHostname(host);
		m_statusLabel->setText("Connecting…");
		m_statusLabel->setStyleSheet("color: orange; font-size: 11px;");
		m_connectBtn->setEnabled(false);
		m_client->connectToCamera(host);
	}
}

void CameraControlWidget::onConnected()
{
	m_statusLabel->setText(QString("● %1").arg(m_client->hostname()));
	m_statusLabel->setStyleSheet("color: #4c4; font-size: 11px;");
	m_connectBtn->setText("Disconnect");
	m_connectBtn->setEnabled(true);
	setControlsEnabled(true);
	m_client->fetchPresets();
	m_client->poll();
	m_pollTimer->start();
}

void CameraControlWidget::onConnectFailed(const QString &reason)
{
	m_statusLabel->setText(QString("✕ %1").arg(reason));
	m_statusLabel->setStyleSheet("color: #c44; font-size: 11px;");
	m_connectBtn->setText("Connect");
	m_connectBtn->setEnabled(true);
	setControlsEnabled(false);
}

void CameraControlWidget::onDisconnected()
{
	m_statusLabel->setText("Not connected");
	m_statusLabel->setStyleSheet("color: gray; font-size: 11px;");
	m_connectBtn->setText("Connect");
	setControlsEnabled(false);
	m_presetList->clear();
}

// ── preset slots ──────────────────────────────────────────────────────────────

void CameraControlWidget::onPresetsReceived(const QStringList &presets)
{
	m_presetList->clear();
	for (const QString &p : presets)
		m_presetList->addItem(p);
}

void CameraControlWidget::onApplyPresetClicked()
{
	QListWidgetItem *item = m_presetList->currentItem();
	if (item)
		m_client->applyPreset(item->text());
}

void CameraControlWidget::onRefreshPresetsClicked()
{
	m_client->fetchPresets();
}

void CameraControlWidget::onWBPresetClicked(int kelvin, int tint)
{
	m_wbSpin->setValue(kelvin);
	m_tintSpin->setValue(tint);
	m_client->setWhiteBalance(kelvin);
	m_client->setWhiteBalanceTint(tint);
}

void CameraControlWidget::onAutoWBClicked()
{
	m_client->doAutoWhiteBalance();
}

// ── poll result slots ─────────────────────────────────────────────────────────

void CameraControlWidget::onShutterReceived(int value, bool isAngle)
{
	if (!m_shutterAngleMode->hasFocus())
		m_shutterAngleMode->setChecked(isAngle);
	safeSet(m_shutterSpin, value);
}

void CameraControlWidget::onISOReceived(int iso)       { safeSet(m_isoSpin, iso); }
void CameraControlWidget::onGainReceived(int db)       { safeSet(m_gainSpin, db); }
void CameraControlWidget::onNDReceived(int stop)       { safeSet(m_ndSpin, stop); }
void CameraControlWidget::onTintReceived(int tint)     { safeSet(m_tintSpin, tint); }

void CameraControlWidget::onWBReceived(int kelvin)
{
	if (!m_wbSpin->hasFocus() && !m_wbSlider->hasFocus()) {
		m_wbSlider->blockSignals(true);
		m_wbSlider->setValue(kelvin);
		m_wbSlider->blockSignals(false);
		m_wbSpin->blockSignals(true);
		m_wbSpin->setValue(kelvin);
		m_wbSpin->blockSignals(false);
	}
}

void CameraControlWidget::onContrastReceived(double pivot, double adjust)
{
	if (!m_contrastAdjust->hasFocus()) m_contrastAdjust->setValue(adjust);
	if (!m_contrastPivot->hasFocus())  m_contrastPivot->setValue(pivot);
}

void CameraControlWidget::onColorReceived(double hueDegrees, double saturation)
{
	safeSet(m_hueSpin, static_cast<int>(hueDegrees));
	safeSet(m_satSpin, static_cast<int>(saturation * 100.0));
}

// ── helpers ───────────────────────────────────────────────────────────────────

void CameraControlWidget::setControlsEnabled(bool enabled)
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

void CameraControlWidget::saveHostname(const QString &hostname)
{
	QSettings s(SETTINGS_ORG, SETTINGS_APP);
	s.setValue("hostname", hostname);
}

QString CameraControlWidget::loadHostname() const
{
	QSettings s(SETTINGS_ORG, SETTINGS_APP);
	return s.value("hostname", "").toString();
}
