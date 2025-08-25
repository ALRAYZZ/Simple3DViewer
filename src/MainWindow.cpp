// Basic UI layout

#include "MainWindow.h"
#include "D3D12Viewport.h"

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
	setWindowTitle("Simple 3D Object Viewer");
	resize(800, 600);
	viewport = new D3D12Viewport(this);
	setCentralWidget(viewport);
}

MainWindow::~MainWindow() {}