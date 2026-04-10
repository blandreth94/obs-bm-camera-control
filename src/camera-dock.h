#pragma once

#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QListWidget>
#include <QCheckBox>

class CameraClient;

class CameraDockWidget : public QDockWidget {
	Q_OBJECT

public:
	explicit CameraDockWidget(QWidget *parent = nullptr);
	~CameraDockWidget();

private slots:
	void onConnectClicked();
	void onConnected();
	void onConnectFailed(const QString &reason);
	void onDisconnected();
	void onPresetsReceived(const QStringList &presets);
	void onApplyPresetClicked();
	void onRefreshPresetsClicked();
	void onWBPresetClicked(int kelvin, int tint);
	void onAutoWBClicked();

private:
	void buildUI();
	void setControlsEnabled(bool enabled);
	void saveHostname(const QString &hostname);
	QString loadHostname() const;

	// Connection
	QLineEdit *m_hostnameEdit;
	QPushButton *m_connectBtn;
	QLabel *m_statusLabel;

	// Exposure
	QSpinBox *m_shutterSpin;
	QCheckBox *m_shutterAngleMode;
	QSpinBox *m_gainSpin;
	QSpinBox *m_isoSpin;
	QSpinBox *m_ndSpin;
	QComboBox *m_autoExposureCombo;

	// White balance
	QSlider *m_wbSlider;
	QSpinBox *m_wbSpin;
	QSpinBox *m_tintSpin;

	// Color / contrast
	QDoubleSpinBox *m_contrastAdjust;
	QDoubleSpinBox *m_contrastPivot;
	QSpinBox *m_hueSpin;    // degrees
	QSpinBox *m_satSpin;    // 0–200 (%)

	// Presets
	QListWidget *m_presetList;

	CameraClient *m_client;
};
