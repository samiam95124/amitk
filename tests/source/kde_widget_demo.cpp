/*******************************************************************************
*                                                                              *
*                      WIDGET BUSY BOX: THE QT EDITION                         *
*                                                                              *
*                    Copyright (C) 2026 Scott A. Franco                        *
*                                                                              *
* The same page as tests/source/widgets_demo.c, built on Qt instead of on      *
* Ami, so that the two can be set beside each other: the same widgets, the     *
* same arrangement, the same dialogs, drawn by the toolkit KDE is built on.    *
*                                                                              *
* This one is C++ because Qt is: the toolkit has no C binding, and none can    *
* be written for it, its whole interface being classes and its signals being   *
* a language extension. The Ami page and the GTK page are C.                   *
*                                                                              *
* Where Qt names a widget differently the nearest one is used and the Ami      *
* name is kept on the label, so the pages read the same down the columns:      *
*                                                                              *
*     Ami                    Qt                                                *
*     button                 QPushButton                                       *
*     checkbox               QCheckBox                                         *
*     radio button           QRadioButton                                      *
*     edit box               QLineEdit                                         *
*     number select box      QSpinBox                                          *
*     drop box               QComboBox                                         *
*     drop edit box          QComboBox, editable                               *
*     list box               QListWidget                                       *
*     tab bar                QTabWidget                                        *
*     scroll bar             QScrollBar                                        *
*     slider                 QSlider                                           *
*     group box              QGroupBox                                         *
*     background             QFrame                                            *
*     progress bar           QProgressBar                                      *
*                                                                              *
* The dialogs are Qt's own: QMessageBox for the alert, and the colour, file    *
* open, file save and font dialogs. Qt has no find or find/replace dialog, so  *
* those two buttons say so rather than pretending.                             *
*                                                                              *
* Build:                                                                       *
*                                                                              *
*     make kde_widget_demo                                                     *
*                                                                              *
*******************************************************************************/

#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QRadioButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QListWidget>
#include <QTabWidget>
#include <QScrollBar>
#include <QSlider>
#include <QGroupBox>
#include <QFrame>
#include <QProgressBar>
#include <QBoxLayout>
#include <QTimer>
#include <QMessageBox>
#include <QColorDialog>
#include <QFileDialog>
#include <QFontDialog>
#include <QButtonGroup>

#define GAP 12  /* space between widgets */

static QLabel* statlab;   /* the line that reports what the widgets say */

/* say what happened, on the top line */
static void status(const QString& s) { statlab->setText(s); }

/* a column of widgets under a heading */
static QVBoxLayout* column(QHBoxLayout* into, const char* head)

{

    QVBoxLayout* c = new QVBoxLayout;

    c->setSpacing(GAP);
    c->addWidget(new QLabel(head));
    c->addStretch(1); /* the column packs upward, as the Ami page does */
    into->addLayout(c);

    return (c);

}

/* add to a column, keeping the stretch at the bottom */
static void put(QVBoxLayout* c, QWidget* w)
    { c->insertWidget(c->count()-1, w); }
static void putl(QVBoxLayout* c, QLayout* l)
    { c->insertLayout(c->count()-1, l); }

int main(int argc, char* argv[])

{

    QApplication app(argc, argv);
    QWidget      win;
    QStringList  items = { "Red", "Green", "Blue" };

    win.setWindowTitle("Qt widget busy box");

    QVBoxLayout* page = new QVBoxLayout(&win);
    page->setSpacing(GAP);

    statlab = new QLabel("every widget Qt has, beside the Ami page. Work one, "
                         "or press a dialog button.");
    page->addWidget(statlab);

    QHBoxLayout* cols = new QHBoxLayout;
    cols->setSpacing(GAP*2);
    page->addLayout(cols);

    /* ------------------------------------------------ column 1: controls */
    QVBoxLayout* c1 = column(cols, "Controls");

    QPushButton* b = new QPushButton("Press me");
    QObject::connect(b, &QPushButton::clicked,
                     [] { status("button pressed"); });
    put(c1, b);

    QCheckBox* ck = new QCheckBox("Checkbox");
    QObject::connect(ck, &QCheckBox::toggled, [](bool on)
                     { status(on? "checkbox checked": "checkbox unchecked"); });
    put(c1, ck);

    QRadioButton* r1 = new QRadioButton("Radio one");
    QRadioButton* r2 = new QRadioButton("Radio two");
    r1->setChecked(true);
    QObject::connect(r1, &QRadioButton::toggled, [](bool on)
                     { if (on) status("radio button 1 chosen"); });
    QObject::connect(r2, &QRadioButton::toggled, [](bool on)
                     { if (on) status("radio button 2 chosen"); });
    put(c1, r1);
    put(c1, r2);

    put(c1, new QLabel("Edit box"));
    QLineEdit* ed = new QLineEdit("Type here");
    QObject::connect(ed, &QLineEdit::returnPressed, [ed]
                     { status("edit box says: "+ed->text()); });
    put(c1, ed);

    put(c1, new QLabel("Number select"));
    QSpinBox* sp = new QSpinBox;
    sp->setRange(1, 10);
    QObject::connect(sp, &QSpinBox::valueChanged, [](int v)
                     { status(QString("number select box: %1").arg(v)); });
    put(c1, sp);

    /* --------------------------------------------------- column 2: lists */
    QVBoxLayout* c2 = column(cols, "Lists");

    put(c2, new QLabel("Drop box"));
    QComboBox* db = new QComboBox;
    db->addItems(items);
    QObject::connect(db, &QComboBox::currentIndexChanged, [](int i)
                     { status(QString("drop box: item %1").arg(i+1)); });
    put(c2, db);

    put(c2, new QLabel("Drop edit box"));
    QComboBox* de = new QComboBox;
    de->addItems(items);
    de->setEditable(true);
    QObject::connect(de, &QComboBox::currentTextChanged, [](const QString& t)
                     { status("drop edit box says: "+t); });
    put(c2, de);

    put(c2, new QLabel("List box"));
    QListWidget* lb = new QListWidget;
    lb->addItems(items);
    lb->setFixedHeight(90);
    QObject::connect(lb, &QListWidget::currentRowChanged, [](int i)
                     { if (i >= 0) status(QString("list box: item %1").arg(i+1)); });
    put(c2, lb);

    put(c2, new QLabel("Tab bar"));
    QTabWidget* tb = new QTabWidget;
    for (const char* t : { "One", "Two", "Three" }) {

        QWidget* pg = new QWidget;

        pg->setMinimumSize(160, 80);
        tb->addTab(pg, t);

    }
    QObject::connect(tb, &QTabWidget::currentChanged, [](int i)
                     { status(QString("tab bar: tab %1").arg(i+1)); });
    put(c2, tb);

    /* ---------------------------------------- column 3: bars and sliders */
    QVBoxLayout* c3 = column(cols, "Bars and sliders");

    QScrollBar* sh = new QScrollBar(Qt::Horizontal);
    sh->setRange(0, 100);
    sh->setPageStep(10);
    sh->setMinimumWidth(200);
    QObject::connect(sh, &QScrollBar::valueChanged, [](int v)
                     { status(QString("scroll bar horizontal at %1%").arg(v)); });
    put(c3, sh);

    QSlider* slh = new QSlider(Qt::Horizontal);
    slh->setRange(0, 100);
    slh->setTickPosition(QSlider::TicksBelow);
    QObject::connect(slh, &QSlider::valueChanged, [](int v)
                     { status(QString("slider horizontal at %1%").arg(v)); });
    put(c3, slh);

    QHBoxLayout* bars = new QHBoxLayout;
    bars->setSpacing(GAP*2);
    QScrollBar* sv = new QScrollBar(Qt::Vertical);
    sv->setRange(0, 100);
    sv->setPageStep(10);
    sv->setMinimumHeight(160);
    QObject::connect(sv, &QScrollBar::valueChanged, [](int v)
                     { status(QString("scroll bar vertical at %1%").arg(v)); });
    bars->addWidget(sv);

    QSlider* slv = new QSlider(Qt::Vertical);
    slv->setRange(0, 100);
    slv->setTickPosition(QSlider::TicksRight);
    slv->setMinimumHeight(160);
    QObject::connect(slv, &QSlider::valueChanged, [](int v)
                     { status(QString("slider vertical at %1%").arg(v)); });
    bars->addWidget(slv);
    bars->addStretch(1);
    putl(c3, bars);

    /* ----------------------------------------- column 4: the components */
    QVBoxLayout* c4 = column(cols, "Components");

    put(c4, new QLabel("Background"));
    QFrame* bg = new QFrame;
    bg->setFrameShape(QFrame::StyledPanel);
    /* Breeze draws a styled panel with no fill of its own. The colour comes
       from the palette rather than a stylesheet, so it stays the desktop's
       colour when the desktop's changes, which is the whole point of the
       background component. */
    {

        QPalette pal = bg->palette();

        pal.setColor(QPalette::Window, pal.color(QPalette::AlternateBase));
        bg->setPalette(pal);

    }
    bg->setAutoFillBackground(true);
    bg->setMinimumSize(200, 60);
    put(c4, bg);

    QGroupBox* gb = new QGroupBox("Group box");
    gb->setMinimumSize(200, 80);
    put(c4, gb);

    put(c4, new QLabel("Progress bar"));
    QProgressBar* pb = new QProgressBar;
    pb->setRange(0, 100);
    pb->setValue(20);
    pb->setTextVisible(false);
    put(c4, pb);

    /* the progress bar walks itself, as the Ami page's does */
    QTimer* tim = new QTimer(&win);
    QObject::connect(tim, &QTimer::timeout, [pb]
                     { pb->setValue(pb->value() >= 100? 0: pb->value()+2); });
    tim->start(100);

    /* -------------------------------------- the dialogs, along the foot */
    QHBoxLayout* row = new QHBoxLayout;
    row->setSpacing(GAP);
    row->addWidget(new QLabel("Dialogs:"));

    QPushButton* d;
    d = new QPushButton("Alert");
    QObject::connect(d, &QPushButton::clicked, [&win] {
        QMessageBox::information(&win, "Alert",
                                 "This is what an alert looks like.");
        status("alert: closed"); });
    row->addWidget(d);

    d = new QPushButton("Color");
    QObject::connect(d, &QPushButton::clicked, [&win] {
        QColor c = QColorDialog::getColor(Qt::magenta, &win);
        status(c.isValid()? QString("color: red %1 green %2 blue %3")
                                .arg(c.red()).arg(c.green()).arg(c.blue())
                          : "color: cancelled"); });
    row->addWidget(d);

    d = new QPushButton("Open");
    QObject::connect(d, &QPushButton::clicked, [&win] {
        QString s = QFileDialog::getOpenFileName(&win, "Open", "myfile.txt");
        status(s.isEmpty()? "open: cancelled": "open: "+s); });
    row->addWidget(d);

    d = new QPushButton("Save");
    QObject::connect(d, &QPushButton::clicked, [&win] {
        QString s = QFileDialog::getSaveFileName(&win, "Save", "myfile.txt");
        status(s.isEmpty()? "save: cancelled": "save: "+s); });
    row->addWidget(d);

    /* Qt has no find or find/replace dialog: an application brings its own */
    for (const char* what : { "Find", "Replace" }) {

        QString w = what;

        d = new QPushButton(what);
        QObject::connect(d, &QPushButton::clicked, [w] {
            status(w.toLower()+": Qt has no such dialog; an application "
                   "brings its own"); });
        row->addWidget(d);

    }

    d = new QPushButton("Font");
    QObject::connect(d, &QPushButton::clicked, [&win] {
        bool ok = false;
        QFont f = QFontDialog::getFont(&ok, QFont("Sans", 10), &win);
        status(ok? "font: "+f.family(): "font: cancelled"); });
    row->addWidget(d);

    row->addStretch(1);
    page->addLayout(row);

    /* Qt sizes the window from what the widgets ask for, which is the same
       thing the Ami page works out for itself in its measuring pass */
    win.show();

    return (app.exec());

}
