#include <functional>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QToolBar>
#include <QLineF>
#include <QMouseEvent>
#include <QFileDialog>
#include <QStandardPaths>
#include <QMessageBox>
#include <QGraphicsPathItem>
#include <QMainWindow>
#include <QStyleOptionGraphicsItem>
#include <QColorDialog>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QAction>
#include <QActionGroup>
#include <QWindow>
#include <QScreen>
#include <QApplication>
#include <QDesktopWidget>
#include <QX11Info>
#include <X11/Xlib.h>

class DragSizeable {
public:
    virtual void sizeTo(QPointF) = 0;
    virtual void lostFocus() {}
};

template<typename Parent, typename...Args>
class SceneItem : public Parent, public DragSizeable {
public:
    SceneItem(QGraphicsScene* scene, Args...args) :
        Parent(args...)
    {
        this->setFlag(Parent::ItemIsMovable);
        this->setFlag(Parent::ItemIsSelectable);
        this->setFlag(Parent::ItemIsFocusable);
        this->setFlag(Parent::ItemSendsGeometryChanges);
        scene->addItem(this);
    }
protected:
    void paintRubberBand(QPainter* painter, const QStyleOptionGraphicsItem* option) {
        // based on Qt source
        if(option->state & QStyle::State_Selected) {
            const QColor fgcolor = option->palette.windowText().color();
            const QColor bgcolor( // ensure good contrast against fgcolor
                fgcolor.red()   > 127 ? 0 : 255,
                fgcolor.green() > 127 ? 0 : 255,
                fgcolor.blue()  > 127 ? 0 : 255);
            qreal pad = 4;
            painter->setPen(QPen(bgcolor, 0, Qt::SolidLine));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(this->boundingRect().adjusted(pad, pad, -pad, -pad));

            painter->setPen(QPen(option->palette.windowText(), 0, Qt::DashLine));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(this->boundingRect().adjusted(pad, pad, -pad, -pad));
        }

    }
};

class Arrow : public SceneItem<QAbstractGraphicsShapeItem> {
public:
    Arrow(QGraphicsScene* scene, QPointF origin, QColor colour) :
    SceneItem<QAbstractGraphicsShapeItem>(scene),
      line(origin,origin),
      colour(colour)
    {
        QPen p(QColor(255,255,255), 2);
        p.setJoinStyle(Qt::MiterJoin);
        setPen(p);
    }

    void sizeTo(QPointF x) override{
        prepareGeometryChange();
        line.setP2(x);
    }

    QPainterPath shape() const override {
        QPainterPathStroker ps;
        ps.setWidth(pen().widthF());
        ps.setJoinStyle(pen().joinStyle());
        ps.setMiterLimit(pen().miterLimit());

        QPainterPath p = path();
        QPainterPath s = ps.createStroke(p);
        s.addPath(p);
        return s;
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0) override {
        painter->setPen(pen());
        painter->drawPath(path());
        painter->fillPath(path(), colour);

        paintRubberBand(painter, option);
    }

    QRectF boundingRect() const override {
        return shape().controlPointRect();
    }

private:

    QPainterPath path() const {
        QPainterPath path;
        path.moveTo(line.p1());
        path.lineTo(vertex(0.8, 3));
        path.lineTo(vertex(0.8, 8));
        path.lineTo(line.p2());
        path.lineTo(vertex(0.8, -8));
        path.lineTo(vertex(0.8, -3));
        path.lineTo(line.p1());

        return path;

    }

    QPointF vertex(qreal mhyp, qreal dang) const {
        return QPointF(
            line.p1().x() + (line.length() * mhyp) * cos((line.angle() + dang) * M_PI / 180.0),
            line.p1().y() - (line.length() * mhyp) * sin((line.angle() + dang) * M_PI / 180.0)
        );
    }

    QLineF line;
    QColor colour;
};

class Ellipse : public SceneItem<QGraphicsEllipseItem> {
public:
    Ellipse(QGraphicsScene* scene, QPointF origin, QColor colour) :
        SceneItem<QGraphicsEllipseItem>(scene),
        colour(colour)
    {
        setPen(QPen(Qt::white, 10));
        setRect(origin.x(),origin.y(),0,0);
    }

    void sizeTo(QPointF p) override {
        QRectF r = rect();
        r.setHeight(p.y() - r.topLeft().y());
        r.setWidth(p.x() - r.topLeft().x());
        prepareGeometryChange();
        setRect(r);
        //update();
    }
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0) override {
        QPainterPath p;
        p.addEllipse(rect());
        painter->setPen(pen());
        painter->drawPath(p);
        painter->setPen(QPen(colour, 8));
        painter->drawPath(p);
        paintRubberBand(painter, option);
    }

private:
    QColor colour;
};

class Text : public SceneItem<QGraphicsTextItem> {
public:
    Text(QGraphicsScene* scene, QPointF origin, QColor colour) :
        SceneItem<QGraphicsTextItem>(scene),
        colour(colour)
    {
        setPos(origin);
        QTextCharFormat fmt;
        fmt.setFont(QFont("sans", 24, 75));
        fmt.setForeground(colour);
        fmt.setTextOutline(QPen(QColor(255,255,255), 1));
        QTextCursor c(document());
        c.setCharFormat(fmt);
        setTextCursor(c);
    }

    void sizeTo(QPointF p) override {
        setTextWidth(p.x() - pos().x());
    }

    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*e) override {
        setTextInteractionFlags(Qt::TextEditorInteraction);
        return QGraphicsTextItem::mouseDoubleClickEvent(e);
    }

    void lostFocus() override {
        setTextInteractionFlags(Qt::TextEditorInteraction);
    }
private:
    QColor colour;
};

class Screencap : public SceneItem<QGraphicsPixmapItem,QPixmap> {
public:
    Screencap(QGraphicsScene* scene, const QPixmap& p, bool fixed = false) :
        SceneItem<QGraphicsPixmapItem,QPixmap>(scene, p)
    {
        setPos((scene->width() - p.width()) / 2, (scene->height()-p.height()) / 2);
        scene->clearSelection();
        setSelected(true);
        if(fixed) {
            setFlag(ItemIsMovable, false);
            setFlag(ItemIsSelectable, false);
            setFlag(ItemIsFocusable, false);
        }
    }
    void sizeTo(QPointF) override {}
};

enum Action {
    ACTION_MOUSE,
    ACTION_ARROW,
    ACTION_ELLIPSE,
    ACTION_TEXT
};

class Canvas : public QGraphicsView {
public:
    Canvas(std::function<Action()> act, std::function<void()> dc, QWidget* parent = 0) :
        QGraphicsView(parent),
        currentColour(Qt::red),
        currentItem(0),
        currentAction(act),
        doneCreate(dc)
    {
        setRenderHint(QPainter::Antialiasing);
    }

    void mousePressEvent(QMouseEvent* e) override {
        switch(currentAction()) {
        case ACTION_ARROW:
            currentItem = new Arrow(scene(), mapToScene(e->pos()), currentColour);
            break;
        case ACTION_ELLIPSE:
            currentItem = new Ellipse(scene(), mapToScene(e->pos()), currentColour);
            break;
        case ACTION_TEXT:
            currentItem = new Text(scene(), mapToScene(e->pos()), currentColour);
        default:
            return QGraphicsView::mousePressEvent(e);
            break;
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        if(currentItem && (e->buttons() & Qt::LeftButton))
            currentItem->sizeTo(mapToScene(e->pos()));
        else
            QGraphicsView::mouseMoveEvent(e);
    }

    void mouseReleaseEvent(QMouseEvent* e) override {
        if(currentItem) {
            currentItem->lostFocus();
            currentItem = 0;
            doneCreate();
        }
        QGraphicsView::mouseReleaseEvent(e);

    }

    void keyPressEvent(QKeyEvent* e) override {
        if(e->key() == Qt::Key_Delete)
            qDeleteAll(scene()->selectedItems());
        else
            QGraphicsView::keyPressEvent(e);
    }

    QColor currentColour;
private:
    DragSizeable* currentItem;
    std::function<Action()> currentAction;
    std::function<void()> doneCreate;
};

class OverlayWin : public QMainWindow {
public:
    OverlayWin() :
        QMainWindow()
    {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setWindowState(Qt::WindowFullScreen);
        setWindowFlags(Qt::FramelessWindowHint);
    }

    void clearRect(QRect r) {
        clear = r;
        update();
    }

protected:
    virtual void paintEvent(QPaintEvent * e) override {
        QPainter painter(this);
        QPainterPath p;
        p.addRect(0,0,width(), height());
        QPainterPath s;
        s.addRect(clear);
        painter.fillPath(p.subtracted(s),  QBrush(QColor(0,0,0,80)));
    }

private:
    QRect clear;
};

class ColouredIcon : public QIcon {
public:

};

class Scappit : public QMainWindow {
public:
    explicit Scappit(QString filename = QString());
    ~Scappit() {}
    void capture();
protected:
    void mouseReleaseEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void closeEvent(QCloseEvent * e) override {
        if(dirty) {
            QString question = filename.isEmpty()
                    ? "The current file is not saved"
                    : QString("The file '") + filename + "' is not saved";
            QMessageBox::StandardButton response = QMessageBox::question(this,
                 question, "Do you want to save it before closing?",
                 QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
            if(response == QMessageBox::Yes) {
                saveAction->trigger();
                if(!dirty) // save successful
                    e->accept();
                else
                    e->ignore();
            } else if(response == QMessageBox::No) {
                e->accept();
            } else { // response == QMessageBox::Cancel
                e->ignore();
            }
        } else {
            e->accept();
        }
    }
    void setDirty(bool v) {
        dirty = v;
        saveAction->setEnabled(dirty);
        // save as becomes permanently available as soon as the doc is touched
        saveAsAction->setEnabled(true);
        fixedCapture = false;
    }
    void setFilename(QString f) {
        filename = f;
        setWindowTitle(QFileInfo(f).fileName() + " - Scappit");
    }

private:
    QGraphicsScene* scene;
    Canvas* view;
    QActionGroup* actions;
    QAction* saveAction;
    QAction* saveAsAction;
    bool grabbing;
    QRect captureRegion;
    OverlayWin overlay;
    bool dirty;
    QString filename;
    bool fixedCapture;
};

Scappit::Scappit(QString filename) :
    QMainWindow(nullptr),
    grabbing(false),
    dirty(false),
    fixedCapture(true)
{
    setWindowTitle("Scappit");
    setWindowIcon(QIcon::fromTheme("applets-screenshooter"));
    scene = new QGraphicsScene(this);

    QToolBar* bar = new QToolBar(this);
    bar->setMovable(false);
    bar->setIconSize(QSize(22,22));

    actions = new QActionGroup(bar);
    connect(scene, &QGraphicsScene::focusItemChanged, [](QGraphicsItem *newFocus, QGraphicsItem *oldFocus, Qt::FocusReason reason){
        if(oldFocus && oldFocus->type() == QGraphicsTextItem::Type) {
            static_cast<QGraphicsTextItem*>(oldFocus)->setTextInteractionFlags(Qt::NoTextInteraction);
        }
    });
    QAction* a;

    a = bar->addAction(QIcon(":cursor"), "Pointer");
    a->setData(ACTION_MOUSE);
    a->setCheckable(true);
    a->setChecked(true);
    actions->addAction(a);

    a = bar->addAction(QIcon(":arrow"), "Draw Arrow");
    a->setData(ACTION_ARROW);
    a->setCheckable(true);
    actions->addAction(a);

    a = bar->addAction(QIcon(":ellipse"), "Draw Ellipse");
    a->setData(ACTION_ELLIPSE);
    a->setCheckable(true);
    actions->addAction(a);

    a = bar->addAction(QIcon(":text"), "Insert Text");
    a->setData(ACTION_TEXT);
    a->setCheckable(true);
    actions->addAction(a);

    auto colourPickerIcon = [](QColor c){
        QPixmap square(18, 12);
        square.fill(c);
        return QIcon(square);
    };

    a = bar->addAction(colourPickerIcon(Qt::red), "Set Colour");
    connect(a, &QAction::triggered, [&,a]{
        QColor newColour = QColorDialog::getColor(view->currentColour);
        if(newColour.isValid()) {
            view->currentColour = newColour;
            a->setIcon(colourPickerIcon(newColour));
        }
    });

    a = bar->addAction(QIcon(":camera"), "Take Screenshot");
    connect(a, &QAction::triggered, [this]{
        capture();
    });

    QWidget* empty = new QWidget();
    empty->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Expanding);
    bar->addWidget(empty);

    a = bar->addAction(QIcon::fromTheme("document-new"), "New File");
    connect(a, &QAction::triggered, []{
        QMainWindow* w = new Scappit(nullptr);
        w->setAttribute(Qt::WA_DeleteOnClose);
        w->show();
    });

    a = bar->addAction(QIcon::fromTheme("document-open"), "Open File");
    connect(a, &QAction::triggered, [this]{
        QString f = QFileDialog::getOpenFileName(this, "Open File",
                                                 QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
                                                 "Images (*.png *.xpm *.jpg)");
        if(!f.isNull()) {
            if(dirty) {
                QMainWindow* w = new Scappit(f);
                w->setAttribute(Qt::WA_DeleteOnClose);
                w->show();
            } else {
                scene->clear();
                new Screencap(scene, QPixmap(f), true);
                setFilename(f);
                setDirty(false);
            }
        }
    });

    auto writePng = [](QGraphicsScene* scene, QString path){
        if(!path.endsWith(".png"))
            path += ".png";
        scene->clearSelection();
        scene->setSceneRect(scene->itemsBoundingRect());
        QImage image(scene->itemsBoundingRect().size().toSize(), QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter p(&image);
        p.setRenderHint(QPainter::Antialiasing);
        scene->render(&p);
        image.save(path);
    };

    a = bar->addAction(QIcon::fromTheme("document-save"), "Save");
    connect(a, &QAction::triggered, [&]{
        if(this->filename.isEmpty())
            saveAsAction->trigger();
        else {
            writePng(scene, this->filename);
            setDirty(false);
        }
    });
    a->setEnabled(false);
    saveAction = a;

    a = bar->addAction(QIcon::fromTheme("document-save-as"), "Save As...");
    connect(a, &QAction::triggered, [&]{
        QString f = QFileDialog::getSaveFileName(this, "Save File",
                                                 QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
                                                                          "PNG Images (*.png)");
        if(!f.isNull()) {
            writePng(scene, f);
            setFilename(f);
            setDirty(false);
        }
    });
    a->setEnabled(false);
    saveAsAction = a;

    addToolBar(Qt::RightToolBarArea, bar);

    if(filename.isNull())
        // sane default
        scene->setSceneRect(0,0,820,560);
    else {
        new Screencap(scene, QPixmap(filename), true);
        setFilename(filename);
        // non-intuitive
        setDirty(false);
    }

    view = new Canvas(
        [=]{ // callback to get current action
            return Action(actions->checkedAction()->data().toInt());
        },
        [=]{ // callback when creation of scene item is complete
            actions->actions().first()->setChecked(true);
            setDirty(true);
        },
        this
    );

    view->setScene(scene);

    setCentralWidget(view);
}

void Scappit::capture() {
    grabbing = true;
    grabMouse(Qt::CrossCursor);
    setMouseTracking(true);
    lower();
}

void Scappit::mousePressEvent(QMouseEvent* e) {
    if(grabbing) {
        captureRegion.setTopLeft(e->globalPos());
        captureRegion.setBottomRight(e->globalPos());
        lower();
    } else
        return QMainWindow::mousePressEvent(e);
}

void Scappit::mouseMoveEvent(QMouseEvent* e) {
    if(grabbing && e->buttons() & Qt::LeftButton) {
        if(captureRegion.topLeft() == captureRegion.bottomRight()) {
            overlay.show();
        }
        captureRegion.setBottomRight(e->globalPos());
        overlay.clearRect(captureRegion);
    } else
        return QMainWindow::mouseMoveEvent(e);
}

void Scappit::mouseReleaseEvent(QMouseEvent* e) {
    if(grabbing) {
        overlay.hide();
        releaseMouse();
        grabbing = false;
        raise();

        if(captureRegion.topLeft() == captureRegion.bottomRight()) {
            Window child;
            long unsigned int c;
            int root_x, root_y, win_x, win_y;
            unsigned int mask;
            XQueryPointer(QX11Info::display(), QX11Info::appRootWindow(), &c, &child, &root_x, &root_y, &win_x, &win_y, &mask);
            new Screencap(scene, windowHandle()->screen()->grabWindow(child), fixedCapture);
        } else {
            QRect r = QRectF(captureRegion).normalized().toAlignedRect();
            new Screencap(scene, windowHandle()->screen()->grabWindow(QX11Info::appRootWindow(), r.x(), r.y(), r.width(), r.height()), fixedCapture);
        }
        if(fixedCapture) {
            QSize s = size() - view->viewport()->size() + scene->sceneRect().size().toSize();
            resize(qMax(s.width(),size().width()), qMax(s.height(),size().height()));
        }
        setDirty(true);
    } else {
        return QMainWindow::mouseReleaseEvent(e);
    }
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    int nWindows = 0;
    for(int i = 1; i < argc; ++i) {
        if(argv[i][0] == '-' && argv[i][1] == 'c' && argv[i][2] == '\0') {
            Scappit* w = new Scappit;
            w->setAttribute(Qt::WA_DeleteOnClose);
            w->show();
            w->capture();
            nWindows++;
            break;
        } else if(QFileInfo(argv[i]).isReadable()) {
            Scappit* w = new Scappit(argv[i]);
            w->setAttribute(Qt::WA_DeleteOnClose);
            w->show();
            nWindows++;
        } else {
            fprintf(stderr, "Usage: scappit [-c] [FILE]...\n\n"
                    "Screen capture and annotation tool\n"
                    "  -c      Start in capture mode\n");
            return 1;
        }
    }
    if(nWindows == 0) {
        Scappit* w = new Scappit;
        w->setAttribute(Qt::WA_DeleteOnClose);
        w->show();
    }
    app.exec();
}
