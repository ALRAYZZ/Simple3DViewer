// Basic QMainWindow setup.

#pragma once
#include <QMainWindow>
#include <QPushButton>

class D3D12Viewport;
class Model;

class MainWindow : public QMainWindow
{
	Q_OBJECT


public slots:
	void openFile();
	void toggleWireframe();

public:
	MainWindow(QWidget* parent = nullptr);
	~MainWindow();

private:
	D3D12Viewport* viewport;
	Model* model;
	QPushButton* wireframeButton;
};