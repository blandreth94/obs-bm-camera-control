#pragma once

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QListWidget>
#include <QCheckBox>
#include <QTimer>

class CameraClient;

class CameraControlWidget : public QWidget {
	Q_OBJECT

public:
	explicit CameraControlWidget(QWidget *parent = nullptr);
	~CameraControlWidget();

private slots:
	// Connection
	void onConnectClicked();
	void onConnected();
	void onConnectFailed(const QString &reason);
	void onDisconnected();

	// Preset list
	void onPresetsReceived(const QStringList &presets);
	void onApplyPresetClicked();
	void onRefreshPresetsClicked();

	// WB helpers
	void onWBPresetClicked(int kelvin, int tint);
	void onAutoWBClicked();

	// Poll result slots — only update widgets that don't have focus
	void onShutterReceived(int value, bool isAngle);
	void onISOReceived(int iso);
	void onGainReceived(int db);
	void onNDReceived(int stop);
	void onWBReceived(int kelvin);
	void onTintReceived(int tint);
	void onContrastReceived(double pivot, double adjust);
	void onColorReceived(double hueDegrees, double saturation);

private:
	void buildUI();
	void setControlsEnabled(bool enabled);
	void saveHostname(const QString &hostname);
	QString loadHostname() const;

	// Helper: set a QSpinBox value only when not focused
	template<typename SpinT, typename ValT>
	void safeSet(SpinT *spin, ValT value)
	{
		if (!spin->hasFocus())
			spin->setValue(static_cast<decltype(spin->value())>(value));
	}

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
	QSpinBox *m_hueSpin;
	QSpinBox *m_satSpin;

	// Presets
	QListWidget *m_presetList;

	CameraClient *m_client;
	QTimer *m_pollTimer;
};
