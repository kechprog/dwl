#include <iostream>
#include <QApplication>
#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QDialog>
#include <LayerShellQt/Window>
#include <LayerShellQt/Shell>

QDialog* createChildWindow(QWidget* parentWidget) {
    QDialog *child = new QDialog(parentWidget);
    child->setWindowTitle("Pop-up Window");
    child->setAttribute(Qt::WA_NativeWindow);

    QVBoxLayout *layout = new QVBoxLayout(child);
    QLabel *label = new QLabel("This is a pop-up window", child);
    layout->addWidget(label);
    child->setLayout(layout);

    auto winHandle = LayerShellQt::Window::get(child->windowHandle());
    winHandle->setExclusiveZone(60);
    winHandle->setExclusiveEdge(LayerShellQt::Window::AnchorTop);
    winHandle->setCloseOnDismissed(true);
    winHandle->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);

    return child;
}

int main(int argc, char *argv[]) {
    LayerShellQt::Shell::useLayerShell();
    QApplication app(argc, argv);

    QWidget mainWindow;
    mainWindow.winId(); // important!!! it forces "windowHandle()" to get valid "QWindow*"
    QVBoxLayout *layout = new QVBoxLayout(&mainWindow);

    QPushButton *buttonClose = new QPushButton("Close");
    layout->addWidget(buttonClose);
    QObject::connect(buttonClose, &QPushButton::clicked, [](){
        std::exit(0);
    });

    QPushButton *buttonPopUp = new QPushButton("PopUp!");
    layout->addWidget(buttonPopUp);
    QObject::connect(buttonPopUp, &QPushButton::clicked, [&](){
        auto child = createChildWindow(&mainWindow);
        child->show();
    });

    mainWindow.setLayout(layout);

    auto winHandle = LayerShellQt::Window::get(mainWindow.windowHandle());

    winHandle->setLayer(LayerShellQt::Window::LayerTop);
    winHandle->setAnchors(LayerShellQt::Window::AnchorTop);
    winHandle->setExclusiveZone(60);
    winHandle->setExclusiveEdge(LayerShellQt::Window::AnchorTop);
    winHandle->setCloseOnDismissed(true);
    winHandle->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone); // TODO: fix this on compositor side

    mainWindow.show();

    return app.exec();
}
