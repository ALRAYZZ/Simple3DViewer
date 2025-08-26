// Basic UI layout

#include <QMenuBar>
#include <QFileDialog>
#include "MainWindow.h"
#include "D3D12Viewport.h"
#include "Model.h"

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
	setWindowTitle("Simple 3D Object Viewer");
	
	QMenuBar* menuBar = new QMenuBar(this);
	setMenuBar(menuBar);
	QMenu* fileMenu = menuBar->addMenu("File");
	QAction* openAction = fileMenu->addAction("Open");
	connect(openAction, &QAction::triggered, this, &MainWindow::openFile);

	resize(800, 600);
	viewport = new D3D12Viewport(this);
	setCentralWidget(viewport);
	model = new Model();
}

MainWindow::~MainWindow() {}

void MainWindow::openFile()
{
	QString filePath = QFileDialog::getOpenFileName(this, "Open 3D Model", "", "3D Models (*.obj *.fbx *.gltf)");
	if (!filePath.isEmpty())
	{
		if (model->loadFromFile(filePath))
		{
			viewport->loadModel(model);
			viewport->update(); // Placeholder to trigger a redraw
		}
	}
}