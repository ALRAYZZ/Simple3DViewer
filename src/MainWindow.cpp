// Basic UI layout

#include "MainWindow.h"

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
	setWindowTitle("Simple 3D Object Viewer");
	resize(800, 600);
}

MainWindow::~MainWindow() {}