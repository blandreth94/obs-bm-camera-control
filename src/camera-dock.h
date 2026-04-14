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
#include <QGridLayout>
#include <QGroupBox>
#include <QResizeEvent>

class CameraClient;

// Width threshold (px) below which we collapse to a single column
static constexpr int WIDE_THRESHOLD = 480;

class CameraControlWidget : public QWidget {
	Q_OBJECT

public:
	explicit CameraControlWidget(QWidget *parent = nullptr);
	~CameraControlWidget();

protected:
	void resizeEvent(QResizeEvent *event) override;

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
	void relayout(bool wide);
	void setControlsEnabled(bool enabled);
	void saveHostname(const QString &hostname);
	QString loadHostname() const;

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

	// Group boxes kept as members so relayout() can reposition them
	QGroupBox *m_exposureGroup;
	QGroupBox *m_wbGroup;
	QGroupBox *m_colorGroup;
	QGroupBox *m_presetsGroup;

	// The grid that holds the four groups — resized dynamically
	QGridLayout *m_groupGrid;

	CameraClient *m_client;
	QTimer *m_pollTimer;
	bool m_wideMode = false;
};
